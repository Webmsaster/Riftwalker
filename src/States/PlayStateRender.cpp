// PlayStateRender.cpp -- Orchestrator render() + renderBackground + file-local helpers
#include <tracy/Tracy.hpp>
// Split from original monolithic PlayStateRender.cpp into:
//   PlayStateRenderHUD.cpp      -- HUD panels, status displays, combat feedback
//   PlayStateRenderOverlays.cpp -- Modal overlays, notifications, tutorial
//   PlayStateRenderWorld.cpp    -- World entities, NPCs, events
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

    // Layer 3: Far mountain/structure silhouettes (10% parallax — large, dark)
    {
        float farParallax = 0.1f;
        int farOffX = static_cast<int>(camPos.x * farParallax);
        int farOffY = static_cast<int>(camPos.y * farParallax * 0.3f);
        Uint8 farR = static_cast<Uint8>(std::min(255, bgColor.r + 10));
        Uint8 farG = static_cast<Uint8>(std::min(255, bgColor.g + 10));
        Uint8 farB = static_cast<Uint8>(std::min(255, bgColor.b + 10));
        SDL_SetRenderDrawColor(renderer, farR, farG, farB, 45);

        for (int i = 0; i < 6; i++) {
            int baseX = ((i * 11731 + 48271) % 2400) - 400;
            int sx = ((baseX - farOffX) % 2200 + 2200) % 2200 - 400;
            int peakH = 120 + (i * 37) % 100; // 120-220 px tall
            int baseW = 160 + (i * 53) % 140;  // 160-300 px wide
            int groundY = SCREEN_HEIGHT + farOffY / 4;

            // Main mass (wide rectangle at bottom)
            SDL_Rect mass = {sx, groundY - peakH, baseW, peakH};
            SDL_RenderFillRect(renderer, &mass);

            // Tapered top (narrower rect on top for a ridge shape)
            int topW = baseW / 3 + (i * 19) % (baseW / 3);
            int topH = peakH / 3;
            int topX = sx + (baseW - topW) / 2 + ((i * 7) % 20 - 10);
            SDL_Rect ridge = {topX, groundY - peakH - topH, topW, topH};
            SDL_RenderFillRect(renderer, &ridge);

            // Small secondary peak next to main
            int secH = peakH / 2 + (i * 11) % 30;
            int secW = baseW / 4;
            int secX = sx + baseW - secW / 2;
            SDL_Rect secondary = {secX, groundY - secH, secW, secH};
            SDL_RenderFillRect(renderer, &secondary);
        }
    }

    // Layer 4: Mid-ground floating structures (30% parallax — medium, semi-transparent)
    {
        float midParallax = 0.3f;
        int midOffX = static_cast<int>(camPos.x * midParallax);
        int midOffY = static_cast<int>(camPos.y * midParallax * 0.5f);
        auto& accent = (m_dimManager.getCurrentDimension() == 1) ?
            m_themeA.colors.accent : m_themeB.colors.accent;
        Uint8 midR = static_cast<Uint8>(std::min(255, (bgColor.r * 2 + accent.r) / 3 + 8));
        Uint8 midG = static_cast<Uint8>(std::min(255, (bgColor.g * 2 + accent.g) / 3 + 8));
        Uint8 midB = static_cast<Uint8>(std::min(255, (bgColor.b * 2 + accent.b) / 3 + 8));

        for (int i = 0; i < 10; i++) {
            int baseX = ((i * 6599 + 31247) % 2000) - 300;
            int baseY = 180 + ((i * 4211) % 300);
            int sx = ((baseX - midOffX) % 1800 + 1800) % 1800 - 300;
            int sy = baseY - (midOffY % 400);
            // Gentle vertical bob
            sy += static_cast<int>(std::sin(ticks * 0.0006f + i * 1.9f) * 5.0f);

            Uint8 ma = static_cast<Uint8>(25 + 8 * std::sin(ticks * 0.0008f + i * 0.7f));
            SDL_SetRenderDrawColor(renderer, midR, midG, midB, ma);

            if (i % 3 == 0) {
                // Floating platform shape (wide, thin)
                int pw = 60 + (i * 23) % 50;
                int ph = 8 + (i * 7) % 6;
                SDL_Rect plat = {sx, sy, pw, ph};
                SDL_RenderFillRect(renderer, &plat);
                // Underside shadow
                SDL_SetRenderDrawColor(renderer, midR, midG, midB, static_cast<Uint8>(ma / 2));
                SDL_Rect shadow = {sx + 4, sy + ph, pw - 8, 3};
                SDL_RenderFillRect(renderer, &shadow);
            } else if (i % 3 == 1) {
                // Tall pillar/column
                int pw = 10 + (i * 3) % 8;
                int ph = 40 + (i * 17) % 50;
                SDL_Rect pillar = {sx, sy - ph, pw, ph};
                SDL_RenderFillRect(renderer, &pillar);
                // Capital on top
                SDL_Rect cap = {sx - 3, sy - ph - 4, pw + 6, 4};
                SDL_RenderFillRect(renderer, &cap);
            } else {
                // Debris cluster (3 small rects)
                for (int d = 0; d < 3; d++) {
                    int dx = sx + d * 12 + ((i + d) * 5) % 8;
                    int dy = sy + ((i + d) * 7) % 10;
                    int ds = 5 + (i + d) % 5;
                    SDL_Rect debris = {dx, dy, ds, ds};
                    SDL_RenderFillRect(renderer, &debris);
                }
            }
        }
    }

    // Layer 5: Near foreground details (60% parallax — close, prominent)
    {
        float nearParallax = 0.6f;
        int nearOffX = static_cast<int>(camPos.x * nearParallax);
        int nearOffY = static_cast<int>(camPos.y * nearParallax * 0.4f);
        Uint8 nearR = static_cast<Uint8>(std::min(255, bgColor.r + 20));
        Uint8 nearG = static_cast<Uint8>(std::min(255, bgColor.g + 20));
        Uint8 nearB = static_cast<Uint8>(std::min(255, bgColor.b + 20));

        for (int i = 0; i < 8; i++) {
            int baseX = ((i * 9281 + 17389) % 2600) - 400;
            int sx = ((baseX - nearOffX) % 2400 + 2400) % 2400 - 400;

            Uint8 na = static_cast<Uint8>(18 + 6 * std::sin(ticks * 0.001f + i * 1.3f));
            SDL_SetRenderDrawColor(renderer, nearR, nearG, nearB, na);

            if (i % 4 == 0) {
                // Tall foreground column (bottom-anchored)
                int colH = 90 + (i * 29) % 70;
                int colW = 14 + (i * 3) % 8;
                int cy = SCREEN_HEIGHT + nearOffY / 3;
                SDL_Rect col = {sx, cy - colH, colW, colH};
                SDL_RenderFillRect(renderer, &col);
                // Broken top (angled line)
                SDL_RenderDrawLine(renderer, sx, cy - colH, sx + colW + 4, cy - colH + 8);
            } else if (i % 4 == 1) {
                // Hanging stalactite / chain (top-anchored)
                int chainLen = 50 + (i * 13) % 40;
                int topY = -nearOffY / 4;
                int cx = sx + 4;
                SDL_Rect chain = {cx, topY, 4, chainLen};
                SDL_RenderFillRect(renderer, &chain);
                // Weight at bottom
                SDL_Rect weight = {cx - 3, topY + chainLen, 10, 8};
                SDL_RenderFillRect(renderer, &weight);
            } else if (i % 4 == 2) {
                // Railing / fence segment
                int railW = 70 + (i * 19) % 50;
                int railY = SCREEN_HEIGHT - 40 + (nearOffY / 5);
                SDL_Rect topRail = {sx, railY, railW, 3};
                SDL_RenderFillRect(renderer, &topRail);
                // Vertical posts
                for (int p = 0; p < railW; p += 16) {
                    SDL_Rect post = {sx + p, railY, 3, 20};
                    SDL_RenderFillRect(renderer, &post);
                }
            } else {
                // Scattered rubble at ground level
                int gy = SCREEN_HEIGHT - 10 + nearOffY / 5;
                for (int r = 0; r < 4; r++) {
                    int rx = sx + r * 18 + ((i + r) * 11) % 10;
                    int rw = 6 + (i + r) % 6;
                    int rh = 4 + (i + r) % 4;
                    SDL_Rect rub = {rx, gy - rh, rw, rh};
                    SDL_RenderFillRect(renderer, &rub);
                }
            }
        }
    }

    // Layer 6: Floating dimension particles (close parallax)
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
    ZoneScopedN("PlayStateRender");
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
    renderCollapseWarning(renderer);

    // Rift progress, exit hints, dimension hints, near-rift indicator, off-screen indicators
    renderRiftProgress(renderer);

    // Puzzle overlay
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    // Level complete: rift warp transition effect
    renderLevelCompleteTransition(renderer);

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

    // Weapon switch display (floating name below player)
    renderWeaponSwitchDisplay(renderer, game->getFont());

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

    // Screen effects (vignette, low-HP pulse, kill flash, boss intro title card, ripple, glitch)
    m_screenEffects.render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, game->getFont());

    // Damage flash overlay
    m_hud.renderFlash(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Directional damage indicators (red edge flash toward damage source)
    renderDamageIndicators(renderer);

    // Camera screen flash (boss death white flash, etc.)
    {
        float flashAlpha = m_camera.getFlashAlpha();
        if (flashAlpha > 0) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_Color fc = m_camera.getFlashColor();
            Uint8 a = static_cast<Uint8>(std::min(255.0f, flashAlpha * 200.0f));
            SDL_SetRenderDrawColor(renderer, fc.r, fc.g, fc.b, a);
            SDL_Rect fullScreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(renderer, &fullScreen);
        }
    }

    // Dynamic level event overlay (announcement + timer bar)
    renderDynamicEventOverlay(renderer, game->getFont());

    // Zone transition banner (non-blocking, on zone change)
    renderZoneTransition(renderer, game->getFont());

    // Wave/area clear celebration text
    if (m_waveClearTimer > 0 && game->getFont()) {
        float t = m_waveClearTimer / 2.0f; // 1.0 -> 0.0
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

    // Kill streak notification (centered, above mid-screen)
    renderKillStreak(renderer, game->getFont());

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

    // NPC quest progress (bottom right, above kill feed)
    renderQuestHUD(renderer, game->getFont());

    // Minimap (top right corner, M key to toggle)
    m_hud.renderMinimap(renderer, m_level.get(), m_player.get(), &m_dimManager,
                        SCREEN_WIDTH, SCREEN_HEIGHT, &m_entities, &m_repairedRiftIndices);

    // Achievement notification popup
    renderAchievementNotification(renderer, game->getFont());

    // Lore discovery notification popup
    renderLoreNotification(renderer, game->getFont());

    // Unlock notification popups (golden, top-center)
    renderUnlockNotifications(renderer, game->getFont());

    // Death sequence overlay: red vignette + desaturation effect
    renderDeathSequence(renderer);

    // Run intro overlay (atmospheric story text)
    if (m_runIntroActive) {
        renderRunIntro(renderer, game->getFont());
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
