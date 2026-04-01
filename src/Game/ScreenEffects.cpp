#include "Game/ScreenEffects.h"
#include "States/GameState.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <SDL2/SDL_ttf.h>

// Clamp helper (avoids repeating std::min/max chains)
static inline Uint8 clampAlpha(float v) {
    return static_cast<Uint8>(std::max(0.0f, std::min(255.0f, v)));
}

// Simple pseudo-random float [0,1] for particle init (avoids std::rand overhead)
static inline float randFloat() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

void ScreenEffects::update(float dt) {
    m_time += dt;
    if (m_killFlashTimer > 0) m_killFlashTimer -= dt;
    if (m_bossIntroTimer > 0) m_bossIntroTimer -= dt;
    if (m_rippleTimer > 0) m_rippleTimer -= dt;
    if (m_chromaticTimer > 0) m_chromaticTimer -= dt;
    if (m_entropy > 80.0f) m_glitchTimer += dt;
    else m_glitchTimer = 0;

    // Update ambient particles (use stored screen dimensions from last render call)
    if (ambientParticlesEnabled && m_ambientParticlesInitialized) {
        // Screen dimensions match Game::SCREEN_WIDTH/HEIGHT (logical resolution)
        updateAmbientParticles(dt, GameState::SCREEN_WIDTH, GameState::SCREEN_HEIGHT, m_currentDimension);
    }
}

void ScreenEffects::render(SDL_Renderer* renderer, int screenW, int screenH, TTF_Font* font) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 1. Cinematic vignette (always on — subtle darkened edges/corners, stronger at low HP)
    {
        // Base: very subtle (max alpha ~25), ramps up when HP drops
        float baseStrength = 0.10f; // Subtle permanent vignette
        float hpBoost = (1.0f - m_hpPercent) * 0.30f; // Gets stronger at low HP
        float strength = baseStrength + hpBoost;
        Uint8 vigA = static_cast<Uint8>(std::min(255.0f, strength * 255.0f));

        // ~5% of screen from each edge for base, wider when HP-boosted
        int edgeW = screenW / 15 + static_cast<int>(hpBoost * screenW * 0.08f);
        int edgeH = screenH / 15 + static_cast<int>(hpBoost * screenH * 0.08f);

        // Use coarser step size for performance (2px bands instead of 1px)
        constexpr int kStep = 2;

        // Top edge gradient
        for (int i = 0; i < edgeH; i += kStep) {
            float grad = 1.0f - static_cast<float>(i) / edgeH;
            grad = grad * grad; // Quadratic falloff for smooth cinematic look
            Uint8 a = static_cast<Uint8>(vigA * grad);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {0, i, screenW, kStep};
            SDL_RenderFillRect(renderer, &r);
        }
        // Bottom edge gradient
        for (int i = 0; i < edgeH; i += kStep) {
            float grad = static_cast<float>(i) / edgeH;
            grad = grad * grad;
            Uint8 a = static_cast<Uint8>(vigA * grad);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {0, screenH - edgeH + i, screenW, kStep};
            SDL_RenderFillRect(renderer, &r);
        }
        // Left edge gradient
        for (int i = 0; i < edgeW; i += kStep) {
            float grad = 1.0f - static_cast<float>(i) / edgeW;
            grad = grad * grad;
            Uint8 a = static_cast<Uint8>(vigA * grad);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {i, 0, kStep, screenH};
            SDL_RenderFillRect(renderer, &r);
        }
        // Right edge gradient
        for (int i = 0; i < edgeW; i += kStep) {
            float grad = static_cast<float>(i) / edgeW;
            grad = grad * grad;
            Uint8 a = static_cast<Uint8>(vigA * grad);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {screenW - edgeW + i, 0, kStep, screenH};
            SDL_RenderFillRect(renderer, &r);
        }

        // Corner darkening: extra alpha where two edges overlap (coarse blocks)
        int cornerW = edgeW;
        int cornerH = edgeH;
        Uint8 cornerMaxA = static_cast<Uint8>(std::min(255, static_cast<int>(vigA) * 3 / 4));
        constexpr int kCornerBlock = 6; // Block size for corner fill
        int cornerPositions[4][2] = {
            {0, 0}, {screenW - cornerW, 0},
            {0, screenH - cornerH}, {screenW - cornerW, screenH - cornerH}
        };
        for (auto& cp : cornerPositions) {
            for (int by = 0; by < cornerH; by += kCornerBlock) {
                for (int bx = 0; bx < cornerW; bx += kCornerBlock) {
                    // Distance from the outer corner (0 = at corner, 1 = at inner edge)
                    float fx = static_cast<float>(cp[0] == 0 ? bx : cornerW - bx) / cornerW;
                    float fy = static_cast<float>(cp[1] == 0 ? by : cornerH - by) / cornerH;
                    float d = (1.0f - fx) * (1.0f - fy); // 1 at corner, 0 at inner edge
                    if (d > 0.1f) {
                        Uint8 a = static_cast<Uint8>(cornerMaxA * d * d);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
                        int w = std::min(kCornerBlock, cornerW - bx);
                        int h = std::min(kCornerBlock, cornerH - by);
                        SDL_Rect r = {cp[0] + bx, cp[1] + by, w, h};
                        SDL_RenderFillRect(renderer, &r);
                    }
                }
            }
        }
    }

    // 2. Low HP warning: pulsing red vignette + heartbeat screen pulse
    if (m_hpPercent < 0.25f && m_hpPercent > 0.0f) {
        float intensity = (0.25f - m_hpPercent) / 0.25f; // 0 at 25%, 1 at 0%

        // Heartbeat-like double pulse (lub-dub pattern, ~1Hz cycle)
        // Phase within one heartbeat cycle (0-1 over ~1 second)
        float pulseSpeed = 1.0f + intensity * 0.8f; // Faster at lower HP (1-1.8 Hz)
        float phase = std::fmod(m_time * pulseSpeed, 1.0f);
        float beat = 0.0f;
        // "Lub" (strong beat) at phase 0.0-0.12
        if (phase < 0.12f) {
            float t = phase / 0.12f;
            beat = std::sin(t * 3.14159f); // Quick rise-fall
        }
        // "Dub" (weaker beat) at phase 0.18-0.28
        else if (phase > 0.18f && phase < 0.28f) {
            float t = (phase - 0.18f) / 0.10f;
            beat = 0.6f * std::sin(t * 3.14159f); // Softer second beat
        }

        // Red vignette gradient at all four edges
        float vigStrength = (0.3f + beat * 0.7f) * intensity;
        int edgeW = static_cast<int>(screenW * (0.06f + vigStrength * 0.08f)); // 6-14% of screen width
        int edgeH = static_cast<int>(screenH * (0.06f + vigStrength * 0.08f));
        Uint8 maxAlpha = static_cast<Uint8>(40 + vigStrength * 50); // 40-90 max alpha

        // Top edge
        for (int i = 0; i < edgeH; i++) {
            float grad = 1.0f - static_cast<float>(i) / edgeH;
            grad = grad * grad; // Quadratic falloff for smoother gradient
            Uint8 a = static_cast<Uint8>(maxAlpha * grad);
            SDL_SetRenderDrawColor(renderer, 160, 15, 15, a);
            SDL_Rect r = {0, i, screenW, 1};
            SDL_RenderFillRect(renderer, &r);
        }
        // Bottom edge
        for (int i = 0; i < edgeH; i++) {
            float grad = static_cast<float>(i) / edgeH;
            grad = grad * grad;
            Uint8 a = static_cast<Uint8>(maxAlpha * grad);
            SDL_SetRenderDrawColor(renderer, 160, 15, 15, a);
            SDL_Rect r = {0, screenH - edgeH + i, screenW, 1};
            SDL_RenderFillRect(renderer, &r);
        }
        // Left edge
        for (int i = 0; i < edgeW; i++) {
            float grad = 1.0f - static_cast<float>(i) / edgeW;
            grad = grad * grad;
            Uint8 a = static_cast<Uint8>(maxAlpha * grad);
            SDL_SetRenderDrawColor(renderer, 160, 15, 15, a);
            SDL_Rect r = {i, 0, 1, screenH};
            SDL_RenderFillRect(renderer, &r);
        }
        // Right edge
        for (int i = 0; i < edgeW; i++) {
            float grad = static_cast<float>(i) / edgeW;
            grad = grad * grad;
            Uint8 a = static_cast<Uint8>(maxAlpha * grad);
            SDL_SetRenderDrawColor(renderer, 160, 15, 15, a);
            SDL_Rect r = {screenW - edgeW + i, 0, 1, screenH};
            SDL_RenderFillRect(renderer, &r);
        }

        // Corner darkening (coarse blocks for performance — 4 corners)
        int cornerSize = std::min(edgeW, edgeH);
        Uint8 cornerAlpha = static_cast<Uint8>(maxAlpha * 0.4f);
        constexpr int kCornerStep = 8; // Block size for corner fill
        // All four corners
        int cornerPositions[4][2] = {{0, 0}, {screenW - cornerSize, 0},
                                     {0, screenH - cornerSize}, {screenW - cornerSize, screenH - cornerSize}};
        for (auto& cp : cornerPositions) {
            for (int by = 0; by < cornerSize; by += kCornerStep) {
                for (int bx = 0; bx < cornerSize; bx += kCornerStep) {
                    // Distance from the corner (0=at corner, 1=at edge of zone)
                    float fx = static_cast<float>(cp[0] == 0 ? bx : cornerSize - bx) / cornerSize;
                    float fy = static_cast<float>(cp[1] == 0 ? by : cornerSize - by) / cornerSize;
                    float d = (1.0f - fx) * (1.0f - fy);
                    if (d > 0.15f) {
                        Uint8 a = static_cast<Uint8>(cornerAlpha * d * d);
                        SDL_SetRenderDrawColor(renderer, 140, 10, 10, a);
                        int w = std::min(kCornerStep, cornerSize - bx);
                        int h = std::min(kCornerStep, cornerSize - by);
                        SDL_Rect r = {cp[0] + bx, cp[1] + by, w, h};
                        SDL_RenderFillRect(renderer, &r);
                    }
                }
            }
        }

        // Heartbeat full-screen pulse (very subtle red wash on each beat)
        if (beat > 0.1f) {
            Uint8 pulseAlpha = static_cast<Uint8>(beat * intensity * 18); // Very subtle, max 18
            SDL_SetRenderDrawColor(renderer, 120, 0, 0, pulseAlpha);
            SDL_Rect full = {0, 0, screenW, screenH};
            SDL_RenderFillRect(renderer, &full);
        }
    }

    // 3. Kill flash
    if (m_killFlashTimer > 0) {
        float t = m_killFlashTimer / 0.05f;
        Uint8 a = static_cast<Uint8>(t * 30);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, a);
        SDL_Rect full = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &full);
    }

    // 3b. Chromatic aberration on player damage (brief RGB split at screen edges)
    if (m_chromaticTimer > 0) {
        float t = m_chromaticTimer / 0.15f; // 1→0
        float intensity = t * m_chromaticIntensity;
        int offset = static_cast<int>(3 * intensity); // 0-3 pixel shift
        Uint8 chromA = static_cast<Uint8>(intensity * 50);
        if (offset > 0 && chromA > 5) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            // Red shift: right edge band
            SDL_SetRenderDrawColor(renderer, chromA, 0, 0, chromA);
            SDL_Rect redBand = {screenW - offset * 8, 0, offset * 8, screenH};
            SDL_RenderFillRect(renderer, &redBand);
            // Blue shift: left edge band
            SDL_SetRenderDrawColor(renderer, 0, 0, chromA, chromA);
            SDL_Rect blueBand = {0, 0, offset * 8, screenH};
            SDL_RenderFillRect(renderer, &blueBand);
            // Also top/bottom for completeness
            SDL_SetRenderDrawColor(renderer, chromA, 0, static_cast<Uint8>(chromA / 2), static_cast<Uint8>(chromA * 0.7f));
            SDL_Rect topBand = {0, 0, screenW, offset * 4};
            SDL_RenderFillRect(renderer, &topBand);
            SDL_Rect botBand = {0, screenH - offset * 4, screenW, offset * 4};
            SDL_RenderFillRect(renderer, &botBand);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        }
    }

    // 4. Boss intro cinematic title card
    if (m_bossIntroTimer > 0) {
        // Timeline: fade in 0.5s -> hold 1.5s -> fade out 0.5s (total 2.5s)
        float elapsed = kBossIntroDuration - m_bossIntroTimer;
        float contentAlpha = 0.0f;
        if (elapsed < 0.5f) {
            contentAlpha = elapsed / 0.5f;       // Fade in
        } else if (elapsed < 2.0f) {
            contentAlpha = 1.0f;                  // Hold
        } else {
            contentAlpha = (kBossIntroDuration - elapsed) / 0.5f; // Fade out
        }
        contentAlpha = std::max(0.0f, std::min(1.0f, contentAlpha));

        // Dark overlay (full screen)
        Uint8 overlayAlpha = static_cast<Uint8>(contentAlpha * 180);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, overlayAlpha);
        SDL_Rect fullScreen = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &fullScreen);

        // Cinematic bars (top and bottom)
        float barH = contentAlpha * 120.0f;
        int bH = static_cast<int>(barH);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 240);
        SDL_Rect topBar = {0, 0, screenW, bH};
        SDL_Rect botBar = {0, screenH - bH, screenW, bH};
        SDL_RenderFillRect(renderer, &topBar);
        SDL_RenderFillRect(renderer, &botBar);

        // Horizontal separator lines (red-purple glow)
        if (contentAlpha > 0.1f) {
            Uint8 lineA = static_cast<Uint8>(contentAlpha * 200);
            int lineW = static_cast<int>(contentAlpha * screenW * 0.5f);
            int lineX = screenW / 2 - lineW / 2;

            // Top line (above boss name)
            SDL_SetRenderDrawColor(renderer, 200, 50, 80, lineA);
            SDL_Rect lineTop = {lineX, screenH / 2 - 56, lineW, 4};
            SDL_RenderFillRect(renderer, &lineTop);
            // Glow around top line
            SDL_SetRenderDrawColor(renderer, 200, 50, 80, static_cast<Uint8>(lineA / 3));
            SDL_Rect lineTopGlow = {lineX, screenH / 2 - 60, lineW, 12};
            SDL_RenderFillRect(renderer, &lineTopGlow);

            // Bottom line (below subtitle)
            SDL_SetRenderDrawColor(renderer, 200, 50, 80, lineA);
            SDL_Rect lineBot = {lineX, screenH / 2 + 60, lineW, 4};
            SDL_RenderFillRect(renderer, &lineBot);
            SDL_SetRenderDrawColor(renderer, 200, 50, 80, static_cast<Uint8>(lineA / 3));
            SDL_Rect lineBotGlow = {lineX, screenH / 2 + 56, lineW, 12};
            SDL_RenderFillRect(renderer, &lineBotGlow);
        }

        // Boss name text (large, white with red glow)
        if (font && contentAlpha > 0.05f && !m_bossName.empty()) {
            Uint8 textA = static_cast<Uint8>(contentAlpha * 255);

            // Red glow behind text (rendered first, slightly larger/offset)
            float pulse = 0.8f + 0.2f * std::sin(m_time * 4.0f);
            Uint8 glowA = static_cast<Uint8>(contentAlpha * pulse * 120);
            SDL_Color glowColor = {220, 40, 60, glowA};
            SDL_Surface* glowSurf = TTF_RenderText_Blended(font, m_bossName.c_str(), glowColor);
            if (glowSurf) {
                SDL_Texture* glowTex = SDL_CreateTextureFromSurface(renderer, glowSurf);
                if (glowTex) {
                    SDL_SetTextureAlphaMod(glowTex, glowA);
                    // Render glow slightly offset in multiple directions
                    for (int dx = -2; dx <= 2; dx += 2) {
                        for (int dy = -2; dy <= 2; dy += 2) {
                            if (dx == 0 && dy == 0) continue;
                            SDL_Rect gr = {screenW / 2 - glowSurf->w / 2 + dx,
                                           screenH / 2 - glowSurf->h / 2 - 16 + dy,
                                           glowSurf->w, glowSurf->h};
                            SDL_RenderCopy(renderer, glowTex, nullptr, &gr);
                        }
                    }
                    SDL_DestroyTexture(glowTex);
                }
                SDL_FreeSurface(glowSurf);
            }

            // Main boss name (white)
            SDL_Color nameColor = {255, 255, 255, textA};
            SDL_Surface* nameSurf = TTF_RenderText_Blended(font, m_bossName.c_str(), nameColor);
            if (nameSurf) {
                SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
                if (nameTex) {
                    SDL_SetTextureAlphaMod(nameTex, textA);
                    SDL_Rect nr = {screenW / 2 - nameSurf->w / 2,
                                   screenH / 2 - nameSurf->h / 2 - 16,
                                   nameSurf->w, nameSurf->h};
                    SDL_RenderCopy(renderer, nameTex, nullptr, &nr);
                    SDL_DestroyTexture(nameTex);
                }
                SDL_FreeSurface(nameSurf);
            }

            // Subtitle text (gray-purple, smaller — rendered at smaller scale)
            if (!m_bossSubtitle.empty()) {
                Uint8 subA = static_cast<Uint8>(contentAlpha * 180);
                SDL_Color subColor = {180, 140, 200, subA};
                SDL_Surface* subSurf = TTF_RenderText_Blended(font, m_bossSubtitle.c_str(), subColor);
                if (subSurf) {
                    SDL_Texture* subTex = SDL_CreateTextureFromSurface(renderer, subSurf);
                    if (subTex) {
                        SDL_SetTextureAlphaMod(subTex, subA);
                        // Render subtitle slightly smaller (75% scale)
                        int subW = subSurf->w * 3 / 4;
                        int subH = subSurf->h * 3 / 4;
                        SDL_Rect sr = {screenW / 2 - subW / 2,
                                       screenH / 2 + 20,
                                       subW, subH};
                        SDL_RenderCopy(renderer, subTex, nullptr, &sr);
                        SDL_DestroyTexture(subTex);
                    }
                    SDL_FreeSurface(subSurf);
                }
            }
        }
    }

    // 5. Dimension ripple (sweep + scanlines + color flash)
    if (m_rippleTimer > 0) {
        float t = 1.0f - (m_rippleTimer / 0.5f);

        // Vertical sweep line moving across screen
        int sweepX = static_cast<int>(t * screenW);
        Uint8 sweepA = static_cast<Uint8>((1.0f - t) * 180);
        SDL_SetRenderDrawColor(renderer, 160, 220, 255, sweepA);
        SDL_Rect sweep = {sweepX - 2, 0, 4, screenH};
        SDL_RenderFillRect(renderer, &sweep);
        // Glow around sweep
        SDL_SetRenderDrawColor(renderer, 100, 180, 255, static_cast<Uint8>(sweepA / 3));
        SDL_Rect sweepGlow = {sweepX - 15, 0, 30, screenH};
        SDL_RenderFillRect(renderer, &sweepGlow);

        // Brief full-screen color tint at start
        if (t < 0.3f) {
            Uint8 tintA = static_cast<Uint8>((0.3f - t) / 0.3f * 40);
            SDL_SetRenderDrawColor(renderer, 80, 160, 255, tintA);
            SDL_Rect full = {0, 0, screenW, screenH};
            SDL_RenderFillRect(renderer, &full);
        }

        // Horizontal scanlines spreading from center
        int spread = static_cast<int>(t * screenH / 2);
        Uint8 a = static_cast<Uint8>((1.0f - t) * 50);
        for (int i = 0; i < 12; i++) {
            int offset = spread + i * 3;
            if (offset > screenH / 2) break;
            SDL_SetRenderDrawColor(renderer, 100, 200, 255, static_cast<Uint8>(a * (1.0f - i * 0.08f)));
            SDL_Rect lineUp = {0, screenH / 2 - offset, screenW, 1};
            SDL_Rect lineDown = {0, screenH / 2 + offset, screenW, 1};
            SDL_RenderFillRect(renderer, &lineUp);
            SDL_RenderFillRect(renderer, &lineDown);
        }
    }

    // 6. Void Storm overlay (pulsing purple danger indicator)
    if (m_voidStormActive) {
        float pulse = 0.5f + 0.5f * std::sin(m_time * 8.0f);
        Uint8 a = static_cast<Uint8>(20 + pulse * 35);
        SDL_SetRenderDrawColor(renderer, 120, 0, 180, a);
        SDL_Rect full = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &full);
        // Danger border
        int border = 6;
        Uint8 ba = static_cast<Uint8>(60 + pulse * 80);
        SDL_SetRenderDrawColor(renderer, 180, 0, 255, ba);
        SDL_Rect top = {0, 0, screenW, border};
        SDL_Rect bot = {0, screenH - border, screenW, border};
        SDL_Rect left = {0, 0, border, screenH};
        SDL_Rect right = {screenW - border, 0, border, screenH};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    }

    // 7. Entropy glitch
    if (m_entropy > 80.0f && m_glitchTimer > 0) {
        float intensity = (m_entropy - 80.0f) / 20.0f;
        int numGlitches = static_cast<int>(intensity * 5);
        for (int i = 0; i < numGlitches; i++) {
            int gx = std::rand() % screenW;
            int gy = std::rand() % screenH;
            int gw = 10 + std::rand() % 40;
            int gh = 2 + std::rand() % 4;
            Uint8 gr = static_cast<uint8_t>(std::rand() % 256);
            Uint8 gg = static_cast<uint8_t>(std::rand() % 256);
            Uint8 gb = static_cast<uint8_t>(std::rand() % 256);
            Uint8 ga = static_cast<Uint8>(intensity * 40);
            SDL_SetRenderDrawColor(renderer, gr, gg, gb, ga);
            SDL_Rect glitch = {gx, gy, gw, gh};
            SDL_RenderFillRect(renderer, &glitch);
        }
    }
}

void ScreenEffects::triggerKillFlash() {
    m_killFlashTimer = 0.05f;
}

void ScreenEffects::triggerDamageChromaticAberration(float intensity) {
    m_chromaticTimer = 0.15f;
    m_chromaticIntensity = std::min(1.0f, intensity);
}

void ScreenEffects::triggerBossIntro(const char* bossName, const char* subtitle) {
    m_bossIntroTimer = kBossIntroDuration;
    m_bossName = bossName;
    m_bossSubtitle = subtitle ? subtitle : "";
}

void ScreenEffects::triggerDimensionRipple() {
    m_rippleTimer = 0.5f;
}

// =============================================================================
// Post-Processing System
// =============================================================================

void ScreenEffects::renderPostProcessing(SDL_Renderer* renderer, int screenW, int screenH,
                                          int currentDimension, float dimBlendAlpha) {
    m_currentDimension = currentDimension;

    // Update ambient particles each frame (driven by renderPostProcessing call rate)
    // Use a fixed dt estimate of ~16ms (60fps) since we don't get dt here;
    // the actual particle update is in update() for timing, but we do visual
    // updates here too for dimension-color responsiveness.
    if (ambientParticlesEnabled) {
        if (!m_ambientParticlesInitialized) {
            initAmbientParticles(screenW, screenH);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 1. Color grading (lowest layer — subtle dimension mood tint)
    if (colorGradingEnabled) {
        renderColorGrading(renderer, screenW, screenH, currentDimension, dimBlendAlpha);
    }

    // 1b. Low HP desaturation: muted gray overlay when health is critical
    if (m_hpPercent < 0.25f) {
        float desatIntensity = (0.25f - m_hpPercent) / 0.25f; // 0→1 as HP drops to 0
        // Semi-transparent gray overlay simulates desaturation
        Uint8 grayA = static_cast<Uint8>(desatIntensity * 35); // Max 35 alpha — subtle
        SDL_SetRenderDrawColor(renderer, 80, 70, 70, grayA);
        SDL_Rect fullScreen = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &fullScreen);
    }

    // 2. Bloom/glow simulation (additive bright spots)
    if (bloomEnabled && m_glowPointCount > 0) {
        renderBloom(renderer);
    }

    // 3. Ambient particles (floating dust motes)
    if (ambientParticlesEnabled) {
        renderAmbientParticles(renderer);
    }

    // 4. Cinematic vignette (darkened edges — drawn on top for cinematic framing)
    if (vignetteEnabled) {
        renderVignette(renderer, screenW, screenH);
    }

    // Clear glow points for next frame
    m_glowPointCount = 0;
}

// -----------------------------------------------------------------------------
// Cinematic Vignette (post-processing version)
// Separate from the HP-reactive vignette in render() — this is purely cinematic.
// Uses concentric rectangular bands with quadratic falloff from ~60% inward to edges.
// -----------------------------------------------------------------------------
void ScreenEffects::renderVignette(SDL_Renderer* renderer, int screenW, int screenH) {
    // Vignette coverage: darken outer ~40% of screen
    // Max alpha at the very edge (corners get darkest)
    constexpr Uint8 kMaxEdgeAlpha = 70;   // Subtle but noticeable
    constexpr int kBands = 16;            // Number of gradient bands (performance-friendly)

    // Calculate band dimensions — from edge inward
    const float innerFractionW = 0.55f;  // Inner clear zone = 55% of width
    const float innerFractionH = 0.50f;  // Inner clear zone = 50% of height
    const int marginW = static_cast<int>(screenW * (1.0f - innerFractionW) * 0.5f);
    const int marginH = static_cast<int>(screenH * (1.0f - innerFractionH) * 0.5f);

    // Draw bands from outside in (outer = darkest, inner = lightest)
    for (int i = 0; i < kBands; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kBands); // 0=outermost, 1=innermost
        float alphaFactor = (1.0f - t) * (1.0f - t); // Quadratic falloff
        Uint8 a = static_cast<Uint8>(kMaxEdgeAlpha * alphaFactor);
        if (a < 2) continue; // Skip invisible bands

        // Inset from edge for this band
        int insetW = static_cast<int>(t * marginW);
        int insetH = static_cast<int>(t * marginH);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);

        // Top band
        SDL_Rect top = {insetW, insetH, screenW - 2 * insetW, marginH / kBands + 1};
        // Bottom band
        SDL_Rect bottom = {insetW, screenH - insetH - marginH / kBands, screenW - 2 * insetW, marginH / kBands + 1};
        // Left band
        SDL_Rect left = {insetW, insetH, marginW / kBands + 1, screenH - 2 * insetH};
        // Right band
        SDL_Rect right = {screenW - insetW - marginW / kBands, insetH, marginW / kBands + 1, screenH - 2 * insetH};

        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bottom);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    }

    // Corner extra darkening (elliptical, 4 corners with coarse blocks)
    constexpr int kCornerBlock = 8;
    const int cornerW = marginW;
    const int cornerH = marginH;
    const Uint8 cornerMaxA = static_cast<Uint8>(kMaxEdgeAlpha * 0.9f);

    struct CornerPos { int x, y; };
    const CornerPos corners[4] = {
        {0, 0},
        {screenW - cornerW, 0},
        {0, screenH - cornerH},
        {screenW - cornerW, screenH - cornerH}
    };

    for (const auto& cp : corners) {
        for (int by = 0; by < cornerH; by += kCornerBlock) {
            for (int bx = 0; bx < cornerW; bx += kCornerBlock) {
                // Normalized distance from outer corner (0 = corner, 1 = inner edge)
                float fx = static_cast<float>(cp.x == 0 ? bx : cornerW - bx) / static_cast<float>(cornerW);
                float fy = static_cast<float>(cp.y == 0 ? by : cornerH - by) / static_cast<float>(cornerH);
                float d = (1.0f - fx) * (1.0f - fy); // Product = strongest at corner
                if (d > 0.08f) {
                    Uint8 a = static_cast<Uint8>(cornerMaxA * d * d);
                    if (a < 2) continue;
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
                    int w = std::min(kCornerBlock, cornerW - bx);
                    int h = std::min(kCornerBlock, cornerH - by);
                    SDL_Rect r = {cp.x + bx, cp.y + by, w, h};
                    SDL_RenderFillRect(renderer, &r);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Color Grading — subtle dimension-based mood tint
// Dim A = cool blue/purple, Dim B = warm orange/amber
// Very low alpha for a subtle cinematic look without washing out the game.
// -----------------------------------------------------------------------------
void ScreenEffects::renderColorGrading(SDL_Renderer* renderer, int screenW, int screenH,
                                        int currentDimension, float dimBlendAlpha) {
    // Determine blend factor (0 = full dim A, 1 = full dim B)
    float dimBFactor = 0.0f;
    if (currentDimension == 2) dimBFactor = 1.0f;
    else if (currentDimension == 1) dimBFactor = 0.0f;
    if (dimBlendAlpha > 0.01f && dimBlendAlpha < 0.99f) {
        dimBFactor = dimBlendAlpha;
    }

    SDL_Rect fullScreen = {0, 0, screenW, screenH};

    // Pass 1: Multiplicative color grading (SDL_BLENDMODE_MOD)
    // This multiplies screen colors: (255,230,200) = warm, (200,220,255) = cool
    // Values >128 brighten that channel, <128 darken it (relative to 255)
    {
        // Dim A: cool blue tint — reduce red, slightly reduce green, boost blue
        // Dim B: warm amber tint — boost red, slightly reduce blue
        Uint8 mr = static_cast<Uint8>(235 + (255 - 235) * dimBFactor); // 235→255 (A→B)
        Uint8 mg = static_cast<Uint8>(240 + (245 - 240) * dimBFactor); // 240→245
        Uint8 mb = static_cast<Uint8>(255 + (230 - 255) * dimBFactor); // 255→230
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_MOD);
        SDL_SetRenderDrawColor(renderer, mr, mg, mb, 255);
        SDL_RenderFillRect(renderer, &fullScreen);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

    // Pass 2: Subtle additive mood overlay (very low alpha for atmosphere)
    {
        float pulse = 1.0f + 0.15f * std::sin(m_time * 0.8f);
        Uint8 r = static_cast<Uint8>(30 + 90 * dimBFactor);   // 30→120
        Uint8 g = static_cast<Uint8>(40 + 30 * dimBFactor);   // 40→70
        Uint8 b = static_cast<Uint8>(100 - 70 * dimBFactor);  // 100→30
        Uint8 a = static_cast<Uint8>(std::min(255.0f, 6.0f * pulse));
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDL_RenderFillRect(renderer, &fullScreen);
    }
}

// -----------------------------------------------------------------------------
// Ambient Particles — floating dust motes / dimensional energy particles
// 30 small particles drift slowly, with dimension-appropriate colors.
// Very subtle: alpha 20-60, size 1-3px.
// -----------------------------------------------------------------------------
void ScreenEffects::initAmbientParticles(int screenW, int screenH) {
    for (int i = 0; i < kMaxAmbientParticles; ++i) {
        auto& p = m_ambientParticles[i];
        p.x = randFloat() * screenW;
        p.y = randFloat() * screenH;
        p.vx = (randFloat() - 0.5f) * 12.0f;  // -6 to +6 px/s
        p.vy = -2.0f - randFloat() * 8.0f;     // Slowly drift upward (-2 to -10 px/s)
        p.size = 1.0f + randFloat() * 2.0f;    // 1-3 px
        p.alpha = 20.0f + randFloat() * 40.0f;
        p.alphaTarget = 20.0f + randFloat() * 40.0f;
        p.lifetime = 2.0f + randFloat() * 8.0f;
        // Default to dimension A colors (purple/blue)
        if (randFloat() > 0.5f) {
            p.r = static_cast<Uint8>(100 + randFloat() * 80);  // Purple
            p.g = static_cast<Uint8>(60 + randFloat() * 60);
            p.b = static_cast<Uint8>(180 + randFloat() * 75);
        } else {
            p.r = static_cast<Uint8>(60 + randFloat() * 60);   // Blue
            p.g = static_cast<Uint8>(120 + randFloat() * 80);
            p.b = static_cast<Uint8>(200 + randFloat() * 55);
        }
    }
    m_ambientParticlesInitialized = true;
}

void ScreenEffects::updateAmbientParticles(float dt, int screenW, int screenH, int currentDimension) {
    for (int i = 0; i < kMaxAmbientParticles; ++i) {
        auto& p = m_ambientParticles[i];

        // Move
        p.x += p.vx * dt;
        p.y += p.vy * dt;

        // Gentle sine wave horizontal drift
        p.x += std::sin(m_time * 0.5f + i * 0.7f) * 0.3f;

        // Smooth alpha breathing
        float alphaDiff = p.alphaTarget - p.alpha;
        p.alpha += alphaDiff * dt * 2.0f;

        p.lifetime -= dt;

        // Respawn when off-screen or lifetime expired
        if (p.lifetime <= 0 || p.x < -10 || p.x > screenW + 10 ||
            p.y < -10 || p.y > screenH + 10) {
            // Respawn at random position (prefer bottom for upward drift)
            p.x = randFloat() * screenW;
            p.y = screenH + randFloat() * 20.0f; // Start below screen
            p.vx = (randFloat() - 0.5f) * 12.0f;
            p.vy = -2.0f - randFloat() * 8.0f;
            p.size = 1.0f + randFloat() * 2.0f;
            p.alphaTarget = 20.0f + randFloat() * 40.0f;
            p.alpha = 0.0f; // Fade in from invisible
            p.lifetime = 4.0f + randFloat() * 8.0f;

            // Set colors based on current dimension
            if (currentDimension == 2) {
                // Dimension B: orange/red/amber
                if (randFloat() > 0.5f) {
                    p.r = static_cast<Uint8>(200 + randFloat() * 55);
                    p.g = static_cast<Uint8>(100 + randFloat() * 80);
                    p.b = static_cast<Uint8>(20 + randFloat() * 40);
                } else {
                    p.r = static_cast<Uint8>(220 + randFloat() * 35);
                    p.g = static_cast<Uint8>(60 + randFloat() * 60);
                    p.b = static_cast<Uint8>(30 + randFloat() * 30);
                }
            } else {
                // Dimension A: purple/blue
                if (randFloat() > 0.5f) {
                    p.r = static_cast<Uint8>(100 + randFloat() * 80);
                    p.g = static_cast<Uint8>(60 + randFloat() * 60);
                    p.b = static_cast<Uint8>(180 + randFloat() * 75);
                } else {
                    p.r = static_cast<Uint8>(60 + randFloat() * 60);
                    p.g = static_cast<Uint8>(120 + randFloat() * 80);
                    p.b = static_cast<Uint8>(200 + randFloat() * 55);
                }
            }
        }
    }
}

void ScreenEffects::renderAmbientParticles(SDL_Renderer* renderer) {
    for (int i = 0; i < kMaxAmbientParticles; ++i) {
        const auto& p = m_ambientParticles[i];
        Uint8 a = clampAlpha(p.alpha);
        if (a < 3) continue; // Skip nearly invisible particles

        SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, a);

        int size = static_cast<int>(p.size);
        int px = static_cast<int>(p.x);
        int py = static_cast<int>(p.y);

        if (size <= 1) {
            // Single pixel
            SDL_RenderDrawPoint(renderer, px, py);
        } else {
            // Small filled rect
            SDL_Rect r = {px - size / 2, py - size / 2, size, size};
            SDL_RenderFillRect(renderer, &r);
        }

        // Faint glow halo for larger particles (1 extra ring, very low alpha)
        if (size >= 2 && a > 15) {
            SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, static_cast<Uint8>(a / 4));
            SDL_Rect glow = {px - size, py - size, size * 2 + 1, size * 2 + 1};
            SDL_RenderDrawRect(renderer, &glow);
        }
    }
}

// -----------------------------------------------------------------------------
// Bloom/Glow Simulation — soft additive-blended rectangles at bright spots
// Games register glow points each frame; we draw soft concentric rects.
// Uses SDL additive blending for the "light bleeding" look.
// -----------------------------------------------------------------------------
void ScreenEffects::registerGlowPoint(float screenX, float screenY, float radius,
                                       Uint8 r, Uint8 g, Uint8 b, Uint8 intensity) {
    if (m_glowPointCount >= kMaxGlowPoints) return;
    auto& gp = m_glowPoints[m_glowPointCount++];
    gp.x = screenX;
    gp.y = screenY;
    gp.radius = radius;
    gp.r = r;
    gp.g = g;
    gp.b = b;
    gp.intensity = intensity;
}

void ScreenEffects::clearGlowPoints() {
    m_glowPointCount = 0;
}

void ScreenEffects::renderBloom(SDL_Renderer* renderer) {
    // Use additive blending for bloom — light adds on top of existing colors
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    for (int i = 0; i < m_glowPointCount; ++i) {
        const auto& gp = m_glowPoints[i];
        int cx = static_cast<int>(gp.x);
        int cy = static_cast<int>(gp.y);
        float r = gp.radius;

        // Draw 4 concentric soft layers (inner = brighter, outer = dimmer)
        constexpr int kLayers = 4;
        for (int layer = 0; layer < kLayers; ++layer) {
            float t = static_cast<float>(layer) / static_cast<float>(kLayers);
            float layerRadius = r * (0.3f + t * 0.7f); // 30% to 100% of radius
            float alphaFactor = (1.0f - t) * (1.0f - t); // Quadratic falloff
            Uint8 a = clampAlpha(gp.intensity * alphaFactor);
            if (a < 2) continue;

            int halfW = static_cast<int>(layerRadius);
            int halfH = static_cast<int>(layerRadius * 0.8f); // Slightly vertically squished

            SDL_SetRenderDrawColor(renderer, gp.r, gp.g, gp.b, a);
            SDL_Rect rect = {cx - halfW, cy - halfH, halfW * 2, halfH * 2};
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    // Restore normal blending
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void ScreenEffects::registerLight(float screenX, float screenY, float radius,
                                   Uint8 r, Uint8 g, Uint8 b, float intensity) {
    if (m_lightCount >= kMaxLights) return;
    m_lights[m_lightCount++] = {screenX, screenY, radius, r, g, b, intensity};
}

void ScreenEffects::renderDynamicLighting(SDL_Renderer* renderer, int screenW, int screenH) {
    if (!dynamicLightingEnabled || m_lightCount == 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Ambient darkness with light radii carved out
    // Performance-optimized: 8px grid + distance-squared (no sqrt)
    constexpr Uint8 kAmbientDarkness = 40;
    constexpr int kStep = 8;

    for (int y = 0; y < screenH; y += kStep) {
        float cy = y + kStep * 0.5f;
        for (int x = 0; x < screenW; x += kStep) {
            float cx = x + kStep * 0.5f;

            float totalLight = 0.0f;
            for (int i = 0; i < m_lightCount; i++) {
                auto& lt = m_lights[i];
                float dx = cx - lt.x;
                float dy = cy - lt.y;
                float distSq = dx * dx + dy * dy;
                float radiusSq = lt.radius * lt.radius;
                if (distSq < radiusSq) {
                    float t = 1.0f - distSq / radiusSq; // Linear in dist², quadratic in dist
                    totalLight += t * lt.intensity;
                    if (totalLight >= 1.0f) break; // Early out — fully lit
                }
            }

            if (totalLight >= 0.95f) continue; // Fully lit, skip draw
            Uint8 darkness = static_cast<Uint8>(kAmbientDarkness * (1.0f - std::min(1.0f, totalLight)));
            if (darkness < 2) continue;

            SDL_SetRenderDrawColor(renderer, 0, 0, 5, darkness);
            SDL_Rect band = {x, y, kStep, kStep};
            SDL_RenderFillRect(renderer, &band);
        }
    }

    // Reset light count for next frame
    m_lightCount = 0;
}
