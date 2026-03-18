// PlayStateRender.cpp — All render method implementations for PlayState
// Split from PlayState.cpp — all member functions unchanged, just moved.
#include "PlayState.h"
#include "Core/Game.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AbilityComponent.h"
#include "Core/AudioManager.h"
#include "Game/AchievementSystem.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/LoreSystem.h"
#include "Game/DailyRun.h"
#include "Game/Bestiary.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

namespace {
constexpr bool kUseAiFinalBackgroundArtTest = true;
constexpr const char* kDimAFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dima_run01_s1103_fin.png";
constexpr const char* kDimBFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dimb_run01_s2103_fin.png";
constexpr const char* kBossFinalBackgroundPath =
    "assets/ai/finals/boss/run01/rw_boss_run01_f03_s3120_fin.png";
constexpr float kArtTestBackgroundParallaxX = 0.24f;

void renderWholeArtBackground(SDL_Renderer* renderer,
                              SDL_Texture* texture,
                              const Vec2& camPos,
                              int screenW,
                              int screenH,
                              int worldPixelWidth,
                              float parallaxX,
                              Uint8 colorR,
                              Uint8 colorG,
                              Uint8 colorB,
                              Uint8 alpha) {
    if (!texture) return;

    int texW = 0;
    int texH = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH) != 0 || texW <= 0 || texH <= 0) {
        return;
    }

    const float scaleX = static_cast<float>(screenW) / static_cast<float>(texW);
    const float scaleY = static_cast<float>(screenH) / static_cast<float>(texH);
    const float coverScale = std::max(scaleX, scaleY);
    const float minCameraX = screenW * 0.5f;
    const float maxCameraX = std::max(minCameraX, static_cast<float>(worldPixelWidth) - screenW * 0.5f);
    const float cameraTravel = std::max(0.0f, maxCameraX - minCameraX);
    const float requiredWidth = static_cast<float>(screenW) + cameraTravel * parallaxX;
    const float scale = std::max(coverScale, requiredWidth / static_cast<float>(texW));
    const int drawW = static_cast<int>(std::ceil(static_cast<float>(texW) * scale));
    const int drawH = static_cast<int>(std::ceil(static_cast<float>(texH) * scale));

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(texture, colorR, colorG, colorB);
    SDL_SetTextureAlphaMod(texture, alpha);

    float cameraT = 0.5f;
    if (cameraTravel > 0.001f) {
        cameraT = std::clamp((camPos.x - minCameraX) / cameraTravel, 0.0f, 1.0f);
    }

    const int travelPixels = std::max(0, drawW - screenW);
    const int dstX = -static_cast<int>(std::round(static_cast<float>(travelPixels) * cameraT));
    const int dstY = (screenH - drawH) / 2;
    SDL_Rect dst{dstX, dstY, drawW, drawH};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);

    SDL_SetTextureColorMod(texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(texture, 255);
}
}


void PlayState::renderBackground(SDL_Renderer* renderer) {
    auto& bgColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.background : m_themeB.colors.background;
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    Vec2 camPos = m_camera.getPosition();
    Uint32 ticks = SDL_GetTicks();
    SDL_Texture* artTestBackground = nullptr;
    if (kUseAiFinalBackgroundArtTest) {
        if (m_isBossLevel && m_bossTestBackground) {
            artTestBackground = m_bossTestBackground;
        } else {
            artTestBackground = (m_dimManager.getCurrentDimension() == 1) ? m_dimATestBackground : m_dimBTestBackground;
        }
    }

    if (artTestBackground) {
        const int worldPixelWidth = m_level ? m_level->getPixelWidth() : SCREEN_WIDTH;
        renderWholeArtBackground(renderer,
                                 artTestBackground,
                                 camPos,
                                 SCREEN_WIDTH,
                                 SCREEN_HEIGHT,
                                 worldPixelWidth,
                                 kArtTestBackgroundParallaxX,
                                 164, 168, 174, 196);

        // Keep AI backgrounds slightly subdued so gameplay silhouettes stay readable.
        SDL_SetRenderDrawColor(renderer, 7, 9, 14, 92);
        SDL_Rect dimmer{0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &dimmer);

        // Extra calm in the gameplay-heavy center band.
        SDL_SetRenderDrawColor(renderer, 10, 12, 18, 28);
        SDL_Rect outerCenterVeil{
            SCREEN_WIDTH * 14 / 100,
            SCREEN_HEIGHT * 20 / 100,
            SCREEN_WIDTH * 72 / 100,
            SCREEN_HEIGHT * 50 / 100
        };
        SDL_RenderFillRect(renderer, &outerCenterVeil);

        SDL_SetRenderDrawColor(renderer, 12, 14, 22, 34);
        SDL_Rect innerCenterVeil{
            SCREEN_WIDTH * 24 / 100,
            SCREEN_HEIGHT * 28 / 100,
            SCREEN_WIDTH * 52 / 100,
            SCREEN_HEIGHT * 34 / 100
        };
        SDL_RenderFillRect(renderer, &innerCenterVeil);
    } else {
        // Layer 1: Distant stars (slow parallax)
        for (int i = 0; i < 60; i++) {
            // Deterministic pseudo-random positions based on index
            int baseX = ((i * 7919 + 104729) % 2560) - 640;
            int baseY = ((i * 6271 + 73856) % 1440) - 360;

            float parallax = 0.05f;
            int sx = baseX - static_cast<int>(camPos.x * parallax) % 2560;
            int sy = baseY - static_cast<int>(camPos.y * parallax) % 1440;
            // Wrap around screen
            sx = ((sx % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH;
            sy = ((sy % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT;

            float twinkle = 0.5f + 0.5f * std::sin(ticks * 0.002f + i * 1.7f);
            Uint8 brightness = static_cast<Uint8>(30 + 40 * twinkle);
            int size = (i % 3 == 0) ? 2 : 1;
            SDL_SetRenderDrawColor(renderer, brightness, brightness,
                                   static_cast<Uint8>(brightness + 30), 255);
            SDL_Rect star = {sx, sy, size, size};
            SDL_RenderFillRect(renderer, &star);
        }

        // Layer 1.5: Nebula clouds (slow drift, theme-colored)
        {
            float nebulaParallax = 0.08f;
            int nOffX = static_cast<int>(camPos.x * nebulaParallax);
            int nOffY = static_cast<int>(camPos.y * nebulaParallax);
            auto& accent = (m_dimManager.getCurrentDimension() == 1) ?
                m_themeA.colors.accent : m_themeB.colors.accent;
            for (int i = 0; i < 5; i++) {
                int baseX = ((i * 12347 + 5281) % 2200) - 400;
                int baseY = ((i * 8731 + 2917) % 1000) - 200;
                int nx = ((baseX - nOffX) % 2000 + 2000) % 2000 - 400;
                int ny = ((baseY - nOffY / 2) % 900 + 900) % 900 - 100;
                int cloudW = 120 + (i * 37) % 180;
                int cloudH = 40 + (i * 23) % 60;
                float drift = std::sin(ticks * 0.0004f + i * 1.3f) * 15.0f;
                nx += static_cast<int>(drift);
                // Draw as overlapping semi-transparent circles
                Uint8 na = static_cast<Uint8>(8 + 4 * std::sin(ticks * 0.001f + i));
                SDL_SetRenderDrawColor(renderer, accent.r / 3, accent.g / 3, accent.b / 3, na);
                for (int j = 0; j < 4; j++) {
                    int cx = nx + (j * cloudW) / 4;
                    int cy = ny + static_cast<int>(std::sin(j * 1.5f) * cloudH / 3);
                    int rw = cloudW / 3 + (j * 13) % 20;
                    int rh = cloudH / 2 + (j * 7) % 10;
                    SDL_Rect cloud = {cx - rw / 2, cy - rh / 2, rw, rh};
                    SDL_RenderFillRect(renderer, &cloud);
                }
            }
        }

        // Layer 2: Grid lines (medium parallax, gives depth)
        float gridParallax = 0.15f;
        int gridSpacing = 120;
        int gridOffX = static_cast<int>(camPos.x * gridParallax) % gridSpacing;
        int gridOffY = static_cast<int>(camPos.y * gridParallax) % gridSpacing;

        Uint8 gridAlpha = 15;
        SDL_SetRenderDrawColor(renderer,
            static_cast<Uint8>(bgColor.r + 30 > 255 ? 255 : bgColor.r + 30),
            static_cast<Uint8>(bgColor.g + 30 > 255 ? 255 : bgColor.g + 30),
            static_cast<Uint8>(bgColor.b + 30 > 255 ? 255 : bgColor.b + 30),
            gridAlpha);

        for (int x = -gridOffX; x < SCREEN_WIDTH; x += gridSpacing) {
            SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
        }
        for (int y = -gridOffY; y < SCREEN_HEIGHT; y += gridSpacing) {
            SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
        }

        // Layer 2.5: Theme-specific silhouettes (0.1x parallax)
        {
            float silParallax = 0.1f;
            int offX = static_cast<int>(camPos.x * silParallax);
            int offY = static_cast<int>(camPos.y * silParallax);
            const auto& theme = (m_dimManager.getCurrentDimension() == 1) ? m_themeA : m_themeB;
            Uint8 silR = static_cast<Uint8>(std::min(255, bgColor.r + 15));
            Uint8 silG = static_cast<Uint8>(std::min(255, bgColor.g + 15));
            Uint8 silB = static_cast<Uint8>(std::min(255, bgColor.b + 15));
            Uint8 silA = 30;
            SDL_SetRenderDrawColor(renderer, silR, silG, silB, silA);

            int themeId = static_cast<int>(theme.id);
            for (int i = 0; i < 8; i++) {
                int baseX = ((i * 8317 + 29741) % 2000) - 200;
                int baseY = SCREEN_HEIGHT - 50 - ((i * 4729) % 200);
                int sx = ((baseX - offX) % 1800 + 1800) % 1800 - 200;
                int sy = baseY - (offY % 300);

                switch (themeId) {
                case 0: { // Victorian - gear outlines
                    int cx = sx + 30; int cy = sy - 20;
                    int r = 15 + (i % 3) * 8;
                    for (int seg = 0; seg < 8; seg++) {
                        float a1 = seg * 0.785f + ticks * 0.0003f * (i % 2 ? 1 : -1);
                        float a2 = a1 + 0.5f;
                        SDL_RenderDrawLine(renderer, cx + static_cast<int>(std::cos(a1) * r), cy + static_cast<int>(std::sin(a1) * r),
                                           cx + static_cast<int>(std::cos(a2) * r), cy + static_cast<int>(std::sin(a2) * r));
                    }
                    break;
                }
                case 1: // DeepOcean - seaweed columns
                    for (int s = 0; s < 4; s++) {
                        int wx = sx + s * 8;
                        int sway = static_cast<int>(std::sin(ticks * 0.001f + i + s * 0.5f) * 6);
                        SDL_Rect seg = {wx + sway, sy - s * 25, 3, 25};
                        SDL_RenderFillRect(renderer, &seg);
                    }
                    break;
                case 2: { // NeonCity - building skyline
                    int bw = 30 + (i * 17) % 40;
                    int bh = 60 + (i * 31) % 120;
                    SDL_Rect bld = {sx, SCREEN_HEIGHT - bh, bw, bh};
                    SDL_RenderFillRect(renderer, &bld);
                    // Window dots
                    for (int wy = SCREEN_HEIGHT - bh + 8; wy < SCREEN_HEIGHT - 8; wy += 12) {
                        for (int wx = sx + 4; wx < sx + bw - 4; wx += 8) {
                            SDL_SetRenderDrawColor(renderer, silR, silG, static_cast<Uint8>(std::min(255, silB + 40)), 20);
                            SDL_Rect win = {wx, wy, 3, 4};
                            SDL_RenderFillRect(renderer, &win);
                        }
                    }
                    SDL_SetRenderDrawColor(renderer, silR, silG, silB, silA);
                    break;
                }
                case 3: { // AncientRuins - columns
                    int colH = 80 + (i * 23) % 60;
                    SDL_Rect col = {sx, SCREEN_HEIGHT - colH, 12, colH};
                    SDL_RenderFillRect(renderer, &col);
                    SDL_Rect cap = {sx - 4, SCREEN_HEIGHT - colH, 20, 6};
                    SDL_RenderFillRect(renderer, &cap);
                    break;
                }
                case 4: // CrystalCavern - stalactites
                    for (int s = 0; s < 3; s++) {
                        int tip = 30 + (i + s) * 13 % 50;
                        int bx = sx + s * 20;
                        SDL_RenderDrawLine(renderer, bx, 0, bx - 5, tip);
                        SDL_RenderDrawLine(renderer, bx, 0, bx + 5, tip);
                        SDL_RenderDrawLine(renderer, bx - 5, tip, bx + 5, tip);
                    }
                    break;
                case 5: { // BioMechanical - pipes
                    int pipeY = sy - 40;
                    SDL_Rect pipe = {sx, pipeY, 60 + (i * 19) % 80, 6};
                    SDL_RenderFillRect(renderer, &pipe);
                    SDL_Rect joint = {sx + pipe.w, pipeY - 3, 6, 12};
                    SDL_RenderFillRect(renderer, &joint);
                    break;
                }
                case 6: // FrozenWasteland - ice spikes
                    for (int s = 0; s < 3; s++) {
                        int spikeH = 40 + (i + s) * 17 % 60;
                        int bx = sx + s * 18;
                        int by = SCREEN_HEIGHT;
                        SDL_RenderDrawLine(renderer, bx - 6, by, bx, by - spikeH);
                        SDL_RenderDrawLine(renderer, bx + 6, by, bx, by - spikeH);
                    }
                    break;
                case 7: { // VolcanicCore - rock pinnacles
                    int rockH = 50 + (i * 29) % 80;
                    int bx = sx;
                    int by = SCREEN_HEIGHT;
                    SDL_RenderDrawLine(renderer, bx - 12, by, bx - 3, by - rockH);
                    SDL_RenderDrawLine(renderer, bx + 12, by, bx + 3, by - rockH);
                    SDL_Rect base = {bx - 12, by - 8, 24, 8};
                    SDL_RenderFillRect(renderer, &base);
                    break;
                }
                case 8: // FloatingIslands - cloud wisps
                    for (int c = 0; c < 3; c++) {
                        int cx = sx + c * 25;
                        int cy = sy - 80 + static_cast<int>(std::sin(ticks * 0.0005f + i + c) * 10);
                        SDL_Rect cloud = {cx, cy, 20 + c * 5, 6};
                        SDL_RenderFillRect(renderer, &cloud);
                    }
                    break;
                case 9: { // VoidRealm - fractured geometry
                    int fx = sx; int fy = sy - 60;
                    for (int s = 0; s < 4; s++) {
                        float a = s * 1.57f + ticks * 0.0004f;
                        int len = 15 + (i * 7 + s) % 20;
                        SDL_RenderDrawLine(renderer, fx, fy, fx + static_cast<int>(std::cos(a) * len), fy + static_cast<int>(std::sin(a) * len));
                    }
                    break;
                }
                case 10: { // SpaceWestern - mesa silhouettes
                    int mesaW = 50 + (i * 23) % 40;
                    int mesaH = 40 + (i * 13) % 50;
                    SDL_Rect mesa = {sx, SCREEN_HEIGHT - mesaH, mesaW, mesaH};
                    SDL_RenderFillRect(renderer, &mesa);
                    SDL_Rect top = {sx + 5, SCREEN_HEIGHT - mesaH - 8, mesaW - 10, 8};
                    SDL_RenderFillRect(renderer, &top);
                    break;
                }
                case 11: // Biopunk - organic bulbs
                    for (int b = 0; b < 2; b++) {
                        int bx = sx + b * 30;
                        int by = sy - 30;
                        int br = 8 + (i + b) % 6;
                        SDL_Rect bulb = {bx - br, by - br, br * 2, br * 2};
                        SDL_RenderDrawRect(renderer, &bulb);
                        SDL_RenderDrawLine(renderer, bx, by + br, bx, by + br + 20);
                    }
                    break;
                default: break;
                }
            }
        }
    }

    // Layer 3: Floating dimension particles (close parallax)
    auto& dimColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.solid : m_themeB.colors.solid;
    for (int i = 0; i < 20; i++) {
        float speed = 0.3f + (i % 5) * 0.1f;
        int baseX = ((i * 4513 + 23143) % 1600) - 160;
        int baseY = ((i * 3251 + 51749) % 900) - 90;

        float px = baseX - camPos.x * speed;
        float py = baseY - camPos.y * speed;
        // Gentle floating motion
        py += std::sin(ticks * 0.001f + i * 2.3f) * 15.0f;

        int screenX = ((static_cast<int>(px) % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH;
        int screenY = ((static_cast<int>(py) % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT;

        int size = 2 + i % 3;
        Uint8 pa = static_cast<Uint8>(20 + 15 * std::sin(ticks * 0.0015f + i));
        SDL_SetRenderDrawColor(renderer, dimColor.r, dimColor.g, dimColor.b, pa);
        SDL_Rect particle = {screenX, screenY, size, size};
        SDL_RenderFillRect(renderer, &particle);
    }
}

void PlayState::render(SDL_Renderer* renderer) {
    // Parallax background
    renderBackground(renderer);

    if (m_level) {
        m_level->render(renderer, m_camera,
                        m_dimManager.getCurrentDimension(),
                        m_dimManager.getBlendAlpha());
    }

    // Render entities
    m_renderSys.render(renderer, m_entities, m_camera,
                       m_dimManager.getCurrentDimension(),
                       m_dimManager.getBlendAlpha());

    // Grappling hook rope: draw line from player to hook/attach point
    if (m_player) {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        Vec2 playerCenter = playerT.getCenter();
        Vec2 camPos = m_camera.getPosition();

        int px = static_cast<int>(playerCenter.x - camPos.x);
        int py = static_cast<int>(playerCenter.y - camPos.y);

        if (m_player->isGrappling) {
            // Draw rope to attach point
            int ax = static_cast<int>(m_player->hookAttachPoint.x - camPos.x);
            int ay = static_cast<int>(m_player->hookAttachPoint.y - camPos.y);

            // Rope: draw 3 lines for thickness effect
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 180, 140, 60, 200);
            SDL_RenderDrawLine(renderer, px, py, ax, ay);
            SDL_SetRenderDrawColor(renderer, 200, 160, 80, 160);
            SDL_RenderDrawLine(renderer, px - 1, py, ax - 1, ay);
            SDL_RenderDrawLine(renderer, px + 1, py, ax + 1, ay);

            // Hook anchor point indicator (small filled rect)
            SDL_SetRenderDrawColor(renderer, 220, 180, 80, 255);
            SDL_Rect hookRect = {ax - 3, ay - 3, 6, 6};
            SDL_RenderFillRect(renderer, &hookRect);
        } else if (m_player->hookFlying && m_player->hookProjectile &&
                   m_player->hookProjectile->isAlive()) {
            // Draw rope to flying hook projectile
            auto& hookT = m_player->hookProjectile->getComponent<TransformComponent>();
            Vec2 hookCenter = hookT.getCenter();
            int hx = static_cast<int>(hookCenter.x - camPos.x);
            int hy = static_cast<int>(hookCenter.y - camPos.y);

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 180, 140, 60, 150);
            SDL_RenderDrawLine(renderer, px, py, hx, hy);
        }
    }

    // Trails (before particles)
    m_trails.render(renderer);

    // Particles
    m_particles.render(renderer, m_camera);

    // Dimension switch visual effect
    m_dimManager.applyVisualEffect(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Entropy visual effects
    m_entropy.applyVisualEffects(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Collapse warning
    if (m_collapsing) {
        float urgency = m_collapseTimer / m_collapseMaxTime;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint8 a = static_cast<Uint8>(urgency * 100);
        SDL_SetRenderDrawColor(renderer, 255, 30, 0, a);
        SDL_Rect border = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderDrawRect(renderer, &border);

        // Collapse timer text
        TTF_Font* font = game->getFont();
        if (font) {
            float remaining = m_collapseMaxTime - m_collapseTimer;
            if (remaining < 0) remaining = 0;
            int secs = static_cast<int>(remaining);
            int tenths = static_cast<int>((remaining - secs) * 10);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "COLLAPSE: %d.%d", secs, tenths);

            // Pulsing red-white text
            Uint8 pulse = static_cast<Uint8>(200 + 55 * std::sin(SDL_GetTicks() * 0.01f));
            SDL_Color tc = {pulse, static_cast<Uint8>(pulse / 4), 0, 255};
            SDL_Surface* ts = TTF_RenderText_Blended(font, buf, tc);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    SDL_Rect tr = {640 - ts->w / 2, 50, ts->w, ts->h};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
        }
    }

    // FIX: Rift progress indicator (top center, always visible when rifts remain)
    if (m_level && !m_collapsing && !m_levelComplete) {
        int totalRifts = static_cast<int>(m_level->getRiftPositions().size());
        int remaining = totalRifts - m_levelRiftsRepaired;
        if (remaining > 0) {
            int dimARemaining = 0;
            int dimBRemaining = 0;
            for (int i = 0; i < totalRifts; i++) {
                if (m_repairedRiftIndices.count(i)) continue;
                int requiredDim = m_level->getRiftRequiredDimension(i);
                if (requiredDim == 2) dimBRemaining++;
                else dimARemaining++;
            }
            TTF_Font* font = game->getFont();
            if (font) {
                char riftBuf[96];
                std::snprintf(riftBuf, sizeof(riftBuf), "Rifts: %d / %d  [A:%d B:%d]",
                              m_levelRiftsRepaired, totalRifts, dimARemaining, dimBRemaining);
                SDL_Color rc = {180, 130, 255, 200};
                SDL_Surface* rs = TTF_RenderText_Blended(font, riftBuf, rc);
                if (rs) {
                    SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
                    if (rt) {
                        SDL_Rect rr = {640 - rs->w / 2, 30, rs->w, rs->h};
                        SDL_RenderCopy(renderer, rt, nullptr, &rr);
                        SDL_DestroyTexture(rt);
                    }
                    SDL_FreeSurface(rs);
                }
            }
        }
    }

    // FIX: Exit locked hint when player reaches exit before repairing rifts
    if (m_exitLockedHintTimer > 0) {
        TTF_Font* font = game->getFont();
        if (font) {
            Uint8 alpha = static_cast<Uint8>(std::min(1.0f, m_exitLockedHintTimer) * 255);
            SDL_Color hc = {255, 100, 80, alpha};
            const char* hintText = (m_isBossLevel && !m_bossDefeated)
                ? "Defeat the boss to unlock exit!"
                : "Repair all rifts to unlock exit!";
            SDL_Surface* hs = TTF_RenderText_Blended(font, hintText, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_SetTextureAlphaMod(ht, alpha);
                    SDL_Rect hr = {640 - hs->w / 2, 460, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }

    if (m_riftDimensionHintTimer > 0 && m_riftDimensionHintRequiredDim > 0) {
        TTF_Font* font = game->getFont();
        if (font) {
            const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
            int dimBShardBonus = getDimensionShiftDimBShardBonusPercent(m_currentDifficulty);
            Uint8 alpha = static_cast<Uint8>(std::min(1.0f, m_riftDimensionHintTimer) * 255);
            SDL_Color hc = (m_riftDimensionHintRequiredDim == 2)
                ? SDL_Color{255, 90, 145, alpha}
                : SDL_Color{90, 180, 255, alpha};
            char hintText[160];
            if (m_riftDimensionHintRequiredDim == 2) {
                std::snprintf(hintText, sizeof(hintText),
                              "This rift stabilizes in DIM-B. +%d%% shards, -%.0f entropy on repair, +%.2f entropy/s.",
                              dimBShardBonus,
                              shiftBalance.dimBEntropyRepairBonus,
                              shiftBalance.dimBEntropyPerSecond);
            } else {
                std::snprintf(hintText, sizeof(hintText),
                              "This rift stabilizes in DIM-A. Safer route, no DIM-B pressure.");
            }
            SDL_Surface* hs = TTF_RenderText_Blended(font, hintText, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_SetTextureAlphaMod(ht, alpha);
                    SDL_Rect hr = {640 - hs->w / 2, 482, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }

    // Near rift indicator
    if (m_nearRiftIndex >= 0 && !m_activePuzzle) {
        TTF_Font* font = game->getFont();
        if (font) {
            const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
            int dimBShardBonus = getDimensionShiftDimBShardBonusPercent(m_currentDifficulty);
            int currentDim = m_dimManager.getCurrentDimension();
            int requiredDim = m_level ? m_level->getRiftRequiredDimension(m_nearRiftIndex) : 0;
            bool riftActive = requiredDim == 0 || requiredDim == currentDim;
            SDL_Color c = (requiredDim == 2)
                ? SDL_Color{255, 90, 145, 220}
                : SDL_Color{90, 180, 255, 220};
            char promptText[160];
            std::snprintf(promptText, sizeof(promptText), "Press F to repair rift");
            if (!riftActive) {
                if (requiredDim == 2) {
                    std::snprintf(promptText, sizeof(promptText),
                                  "Shift to DIM-B: +%d%% shards, -%.0f entropy on repair",
                                  dimBShardBonus,
                                  shiftBalance.dimBEntropyRepairBonus);
                } else {
                    std::snprintf(promptText, sizeof(promptText),
                                  "Shift to DIM-A to stabilize this rift");
                }
            } else if (requiredDim == 2) {
                std::snprintf(promptText, sizeof(promptText),
                              "Press F to repair volatile DIM-B rift (+%d%% shards, -%.0f entropy)",
                              dimBShardBonus,
                              shiftBalance.dimBEntropyRepairBonus);
            } else if (requiredDim == 1) {
                std::snprintf(promptText, sizeof(promptText),
                              "Press F to repair stable DIM-A rift");
            }
            SDL_Surface* s = TTF_RenderText_Blended(font, promptText, c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 500, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Off-screen rift direction indicators
    if (m_level && !m_activePuzzle) {
        auto rifts = m_level->getRiftPositions();
        Vec2 camPos = m_camera.getPosition();
        float halfW = 640.0f, halfH = 360.0f;

        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            // Skip already-repaired rifts
            if (m_repairedRiftIndices.count(i)) continue;

            float sx = (rifts[i].x - camPos.x) * m_camera.zoom + halfW;
            float sy = (rifts[i].y - camPos.y) * m_camera.zoom + halfH;

            // Only show if off-screen
            if (sx >= -10 && sx <= 1290 && sy >= -10 && sy <= 730) continue;

            // Clamp to screen edge with margin
            float cx = std::max(25.0f, std::min(1255.0f, sx));
            float cy = std::max(25.0f, std::min(695.0f, sy));

            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.005f + i * 1.5f);
            Uint8 pa = static_cast<Uint8>(120 + 80 * pulse);
            int requiredDim = m_level->getRiftRequiredDimension(i);
            Uint8 rr = (requiredDim == 2) ? 255 : 90;
            Uint8 rg = (requiredDim == 2) ? 90 : 180;
            Uint8 rb = (requiredDim == 2) ? 145 : 255;

            // Diamond indicator
            SDL_SetRenderDrawColor(renderer, rr, rg, rb, pa);
            SDL_Rect ind = {static_cast<int>(cx) - 5, static_cast<int>(cy) - 5, 10, 10};
            SDL_RenderFillRect(renderer, &ind);

            // Arrow pointing toward rift
            float angle = std::atan2(sy - cy, sx - cx);
            int ax = static_cast<int>(cx + std::cos(angle) * 14);
            int ay = static_cast<int>(cy + std::sin(angle) * 14);
            SDL_SetRenderDrawColor(renderer, rr, rg, rb, pa);
            SDL_RenderDrawLine(renderer, static_cast<int>(cx), static_cast<int>(cy), ax, ay);
        }
    }

    // Off-screen exit direction indicator (visible when exit is active OR during collapse)
    if (m_level && (m_level->isExitActive() || m_collapsing) && !m_levelComplete) {
        Vec2 exitPos = m_level->getExitPoint();
        Vec2 camPos = m_camera.getPosition();
        float halfW = 640.0f, halfH = 360.0f;

        float sx = (exitPos.x - camPos.x) * m_camera.zoom + halfW;
        float sy = (exitPos.y - camPos.y) * m_camera.zoom + halfH;

        // Only show if exit is off-screen
        if (sx < -10 || sx > 1290 || sy < -10 || sy > 730) {
            // Clamp to screen edge with margin
            float cx = std::max(30.0f, std::min(1250.0f, sx));
            float cy = std::max(30.0f, std::min(690.0f, sy));

            Uint8 pa, gr, gg;
            if (m_collapsing) {
                // Urgency-based pulsing during collapse
                float urgency = m_collapseTimer / std::max(1.0f, m_collapseMaxTime);
                float pulseSpeed = 0.006f + urgency * 0.012f;
                float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * pulseSpeed);
                pa = static_cast<Uint8>(160 + 95 * pulse);
                gr = static_cast<Uint8>(80 + 175 * urgency);
                gg = static_cast<Uint8>(255 - 80 * urgency);
            } else {
                // Calm green pulsing when exit is simply active
                float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.004f);
                pa = static_cast<Uint8>(120 + 80 * pulse);
                gr = 60;
                gg = 220;
            }
            SDL_SetRenderDrawColor(renderer, gr, gg, 60, pa);

            // Larger diamond for exit (8px vs 5px for rifts)
            int icx = static_cast<int>(cx);
            int icy = static_cast<int>(cy);
            SDL_Rect ind = {icx - 8, icy - 8, 16, 16};
            SDL_RenderFillRect(renderer, &ind);

            // Inner glow
            SDL_SetRenderDrawColor(renderer, 255, 255, 200, static_cast<Uint8>(pa * 0.6f));
            SDL_Rect inner = {icx - 4, icy - 4, 8, 8};
            SDL_RenderFillRect(renderer, &inner);

            // Arrow pointing toward exit
            float angle = std::atan2(sy - cy, sx - cx);
            int ax = static_cast<int>(cx + std::cos(angle) * 18);
            int ay = static_cast<int>(cy + std::sin(angle) * 18);
            SDL_SetRenderDrawColor(renderer, gr, gg, 60, pa);
            SDL_RenderDrawLine(renderer, icx, icy, ax, ay);
            // Thicker arrow: draw offset lines
            SDL_RenderDrawLine(renderer, icx + 1, icy, ax + 1, ay);
            SDL_RenderDrawLine(renderer, icx, icy + 1, ax, ay + 1);

            // "EXIT" label next to indicator
            TTF_Font* font = game->getFont();
            if (font) {
                SDL_Color labelColor = {gr, gg, 60, pa};
                SDL_Surface* ls = TTF_RenderText_Blended(font, "EXIT", labelColor);
                if (ls) {
                    SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                    if (lt) {
                        // Position label offset from indicator, avoid going off-screen
                        int lx = icx - ls->w / 2;
                        int ly = icy - 22;
                        if (ly < 5) ly = icy + 12; // Flip below if at top edge
                        lx = std::max(5, std::min(SCREEN_WIDTH - ls->w - 5, lx));
                        SDL_Rect lr = {lx, ly, ls->w, ls->h};
                        SDL_RenderCopy(renderer, lt, nullptr, &lr);
                        SDL_DestroyTexture(lt);
                    }
                    SDL_FreeSurface(ls);
                }
            }
        }
    }

    // Puzzle overlay
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    // Level complete: rift warp transition effect
    if (m_levelComplete) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        float progress = m_levelCompleteTimer / 2.0f; // 0 to 1 over 2 seconds
        if (progress > 1.0f) progress = 1.0f;
        Uint32 ticks = SDL_GetTicks();

        // Growing dark overlay
        Uint8 overlayA = static_cast<Uint8>(progress * 180);
        SDL_SetRenderDrawColor(renderer, 10, 5, 20, overlayA);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);

        // Expanding rift circle from center
        int maxRadius = 400;
        int radius = static_cast<int>(progress * maxRadius);
        float pulse = 0.7f + 0.3f * std::sin(ticks * 0.01f);
        Uint8 riftA = static_cast<Uint8>((1.0f - progress * 0.5f) * 120 * pulse);

        // Rift ring (multiple concentric rings)
        for (int r = radius - 6; r <= radius + 6; r += 3) {
            if (r < 0) continue;
            for (int angle = 0; angle < 60; angle++) {
                float a = angle * 6.283185f / 60.0f + ticks * 0.002f;
                int px = 640 + static_cast<int>(std::cos(a) * r);
                int py = 360 + static_cast<int>(std::sin(a) * r);
                if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT) continue;
                SDL_SetRenderDrawColor(renderer, 180, 100, 255, riftA);
                SDL_Rect dot = {px - 1, py - 1, 3, 3};
                SDL_RenderFillRect(renderer, &dot);
            }
        }

        // Scanlines converging toward center
        if (progress > 0.3f) {
            int lineCount = static_cast<int>((progress - 0.3f) * 20);
            for (int i = 0; i < lineCount; i++) {
                int y = ((i * 4513 + ticks / 40) % SCREEN_HEIGHT);
                Uint8 la = static_cast<Uint8>(progress * 60);
                SDL_SetRenderDrawColor(renderer, 150, 80, 220, la);
                SDL_Rect line = {0, y, SCREEN_WIDTH, 1};
                SDL_RenderFillRect(renderer, &line);
            }
        }

        // Text banner
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(180 * std::min(1.0f, progress * 3.0f)));
        SDL_Rect banner = {0, 330, SCREEN_WIDTH, 60};
        SDL_RenderFillRect(renderer, &banner);

        TTF_Font* font = game->getFont();
        if (font) {
            float textAlpha = std::min(1.0f, progress * 3.0f);
            Uint8 ta = static_cast<Uint8>(textAlpha * 255);
            SDL_Color c = {140, 255, 180, ta};
            SDL_Surface* s = TTF_RenderText_Blended(font, "RIFT STABILIZED - Warping to next dimension...", c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 348, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Random events (NPCs in world space)
    renderRandomEvents(renderer, game->getFont());

    // NPCs
    renderNPCs(renderer, game->getFont());

    // NPC dialog overlay
    if (m_showNPCDialog) {
        renderNPCDialog(renderer, game->getFont());
    }

    // Tutorial hints (first 3 levels only)
    renderTutorialHints(renderer, game->getFont());

    // Floating damage numbers
    renderDamageNumbers(renderer, game->getFont());

    // Kill feed (bottom-right)
    renderKillFeed(renderer, game->getFont());

    // Boss HP bar at top of screen
    if (m_isBossLevel && !m_bossDefeated) {
        renderBossHealthBar(renderer, game->getFont());
    }

    // Combo milestone golden flash overlay
    if (m_comboMilestoneFlash > 0) {
        float alpha = m_comboMilestoneFlash / 0.4f;
        Uint8 a = static_cast<Uint8>(alpha * 60);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, a);
        SDL_Rect fullScreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fullScreen);
    }

    // Screen effects (vignette, low-HP pulse, kill flash, boss intro, ripple, glitch)
    m_screenEffects.render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Damage flash overlay
    m_hud.renderFlash(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Wave/area clear celebration text
    if (m_waveClearTimer > 0 && game->getFont()) {
        float t = m_waveClearTimer / 2.0f; // 1.0 → 0.0
        // Fade in fast (first 0.3s), then hold, then fade out (last 0.5s)
        float alpha;
        if (t > 0.85f) alpha = (1.0f - t) / 0.15f; // fade in
        else if (t < 0.25f) alpha = t / 0.25f;       // fade out
        else alpha = 1.0f;                             // hold
        Uint8 a = static_cast<Uint8>(alpha * 255);

        // Gold text "AREA CLEARED"
        SDL_Color gold = {255, 215, 0, a};
        SDL_Surface* surf = TTF_RenderText_Blended(game->getFont(), "AREA CLEARED", gold);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                // Scale up with slight pulse
                float pulse = 1.0f + 0.05f * std::sin(m_waveClearTimer * 8.0f);
                float scale = 2.0f * pulse;
                int w = static_cast<int>(surf->w * scale);
                int h = static_cast<int>(surf->h * scale);
                SDL_Rect dst = {SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT / 3 - h / 2, w, h};
                SDL_SetTextureAlphaMod(tex, a);
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        // Subtle gold line underneath
        if (a > 30) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            int lineW = static_cast<int>(200 * alpha);
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, static_cast<Uint8>(a * 0.5f));
            SDL_Rect line = {SCREEN_WIDTH / 2 - lineW / 2, SCREEN_HEIGHT / 3 + 14, lineW, 2};
            SDL_RenderFillRect(renderer, &line);
        }
    }

    // HUD (on top of everything)
    m_hud.setFloor(m_currentDifficulty);
    m_hud.setKillCount(enemiesKilled);
    m_hud.render(renderer, game->getFont(), m_player.get(), &m_entropy, &m_dimManager,
                 SCREEN_WIDTH, SCREEN_HEIGHT, game->getFPS(), game->getUpgradeSystem().getRiftShards());

    // Relic bar (bottom left, below buffs)
    renderRelicBar(renderer, game->getFont());

    // Challenge HUD (left side, below main HUD)
    renderChallengeHUD(renderer, game->getFont());

    // Combat challenge indicator (top center)
    renderCombatChallenge(renderer, game->getFont());

    // Event chain tracker (left side)
    renderEventChain(renderer, game->getFont());

    // Minimap (top right corner, M key to toggle)
    m_hud.renderMinimap(renderer, m_level.get(), m_player.get(), &m_dimManager,
                        SCREEN_WIDTH, SCREEN_HEIGHT, &m_entities, &m_repairedRiftIndices);

    // Achievement notification popup
    auto* notif = game->getAchievements().getActiveNotification();
    TTF_Font* achFont = game->getFont();
    if (notif && achFont) {
        float t = notif->timer / notif->duration;
        float slideIn = std::min(1.0f, (1.0f - t) * 5.0f); // fast slide in
        float fadeOut = std::min(1.0f, t * 3.0f); // fade out at end
        float alpha = slideIn * fadeOut;
        Uint8 a = static_cast<Uint8>(alpha * 220);

        bool hasReward = !notif->rewardText.empty();
        int popW = 300, popH = hasReward ? 56 : 40;
        int popX = 640 - popW / 2;
        int popY = 660 - popH - static_cast<int>(slideIn * 20);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 40, 25, a);
        SDL_Rect bg = {popX, popY, popW, popH};
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 100, 220, 120, a);
        SDL_RenderDrawRect(renderer, &bg);

        // Trophy icon
        SDL_SetRenderDrawColor(renderer, 255, 200, 50, a);
        SDL_Rect trophy = {popX + 10, popY + 8, 16, 16};
        SDL_RenderFillRect(renderer, &trophy);

        // Achievement name
        char achText[128];
        snprintf(achText, sizeof(achText), "Achievement: %s", notif->name.c_str());
        SDL_Color tc = {200, 255, 210, a};
        SDL_Surface* ts = TTF_RenderText_Blended(achFont, achText, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_SetTextureAlphaMod(tt, a);
                SDL_Rect tr = {popX + 34, popY + 4, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }

        // Reward text line (golden)
        if (hasReward) {
            char rewardText[128];
            snprintf(rewardText, sizeof(rewardText), "Reward: %s", notif->rewardText.c_str());
            SDL_Color rc = {255, 220, 80, a};
            SDL_Surface* rs = TTF_RenderText_Blended(achFont, rewardText, rc);
            if (rs) {
                SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
                if (rt) {
                    SDL_SetTextureAlphaMod(rt, a);
                    SDL_Rect rr = {popX + 34, popY + 28, rs->w, rs->h};
                    SDL_RenderCopy(renderer, rt, nullptr, &rr);
                    SDL_DestroyTexture(rt);
                }
                SDL_FreeSurface(rs);
            }
        }
    }

    // Lore discovery notification popup
    if (auto* lore = game->getLoreSystem()) {
        auto* loreNotif = lore->getActiveNotification();
        TTF_Font* loreFont = game->getFont();
        if (loreNotif && loreFont) {
            float t = loreNotif->timer / loreNotif->duration;
            float slideIn = std::min(1.0f, (1.0f - t) * 4.0f);
            float fadeOut = std::min(1.0f, t * 2.5f);
            float alpha = slideIn * fadeOut;
            Uint8 a = static_cast<Uint8>(alpha * 230);

            int popW = 320, popH = 44;
            int popX = SCREEN_WIDTH / 2 - popW / 2;
            int popY = 20 + static_cast<int>((1.0f - slideIn) * 30);

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            // Dark purple background
            SDL_SetRenderDrawColor(renderer, 25, 15, 40, a);
            SDL_Rect bg = {popX, popY, popW, popH};
            SDL_RenderFillRect(renderer, &bg);
            // Purple border with glow
            float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.006f);
            Uint8 borderA = static_cast<Uint8>(a * pulse);
            SDL_SetRenderDrawColor(renderer, 160, 100, 255, borderA);
            SDL_RenderDrawRect(renderer, &bg);
            SDL_Rect innerBorder = {popX + 1, popY + 1, popW - 2, popH - 2};
            SDL_SetRenderDrawColor(renderer, 120, 70, 200, static_cast<Uint8>(borderA * 0.5f));
            SDL_RenderDrawRect(renderer, &innerBorder);

            // Scroll/book icon (small rectangle with lines)
            SDL_SetRenderDrawColor(renderer, 180, 140, 255, a);
            SDL_Rect scrollIcon = {popX + 12, popY + 10, 12, 16};
            SDL_RenderFillRect(renderer, &scrollIcon);
            SDL_SetRenderDrawColor(renderer, 100, 60, 180, a);
            for (int line = 0; line < 3; line++) {
                SDL_RenderDrawLine(renderer, popX + 14, popY + 14 + line * 4,
                                   popX + 22, popY + 14 + line * 4);
            }

            // "Lore Discovered" label
            SDL_Color labelColor = {140, 110, 200, a};
            SDL_Surface* labelSurf = TTF_RenderText_Blended(loreFont, "LORE DISCOVERED", labelColor);
            if (labelSurf) {
                SDL_Texture* labelTex = SDL_CreateTextureFromSurface(renderer, labelSurf);
                if (labelTex) {
                    SDL_SetTextureAlphaMod(labelTex, a);
                    SDL_Rect lr = {popX + 32, popY + 4, labelSurf->w, labelSurf->h};
                    SDL_RenderCopy(renderer, labelTex, nullptr, &lr);
                    SDL_DestroyTexture(labelTex);
                }
                SDL_FreeSurface(labelSurf);
            }

            // Lore title
            SDL_Color titleColor = {220, 200, 255, a};
            SDL_Surface* titleSurf = TTF_RenderText_Blended(loreFont, loreNotif->title.c_str(), titleColor);
            if (titleSurf) {
                SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
                if (titleTex) {
                    SDL_SetTextureAlphaMod(titleTex, a);
                    SDL_Rect tr = {popX + 32, popY + 22, titleSurf->w, titleSurf->h};
                    SDL_RenderCopy(renderer, titleTex, nullptr, &tr);
                    SDL_DestroyTexture(titleTex);
                }
                SDL_FreeSurface(titleSurf);
            }
        }
    }

    // Unlock notification popups (golden, top-center)
    renderUnlockNotifications(renderer, game->getFont());

    // Death sequence overlay: red vignette + desaturation effect
    if (m_playerDying) {
        float progress = 1.0f - (m_deathSequenceTimer / m_deathSequenceDuration);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Full screen red-black fade (intensifies over time)
        Uint8 overlayAlpha = static_cast<Uint8>(std::min(180.0f, progress * 220.0f));
        SDL_SetRenderDrawColor(renderer, 40, 0, 0, overlayAlpha);
        SDL_Rect fullScreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fullScreen);

        // Vignette border (thick red edges)
        int borderW = static_cast<int>(20 + progress * 60);
        Uint8 borderAlpha = static_cast<Uint8>(std::min(200.0f, progress * 250.0f));
        SDL_SetRenderDrawColor(renderer, 180, 20, 0, borderAlpha);
        SDL_Rect top = {0, 0, SCREEN_WIDTH, borderW};
        SDL_Rect bot = {0, SCREEN_HEIGHT - borderW, SCREEN_WIDTH, borderW};
        SDL_Rect lft = {0, 0, borderW, SCREEN_HEIGHT};
        SDL_Rect rgt = {SCREEN_WIDTH - borderW, 0, borderW, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &lft);
        SDL_RenderFillRect(renderer, &rgt);

        // "SUIT FAILURE" text (fades in)
        TTF_Font* font = game->getFont();
        if (font && progress > 0.2f) {
            float textAlpha = std::min(1.0f, (progress - 0.2f) * 2.0f);
            Uint8 ta = static_cast<Uint8>(textAlpha * 255);
            float pulse = 0.7f + 0.3f * std::sin(SDL_GetTicks() * 0.015f);
            Uint8 tr = static_cast<Uint8>(200 * pulse + 55);
            SDL_Color deathColor = {tr, 30, 20, ta};
            SDL_Surface* ds = TTF_RenderText_Blended(font, "SUIT FAILURE", deathColor);
            if (ds) {
                SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
                if (dt) {
                    SDL_SetTextureAlphaMod(dt, ta);
                    // Center of screen, slight upward drift
                    int yOff = static_cast<int>(progress * 15.0f);
                    SDL_Rect dr = {SCREEN_WIDTH / 2 - ds->w / 2,
                                   SCREEN_HEIGHT / 2 - ds->h / 2 - yOff,
                                   ds->w, ds->h};
                    SDL_RenderCopy(renderer, dt, nullptr, &dr);
                    SDL_DestroyTexture(dt);
                }
                SDL_FreeSurface(ds);
            }
        }
    }

    // Debug overlay (F3)
    if (m_showDebugOverlay) {
        renderDebugOverlay(renderer, game->getFont());
    }

    // Relic choice overlay
    if (m_showRelicChoice) {
        renderRelicChoice(renderer, game->getFont());
    }
}

void PlayState::renderDebugOverlay(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || !m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // Compute raw damage multiplier (before cap)
    float rawDmg = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::BloodFrenzy && hpPct < 0.3f) rawDmg += 0.25f;
        if (r.id == RelicID::BerserkerCore) rawDmg += 0.50f;
        if (r.id == RelicID::VoidHunger) rawDmg += relics.voidHungerBonus;
        if (r.id == RelicID::DualityGem && (curDim == 1 || RelicSynergy::isDualNatureActive(relics)))
            rawDmg += 0.25f;
        if (r.id == RelicID::StabilityMatrix) {
            float rate = RelicSynergy::getStabilityDmgPerSec(relics);
            float maxB = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
            rawDmg += std::min(relics.stabilityTimer * rate, maxB);
        }
    }
    rawDmg *= RelicSynergy::getPhaseHunterDamageMult(relics);
    float clampedDmg = RelicSystem::getDamageMultiplier(relics, hpPct, curDim);

    // Compute raw attack speed multiplier (before cap)
    float rawSpd = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) rawSpd += 0.15f;
    }
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0)
        rawSpd += relics.riftConduitStacks * 0.10f;
    float clampedSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    // Switch CD with floor detection
    float baseCD = 0.5f;
    float cdMult = RelicSystem::getSwitchCooldownMult(relics);
    float finalCD = m_dimManager.switchCooldown;
    bool cdFloored = (baseCD * cdMult) < finalCD + 0.001f && relics.hasRelic(RelicID::RiftMantle);

    // Zone count
    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
    });

    // Gameplay paused detection
    bool gameplayPaused = m_showRelicChoice || m_showNPCDialog
        || (m_activePuzzle && m_activePuzzle->isActive());

    // Stability derived values
    float stabRate = RelicSynergy::getStabilityDmgPerSec(relics);
    float stabMax = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
    float stabPct = std::min(relics.stabilityTimer * stabRate, stabMax) * 100.0f;

    int synergyCount = RelicSynergy::getActiveSynergyCount(relics);
    float dmgTakenMult = RelicSystem::getDamageTakenMult(relics, curDim);

    // Build lines into stack buffer
    struct Line { char text[96]; SDL_Color color; };
    Line lines[15];
    int lineCount = 0;

    auto addLine = [&](SDL_Color c, const char* fmt, ...) {
        if (lineCount >= 15) return;
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(lines[lineCount].text, sizeof(lines[0].text), fmt, args);
        va_end(args);
        lines[lineCount].color = c;
        lineCount++;
    };

    SDL_Color cHeader = {255, 220, 80, 255};
    SDL_Color cNormal = {200, 220, 240, 255};
    SDL_Color cWarn   = {255, 100, 80, 255};
    SDL_Color cGreen  = {100, 255, 140, 255};

    addLine(cHeader, "=== RELIC SAFETY RAILS (F3/F4) ===");
    addLine(gameplayPaused ? cWarn : cGreen, "Paused: %s", gameplayPaused ? "YES" : "NO");
    addLine(rawDmg > clampedDmg + 0.001f ? cWarn : cNormal,
            "DMG Mult: %.2fx raw -> %.2fx clamped", rawDmg, clampedDmg);
    addLine(rawSpd > clampedSpd + 0.001f ? cWarn : cNormal,
            "ATK Speed: %.2fx raw -> %.2fx clamped", rawSpd, clampedSpd);
    addLine(dmgTakenMult > 1.001f ? cWarn : dmgTakenMult < 0.999f ? cGreen : cNormal,
            "Damage Taken Mult: %.2fx", dmgTakenMult);
    if (cdFloored)
        addLine(cWarn, "Switch CD: %.2fs x%.2f = %.2fs FLOOR", baseCD, cdMult, finalCD);
    else
        addLine(cNormal, "Switch CD: %.2fs x%.2f = %.2fs", baseCD, cdMult, finalCD);
    addLine(cNormal, "VoidHunger: %.0f%% / %.0f%% cap",
            relics.voidHungerBonus * 100.0f, 40.0f);
    addLine(cNormal, "VoidRes ICD: %.2fs remain", std::max(0.0f, relics.voidResonanceProcCD));
    addLine(zoneCount > 0 ? cHeader : cNormal,
            "Residue: %d/%d zones, spawn ICD %.2fs",
            zoneCount, RelicSystem::getMaxResidueZones(),
            std::max(0.0f, relics.dimResidueSpawnCD));
    addLine(cNormal, "Stability: %.1fs (%.0f%% DMG)",
            relics.stabilityTimer, stabPct);
    addLine(cNormal, "Conduit: %d stacks, %.1fs left",
            relics.riftConduitStacks, std::max(0.0f, relics.riftConduitTimer));
    addLine(cNormal, "Dim: %d | Synergies: %d | Seed: %d",
            curDim, synergyCount, m_runSeed);
    addLine({140, 140, 160, 255}, "F4: snapshot to balance_snapshots.csv");

    // Render panel
    int panelX = 10, panelY = 190;
    int lineH = 16, padX = 8, padY = 4;
    int panelW = 360;
    int panelH = padY * 2 + lineCount * lineH;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 255, 220, 80, 120);
    SDL_RenderDrawRect(renderer, &bg);

    for (int i = 0; i < lineCount; i++) {
        SDL_Surface* s = TTF_RenderText_Blended(font, lines[i].text, lines[i].color);
        if (!s) continue;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        if (t) {
            SDL_Rect dst = {panelX + padX, panelY + padY + i * lineH, s->w, s->h};
            SDL_RenderCopy(renderer, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
    }

    // Process pending F4 snapshot (only on keypress, not per frame)
    if (m_pendingSnapshot) {
        m_pendingSnapshot = false;
        writeBalanceSnapshot();
    }
}

void PlayState::renderRelicChoice(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_relicChoices.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Dark overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Title
    {
        SDL_Color tc = {255, 215, 100, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "Choose a Relic", tc);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {640 - s->w / 2, 180, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Relic cards
    int cardW = 200;
    int cardH = 260;
    int gap = 30;
    int totalW = static_cast<int>(m_relicChoices.size()) * cardW + (static_cast<int>(m_relicChoices.size()) - 1) * gap;
    int startX = 640 - totalW / 2;
    int cardY = 230;

    for (int i = 0; i < static_cast<int>(m_relicChoices.size()); i++) {
        int cx = startX + i * (cardW + gap);
        bool selected = (i == m_relicChoiceSelected);
        auto& data = RelicSystem::getRelicData(m_relicChoices[i]);

        // Card background
        SDL_SetRenderDrawColor(renderer, 15, 15, 25, 220);
        SDL_Rect cardBg = {cx, cardY, cardW, cardH};
        SDL_RenderFillRect(renderer, &cardBg);

        // Border (glow when selected)
        Uint8 borderA = selected ? 255 : 100;
        SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, borderA);
        SDL_RenderDrawRect(renderer, &cardBg);
        if (selected) {
            SDL_Rect outer = {cx - 2, cardY - 2, cardW + 4, cardH + 4};
            SDL_RenderDrawRect(renderer, &outer);
        }

        // Relic icon (colored orb)
        int orbX = cx + cardW / 2;
        int orbY = cardY + 50;
        int orbR = 24;
        for (int oy = -orbR; oy <= orbR; oy++) {
            for (int ox = -orbR; ox <= orbR; ox++) {
                if (ox * ox + oy * oy <= orbR * orbR) {
                    float dist = std::sqrt(static_cast<float>(ox * ox + oy * oy)) / orbR;
                    Uint8 a = static_cast<Uint8>((1.0f - dist * 0.5f) * 255);
                    SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, a);
                    SDL_RenderDrawPoint(renderer, orbX + ox, orbY + oy);
                }
            }
        }

        // Tier text + CURSED label
        bool isCursedRelic = RelicSystem::isCursed(m_relicChoices[i]);
        const char* tierText = "COMMON";
        SDL_Color tierColor = {180, 180, 180, 255};
        if (data.tier == RelicTier::Rare)      { tierText = "RARE";      tierColor = {80, 180, 255, 255}; }
        else if (data.tier == RelicTier::Legendary) { tierText = "LEGENDARY"; tierColor = {255, 200, 50, 255}; }
        // CURSED override: override tier display with red "CURSED" label
        if (isCursedRelic) { tierText = "CURSED"; tierColor = {255, 50, 50, 255}; }
        {
            SDL_Surface* s = TTF_RenderText_Blended(font, tierText, tierColor);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 90, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
        // Cursed relics: draw a red skull-mark (X lines) in the top-right corner of the card
        if (isCursedRelic) {
            SDL_SetRenderDrawColor(renderer, 200, 20, 20, 200);
            int mx = cx + cardW - 14, my = cardY + 8;
            SDL_RenderDrawLine(renderer, mx, my, mx + 10, my + 10);
            SDL_RenderDrawLine(renderer, mx + 10, my, mx, my + 10);
            SDL_RenderDrawLine(renderer, mx + 1, my, mx + 11, my + 10);
            SDL_RenderDrawLine(renderer, mx + 11, my, mx + 1, my + 10);
            // Dark red card tint overlay
            SDL_SetRenderDrawColor(renderer, 80, 0, 0, 30);
            SDL_RenderFillRect(renderer, &cardBg);
        }

        // Name
        {
            SDL_Color nc = {255, 255, 255, 255};
            SDL_Surface* s = TTF_RenderText_Blended(font, data.name, nc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 120, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Description
        {
            SDL_Color dc = {180, 180, 200, 220};
            SDL_Surface* s = TTF_RenderText_Blended(font, data.description, dc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 160, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Key hint
        char keyBuf[8];
        std::snprintf(keyBuf, sizeof(keyBuf), "[%d]", i + 1);
        {
            SDL_Color kc = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 120, 180};
            SDL_Surface* s = TTF_RenderText_Blended(font, keyBuf, kc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + cardH - 35, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }
}

void PlayState::renderRelicBar(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    if (relics.relics.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int iconSize = 20;
    int gap = 4;
    int startX = 15;
    int startY = SCREEN_HEIGHT - 50;

    for (int i = 0; i < static_cast<int>(relics.relics.size()) && i < 12; i++) {
        auto& data = RelicSystem::getRelicData(relics.relics[i].id);
        int ix = startX + i * (iconSize + gap);

        // Background
        SDL_SetRenderDrawColor(renderer, 10, 10, 20, 180);
        SDL_Rect bg = {ix, startY, iconSize, iconSize};
        SDL_RenderFillRect(renderer, &bg);

        // Colored orb
        Uint8 r = data.glowColor.r, g = data.glowColor.g, b = data.glowColor.b;
        if (relics.relics[i].consumed) { r /= 3; g /= 3; b /= 3; }
        SDL_SetRenderDrawColor(renderer, r, g, b, 200);
        SDL_Rect orb = {ix + 3, startY + 3, iconSize - 6, iconSize - 6};
        SDL_RenderFillRect(renderer, &orb);

        // Border: cursed relics get a red border, others get tier-based opacity
        if (RelicSystem::isCursed(relics.relics[i].id)) {
            SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
            SDL_RenderDrawRect(renderer, &bg);
            // Double border for emphasis
            SDL_Rect outerBorder = {ix - 1, startY - 1, iconSize + 2, iconSize + 2};
            SDL_SetRenderDrawColor(renderer, 255, 60, 60, 120);
            SDL_RenderDrawRect(renderer, &outerBorder);
        } else {
            Uint8 borderTierA = 120;
            if (data.tier == RelicTier::Rare) borderTierA = 180;
            if (data.tier == RelicTier::Legendary) borderTierA = 255;
            SDL_SetRenderDrawColor(renderer, r, g, b, borderTierA);
            SDL_RenderDrawRect(renderer, &bg);
        }
    }
}

void PlayState::renderDamageNumbers(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    for (auto& dn : m_damageNumbers) {
        float t = dn.lifetime / dn.maxLifetime; // 1.0 → 0.0
        Uint8 alpha = static_cast<Uint8>(255 * t);

        // Color: green for heals, purple for shards, cyan for buffs, red for player damage, orange for crits, yellow for normal
        SDL_Color color;
        if (dn.isShard) {
            color = {200, 150, 255, alpha};
        } else if (dn.isBuff) {
            color = {100, 220, 255, alpha};
        } else if (dn.isHeal) {
            color = {50, 255, 80, alpha};
        } else if (dn.isPlayerDamage) {
            color = {255, 60, 40, alpha};
        } else if (dn.isCritical) {
            color = {255, 180, 40, alpha};
        } else {
            color = {255, 240, 100, alpha};
        }

        char buf[32];
        if (dn.isShard) {
            std::snprintf(buf, sizeof(buf), "+%.0f", dn.value);
        } else if (dn.isBuff && dn.buffText) {
            std::snprintf(buf, sizeof(buf), "%s", dn.buffText);
        } else if (dn.isHeal) {
            std::snprintf(buf, sizeof(buf), "+%.0f", dn.value);
        } else if (dn.isCritical) {
            std::snprintf(buf, sizeof(buf), "CRIT! %.0f", dn.value);
        } else {
            std::snprintf(buf, sizeof(buf), "%.0f", dn.value);
        }

        // Convert world position to screen position
        SDL_FRect worldRect = {dn.position.x - 10, dn.position.y - 8, 20, 16};
        SDL_Rect screenRect = m_camera.worldToScreen(worldRect);

        SDL_Surface* surface = TTF_RenderText_Blended(font, buf, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                // Scale up for crits and large damage
                float scale = 1.0f;
                if (dn.isCritical) {
                    scale = 1.5f;
                } else if (dn.value > 20) {
                    scale = 1.3f;
                }
                SDL_Rect dst = {screenRect.x - static_cast<int>(surface->w * scale) / 2,
                               screenRect.y,
                               static_cast<int>(surface->w * scale),
                               static_cast<int>(surface->h * scale)};
                SDL_SetTextureAlphaMod(texture, alpha);
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void PlayState::renderKeyBox(SDL_Renderer* renderer, TTF_Font* font,
                              const char* key, int x, int y, Uint8 alpha) {
    // Render a key label as a bordered box
    SDL_Surface* surface = TTF_RenderText_Blended(font, key, {255, 255, 255, alpha});
    if (!surface) return;
    int pad = 4;
    SDL_Rect box = {x - pad, y - 2, surface->w + pad * 2, surface->h + 4};
    SDL_SetRenderDrawColor(renderer, 40, 50, 80, static_cast<Uint8>(alpha * 0.8f));
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 120, 160, 220, alpha);
    SDL_RenderDrawRect(renderer, &box);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void PlayState::renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Only show tutorial hints on the first run (totalRuns == 0 means no completed run yet)
    if (game->getUpgradeSystem().totalRuns > 0) return;

    // Context-based hint system: show hints when conditions are met
    const char* hint = nullptr;
    const char* keyLabel = nullptr;
    int hintSlot = -1; // which slot to mark done
    bool conditionMet = false;

    // === LEVEL 1: Basic controls (sequential) ===
    if (m_currentDifficulty == 1) {
        if (!m_tutorialHintDone[0]) {
            if (m_tutorialTimer > 1.5f && !m_hasMovedThisRun) {
                hint = "Move";
                keyLabel = "WASD";
            }
            if (m_hasMovedThisRun) { conditionMet = true; hintSlot = 0; }
        } else if (!m_tutorialHintDone[1]) {
            hint = "Jump";
            keyLabel = "SPACE";
            if (m_hasJumpedThisRun) { conditionMet = true; hintSlot = 1; }
        } else if (!m_tutorialHintDone[2]) {
            hint = "Dash through danger (also in air!)";
            keyLabel = "SHIFT";
            if (m_hasDashedThisRun) { conditionMet = true; hintSlot = 2; }
        } else if (!m_tutorialHintDone[3]) {
            bool enemyNear = false;
            if (m_player) {
                Vec2 pp = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_entities.forEach([&](Entity& e) {
                    if (e.getTag().find("enemy") != std::string::npos &&
                        e.hasComponent<TransformComponent>()) {
                        Vec2 ep = e.getComponent<TransformComponent>().getCenter();
                        float d = std::sqrt((pp.x-ep.x)*(pp.x-ep.x) + (pp.y-ep.y)*(pp.y-ep.y));
                        if (d < 250.0f) enemyNear = true;
                    }
                });
            }
            if (enemyNear) {
                hint = "Melee Attack";
                keyLabel = "J";
            }
            if (m_hasAttackedThisRun) { conditionMet = true; hintSlot = 3; }
        } else if (!m_tutorialHintDone[4]) {
            hint = "Ranged Attack (hold for charged shot!)";
            keyLabel = "K";
            if (m_hasRangedThisRun) { conditionMet = true; hintSlot = 4; }
        } else if (!m_tutorialHintDone[5]) {
            hint = "Switch Dimension - paths differ between worlds!";
            keyLabel = "E";
            if (m_dimManager.getSwitchCount() > 0) { conditionMet = true; hintSlot = 5; }
        }
    }

    // === CONTEXT-BASED HINTS (any level, triggered by situation) ===
    if (!hint) {
        if (!m_tutorialHintDone[16] && m_nearRiftIndex >= 0 && m_level) {
            int requiredDim = m_level->getRiftRequiredDimension(m_nearRiftIndex);
            int currentDim = m_dimManager.getCurrentDimension();
            if (requiredDim > 0 && requiredDim != currentDim) {
                hint = (requiredDim == 2)
                    ? "This Rift only stabilizes in DIM-B. Shift with [E], but entropy rises faster there."
                    : "This Rift only stabilizes in DIM-A. Shift with [E] to secure it safely.";
                keyLabel = "E";
                hintSlot = 16;
            }
        }
        // Near a rift: explain how to repair
        if (!hint && !m_tutorialHintDone[6] && m_nearRiftIndex >= 0) {
            hint = "Repair this Rift! Solve the puzzle to reduce Entropy.";
            keyLabel = "F";
            if (riftsRepaired > 0) { conditionMet = true; hintSlot = 6; }
        }
        // Entropy getting high
        else if (!m_tutorialHintDone[7] && !m_shownEntropyWarning && m_entropy.getPercent() > 0.5f) {
            hint = "Entropy rising! Repair Rifts to lower it. At 100% your suit fails!";
            keyLabel = nullptr;
            m_shownEntropyWarning = true;
            m_tutorialHintShowTimer = 0;
            hintSlot = 7;
            // Auto-dismiss after 6s
            if (m_tutorialHintShowTimer > 6.0f) conditionMet = true;
        }
        // On/near a conveyor
        else if (!m_tutorialHintDone[8] && !m_shownConveyorHint && m_player) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            int ts = m_level ? m_level->getTileSize() : 32;
            int fx = static_cast<int>(pt.getCenter().x) / ts;
            int fy = static_cast<int>(pt.position.y + pt.height) / ts;
            int dim = m_dimManager.getCurrentDimension();
            int cdir = 0;
            if (m_level && (m_level->isOnConveyor(fx, fy, dim, cdir) || m_level->isOnConveyor(fx, fy+1, dim, cdir))) {
                hint = "Conveyor Belt - pushes you along. Use it for speed!";
                m_shownConveyorHint = true;
                hintSlot = 8;
                m_tutorialHintShowTimer = 0;
            }
        }
        // Near a dimension-exclusive platform
        else if (!m_tutorialHintDone[9] && !m_shownDimPlatformHint && m_player && m_level) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            int ts = m_level->getTileSize();
            int px = static_cast<int>(pt.getCenter().x) / ts;
            int py = static_cast<int>(pt.getCenter().y) / ts;
            int dim = m_dimManager.getCurrentDimension();
            // Check nearby tiles for dim-exclusive platforms
            for (int dy = -2; dy <= 2 && !hint; dy++) {
                for (int dx = -3; dx <= 3 && !hint; dx++) {
                    if (m_level->isDimensionExclusive(px+dx, py+dy, dim)) {
                        hint = "Glowing platform - only exists in one dimension! Switch with [E]";
                        m_shownDimPlatformHint = true;
                        hintSlot = 9;
                        m_tutorialHintShowTimer = 0;
                    }
                }
            }
        }
        // Show wall slide hint if next to wall in air
        else if (!m_tutorialHintDone[10] && !m_shownWallSlideHint && m_player) {
            auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
            if ((phys.onWallLeft || phys.onWallRight) && !phys.onGround) {
                hint = "Wall Slide! Jump off walls to reach higher areas.";
                keyLabel = "SPACE";
                m_shownWallSlideHint = true;
                hintSlot = 10;
                m_tutorialHintShowTimer = 0;
            }
        }
        // Show abilities hint (level 1, after combat basics done)
        else if (!m_tutorialHintDone[11] && m_hasAttackedThisRun && !m_hasUsedAbilityThisRun
                 && m_tutorialTimer > 30.0f) {
            hint = "Use special abilities: Slam / Shield / Phase Strike";
            keyLabel = "1 2 3";
            if (m_hasUsedAbilityThisRun) { conditionMet = true; hintSlot = 11; }
        }
        // Relic choice hint
        else if (!m_tutorialHintDone[12] && !m_shownRelicHint && m_showRelicChoice) {
            hint = "Choose a Relic! Each gives a unique passive bonus for this run.";
            m_shownRelicHint = true;
            hintSlot = 12;
            m_tutorialHintShowTimer = 0;
        }
        // Combo hint when first combo reaches 3
        else if (!m_tutorialHintDone[13] && m_player &&
                 m_player->getEntity()->hasComponent<CombatComponent>()) {
            auto& cb = m_player->getEntity()->getComponent<CombatComponent>();
            if (cb.comboCount >= 3) {
                hint = "Combo x3! Chain hits without pause for bonus damage!";
                conditionMet = true;
                hintSlot = 13;
            }
        }
        // Exit hint when all rifts repaired
        else if (!m_tutorialHintDone[14] && m_levelComplete && !m_collapsing) {
            hint = "All Rifts repaired! Find the EXIT before the dimension collapses!";
            conditionMet = false;
            hintSlot = 14;
            m_tutorialHintShowTimer = 0;
        }
        // Weapon switch hint after a few levels
        else if (!m_tutorialHintDone[15] && m_currentDifficulty >= 2 && m_tutorialTimer > 10.0f
                 && m_tutorialHintDone[3]) {
            hint = "Switch weapons: [Q] Melee  [R] Ranged - try different styles!";
            hintSlot = 15;
            m_tutorialHintShowTimer = 0;
        }
    }

    // Auto-dismiss timed hints after showing
    if (hint && hintSlot >= 7 && !conditionMet) {
        if (m_tutorialHintShowTimer > 5.0f) {
            conditionMet = true;
        }
    }

    // Mark completed hints
    if (conditionMet && hintSlot >= 0 && hintSlot < 20) {
        m_tutorialHintDone[hintSlot] = true;
        m_tutorialHintShowTimer = 0;
    }

    if (!hint) return;

    // Fade in over 0.3s
    m_tutorialHintShowTimer += 1.0f / 60.0f;
    float alpha = std::min(1.0f, m_tutorialHintShowTimer / 0.3f);
    Uint8 a = static_cast<Uint8>(alpha * 200);

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(a * 0.4f));
    SDL_Rect bg = {240, 140, 800, 36};
    SDL_RenderFillRect(renderer, &bg);

    if (keyLabel) {
        SDL_Surface* hintSurf = TTF_RenderText_Blended(font, hint, {180, 220, 255, a});
        SDL_Surface* keySurf = TTF_RenderText_Blended(font, keyLabel, {255, 255, 255, 255});
        int totalW = (keySurf ? keySurf->w + 16 : 0) + (hintSurf ? hintSurf->w : 0);
        int startX = 640 - totalW / 2;

        if (keySurf) {
            renderKeyBox(renderer, font, keyLabel, startX, 148, a);
            startX += keySurf->w + 16;
            SDL_FreeSurface(keySurf);
        }
        if (hintSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, hintSurf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect dst = {startX, 148, hintSurf->w, hintSurf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(hintSurf);
        }
    } else {
        SDL_Color color = {180, 220, 255, a};
        SDL_Surface* surface = TTF_RenderText_Blended(font, hint, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, a);
                SDL_Rect dst = {640 - surface->w / 2, 148, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void PlayState::renderBossHealthBar(SDL_Renderer* renderer, TTF_Font* font) {
    // Find boss entity
    Entity* boss = nullptr;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "enemy_boss") boss = &e;
    });
    if (!boss || !boss->hasComponent<HealthComponent>()) return;

    auto& hp = boss->getComponent<HealthComponent>();
    int bossPhase = 1;
    if (boss->hasComponent<AIComponent>()) {
        bossPhase = boss->getComponent<AIComponent>().bossPhase;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Bar background
    int barW = 400;
    int barH = 12;
    int barX = 640 - barW / 2;
    int barY = 20;

    // Dark frame
    SDL_SetRenderDrawColor(renderer, 20, 10, 30, 220);
    SDL_Rect frame = {barX - 2, barY - 2, barW + 4, barH + 4};
    SDL_RenderFillRect(renderer, &frame);

    // Background
    SDL_SetRenderDrawColor(renderer, 50, 25, 50, 200);
    SDL_Rect bg = {barX, barY, barW, barH};
    SDL_RenderFillRect(renderer, &bg);

    // HP fill (color changes per phase and boss type)
    float pct = hp.getPercent();
    int fillW = static_cast<int>(barW * pct);
    int bt = 0;
    if (boss->hasComponent<AIComponent>()) bt = boss->getComponent<AIComponent>().bossType;
    Uint8 r, g, b;
    if (bt == 4) {
        // Void Sovereign: dark purple/magenta tones
        switch (bossPhase) {
            case 1: r = 120; g = 0; b = 180; break;
            case 2: r = 180; g = 0; b = 150; break;
            case 3: r = 255; g = 0; b = 120; break;
            default: r = 120; g = 0; b = 180; break;
        }
    } else if (bt == 3) {
        // Temporal Weaver: golden/amber tones
        switch (bossPhase) {
            case 1: r = 200; g = 170; b = 60; break;
            case 2: r = 230; g = 180; b = 40; break;
            case 3: r = 255; g = 200; b = 30; break;
            default: r = 200; g = 170; b = 60; break;
        }
    } else if (bt == 2) {
        // Dimensional Architect: blue/purple tones
        switch (bossPhase) {
            case 1: r = 80; g = 140; b = 255; break;
            case 2: r = 140; g = 100; b = 255; break;
            case 3: r = 200; g = 60; b = 255; break;
            default: r = 80; g = 140; b = 255; break;
        }
    } else if (bt == 1) {
        // Void Wyrm: green tones
        switch (bossPhase) {
            case 1: r = 40; g = 180; b = 120; break;
            case 2: r = 60; g = 220; b = 80; break;
            case 3: r = 120; g = 255; b = 40; break;
            default: r = 40; g = 180; b = 120; break;
        }
    } else {
        // Rift Guardian: purple/orange/red tones
        switch (bossPhase) {
            case 1: r = 200; g = 50; b = 180; break;
            case 2: r = 220; g = 120; b = 40; break;
            case 3: r = 255; g = 40; b = 40; break;
            default: r = 200; g = 50; b = 180; break;
        }
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, 240);
    SDL_Rect fill = {barX, barY, fillW, barH};
    SDL_RenderFillRect(renderer, &fill);

    // Phase markers at 66% and 33%
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
    SDL_RenderDrawLine(renderer, barX + barW * 2 / 3, barY, barX + barW * 2 / 3, barY + barH);
    SDL_RenderDrawLine(renderer, barX + barW / 3, barY, barX + barW / 3, barY + barH);

    // Boss name (dynamic based on boss type)
    if (font) {
        int bt = 0;
        if (boss->hasComponent<AIComponent>()) bt = boss->getComponent<AIComponent>().bossType;
        const char* bossName = (bt == 4) ? "VOID SOVEREIGN" : (bt == 3) ? "TEMPORAL WEAVER" : (bt == 2) ? "DIMENSIONAL ARCHITECT" : (bt == 1) ? "VOID WYRM" : "RIFT GUARDIAN";
        SDL_Color tc = (bt == 4) ? SDL_Color{180, 80, 255, 220} : (bt == 3) ? SDL_Color{220, 190, 100, 220} : (bt == 2) ? SDL_Color{160, 180, 255, 220} : (bt == 1) ? SDL_Color{180, 255, 200, 220} : SDL_Color{220, 180, 255, 220};
        SDL_Surface* ts = TTF_RenderText_Blended(font, bossName, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {640 - ts->w / 2, barY + barH + 4, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }
}

void PlayState::renderRandomEvents(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& events = m_level->getRandomEvents();

    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        SDL_FRect worldRect = {event.position.x - 16, event.position.y - 32, 32, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        // Skip if off screen
        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 2.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC body
        SDL_Color npcColor;
        switch (event.type) {
            case RandomEventType::Merchant:           npcColor = {80, 200, 80, 255}; break;
            case RandomEventType::Shrine:             npcColor = event.getShrineColor(); break;
            case RandomEventType::DimensionalAnomaly: npcColor = {200, 50, 200, 255}; break;
            case RandomEventType::RiftEcho:           npcColor = {150, 150, 255, 255}; break;
            case RandomEventType::SuitRepairStation:  npcColor = {100, 255, 150, 255}; break;
            case RandomEventType::GamblingRift:       npcColor = {255, 200, 50, 255}; break;
        }

        // Glow
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i);
        Uint8 ga = static_cast<Uint8>(40 * glow);
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, ga);
        SDL_Rect glowRect = {sr.x - 6, sr.y - 6, sr.w + 12, sr.h + 12};
        SDL_RenderFillRect(renderer, &glowRect);

        // Body
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 220);
        SDL_RenderFillRect(renderer, &sr);

        // Eye
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        SDL_Rect eye = {sr.x + sr.w / 2 - 2, sr.y + 8, 4, 4};
        SDL_RenderFillRect(renderer, &eye);

        // Icon on top based on type
        int iconY = sr.y - 6;
        int iconX = sr.x + sr.w / 2;
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 180);
        switch (event.type) {
            case RandomEventType::Merchant:
                // Coin
                SDL_RenderDrawLine(renderer, iconX - 4, iconY, iconX + 4, iconY);
                SDL_RenderDrawLine(renderer, iconX, iconY - 4, iconX, iconY + 4);
                break;
            case RandomEventType::Shrine:
                // Diamond
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX + 5, iconY);
                SDL_RenderDrawLine(renderer, iconX + 5, iconY, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX, iconY + 5, iconX - 5, iconY);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX, iconY - 5);
                break;
            case RandomEventType::DimensionalAnomaly: {
                // Exclamation
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 1);
                SDL_Rect dot = {iconX - 1, iconY + 3, 2, 2};
                SDL_RenderFillRect(renderer, &dot);
                break;
            }
            default:
                // Star
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX + 5, iconY);
                break;
        }

        // Interaction hint when near
        if (i == m_nearEventIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", event.getName());
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            int nameH = 0;
            if (hs) {
                nameH = hs->h;
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    int offy = (event.type == RandomEventType::Shrine) ? -36 : -24;
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y + offy, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }

            // Shrine: show effect preview below the name
            if (event.type == RandomEventType::Shrine) {
                SDL_Color ec = event.getShrineColor();
                ec.a = static_cast<Uint8>(180 * blink);
                SDL_Surface* es = TTF_RenderText_Blended(font, event.getShrineEffect(), ec);
                if (es) {
                    SDL_Texture* et = SDL_CreateTextureFromSurface(renderer, es);
                    if (et) {
                        SDL_Rect er = {sr.x + sr.w / 2 - es->w / 2, sr.y - 36 + nameH + 2, es->w, es->h};
                        SDL_RenderCopy(renderer, et, nullptr, &er);
                        SDL_DestroyTexture(et);
                    }
                    SDL_FreeSurface(es);
                }
            }
        }
    }
}

void PlayState::renderChallengeHUD(SDL_Renderer* renderer, TTF_Font* font) {
    if (g_activeChallenge == ChallengeID::None && g_activeMutators[0] == MutatorID::None) return;
    if (!font) return;

    int y = 90;
    // Challenge name
    if (g_activeChallenge != ChallengeID::None) {
        const auto& cd = ChallengeMode::getChallengeData(g_activeChallenge);
        SDL_Color cc = {255, 200, 60, 200};
        SDL_Surface* s = TTF_RenderText_Blended(font, cd.name, cc);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {15, y, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
        y += 20;

        // Speedrun timer
        if (g_activeChallenge == ChallengeID::Speedrun && m_challengeTimer > 0) {
            int mins = static_cast<int>(m_challengeTimer) / 60;
            int secs = static_cast<int>(m_challengeTimer) % 60;
            char timerText[32];
            std::snprintf(timerText, sizeof(timerText), "%d:%02d", mins, secs);
            SDL_Color tc = (m_challengeTimer < 60) ? SDL_Color{255, 60, 60, 255} : SDL_Color{200, 200, 200, 220};
            SDL_Surface* ts = TTF_RenderText_Blended(font, timerText, tc);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    SDL_Rect tr = {15, y, ts->w, ts->h};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
            y += 20;
        }

        // Endless score
        if (g_activeChallenge == ChallengeID::EndlessRift) {
            char scoreText[32];
            std::snprintf(scoreText, sizeof(scoreText), "Score: %d", m_endlessScore);
            SDL_Surface* ss = TTF_RenderText_Blended(font, scoreText, SDL_Color{200, 180, 255, 220});
            if (ss) {
                SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
                if (st) {
                    SDL_Rect sr = {15, y, ss->w, ss->h};
                    SDL_RenderCopy(renderer, st, nullptr, &sr);
                    SDL_DestroyTexture(st);
                }
                SDL_FreeSurface(ss);
            }
            y += 20;
        }
    }

    // Active mutators
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::None) continue;
        const auto& md = ChallengeMode::getMutatorData(g_activeMutators[i]);
        SDL_Surface* ms = TTF_RenderText_Blended(font, md.name, SDL_Color{180, 180, 220, 160});
        if (ms) {
            SDL_Texture* mt = SDL_CreateTextureFromSurface(renderer, ms);
            if (mt) {
                SDL_Rect mr = {15, y, ms->w, ms->h};
                SDL_RenderCopy(renderer, mt, nullptr, &mr);
                SDL_DestroyTexture(mt);
            }
            SDL_FreeSurface(ms);
        }
        y += 16;
    }
}

void PlayState::renderCombatChallenge(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Active challenge indicator (top center, below boss bar)
    if (m_combatChallenge.active && !m_combatChallenge.completed) {
        int cx = SCREEN_WIDTH / 2;
        int cy = m_isBossLevel ? 60 : 28;

        // Background panel
        int panelW = 240, panelH = 36;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 180);
        SDL_Rect bg = {cx - panelW / 2, cy - panelH / 2, panelW, panelH};
        SDL_RenderFillRect(renderer, &bg);

        // Gold border
        SDL_SetRenderDrawColor(renderer, 200, 170, 50, 200);
        SDL_RenderDrawRect(renderer, &bg);

        // Challenge name
        SDL_Color gold = {255, 215, 0, 255};
        SDL_Surface* ns = TTF_RenderText_Blended(font, m_combatChallenge.name, gold);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {cx - ns->w / 2, cy - panelH / 2 + 2, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Description
        SDL_Color desc = {180, 180, 200, 200};
        SDL_Surface* ds = TTF_RenderText_Blended(font, m_combatChallenge.desc, desc);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {cx - ds->w / 2, cy - panelH / 2 + 16, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }

        // Progress bar for multi-kill (timer)
        if (m_combatChallenge.type == CombatChallengeType::MultiKill &&
            m_combatChallenge.currentCount > 0 && m_combatChallenge.maxTimer > 0) {
            float pct = m_combatChallenge.timer / m_combatChallenge.maxTimer;
            int barW = panelW - 8;
            SDL_Rect barBg = {cx - barW / 2, cy + panelH / 2 - 5, barW, 3};
            SDL_SetRenderDrawColor(renderer, 40, 40, 50, 200);
            SDL_RenderFillRect(renderer, &barBg);
            SDL_Rect barFill = {cx - barW / 2, cy + panelH / 2 - 5,
                                static_cast<int>(barW * pct), 3};
            Uint8 r = static_cast<Uint8>(255 * (1.0f - pct));
            Uint8 g = static_cast<Uint8>(255 * pct);
            SDL_SetRenderDrawColor(renderer, r, g, 50, 230);
            SDL_RenderFillRect(renderer, &barFill);

            // Kill count indicator
            char countBuf[16];
            std::snprintf(countBuf, sizeof(countBuf), "%d/%d",
                         m_combatChallenge.currentCount, m_combatChallenge.targetCount);
            SDL_Surface* cs = TTF_RenderText_Blended(font, countBuf, gold);
            if (cs) {
                SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
                if (ct) {
                    SDL_Rect cr = {cx + panelW / 2 + 4, cy - cs->h / 2, cs->w, cs->h};
                    SDL_RenderCopy(renderer, ct, nullptr, &cr);
                    SDL_DestroyTexture(ct);
                }
                SDL_FreeSurface(cs);
            }
        }

        // Shard reward preview
        char rewardBuf[24];
        std::snprintf(rewardBuf, sizeof(rewardBuf), "+%d", m_combatChallenge.shardReward);
        SDL_Color shardCol = {100, 200, 255, 160};
        SDL_Surface* rs = TTF_RenderText_Blended(font, rewardBuf, shardCol);
        if (rs) {
            SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
            if (rt) {
                SDL_Rect rr = {cx + panelW / 2 - rs->w - 4, cy - panelH / 2 + 2, rs->w, rs->h};
                SDL_RenderCopy(renderer, rt, nullptr, &rr);
                SDL_DestroyTexture(rt);
            }
            SDL_FreeSurface(rs);
        }
    }

    // Completion popup (fades out)
    if (m_challengeCompleteTimer > 0 && m_combatChallenge.completed) {
        float alpha = std::min(1.0f, m_challengeCompleteTimer / 0.5f);
        Uint8 a = static_cast<Uint8>(alpha * 255);

        int cx = SCREEN_WIDTH / 2;
        int cy = 80;

        // Slide up as it fades
        float slideUp = (1.0f - alpha) * 20.0f;
        cy -= static_cast<int>(slideUp);

        char completeBuf[64];
        std::snprintf(completeBuf, sizeof(completeBuf), "CHALLENGE COMPLETE! +%d Shards",
                     m_combatChallenge.shardReward);
        SDL_Color compColor = {255, 215, 0, a};
        SDL_Surface* cs = TTF_RenderText_Blended(font, completeBuf, compColor);
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_Rect cr = {cx - cs->w / 2, cy, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }
}

void PlayState::renderNPCs(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& npcs = m_level->getNPCs();

    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        SDL_FRect worldRect = {npc.position.x - 12, npc.position.y - 32, 24, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 3.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC color by type
        SDL_Color col;
        switch (npc.type) {
            case NPCType::RiftScholar:  col = {100, 180, 255, 255}; break; // Blue
            case NPCType::DimRefugee:   col = {255, 180, 80, 255}; break;  // Orange
            case NPCType::LostEngineer: col = {180, 255, 120, 255}; break; // Green
            case NPCType::EchoOfSelf:   col = {220, 120, 255, 255}; break; // Purple
            case NPCType::Blacksmith:   col = {255, 160, 50, 255}; break;  // Forge orange
            default:                    col = {200, 200, 200, 255}; break;
        }

        // Glow aura
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i * 2.0f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(35 * glow));
        SDL_Rect glowR = {sr.x - 8, sr.y - 8, sr.w + 16, sr.h + 16};
        SDL_RenderFillRect(renderer, &glowR);

        // Body (hooded figure shape)
        SDL_SetRenderDrawColor(renderer, col.r / 2, col.g / 2, col.b / 2, 220);
        SDL_RenderFillRect(renderer, &sr); // Robe

        // Head (smaller rect on top)
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 240);
        SDL_Rect head = {sr.x + sr.w / 4, sr.y + 2, sr.w / 2, sr.h / 3};
        SDL_RenderFillRect(renderer, &head);

        // Eyes
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        int eyeY = sr.y + sr.h / 5;
        SDL_Rect leftEye = {sr.x + sr.w / 3 - 2, eyeY, 3, 3};
        SDL_Rect rightEye = {sr.x + sr.w * 2 / 3 - 1, eyeY, 3, 3};
        SDL_RenderFillRect(renderer, &leftEye);
        SDL_RenderFillRect(renderer, &rightEye);

        // Type-specific detail
        switch (npc.type) {
            case NPCType::RiftScholar: {
                SDL_SetRenderDrawColor(renderer, 100, 180, 255, 160);
                SDL_Rect book = {sr.x + sr.w / 2 - 4, sr.y - 12, 8, 6};
                SDL_RenderFillRect(renderer, &book);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 12, sr.x + sr.w / 2, sr.y - 6);
                break;
            }
            case NPCType::DimRefugee: {
                SDL_SetRenderDrawColor(renderer, 255, 200, 60, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 14, sr.x + sr.w / 2 + 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 + 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 2);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 2, sr.x + sr.w / 2 - 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 14);
                break;
            }
            case NPCType::LostEngineer: {
                SDL_SetRenderDrawColor(renderer, 180, 255, 120, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 4, sr.y - 12, sr.x + sr.w / 2 + 4, sr.y - 4);
                break;
            }
            case NPCType::EchoOfSelf: {
                SDL_SetRenderDrawColor(renderer, 220, 120, 255, static_cast<Uint8>(100 * glow));
                SDL_Rect mirrorR = {sr.x - 2, sr.y, sr.w + 4, sr.h};
                SDL_RenderDrawRect(renderer, &mirrorR);
                break;
            }
            case NPCType::Blacksmith: {
                // Anvil shape above head
                SDL_SetRenderDrawColor(renderer, 255, 160, 50, 180);
                SDL_Rect anvil = {sr.x + sr.w / 2 - 6, sr.y - 8, 12, 4};
                SDL_RenderFillRect(renderer, &anvil);
                SDL_Rect base = {sr.x + sr.w / 2 - 3, sr.y - 4, 6, 4};
                SDL_RenderFillRect(renderer, &base);
                // Spark particles
                float sparkPhase = std::sin(ticks * 0.008f + i) * 0.5f + 0.5f;
                if (sparkPhase > 0.7f) {
                    SDL_SetRenderDrawColor(renderer, 255, 220, 80, 200);
                    SDL_Rect spark = {sr.x + sr.w / 2 + static_cast<int>(sparkPhase * 8) - 4, sr.y - 14, 2, 2};
                    SDL_RenderFillRect(renderer, &spark);
                }
                break;
            }
            default: break;
        }

        // Interaction hint
        if (i == m_nearNPCIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", npc.name);
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y - 28, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }
}

void PlayState::renderNPCDialog(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_nearNPCIndex < 0 || !m_level) return;

    auto& npcs = m_level->getNPCs();
    if (m_nearNPCIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[m_nearNPCIndex];

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Dialog box (taller for returning NPCs with longer story text)
    int stage = getNPCStoryStage(npc.type);
    int boxW = 500, boxH = 260 + (stage > 0 ? 30 : 0);
    int boxX = 640 - boxW / 2, boxY = 360 - boxH / 2;

    SDL_SetRenderDrawColor(renderer, 20, 15, 35, 240);
    SDL_Rect box = {boxX, boxY, boxW, boxH};
    SDL_RenderFillRect(renderer, &box);

    // Border
    SDL_SetRenderDrawColor(renderer, 120, 80, 200, 200);
    SDL_RenderDrawRect(renderer, &box);

    // NPC name
    SDL_Color nameColor = {180, 140, 255, 255};
    SDL_Surface* ns = TTF_RenderText_Blended(font, npc.name, nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            SDL_Rect nr = {boxX + 20, boxY + 15, ns->w, ns->h};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    // Story stage indicator (show returning NPC badge)
    if (stage > 0) {
        const char* stageLabel = (stage >= 2) ? "[Old Friend]" : "[Returning]";
        SDL_Color stageColor = (stage >= 2) ? SDL_Color{255, 215, 80, 200} : SDL_Color{140, 200, 140, 180};
        SDL_Surface* stS = TTF_RenderText_Blended(font, stageLabel, stageColor);
        if (stS) {
            SDL_Texture* stT = SDL_CreateTextureFromSurface(renderer, stS);
            if (stT) {
                SDL_Rect stR = {boxX + boxW - stS->w - 20, boxY + 15, stS->w, stS->h};
                SDL_RenderCopy(renderer, stT, nullptr, &stR);
                SDL_DestroyTexture(stT);
            }
            SDL_FreeSurface(stS);
        }
    }

    // Greeting text (stage-based)
    const char* greeting = NPCSystem::getGreeting(npc.type, stage);
    SDL_Color textColor = {200, 200, 220, 255};
    SDL_Surface* ds = TTF_RenderText_Blended(font, greeting, textColor);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            SDL_Rect dr = {boxX + 20, boxY + 45, ds->w, ds->h};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }

    // Story line (stage-based, may contain newlines — render line by line)
    const char* storyText = NPCSystem::getStoryLine(npc.type, stage);
    int lineY = boxY + 70;
    {
        // Split on newline and render each line
        std::string story(storyText);
        size_t pos = 0;
        while (pos < story.size()) {
            size_t nl = story.find('\n', pos);
            std::string line = (nl != std::string::npos) ? story.substr(pos, nl - pos) : story.substr(pos);
            pos = (nl != std::string::npos) ? nl + 1 : story.size();
            if (line.empty()) { lineY += 18; continue; }
            SDL_Surface* ls = TTF_RenderText_Blended(font, line.c_str(), SDL_Color{160, 160, 180, 200});
            if (ls) {
                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                if (lt) {
                    SDL_Rect lr = {boxX + 20, lineY, ls->w, ls->h};
                    SDL_RenderCopy(renderer, lt, nullptr, &lr);
                    SDL_DestroyTexture(lt);
                }
                SDL_FreeSurface(ls);
            }
            lineY += 18;
        }
    }

    // Dialog options (stage-based) — positioned below story text
    auto options = NPCSystem::getDialogOptions(npc.type, stage);
    int optY = std::max(lineY + 8, boxY + 110);
    for (int i = 0; i < static_cast<int>(options.size()); i++) {
        bool selected = (i == m_npcDialogChoice);
        SDL_Color oc = selected ? SDL_Color{255, 220, 100, 255} : SDL_Color{180, 180, 200, 200};

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 80, 60, 120, 100);
            SDL_Rect selR = {boxX + 15, optY - 2, boxW - 30, 22};
            SDL_RenderFillRect(renderer, &selR);

            // Arrow
            SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
            SDL_RenderDrawLine(renderer, boxX + 20, optY + 8, boxX + 28, optY + 4);
            SDL_RenderDrawLine(renderer, boxX + 28, optY + 4, boxX + 20, optY);
        }

        SDL_Surface* os = TTF_RenderText_Blended(font, options[i], oc);
        if (os) {
            SDL_Texture* ot = SDL_CreateTextureFromSurface(renderer, os);
            if (ot) {
                SDL_Rect or_ = {boxX + 35, optY, os->w, os->h};
                SDL_RenderCopy(renderer, ot, nullptr, &or_);
                SDL_DestroyTexture(ot);
            }
            SDL_FreeSurface(os);
        }
        optY += 26;
    }

    // Controls hint
    SDL_Surface* hs = TTF_RenderText_Blended(font, "[W/S] Navigate  [Enter] Select  [Esc] Close",
                                              SDL_Color{120, 120, 140, 150});
    if (hs) {
        SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
        if (ht) {
            SDL_Rect hr = {boxX + boxW / 2 - hs->w / 2, boxY + boxH - 25, hs->w, hs->h};
            SDL_RenderCopy(renderer, ht, nullptr, &hr);
            SDL_DestroyTexture(ht);
        }
        SDL_FreeSurface(hs);
    }
}

void PlayState::renderEventChain(SDL_Renderer* renderer, TTF_Font* font) {
    if (m_eventChain.stage <= 0 && m_chainRewardTimer <= 0) return;
    if (!font) return;

    SDL_Color cc = m_eventChain.getColor();

    // Active chain: show progress tracker at top-left
    if (m_eventChain.stage > 0 && !m_eventChain.completed) {
        int panelX = 10;
        int panelY = 90; // Below existing HUD elements
        int panelW = 220;
        int panelH = 50;

        // Background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect bg = {panelX, panelY, panelW, panelH};
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 180);
        SDL_RenderFillRect(renderer, &bg);

        // Border in chain color
        SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 200);
        SDL_RenderDrawRect(renderer, &bg);

        // Chain icon: linked circles
        for (int i = 0; i < m_eventChain.maxStages; i++) {
            int cx = panelX + 15 + i * 18;
            int cy = panelY + 14;
            int r = 5;
            if (i < m_eventChain.stage) {
                // Filled circle for completed stages
                SDL_Rect dot = {cx - r, cy - r, r * 2, r * 2};
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 255);
                SDL_RenderFillRect(renderer, &dot);
            } else {
                // Hollow for pending
                SDL_Rect dot = {cx - r, cy - r, r * 2, r * 2};
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 100);
                SDL_RenderDrawRect(renderer, &dot);
            }
            // Link line
            if (i < m_eventChain.maxStages - 1) {
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b,
                    static_cast<Uint8>(i < m_eventChain.stage - 1 ? 200 : 60));
                SDL_RenderDrawLine(renderer, cx + r + 1, cy, cx + 13 - r, cy);
            }
        }

        // Chain name
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* nameSurf = TTF_RenderText_Blended(font, m_eventChain.getName(), white);
        if (nameSurf) {
            SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
            SDL_Rect nameR = {panelX + 70, panelY + 4, nameSurf->w, nameSurf->h};
            SDL_RenderCopy(renderer, nameTex, nullptr, &nameR);
            SDL_DestroyTexture(nameTex);
            SDL_FreeSurface(nameSurf);
        }

        // Stage description
        const char* desc = m_eventChain.getStageDesc();
        if (desc && desc[0]) {
            SDL_Color dimWhite = {200, 200, 200, 200};
            SDL_Surface* descSurf = TTF_RenderText_Blended(font, desc, dimWhite);
            if (descSurf) {
                SDL_Texture* descTex = SDL_CreateTextureFromSurface(renderer, descSurf);
                int maxW = panelW - 10;
                int dw = std::min(descSurf->w, maxW);
                int dh = descSurf->h * dw / std::max(descSurf->w, 1);
                SDL_Rect descR = {panelX + 5, panelY + 26, dw, dh};
                SDL_RenderCopy(renderer, descTex, nullptr, &descR);
                SDL_DestroyTexture(descTex);
                SDL_FreeSurface(descSurf);
            }
        }
    }

    // Stage advance notification (slide-in from top)
    if (m_chainNotifyTimer > 0 && !m_eventChain.completed) {
        float slideT = std::min(m_chainNotifyTimer / 3.5f, 1.0f);
        float slideIn = (slideT > 0.85f) ? (1.0f - slideT) / 0.15f : (slideT < 0.15f ? slideT / 0.15f : 1.0f);
        Uint8 alpha = static_cast<Uint8>(255 * slideIn);

        char stageText[64];
        snprintf(stageText, sizeof(stageText), "CHAIN: Stage %d/%d", m_eventChain.stage, m_eventChain.maxStages);

        SDL_Color textCol = {cc.r, cc.g, cc.b, alpha};
        SDL_Surface* surf = TTF_RenderText_Blended(font, stageText, textCol);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_SetTextureAlphaMod(tex, alpha);
            int tx = (SCREEN_WIDTH - surf->w) / 2;
            int ty = 50 + static_cast<int>((1.0f - slideIn) * -20);
            SDL_Rect r = {tx, ty, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &r);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }

    // Completion reward popup (event chain)
    if (m_chainRewardTimer > 0) {
        float fadeT = std::min(m_chainRewardTimer / 4.0f, 1.0f);
        float alpha01 = (fadeT > 0.8f) ? (1.0f - fadeT) / 0.2f : (fadeT < 0.2f ? fadeT / 0.2f : 1.0f);
        Uint8 alpha = static_cast<Uint8>(255 * alpha01);

        // "CHAIN COMPLETE" title
        SDL_Color goldCol = {255, 215, 60, alpha};
        SDL_Surface* titleSurf = TTF_RenderText_Blended(font, "CHAIN COMPLETE!", goldCol);
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_SetTextureAlphaMod(titleTex, alpha);
            int tx = (SCREEN_WIDTH - titleSurf->w) / 2;
            int ty = 100;
            SDL_Rect r = {tx, ty, titleSurf->w, titleSurf->h};
            SDL_RenderCopy(renderer, titleTex, nullptr, &r);
            SDL_DestroyTexture(titleTex);
            SDL_FreeSurface(titleSurf);
        }

        // Chain name + reward
        char rewardText[64];
        snprintf(rewardText, sizeof(rewardText), "%s — +%d Shards", m_eventChain.getName(), m_chainRewardShards);
        SDL_Color rewCol = {cc.r, cc.g, cc.b, alpha};
        SDL_Surface* rewSurf = TTF_RenderText_Blended(font, rewardText, rewCol);
        if (rewSurf) {
            SDL_Texture* rewTex = SDL_CreateTextureFromSurface(renderer, rewSurf);
            SDL_SetTextureAlphaMod(rewTex, alpha);
            int rx = (SCREEN_WIDTH - rewSurf->w) / 2;
            int ry = 125;
            SDL_Rect r = {rx, ry, rewSurf->w, rewSurf->h};
            SDL_RenderCopy(renderer, rewTex, nullptr, &r);
            SDL_DestroyTexture(rewTex);
            SDL_FreeSurface(rewSurf);
        }
    }
}

void PlayState::renderKillFeed(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    int x = SCREEN_WIDTH - 20;
    int y = SCREEN_HEIGHT - 140;

    // Collect active entries in display order (newest at bottom)
    struct DisplayEntry { const char* text; SDL_Color color; float timer; };
    DisplayEntry display[MAX_KILL_FEED];
    int count = 0;

    for (int i = MAX_KILL_FEED - 1; i >= 0; i--) {
        int idx = (m_killFeedHead - 1 - i + MAX_KILL_FEED * 2) % MAX_KILL_FEED;
        if (m_killFeed[idx].timer > 0 && m_killFeed[idx].text[0] != '\0') {
            display[count++] = {m_killFeed[idx].text, m_killFeed[idx].color, m_killFeed[idx].timer};
        }
    }

    for (int i = 0; i < count; i++) {
        float alpha = std::min(display[i].timer / 0.5f, 1.0f); // fade out in last 0.5s
        SDL_Color c = display[i].color;
        c.a = static_cast<Uint8>(c.a * alpha);

        SDL_Surface* surf = TTF_RenderText_Blended(font, display[i].text, c);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, c.a);
                SDL_Rect r = {x - surf->w, y, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
        y += 18;
    }
}

void PlayState::renderUnlockNotifications(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;
    // Stack popups in top-center
    int y = 140;
    for (int i = 0; i < MAX_UNLOCK_NOTIFS; i++) {
        auto& notif = m_unlockNotifs[i];
        if (notif.timer <= 0) continue;

        // Slide in from top during first 0.2s, fade out during last 0.5s
        float slideIn = std::min(1.0f, (notif.maxTimer - notif.timer) / 0.2f);
        float fadeOut = std::min(1.0f, notif.timer / 0.5f);
        Uint8 alpha = static_cast<Uint8>(255 * slideIn * fadeOut);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Golden pulsing background panel
        float pulse = 0.7f + 0.3f * std::sin(notif.timer * 8.0f);
        SDL_SetRenderDrawColor(renderer, 40, 30, 5,
                               static_cast<Uint8>(180 * slideIn * fadeOut));
        SDL_Rect bg = {640 - 220, y - 4, 440, 30};
        SDL_RenderFillRect(renderer, &bg);
        // Gold border
        SDL_SetRenderDrawColor(renderer, 255, 200, 50,
                               static_cast<Uint8>(220 * pulse * slideIn * fadeOut));
        SDL_RenderDrawRect(renderer, &bg);

        // Text
        SDL_Color textColor = {255, 215, 0, alpha};
        SDL_Surface* surf = TTF_RenderText_Blended(font, notif.text, textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, alpha);
                int tw = surf->w, th = surf->h;
                SDL_Rect r = {640 - tw / 2, y, tw, th};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
        y += 36;
    }
}

