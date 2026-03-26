#include "Game/ScreenEffects.h"
#include <cmath>
#include <cstdlib>
#include <SDL2/SDL_ttf.h>

void ScreenEffects::update(float dt) {
    m_time += dt;
    if (m_killFlashTimer > 0) m_killFlashTimer -= dt;
    if (m_bossIntroTimer > 0) m_bossIntroTimer -= dt;
    if (m_rippleTimer > 0) m_rippleTimer -= dt;
    if (m_entropy > 80.0f) m_glitchTimer += dt;
    else m_glitchTimer = 0;
}

void ScreenEffects::render(SDL_Renderer* renderer, int screenW, int screenH, TTF_Font* font) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 1. Vignette (always, stronger at low HP)
    {
        float strength = 0.15f + (1.0f - m_hpPercent) * 0.25f;
        Uint8 vigA = static_cast<Uint8>(strength * 255);
        int edgeW = screenW / 6;
        int edgeH = screenH / 6;
        // Top
        for (int i = 0; i < edgeH; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (1.0f - static_cast<float>(i) / edgeH));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {0, i, screenW, 1};
            SDL_RenderFillRect(renderer, &r);
        }
        // Bottom
        for (int i = 0; i < edgeH; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (static_cast<float>(i) / edgeH));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {0, screenH - edgeH + i, screenW, 1};
            SDL_RenderFillRect(renderer, &r);
        }
        // Left
        for (int i = 0; i < edgeW; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (1.0f - static_cast<float>(i) / edgeW));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {i, 0, 1, screenH};
            SDL_RenderFillRect(renderer, &r);
        }
        // Right
        for (int i = 0; i < edgeW; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (static_cast<float>(i) / edgeW));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {screenW - edgeW + i, 0, 1, screenH};
            SDL_RenderFillRect(renderer, &r);
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
        float barH = contentAlpha * 60.0f;
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
            SDL_Rect lineTop = {lineX, screenH / 2 - 28, lineW, 2};
            SDL_RenderFillRect(renderer, &lineTop);
            // Glow around top line
            SDL_SetRenderDrawColor(renderer, 200, 50, 80, static_cast<Uint8>(lineA / 3));
            SDL_Rect lineTopGlow = {lineX, screenH / 2 - 30, lineW, 6};
            SDL_RenderFillRect(renderer, &lineTopGlow);

            // Bottom line (below subtitle)
            SDL_SetRenderDrawColor(renderer, 200, 50, 80, lineA);
            SDL_Rect lineBot = {lineX, screenH / 2 + 30, lineW, 2};
            SDL_RenderFillRect(renderer, &lineBot);
            SDL_SetRenderDrawColor(renderer, 200, 50, 80, static_cast<Uint8>(lineA / 3));
            SDL_Rect lineBotGlow = {lineX, screenH / 2 + 28, lineW, 6};
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
                                           screenH / 2 - glowSurf->h / 2 - 8 + dy,
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
                                   screenH / 2 - nameSurf->h / 2 - 8,
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
                                       screenH / 2 + 10,
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

void ScreenEffects::triggerBossIntro(const char* bossName, const char* subtitle) {
    m_bossIntroTimer = kBossIntroDuration;
    m_bossName = bossName;
    m_bossSubtitle = subtitle ? subtitle : "";
}

void ScreenEffects::triggerDimensionRipple() {
    m_rippleTimer = 0.5f;
}
