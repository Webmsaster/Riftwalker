#include "HUD.h"
#include "Core/Game.h"
#include "Game/Player.h"
#include "Game/SuitEntropy.h"
#include "Game/DimensionManager.h"
#include "Game/Level.h"
#include "ECS/EntityManager.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/TransformComponent.h"
#include "Components/AbilityComponent.h"
#include "Game/WeaponSystem.h"
#include "Game/ClassSystem.h"
#include "Systems/CombatSystem.h"
#include "Components/RelicComponent.h"
#include "Game/RelicSynergy.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

// Safe float-to-Uint8 conversion clamped to [0,255]
static inline Uint8 clampU8(float v) {
    return static_cast<Uint8>(std::clamp(v, 0.0f, 255.0f));
}

void HUD::updateFlash(float dt) {
    if (m_damageFlash > 0) m_damageFlash -= dt;
    for (int i = 0; i < 4; i++) {
        if (m_abilityReadyFlash[i] > 0) m_abilityReadyFlash[i] -= dt;
    }
    // Weapon mastery tier-up flash decay
    for (int i = 0; i < 2; i++) {
        if (m_masteryFlash[i] > 0) m_masteryFlash[i] -= dt;
    }
}

void HUD::renderFlash(SDL_Renderer* renderer, int screenW, int screenH) {
    if (m_damageFlash <= 0) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    Uint8 alpha = clampU8(m_damageFlash / 0.3f * 80);
    // Red vignette flash
    for (int i = 0; i < 3; i++) {
        SDL_SetRenderDrawColor(renderer, 255, 20, 0, static_cast<Uint8>(alpha / (i + 1)));
        SDL_Rect border = {i * 4, i * 4, screenW - i * 8, screenH - i * 8};
        SDL_RenderDrawRect(renderer, &border);
    }
    // Top/bottom red bars
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, static_cast<Uint8>(alpha * 0.5f));
    SDL_Rect top = {0, 0, screenW, 16};
    SDL_Rect bot = {0, screenH - 16, screenW, 16};
    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bot);
}

void HUD::renderMinimap(SDL_Renderer* renderer, const Level* level,
                         const Player* player, const DimensionManager* dimMgr,
                         int screenW, int screenH,
                         EntityManager* entities,
                         const std::set<int>* repairedRifts) {
    if (!level) return;
    if (!m_showMinimap) return;

    // Minimap dimensions: scaled for 2K, positioned top-right corner
    const int mapW = 320;
    const int mapH = 220;
    const int mapX = screenW - mapW - 20;
    const int mapY = 20;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Outer border frame
    SDL_SetRenderDrawColor(renderer, 60, 60, 90, 200);
    SDL_Rect outerBorder = {mapX - 3, mapY - 3, mapW + 6, mapH + 6};
    SDL_RenderFillRect(renderer, &outerBorder);

    // Dark background
    SDL_SetRenderDrawColor(renderer, 8, 8, 18, 220);
    SDL_Rect bg = {mapX, mapY, mapW, mapH};
    SDL_RenderFillRect(renderer, &bg);

    // Clip rendering to minimap bounds via scissor rect approach: track manually
    int levelW = level->getWidth();
    int levelH = level->getHeight();
    if (levelW <= 0 || levelH <= 0) return;

    // Scale level to fit minimap (maintain aspect ratio)
    float scaleX = static_cast<float>(mapW) / levelW;
    float scaleY = static_cast<float>(mapH) / levelH;
    float scale = std::min(scaleX, scaleY);

    // Center the level in the minimap
    int offsetX = mapX + (mapW - static_cast<int>(levelW * scale)) / 2;
    int offsetY = mapY + (mapH - static_cast<int>(levelH * scale)) / 2;

    int dim = dimMgr ? dimMgr->getCurrentDimension() : 1;
    int tileSize = level->getTileSize();

    // Helper: clamp a minimap pixel to be within the map area
    auto clampInMap = [&](int& px, int& py, int pw, int ph) -> bool {
        return px >= mapX && py >= mapY && px + pw <= mapX + mapW && py + ph <= mapY + mapH;
    };
    (void)clampInMap; // suppress unused warning

    // --- Draw tile terrain (room-based look via solid block detection) ---
    // Sample with step size to derive room shapes efficiently.
    // Use a step that gives ~2px per minimap cell for good coverage.
    int step = std::max(1, static_cast<int>(1.0f / scale));

    for (int ty = 0; ty < levelH; ty += step) {
        for (int tx = 0; tx < levelW; tx += step) {
            const auto& tileA = level->getTile(tx, ty, dim);

            int px = offsetX + static_cast<int>(tx * scale);
            int py = offsetY + static_cast<int>(ty * scale);
            int pw = std::max(1, static_cast<int>(step * scale));
            int ph = std::max(1, static_cast<int>(step * scale));

            // Stay within minimap bounds
            if (px < mapX || py < mapY || px + pw > mapX + mapW || py + ph > mapY + mapH) continue;

            SDL_Rect r = {px, py, pw, ph};
            if (tileA.type == TileType::Solid) {
                // Solid terrain: use a muted version of the tile color
                SDL_SetRenderDrawColor(renderer,
                    static_cast<Uint8>(tileA.color.r / 3 + 20),
                    static_cast<Uint8>(tileA.color.g / 3 + 20),
                    static_cast<Uint8>(tileA.color.b / 3 + 30),
                    200);
                SDL_RenderFillRect(renderer, &r);
            } else if (tileA.type == TileType::OneWay) {
                // One-way platforms: subtle brownish line
                SDL_SetRenderDrawColor(renderer, 80, 70, 40, 140);
                SDL_RenderFillRect(renderer, &r);
            } else if (tileA.type == TileType::Spike) {
                // Spike hazard: dark red pixel
                SDL_SetRenderDrawColor(renderer, 160, 40, 40, 160);
                SDL_RenderFillRect(renderer, &r);
            } else if (tileA.type == TileType::Fire || tileA.type == TileType::LaserEmitter) {
                // Fire/laser hazard: orange/red
                SDL_SetRenderDrawColor(renderer, 180, 80, 20, 130);
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }

    // --- Dimension tint overlay ---
    // Blue tint for dimension 1, red tint for dimension 2
    if (dim == 1) {
        SDL_SetRenderDrawColor(renderer, 30, 60, 120, 25);
    } else {
        SDL_SetRenderDrawColor(renderer, 120, 30, 30, 25);
    }
    SDL_RenderFillRect(renderer, &bg);

    // --- Rift markers (colored diamonds: green=repaired, purple/red=unrepaired by dimension) ---
    auto rifts = level->getRiftPositions();
    for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
        int rx = offsetX + static_cast<int>((rifts[i].x / tileSize) * scale);
        int ry = offsetY + static_cast<int>((rifts[i].y / tileSize) * scale);

        // Skip if outside minimap area
        if (rx < mapX || rx >= mapX + mapW || ry < mapY || ry >= mapY + mapH) continue;

        bool repaired = repairedRifts && repairedRifts->count(i) > 0;
        float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.006f);

        Uint8 alpha = static_cast<Uint8>(160 + 90 * pulse);
        if (repaired) {
            // Green diamond for repaired rift
            SDL_SetRenderDrawColor(renderer, 80, 255, 120, alpha);
        } else {
            // Unrepaired: dimension 1 = blue-purple, dimension 2 = red-purple
            if (dim == 1) {
                SDL_SetRenderDrawColor(renderer, 140, 80, 255, alpha);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 80, 120, alpha);
            }
        }

        // Draw diamond shape (4 pixels in cross pattern)
        SDL_Rect d1 = {rx - 1, ry - 2, 2, 1};  // top
        SDL_Rect d2 = {rx - 2, ry - 1, 4, 2};  // middle
        SDL_Rect d3 = {rx - 1, ry + 1, 2, 1};  // bottom
        SDL_RenderFillRect(renderer, &d1);
        SDL_RenderFillRect(renderer, &d2);
        SDL_RenderFillRect(renderer, &d3);
    }

    // --- Exit marker (diamond shape, pulsing urgently when collapse active) ---
    {
        Vec2 exitPt = level->getExitPoint();
        int ex = offsetX + static_cast<int>((exitPt.x / tileSize) * scale);
        int ey = offsetY + static_cast<int>((exitPt.y / tileSize) * scale);

        if (ex >= mapX && ex < mapX + mapW && ey >= mapY && ey < mapY + mapH) {
            if (level->isExitActive()) {
                // Collapse active: urgent pulsing green diamond
                // Fast pulse (0.015 rad/ms) to draw attention
                float ePulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.015f);
                Uint8 eAlpha = clampU8(180 + 75 * ePulse);

                // Outer glow ring (fades in/out with pulse)
                Uint8 glowAlpha = clampU8(60 + 80 * ePulse);
                SDL_SetRenderDrawColor(renderer, 50, 255, 80, glowAlpha);
                SDL_Rect g1 = {ex - 1, ey - 5, 2, 1};  // top
                SDL_Rect g2 = {ex - 5, ey - 1, 1, 2};  // left
                SDL_Rect g3 = {ex + 4, ey - 1, 1, 2};  // right
                SDL_Rect g4 = {ex - 1, ey + 4, 2, 1};  // bottom
                SDL_RenderFillRect(renderer, &g1);
                SDL_RenderFillRect(renderer, &g2);
                SDL_RenderFillRect(renderer, &g3);
                SDL_RenderFillRect(renderer, &g4);

                // Diamond body
                SDL_SetRenderDrawColor(renderer, 50, 255, 80, eAlpha);
                SDL_Rect d1 = {ex - 1, ey - 4, 2, 1};  // top tip
                SDL_Rect d2 = {ex - 2, ey - 3, 4, 1};
                SDL_Rect d3 = {ex - 3, ey - 2, 6, 1};
                SDL_Rect d4 = {ex - 4, ey - 1, 8, 2};  // widest
                SDL_Rect d5 = {ex - 3, ey + 1, 6, 1};
                SDL_Rect d6 = {ex - 2, ey + 2, 4, 1};
                SDL_Rect d7 = {ex - 1, ey + 3, 2, 1};  // bottom tip
                SDL_RenderFillRect(renderer, &d1);
                SDL_RenderFillRect(renderer, &d2);
                SDL_RenderFillRect(renderer, &d3);
                SDL_RenderFillRect(renderer, &d4);
                SDL_RenderFillRect(renderer, &d5);
                SDL_RenderFillRect(renderer, &d6);
                SDL_RenderFillRect(renderer, &d7);

                // Bright core
                SDL_SetRenderDrawColor(renderer, 200, 255, 200, 255);
                SDL_Rect ec = {ex - 1, ey - 1, 2, 2};
                SDL_RenderFillRect(renderer, &ec);
            } else {
                // Inactive: dim gray diamond (smaller)
                SDL_SetRenderDrawColor(renderer, 80, 80, 80, 100);
                SDL_Rect d1 = {ex, ey - 2, 1, 1};
                SDL_Rect d2 = {ex - 1, ey - 1, 3, 2};
                SDL_Rect d3 = {ex, ey + 1, 1, 1};
                SDL_RenderFillRect(renderer, &d1);
                SDL_RenderFillRect(renderer, &d2);
                SDL_RenderFillRect(renderer, &d3);
            }
        }
    }

    // --- Enemy dots (red for regular, orange pulsing for bosses) ---
    if (entities) {
        entities->forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.hasComponent<TransformComponent>()) return;

            auto& et = e.getComponent<TransformComponent>();
            int emx = offsetX + static_cast<int>((et.getCenter().x / tileSize) * scale);
            int emy = offsetY + static_cast<int>((et.getCenter().y / tileSize) * scale);

            if (emx < mapX || emx >= mapX + mapW || emy < mapY || emy >= mapY + mapH) return;

            if (e.getTag() == "enemy_boss") {
                // Boss: larger pulsing orange dot
                float bPulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.008f);
                SDL_SetRenderDrawColor(renderer, 255, 140, 40, clampU8(200 + 55 * bPulse));
                SDL_Rect er = {emx - 3, emy - 3, 6, 6};
                SDL_RenderFillRect(renderer, &er);
                // Bright center
                SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
                SDL_Rect ec = {emx - 1, emy - 1, 2, 2};
                SDL_RenderFillRect(renderer, &ec);
            } else {
                // Regular enemy: small red dot
                SDL_SetRenderDrawColor(renderer, 255, 55, 55, 200);
                SDL_Rect er = {emx - 1, emy - 1, 3, 3};
                SDL_RenderFillRect(renderer, &er);
            }
        });
    }

    // --- Player position dot (blinking white) with facing direction arrow ---
    if (player && player->getEntity() && player->getEntity()->hasComponent<TransformComponent>()) {
        auto& pt = player->getEntity()->getComponent<TransformComponent>();
        int playerMX = offsetX + static_cast<int>((pt.getCenter().x / tileSize) * scale);
        int playerMY = offsetY + static_cast<int>((pt.getCenter().y / tileSize) * scale);

        if (playerMX >= mapX && playerMX < mapX + mapW && playerMY >= mapY && playerMY < mapY + mapH) {
            Uint32 t = SDL_GetTicks();
            // Blink: white on, dim cyan off (400ms period)
            if ((t / 400) % 2 == 0) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 120, 200, 255, 180);
            }
            SDL_Rect pr = {playerMX - 2, playerMY - 2, 5, 5};
            SDL_RenderFillRect(renderer, &pr);
            // Bright center pixel
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_Rect pc = {playerMX, playerMY, 1, 1};
            SDL_RenderFillRect(renderer, &pc);

            // Direction arrow: small chevron pointing left or right
            SDL_SetRenderDrawColor(renderer, 200, 240, 255, 220);
            if (player->facingRight) {
                // Right-pointing arrow: 3px to the right of the dot
                int ax = playerMX + 4;
                SDL_Rect a1 = {ax, playerMY, 1, 1};      // tip
                SDL_Rect a2 = {ax - 1, playerMY - 1, 1, 1}; // top barb
                SDL_Rect a3 = {ax - 1, playerMY + 1, 1, 1}; // bottom barb
                SDL_RenderFillRect(renderer, &a1);
                SDL_RenderFillRect(renderer, &a2);
                SDL_RenderFillRect(renderer, &a3);
            } else {
                // Left-pointing arrow: 3px to the left of the dot
                int ax = playerMX - 4;
                SDL_Rect a1 = {ax, playerMY, 1, 1};      // tip
                SDL_Rect a2 = {ax + 1, playerMY - 1, 1, 1}; // top barb
                SDL_Rect a3 = {ax + 1, playerMY + 1, 1, 1}; // bottom barb
                SDL_RenderFillRect(renderer, &a1);
                SDL_RenderFillRect(renderer, &a2);
                SDL_RenderFillRect(renderer, &a3);
            }
        }
    }

    // --- Minimap border (dimension-tinted) ---
    if (dim == 1) {
        SDL_SetRenderDrawColor(renderer, 60, 100, 200, 180);
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 180);
    }
    SDL_RenderDrawRect(renderer, &bg);
    // Double border for polish
    SDL_Rect outerLine = {mapX - 1, mapY - 1, mapW + 2, mapH + 2};
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 140);
    SDL_RenderDrawRect(renderer, &outerLine);

    // --- "M: MAP" hint label in corner ---
    // Small label at bottom-right of minimap to remind player of toggle key
    // (rendered without font dependency - just a tiny colored marker)
}

void HUD::render(SDL_Renderer* renderer, TTF_Font* font,
                 const Player* player, const SuitEntropy* entropy,
                 const DimensionManager* dimMgr,
                 int screenW, int screenH, int fps, int riftShards) {
    // Recreate offscreen texture if screen size changed
    if (!m_hudTarget || m_hudTargetW != screenW || m_hudTargetH != screenH) {
        if (m_hudTarget) SDL_DestroyTexture(m_hudTarget);
        m_hudTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET, screenW, screenH);
        if (m_hudTarget) SDL_SetTextureBlendMode(m_hudTarget, SDL_BLENDMODE_BLEND);
        m_hudTargetW = screenW;
        m_hudTargetH = screenH;
    }

    // Redirect rendering to offscreen texture when opacity < 1
    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer);
    bool useTarget = (m_hudTarget && g_hudOpacity < 0.999f);
    if (useTarget) {
        SDL_SetRenderTarget(renderer, m_hudTarget);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Scale all panel layout constants with g_hudScale (base values doubled for 2K)
    int margin = static_cast<int>(30 * g_hudScale);
    int barW   = static_cast<int>(440 * g_hudScale);
    int barH   = static_cast<int>(36 * g_hudScale);

    // Professional HUD backing panel with gradient and border
    SDL_Rect hudBg = {margin - 10, margin - 10, barW + 40, barH * 4 + 140 + static_cast<int>(16 * g_hudScale) + 8};
    // Multi-layer gradient: darker at top, lighter at bottom
    int panelH = hudBg.h;
    int bands = 4;
    for (int b = 0; b < bands; b++) {
        int bandY = hudBg.y + (panelH * b) / bands;
        int bandH = panelH / bands + 1;
        // Fade from alpha 100 (top) to 60 (bottom)
        Uint8 ba = static_cast<Uint8>(100 - b * 10);
        SDL_SetRenderDrawColor(renderer, 5 + b * 3, 5 + b * 2, 12 + b * 4, ba);
        SDL_Rect bandR = {hudBg.x, bandY, hudBg.w, bandH};
        SDL_RenderFillRect(renderer, &bandR);
    }
    // Subtle inner highlight at top edge
    SDL_SetRenderDrawColor(renderer, 120, 110, 160, 30);
    SDL_Rect topHighlight = {hudBg.x + 1, hudBg.y, hudBg.w - 2, 1};
    SDL_RenderFillRect(renderer, &topHighlight);
    // Outer border (subtle purple-gray)
    SDL_SetRenderDrawColor(renderer, 70, 60, 100, 80);
    SDL_RenderDrawRect(renderer, &hudBg);
    // Inner border for depth
    SDL_Rect innerBorder = {hudBg.x + 1, hudBg.y + 1, hudBg.w - 2, hudBg.h - 2};
    SDL_SetRenderDrawColor(renderer, 40, 35, 60, 50);
    SDL_RenderDrawRect(renderer, &innerBorder);

    // Class icon (small colored square with class initial)
    if (player) {
        const auto& classData = ClassSystem::getData(player->playerClass);
        int iconSize = static_cast<int>(40 * g_hudScale);
        int iconX = margin;
        int iconY = margin - static_cast<int>(4 * g_hudScale);

        // Icon background
        SDL_SetRenderDrawColor(renderer, classData.color.r, classData.color.g, classData.color.b, 200);
        SDL_Rect iconRect = {iconX, iconY, iconSize, iconSize};
        SDL_RenderFillRect(renderer, &iconRect);

        // Class initial letter
        if (font) {
            char initial[2] = {classData.name[0], '\0'};
            renderText(renderer, font, initial, iconX + 10, iconY + 4, {255, 255, 255, 255});
        }

        // Blood Rage indicator for Berserker
        if (player->isBloodRageActive()) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.012f);
            Uint8 a = static_cast<Uint8>(180 * pulse);
            SDL_SetRenderDrawColor(renderer, 255, 60, 30, a);
            SDL_Rect rageRect = {iconX - 2, iconY - 2, iconSize + 4, iconSize + 4};
            SDL_RenderDrawRect(renderer, &rageRect);
        }

        // Berserker: Momentum stacks indicator
        if (player->hasMomentum()) {
            int mY = iconY + iconSize + 4;
            float intensity = static_cast<float>(player->momentumStacks) / player->momentumMaxStacks;

            // Stack pips (small squares, filled per stack)
            for (int s = 0; s < player->momentumMaxStacks; s++) {
                int px = iconX + s * 5;
                SDL_Rect pip = {px, mY, 4, 4};
                if (s < player->momentumStacks) {
                    // Filled: orange-red scaling with intensity
                    Uint8 g = clampU8(140 - 80 * intensity);
                    SDL_SetRenderDrawColor(renderer, 255, g, 40, 230);
                    SDL_RenderFillRect(renderer, &pip);
                } else {
                    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 120);
                    SDL_RenderFillRect(renderer, &pip);
                }
            }

            // Timer bar below pips
            float timerPct = player->momentumTimer / player->momentumDuration;
            int timerW = static_cast<int>(24 * timerPct);
            SDL_SetRenderDrawColor(renderer, 255, 120, 40, 160);
            SDL_Rect timerBar = {iconX, mY + 6, timerW, 2};
            SDL_RenderFillRect(renderer, &timerBar);

            // Pulsing border at high stacks
            if (player->momentumStacks >= 3) {
                float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.01f);
                Uint8 a = static_cast<Uint8>(150 * pulse);
                SDL_SetRenderDrawColor(renderer, 255, 100, 30, a);
                SDL_Rect glowRect = {iconX - 2, iconY - 2, iconSize + 4, iconSize + 4};
                SDL_RenderDrawRect(renderer, &glowRect);
            }

            if (font) {
                char momText[16];
                std::snprintf(momText, sizeof(momText), "x%d", player->momentumStacks);
                SDL_Color momColor = {255, clampU8(180 - 100 * intensity), 40, 220};
                renderText(renderer, font, momText, iconX + 26, mY - 1, momColor);
            }
        }

        // Phantom Phase Through indicator (active during dash)
        if (player->isPhaseThrough()) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.025f);
            Uint8 a = static_cast<Uint8>(220 * pulse);
            SDL_SetRenderDrawColor(renderer, 60, 220, 200, a);
            SDL_Rect phaseRect = {iconX - 3, iconY - 3, iconSize + 6, iconSize + 6};
            SDL_RenderDrawRect(renderer, &phaseRect);
            SDL_Rect phaseRect2 = {iconX - 2, iconY - 2, iconSize + 4, iconSize + 4};
            SDL_RenderDrawRect(renderer, &phaseRect2);
        }
        // Post-dash invis timer for Phantom
        if (player->postDashInvisTimer > 0 && player->playerClass == PlayerClass::Phantom) {
            float pct = player->postDashInvisTimer / ClassSystem::getData(PlayerClass::Phantom).postDashInvisTime;
            int barW2 = static_cast<int>(iconSize * pct);
            SDL_SetRenderDrawColor(renderer, 60, 220, 200, 160);
            SDL_Rect invisBar = {iconX, iconY + iconSize + 2, barW2, 3};
            SDL_RenderFillRect(renderer, &invisBar);
        }

        // Rift Charge indicator for Voidwalker (Dimensional Affinity)
        if (player->isRiftChargeActive()) {
            float pct = player->riftChargeTimer / player->riftChargeDuration;
            float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.015f);
            Uint8 a = static_cast<Uint8>(200 * pulse);
            SDL_SetRenderDrawColor(renderer, 80, 160, 255, a);
            SDL_Rect chargeRect = {iconX - 2, iconY - 2, iconSize + 4, iconSize + 4};
            SDL_RenderDrawRect(renderer, &chargeRect);
            // Small timer bar below class icon
            int chargeBarW = static_cast<int>(iconSize * pct);
            SDL_SetRenderDrawColor(renderer, 80, 160, 255, 200);
            SDL_Rect chargeBar = {iconX, iconY + iconSize + 2, chargeBarW, 3};
            SDL_RenderFillRect(renderer, &chargeBar);
        }
    }

    int hpBarOffset = static_cast<int>(52 * g_hudScale); // offset HP bar to the right of class icon (scaled for 2K)

    // HP Bar
    if (player && player->getEntity() && player->getEntity()->hasComponent<HealthComponent>()) {
        auto& hp = player->getEntity()->getComponent<HealthComponent>();
        float pct = hp.getPercent();

        SDL_Color hpColor;
        if (pct > 0.6f) hpColor = {60, 200, 80, 255};
        else if (pct > 0.3f) hpColor = {220, 200, 50, 255};
        else hpColor = {220, 50, 50, 255};

        if (pct < 0.3f) {
            float pulse = 0.7f + 0.3f * std::sin(SDL_GetTicks() * 0.008f);
            hpColor.r = static_cast<Uint8>(hpColor.r * pulse);
        }

        renderBar(renderer, margin + hpBarOffset, margin, barW - hpBarOffset, barH, pct, hpColor, {20, 20, 25, 220});

        if (font) {
            char hpText[32];
            std::snprintf(hpText, sizeof(hpText), "HP %.0f/%.0f", hp.currentHP, hp.maxHP);
            renderText(renderer, font, hpText, margin + hpBarOffset + 10, margin + 4, {255, 255, 255, 220});
        }
    }

    // XP Bar (thin, below HP bar)
    if (player) {
        int xpBarH = static_cast<int>(16 * g_hudScale);  // Readable at 2K
        int xpY = margin + barH + 4;
        float xpPct = player->getXPPercent();

        SDL_Color xpColor = {255, 215, 0, 255}; // Gold
        // Slight pulse when close to leveling up
        if (xpPct > 0.8f) {
            float pulse = 0.8f + 0.2f * std::sin(SDL_GetTicks() * 0.008f);
            xpColor.g = static_cast<Uint8>(215 * pulse);
        }

        renderBar(renderer, margin + hpBarOffset, xpY, barW - hpBarOffset, xpBarH, xpPct, xpColor, {20, 18, 10, 200});

        if (font) {
            char xpText[32];
            std::snprintf(xpText, sizeof(xpText), "Lv.%d  %d/%d", player->level, player->xp, player->xpToNextLevel);
            renderText(renderer, font, xpText, margin + hpBarOffset + 10, xpY, {255, 230, 160, 220});
        }
    }

    // Extra vertical offset introduced by XP bar
    int xpExtraH = static_cast<int>(16 * g_hudScale) + 8;

    // Entropy Bar
    if (entropy) {
        int ey = margin + barH + 4 + xpExtraH;
        float ep = entropy->getPercent();
        SDL_Color entropyColor;
        if (ep < 0.25f) entropyColor = {60, 180, 60, 255};
        else if (ep < 0.5f) entropyColor = {200, 200, 60, 255};
        else if (ep < 0.75f) entropyColor = {220, 140, 40, 255};
        else entropyColor = {220, 50, 50, 255};

        if (ep >= 0.75f) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.01f);
            entropyColor.r = static_cast<Uint8>(entropyColor.r * (0.7f + 0.3f * pulse));
        }

        renderBar(renderer, margin, ey, barW, barH, ep, entropyColor, {20, 25, 20, 220});

        if (font) {
            char entText[32];
            std::snprintf(entText, sizeof(entText), "ENTROPY %.0f%%", ep * 100);
            renderText(renderer, font, entText, margin + 10, ey + 4, {255, 255, 255, 220});
        }
    }

    // Dimension indicator
    if (dimMgr) {
        int dimY = margin + (barH + 12) * 2 + xpExtraH;
        int dim = dimMgr->getCurrentDimension();
        SDL_Color dimColorA = dimMgr->getDimColorA();
        SDL_Color dimColorB = dimMgr->getDimColorB();
        SDL_Color activeColor = (dim == 1) ? dimColorA : dimColorB;

        SDL_Rect dimBg = {margin, dimY, 100, 40};
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 200);
        SDL_RenderFillRect(renderer, &dimBg);

        SDL_Rect dimA = {margin + 2, dimY + 2, 46, 36};
        SDL_Rect dimB = {margin + 52, dimY + 2, 46, 36};
        SDL_SetRenderDrawColor(renderer, dimColorA.r, dimColorA.g, dimColorA.b,
                               static_cast<Uint8>(dim == 1 ? 255 : 80));
        SDL_RenderFillRect(renderer, &dimA);
        SDL_SetRenderDrawColor(renderer, dimColorB.r, dimColorB.g, dimColorB.b,
                               static_cast<Uint8>(dim == 2 ? 255 : 80));
        SDL_RenderFillRect(renderer, &dimB);

        SDL_SetRenderDrawColor(renderer, activeColor.r, activeColor.g, activeColor.b, 200);
        SDL_RenderDrawRect(renderer, &dimBg);

        if (font) {
            const char* dimText = (dim == 1) ? "DIM-A" : "DIM-B";
            renderText(renderer, font, dimText, margin + 112, dimY + 6,
                       {activeColor.r, activeColor.g, activeColor.b, 255});

            char switchText[32];
            std::snprintf(switchText, sizeof(switchText), "[E] x%d", dimMgr->getSwitchCount());
            renderText(renderer, font, switchText, margin + 220, dimY + 6, {150, 150, 170, 180});
        }

        // Resonance bar (below dimension indicator)
        float resonance = dimMgr->getResonance();
        if (resonance > 0.01f) {
            int resY = dimY + 48;
            int resBarW = 220;
            int resBarH = 16;

            // Color by tier: cyan -> purple -> gold
            int tier = dimMgr->getResonanceTier();
            SDL_Color resColor;
            if (tier >= 3) resColor = {255, 220, 80, 255};
            else if (tier >= 2) resColor = {180, 100, 255, 255};
            else resColor = {80, 200, 220, 255};

            // Pulsing glow at high tiers
            if (tier >= 2) {
                float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.008f);
                resColor.a = clampU8(200 + 55 * pulse);
            }

            renderBar(renderer, margin, resY, resBarW, resBarH, resonance, resColor, {15, 15, 25, 200});

            if (font) {
                const char* resLabel = (tier >= 3) ? "RESONANCE MAX" :
                                       (tier >= 2) ? "RESONANCE++" :
                                       (tier >= 1) ? "RESONANCE+" : "RESONANCE";
                renderText(renderer, font, resLabel, margin + resBarW + 12, resY - 2,
                           {resColor.r, resColor.g, resColor.b, 200});
            }
        }
    }

    renderAbilityBar(renderer, font, player, dimMgr, screenW, screenH,
                     margin + (barH + static_cast<int>(6 * g_hudScale)) * 3 + xpExtraH);
    renderCombatOverlay(renderer, font, player, screenW, screenH);

    // Combo counter (center top, only when combo > 1)
    if (player && player->getEntity() && player->getEntity()->hasComponent<CombatComponent>() && font) {
        auto& combat = player->getEntity()->getComponent<CombatComponent>();
        if (combat.comboCount > 1 && combat.comboTimer > 0) {
            char comboText[32];
            std::snprintf(comboText, sizeof(comboText), "%dx COMBO", combat.comboCount);

            // Color shifts from yellow to orange to red based on combo count
            SDL_Color comboColor;
            if (combat.comboCount < 4) {
                comboColor = {255, 220, 50, 255};
            } else if (combat.comboCount < 7) {
                comboColor = {255, 160, 30, 255};
            } else {
                comboColor = {255, 60, 30, 255};
            }

            // Scale: grows with combo count, pulses at 7+
            float comboScale = 1.0f;
            if (combat.comboCount >= 10) comboScale = 1.6f;
            else if (combat.comboCount >= 7) comboScale = 1.4f;
            else if (combat.comboCount >= 4) comboScale = 1.2f;
            // Pulsing at high combos
            if (combat.comboCount >= 5) {
                float pulseRate = 0.008f + (combat.comboCount - 5) * 0.002f;
                float pulse = 0.05f * std::sin(SDL_GetTicks() * pulseRate);
                comboScale += pulse;
            }

            // Damage bonus text (matches CombatSystem formula: comboCount * (0.15 + comboBonus))
            float comboPctPerHit = 15.0f + (m_combatSystem ? m_combatSystem->getComboBonus() * 100.0f : 0.0f);
            char bonusText[32];
            std::snprintf(bonusText, sizeof(bonusText), "+%d%% DMG", static_cast<int>(combat.comboCount * comboPctPerHit));

            int comboY = 120;

            // Render combo text with glow outline and background
            SDL_Surface* comboSurf = TTF_RenderText_Blended(font, comboText, comboColor);
            if (comboSurf) {
                int tw = static_cast<int>(comboSurf->w * comboScale);
                int th = static_cast<int>(comboSurf->h * comboScale);
                int cx = screenW / 2 - tw / 2;

                // Background panel (semi-transparent dark with colored accent)
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 60);
                SDL_Rect comboBg = {cx - 12, comboY - 4, tw + 24, th + 8};
                SDL_RenderFillRect(renderer, &comboBg);
                SDL_SetRenderDrawColor(renderer, comboColor.r, comboColor.g, comboColor.b, 40);
                SDL_RenderDrawRect(renderer, &comboBg);

                SDL_Texture* comboTex = SDL_CreateTextureFromSurface(renderer, comboSurf);
                if (comboTex) {
                    // Glow pass (additive, larger, dimmer)
                    SDL_SetTextureBlendMode(comboTex, SDL_BLENDMODE_ADD);
                    SDL_SetTextureAlphaMod(comboTex, 35);
                    SDL_Rect glowDst = {cx - 3, comboY - 2, tw + 6, th + 4};
                    SDL_RenderCopy(renderer, comboTex, nullptr, &glowDst);

                    // Shadow pass
                    SDL_SetTextureBlendMode(comboTex, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureColorMod(comboTex, 0, 0, 0);
                    SDL_SetTextureAlphaMod(comboTex, 160);
                    SDL_Rect shadowDst = {cx + 1, comboY + 1, tw, th};
                    SDL_RenderCopy(renderer, comboTex, nullptr, &shadowDst);

                    // Main text
                    SDL_SetTextureColorMod(comboTex, 255, 255, 255);
                    SDL_SetTextureAlphaMod(comboTex, 255);
                    SDL_Rect dst = {cx, comboY, tw, th};
                    SDL_RenderCopy(renderer, comboTex, nullptr, &dst);
                    SDL_DestroyTexture(comboTex);
                }
                SDL_FreeSurface(comboSurf);
            }

            // Bonus text (slightly scaled at high combos)
            float bonusScale = comboScale > 1.2f ? 1.1f : 1.0f;
            SDL_Color bonusColor = {comboColor.r, comboColor.g, comboColor.b, 180};
            SDL_Surface* bonusSurf = TTF_RenderText_Blended(font, bonusText, bonusColor);
            if (bonusSurf) {
                SDL_Texture* bonusTex = SDL_CreateTextureFromSurface(renderer, bonusSurf);
                if (bonusTex) {
                    int bw = static_cast<int>(bonusSurf->w * bonusScale);
                    int bh = static_cast<int>(bonusSurf->h * bonusScale);
                    int bonusY = comboY + static_cast<int>(20 * comboScale) + 2;
                    SDL_Rect dst = {screenW / 2 - bw / 2, bonusY, bw, bh};
                    SDL_RenderCopy(renderer, bonusTex, nullptr, &dst);
                    SDL_DestroyTexture(bonusTex);
                }
                SDL_FreeSurface(bonusSurf);
            }

            // Combo timer bar (shrinking, width scales with combo)
            float timerPct = combat.comboTimer / std::max(0.01f, combat.comboWindow);
            int comboBarW = static_cast<int>(80 * comboScale);
            int barX = screenW / 2 - comboBarW / 2;
            int barY = comboY + static_cast<int>(40 * comboScale) + 4;
            SDL_Rect timerBg = {barX, barY, comboBarW, 4};
            SDL_SetRenderDrawColor(renderer, 40, 40, 50, 150);
            SDL_RenderFillRect(renderer, &timerBg);
            SDL_Rect timerFill = {barX, barY, static_cast<int>(comboBarW * timerPct), 4};
            SDL_SetRenderDrawColor(renderer, comboColor.r, comboColor.g, comboColor.b, 200);
            SDL_RenderFillRect(renderer, &timerFill);
        }
    }

    // Combo Finisher prompt (below combo counter)
    if (player && player->isFinisherAvailable() && font) {
        // Determine finisher name and color by class
        const char* finisherName = "FINISHER";
        SDL_Color fColor = {255, 215, 0, 255};
        switch (player->playerClass) {
            case PlayerClass::Voidwalker:
                finisherName = "[E] RIFT PULSE";
                fColor = {180, 80, 255, 255};
                break;
            case PlayerClass::Berserker:
                finisherName = "[E] BLOOD CLEAVE";
                fColor = {255, 80, 80, 255};
                break;
            case PlayerClass::Phantom:
                finisherName = "[E] PHASE BURST";
                fColor = {80, 220, 255, 255};
                break;
            default: break;
        }

        // Pulsing alpha
        float pulse = 0.7f + 0.3f * std::sin(SDL_GetTicks() * 0.01f);
        fColor.a = clampU8(180 + 75 * pulse);

        // Pulsing scale
        float fScale = 1.0f + 0.1f * std::sin(SDL_GetTicks() * 0.012f);

        int finisherY = 220; // below combo area (scaled for 2K)
        SDL_Surface* fSurf = TTF_RenderText_Blended(font, finisherName, fColor);
        if (fSurf) {
            SDL_Texture* fTex = SDL_CreateTextureFromSurface(renderer, fSurf);
            if (fTex) {
                int fw = static_cast<int>(fSurf->w * fScale);
                int fh = static_cast<int>(fSurf->h * fScale);
                SDL_Rect dst = {screenW / 2 - fw / 2, finisherY, fw, fh};
                SDL_RenderCopy(renderer, fTex, nullptr, &dst);
                SDL_DestroyTexture(fTex);
            }
            SDL_FreeSurface(fSurf);
        }

        // Finisher availability timer bar
        float fTimerPct = player->finisherAvailableTimer / std::max(0.01f, player->finisherAvailableMaxTime);
        int fBarW = 120;
        int fBarX = screenW / 2 - fBarW / 2;
        int fBarY = finisherY + static_cast<int>(18 * fScale) + 2;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect fBg = {fBarX, fBarY, fBarW, 3};
        SDL_SetRenderDrawColor(renderer, 40, 40, 50, 150);
        SDL_RenderFillRect(renderer, &fBg);
        SDL_Rect fFill = {fBarX, fBarY, static_cast<int>(fBarW * fTimerPct), 3};
        SDL_SetRenderDrawColor(renderer, fColor.r, fColor.g, fColor.b, 200);
        SDL_RenderFillRect(renderer, &fFill);
    }

    // Floor / Zone + Rift Shards — compact info strip below minimap
    {
        const int infoX = screenW - 170;
        const int infoY = m_showMinimap ? 130 : 12;
        const int zone  = std::clamp((m_currentFloor - 1) / 6, 0, 4) + 1;

        // Background panel
        SDL_Rect infoBg = {infoX - 5, infoY - 2, 170, 36};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 90);
        SDL_RenderFillRect(renderer, &infoBg);

        if (font) {
            // Line 1: "F12 / Zone 2"  (boss floors pulse red)
            char floorText[48];
            bool isBoss = (m_currentFloor % 3 == 0);
            if (isBoss) {
                std::snprintf(floorText, sizeof(floorText), "F%d / Zone %d  BOSS", m_currentFloor, zone);
                float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.008f);
                Uint8 a = static_cast<Uint8>(220 * pulse);
                renderText(renderer, font, floorText, infoX, infoY, {255, 80, 60, a});
            } else {
                std::snprintf(floorText, sizeof(floorText), "F%d / Zone %d", m_currentFloor, zone);
                renderText(renderer, font, floorText, infoX, infoY, {180, 180, 200, 180});
            }

            // Kill count + NG+ on same line (right side)
            char extraText[32];
            if (g_newGamePlusLevel > 0)
                std::snprintf(extraText, sizeof(extraText), "K:%d NG+%d", m_killCount, g_newGamePlusLevel);
            else
                std::snprintf(extraText, sizeof(extraText), "K:%d", m_killCount);
            renderText(renderer, font, extraText, infoX + 100, infoY, {200, 160, 160, 150});

            // Line 2: diamond icon + shard count
            int iy2 = infoY + 17;
            int ix = infoX + 6, iy = iy2 + 6;
            SDL_SetRenderDrawColor(renderer, 180, 130, 255, 200);
            SDL_RenderDrawLine(renderer, ix, iy - 5, ix + 5, iy);
            SDL_RenderDrawLine(renderer, ix + 5, iy, ix, iy + 5);
            SDL_RenderDrawLine(renderer, ix, iy + 5, ix - 5, iy);
            SDL_RenderDrawLine(renderer, ix - 5, iy, ix, iy - 5);

            char shardText[32];
            std::snprintf(shardText, sizeof(shardText), "%d", riftShards);
            renderText(renderer, font, shardText, infoX + 16, iy2, {200, 170, 255, 180});

            // Run timer — pulses gold under 10:00 (speedrun territory)
            int mins = static_cast<int>(m_runTime) / 60;
            int secs = static_cast<int>(m_runTime) % 60;
            char timerText[16];
            std::snprintf(timerText, sizeof(timerText), "%02d:%02d", mins, secs);
            SDL_Color timerCol = {160, 160, 180, 150}; // dim default
            if (m_runTime < 600.0f) { // under 10 minutes — gold pulse
                float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.005f);
                Uint8 a = static_cast<Uint8>(200 * pulse);
                timerCol = {255, 215, 80, a};
            }
            renderText(renderer, font, timerText, infoX + 100, iy2, timerCol);
        }
    }

    renderWeaponPanel(renderer, font, player, screenW, screenH);

    // FPS counter (bottom right corner)
    if (font) {
        char fpsText[16];
        std::snprintf(fpsText, sizeof(fpsText), "%d FPS", fps);
        renderText(renderer, font, fpsText, screenW - 75, screenH - 30, {80, 80, 90, 150});
    }

    // NG+ tier indicator (gold number, top-right corner)
    if (font && m_ngPlusTier > 0) {
        char ngBuf[16];
        std::snprintf(ngBuf, sizeof(ngBuf), "NG+%d", m_ngPlusTier);
        // Pulsing gold: use SDL_GetTicks for animation without a stored timer
        float pulse = 0.85f + 0.15f * std::sin(SDL_GetTicks() * 0.003f);
        Uint8 ngR = static_cast<Uint8>(255);
        Uint8 ngG = static_cast<Uint8>(200 * pulse);
        Uint8 ngB = static_cast<Uint8>(30);
        SDL_Color ngColor = {ngR, ngG, ngB, 230};
        SDL_Surface* ns = TTF_RenderText_Blended(font, ngBuf, ngColor);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                // Scale up 1.8x for visibility
                int nw = static_cast<int>(ns->w * 1.8f);
                int nh = static_cast<int>(ns->h * 1.8f);
                // Position: top-right, with a background panel
                int nx = screenW - nw - 24;
                int ny = 20;
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
                SDL_Rect bg = {nx - 6, ny - 3, nw + 12, nh + 6};
                SDL_RenderFillRect(renderer, &bg);
                SDL_SetRenderDrawColor(renderer, ngR, ngG, ngB, 160);
                SDL_RenderDrawRect(renderer, &bg);
                SDL_Rect nr = {nx, ny, nw, nh};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }

    // Controls hint (bottom left)
    if (font) {
        renderText(renderer, font,
                   "WASD Move  SPACE Jump  SHIFT Dash  J/K Attack  Q/R Weapon  E Dim  F Interact  1/2/3 Abilities",
                   15, screenH - 22, {60, 60, 80, 100});
    }

    // Restore main render target and blit HUD texture with opacity
    if (useTarget) {
        SDL_SetRenderTarget(renderer, previousTarget);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint8 alpha = static_cast<Uint8>(g_hudOpacity * 255.0f);
        SDL_SetTextureAlphaMod(m_hudTarget, alpha);
        SDL_Rect dst = {0, 0, screenW, screenH};
        SDL_RenderCopy(renderer, m_hudTarget, nullptr, &dst);
    }
}

// ============================================================
// Extracted sub-renderers from HUD::render()
// ============================================================

void HUD::renderAbilityBar(SDL_Renderer* renderer, TTF_Font* font,
                           const Player* player, const DimensionManager* dimMgr,
                           int screenW, int screenH, int startY) {
    int margin = static_cast<int>(30 * g_hudScale);
    int barH   = static_cast<int>(36 * g_hudScale);

    // Ability bar with cooldown indicators
    if (player && player->getEntity() && player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = player->getEntity()->getComponent<CombatComponent>();
        int abY = startY;
        int iconSize = static_cast<int>(44 * g_hudScale);
        int iconGap  = static_cast<int>(12 * g_hudScale);

        struct AbilityInfo { float cooldownPct; SDL_Color readyColor; const char* label; };

        float meleePct = (combat.cooldownTimer <= 0) ? 1.0f
            : 1.0f - (combat.cooldownTimer / std::max(0.01f, combat.meleeAttack.cooldown));
        float rangedPct = (combat.cooldownTimer <= 0) ? 1.0f
            : 1.0f - (combat.cooldownTimer / std::max(0.01f, combat.rangedAttack.cooldown));
        float dashPct = 1.0f - (player->dashCooldownTimer / std::max(0.01f, player->dashCooldown));
        if (dashPct > 1.0f) dashPct = 1.0f;
        float dimPct = dimMgr ? 1.0f - (dimMgr->getCooldownTimer() / std::max(0.01f, dimMgr->switchCooldown)) : 1.0f;
        if (dimPct > 1.0f) dimPct = 1.0f;

        AbilityInfo abilities[4] = {
            {std::max(0.0f, meleePct), {255, 220, 100, 255}, "J"},
            {std::max(0.0f, rangedPct), {100, 200, 255, 255}, "K"},
            {std::max(0.0f, dashPct), {100, 255, 200, 255}, "SH"},
            {std::max(0.0f, dimPct), {200, 150, 255, 255}, "E"}
        };

        for (int i = 0; i < 4; i++) {
            int ix = margin + i * (iconSize + iconGap);
            bool ready = abilities[i].cooldownPct >= 1.0f;
            if (ready && m_abilityWasOnCooldown[i]) m_abilityReadyFlash[i] = 0.35f;
            m_abilityWasOnCooldown[i] = !ready;
            SDL_Color c = ready ? abilities[i].readyColor : SDL_Color{50, 50, 60, 200};

            SDL_SetRenderDrawColor(renderer, 15, 15, 25, 180);
            SDL_Rect bg = {ix, abY, iconSize, iconSize};
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
            int cx = ix + iconSize / 2, cy = abY + iconSize / 2;
            switch (i) {
                case 0: SDL_RenderDrawLine(renderer, cx-5,cy+5,cx+5,cy-5); SDL_RenderDrawLine(renderer, cx-5,cy+4,cx+5,cy-6); SDL_RenderDrawLine(renderer, cx-3,cy-1,cx+3,cy+1); break;
                case 1: SDL_RenderDrawLine(renderer, cx-6,cy,cx-2,cy); SDL_RenderDrawLine(renderer, cx-5,cy-1,cx-2,cy-1); { SDL_Rect dot={cx,cy-3,6,6}; SDL_RenderFillRect(renderer,&dot); } break;
                case 2: SDL_RenderDrawLine(renderer, cx-6,cy,cx+4,cy); SDL_RenderDrawLine(renderer, cx+4,cy,cx,cy-4); SDL_RenderDrawLine(renderer, cx+4,cy,cx,cy+4); SDL_RenderDrawLine(renderer, cx-6,cy-1,cx+4,cy-1); break;
                case 3: { SDL_Rect sq1={cx-5,cy-5,7,7}, sq2={cx-1,cy-1,7,7}; SDL_RenderDrawRect(renderer,&sq1); SDL_RenderDrawRect(renderer,&sq2); } break;
            }
            if (!ready) {
                int coverH = static_cast<int>(iconSize * (1.0f - abilities[i].cooldownPct));
                SDL_SetRenderDrawColor(renderer, 0,0,0,150);
                SDL_Rect cover = {ix, abY, iconSize, coverH};
                SDL_RenderFillRect(renderer, &cover);
                if (font) {
                    float remain = 0;
                    if (i==0) remain=combat.cooldownTimer; else if (i==1) remain=combat.cooldownTimer;
                    else if (i==2) remain=player->dashCooldownTimer; else if (i==3&&dimMgr) remain=dimMgr->getCooldownTimer();
                    if (remain>0.05f) { char cdTxt[8]; std::snprintf(cdTxt,sizeof(cdTxt),"%.1f",remain); renderText(renderer,font,cdTxt,ix+2,abY+iconSize/2-5,{255,255,255,220}); }
                }
            }
            if (ready) { float pulse=0.5f+0.5f*std::sin(SDL_GetTicks()*0.006f); SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,static_cast<Uint8>(140+60*pulse)); }
            else SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,60);
            SDL_RenderDrawRect(renderer, &bg);
            if (m_abilityReadyFlash[i]>0) { float flashT=m_abilityReadyFlash[i]/0.35f; Uint8 flashA=static_cast<Uint8>(180*flashT); auto& rc=abilities[i].readyColor; SDL_SetRenderDrawColor(renderer,rc.r,rc.g,rc.b,flashA); SDL_Rect flashRect={ix-2,abY-2,iconSize+4,iconSize+4}; SDL_RenderFillRect(renderer,&flashRect); }
            if (font) renderText(renderer,font,abilities[i].label,ix+4,abY+iconSize+1,{c.r,c.g,c.b,static_cast<Uint8>(ready?200:80)});
        }
    }

    // Active buff/debuff indicators
    if (player) {
        int buffY = startY + static_cast<int>(36 * g_hudScale);
        int buffX = margin;
        int buffSize = static_cast<int>(12 * g_hudScale);
        int buffGap  = static_cast<int>(3 * g_hudScale);
        int timerBarH2 = static_cast<int>(3 * g_hudScale);

        auto drawBuff = [&](const char* label, float timer, float maxTime, SDL_Color color) {
            if (timer<=0) return;
            float pct=timer/std::max(0.01f,maxTime);
            Uint8 fadeAlpha=(timer<2.0f)?static_cast<Uint8>(80+175*(timer/2.0f)):255;
            SDL_SetRenderDrawColor(renderer,10,10,20,static_cast<Uint8>(fadeAlpha*0.7f));
            SDL_Rect bg={buffX,buffY,buffSize,buffSize}; SDL_RenderFillRect(renderer,&bg);
            SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,static_cast<Uint8>(fadeAlpha*0.7f));
            SDL_RenderFillRect(renderer,&bg);
            if (font) renderText(renderer,font,label,buffX+1,buffY,{255,255,255,fadeAlpha});
            SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,static_cast<Uint8>(fadeAlpha*0.8f));
            SDL_RenderDrawRect(renderer,&bg);
            int timerW=static_cast<int>(buffSize*pct);
            SDL_SetRenderDrawColor(renderer,20,20,30,static_cast<Uint8>(fadeAlpha*0.6f));
            SDL_Rect timerBg2={buffX,buffY+buffSize+1,buffSize,timerBarH2}; SDL_RenderFillRect(renderer,&timerBg2);
            SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,fadeAlpha);
            SDL_Rect timerFill2={buffX,buffY+buffSize+1,timerW,timerBarH2}; SDL_RenderFillRect(renderer,&timerFill2);
            buffX+=buffSize+buffGap;
        };
        drawBuff("S",player->speedBoostTimer,6.0f,{255,255,80,255});
        drawBuff("D",player->damageBoostTimer,8.0f,{255,80,80,255});
        if (player->hasShield) drawBuff("W",player->shieldTimer,8.0f,{100,180,255,255});
        if (player->isRiftChargeActive()) drawBuff("R",player->riftChargeTimer,player->riftChargeDuration,{80,160,255,255});
        if (player->hasMomentum()) drawBuff("M",player->momentumTimer,player->momentumDuration,{255,140,40,255});
        if (player->postDashInvisTimer>0 && player->playerClass==PlayerClass::Phantom)
            drawBuff("I",player->postDashInvisTimer,ClassSystem::getData(PlayerClass::Phantom).postDashInvisTime,{60,220,200,255});
        if (player->isBurning()) drawBuff("F",player->burnTimer,3.0f,{255,120,30,255});
        if (player->isFrozen()) drawBuff("C",player->freezeTimer,2.0f,{80,180,255,255});
        if (player->isPoisoned()) drawBuff("P",player->poisonTimer,4.0f,{80,220,80,255});
    }

    // Ability icons (below active buffs)
    if (player && player->getEntity() && player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = player->getEntity()->getComponent<AbilityComponent>();
        int abStartY = startY + static_cast<int>(58 * g_hudScale);
        int abIconSize = static_cast<int>(26 * g_hudScale);
        int abIconGap  = static_cast<int>(6 * g_hudScale);

        struct AbilityIconInfo { const char* label; SDL_Color color; float cdPct; bool active; };
        AbilityIconInfo abIcons[3];
        if (player->playerClass == PlayerClass::Technomancer) {
            float turretCdPct=(player->turretCooldownTimer<=0)?1.0f:1.0f-player->turretCooldownTimer/std::max(0.01f,player->turretCooldown);
            float trapCdPct=(player->trapCooldownTimer<=0)?1.0f:1.0f-player->trapCooldownTimer/std::max(0.01f,player->trapCooldown);
            abIcons[0]={"Q",{230,180,50,255},turretCdPct,player->activeTurrets>=player->maxTurrets};
            abIcons[1]={"E",{255,200,50,255},trapCdPct,player->activeTraps>=player->maxTraps};
            abIcons[2]={"3",{180,80,255,255},abil.abilities[2].getCooldownPercent(),abil.abilities[2].active};
        } else {
            abIcons[0]={"1",{255,180,60,255},abil.abilities[0].getCooldownPercent(),abil.abilities[0].active||abil.slamFalling};
            abIcons[1]={"2",{80,220,255,255},abil.abilities[1].getCooldownPercent(),abil.abilities[1].active};
            abIcons[2]={"3",{180,80,255,255},abil.abilities[2].getCooldownPercent(),abil.abilities[2].active};
        }
        for (int i=0;i<3;i++) {
            int ix=margin+i*(abIconSize+abIconGap);
            bool ready=abIcons[i].cdPct>=1.0f;
            bool active=abIcons[i].active;
            SDL_Color c=active?SDL_Color{255,255,255,255}:(ready?abIcons[i].color:SDL_Color{40,40,50,200});
            SDL_SetRenderDrawColor(renderer,10,10,20,200);
            SDL_Rect bg={ix,abStartY,abIconSize,abIconSize}; SDL_RenderFillRect(renderer,&bg);
            SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);
            int acx=ix+abIconSize/2, acy=abStartY+abIconSize/2;
            if (player->playerClass==PlayerClass::Technomancer) {
                switch(i){case 0:SDL_RenderDrawLine(renderer,acx-5,acy+4,acx+5,acy+4);{SDL_Rect tb={acx-4,acy-2,8,6};SDL_RenderDrawRect(renderer,&tb);}SDL_RenderDrawLine(renderer,acx+4,acy,acx+8,acy);SDL_RenderDrawLine(renderer,acx+4,acy+1,acx+8,acy+1);break;case 1:SDL_RenderDrawLine(renderer,acx,acy-6,acx+6,acy);SDL_RenderDrawLine(renderer,acx+6,acy,acx,acy+6);SDL_RenderDrawLine(renderer,acx,acy+6,acx-6,acy);SDL_RenderDrawLine(renderer,acx-6,acy,acx,acy-6);SDL_RenderDrawLine(renderer,acx-1,acy-2,acx+1,acy+2);break;case 2:SDL_RenderDrawLine(renderer,acx-2,acy-7,acx+2,acy-2);SDL_RenderDrawLine(renderer,acx+2,acy-2,acx-3,acy-1);SDL_RenderDrawLine(renderer,acx-3,acy-1,acx+2,acy+7);break;}
            } else {
                switch(i){case 0:SDL_RenderDrawLine(renderer,acx,acy-6,acx,acy+4);SDL_RenderDrawLine(renderer,acx,acy+4,acx-4,acy);SDL_RenderDrawLine(renderer,acx,acy+4,acx+4,acy);SDL_RenderDrawLine(renderer,acx-6,acy+6,acx-3,acy+4);SDL_RenderDrawLine(renderer,acx+6,acy+6,acx+3,acy+4);break;case 1:for(int h=0;h<6;h++){float a1=h*6.283185f/6.0f-1.5708f;float a2=(h+1)*6.283185f/6.0f-1.5708f;SDL_RenderDrawLine(renderer,acx+static_cast<int>(std::cos(a1)*8),acy+static_cast<int>(std::sin(a1)*8),acx+static_cast<int>(std::cos(a2)*8),acy+static_cast<int>(std::sin(a2)*8));}break;case 2:SDL_RenderDrawLine(renderer,acx-2,acy-7,acx+2,acy-2);SDL_RenderDrawLine(renderer,acx+2,acy-2,acx-3,acy-1);SDL_RenderDrawLine(renderer,acx-3,acy-1,acx+2,acy+7);break;}
            }
            if (!ready&&!active) {
                int coverH=static_cast<int>(abIconSize*(1.0f-abIcons[i].cdPct));
                SDL_SetRenderDrawColor(renderer,0,0,0,160); SDL_Rect cover={ix,abStartY,abIconSize,coverH}; SDL_RenderFillRect(renderer,&cover);
                if (font) { float remain=0; if(player->playerClass==PlayerClass::Technomancer){if(i==0)remain=player->turretCooldownTimer;else if(i==1)remain=player->trapCooldownTimer;else remain=abil.abilities[2].cooldownTimer;}else{remain=abil.abilities[i].cooldownTimer;} if(remain>0.05f){char cdTxt[8];std::snprintf(cdTxt,sizeof(cdTxt),"%.1f",remain);renderText(renderer,font,cdTxt,ix+3,abStartY+abIconSize/2-5,{255,255,255,220});} }
            }
            if (active) { float pulse=0.5f+0.5f*std::sin(SDL_GetTicks()*0.01f); Uint8 gA=static_cast<Uint8>(100+80*pulse); SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,gA); SDL_Rect glow={ix-2,abStartY-2,abIconSize+4,abIconSize+4}; SDL_RenderDrawRect(renderer,&glow); }
            if (ready&&!active){float pulse=0.5f+0.5f*std::sin(SDL_GetTicks()*0.006f);SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,static_cast<Uint8>(150+50*pulse));}else{SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,static_cast<Uint8>(active?200:60));}
            SDL_RenderDrawRect(renderer,&bg);
            if (font) renderText(renderer,font,abIcons[i].label,ix+9,abStartY+abIconSize+1,{c.r,c.g,c.b,static_cast<Uint8>(ready?200:80)});
        }
    }
}

void HUD::renderCombatOverlay(SDL_Renderer* renderer, TTF_Font* font,
                              const Player* player, int screenW, int screenH) {
    // Weapon names now shown via renderWeaponPanel (visual card with icons)
    // No duplicate text rendering needed
    (void)renderer; (void)font; (void)player; (void)screenW; (void)screenH;
}

void HUD::renderWeaponPanel(SDL_Renderer* renderer, TTF_Font* font,
                            const Player* player, int screenW, int screenH) {
    if (!player || !player->getEntity() || !player->getEntity()->hasComponent<CombatComponent>()) return;
    auto& combat = player->getEntity()->getComponent<CombatComponent>();
    int wpnY = screenH - 132;
    int wpnX = screenW / 2 - 200;

    SDL_Rect wpnBg = {wpnX, wpnY, 400, 104};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
    SDL_RenderFillRect(renderer, &wpnBg);
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 80);
    SDL_RenderDrawRect(renderer, &wpnBg);

    SDL_Rect meleeBg = {wpnX+4,wpnY+4,192,96}; SDL_SetRenderDrawColor(renderer,180,60,60,60); SDL_RenderFillRect(renderer,&meleeBg);
    SDL_SetRenderDrawColor(renderer,220,180,100,200); SDL_RenderDrawLine(renderer,wpnX+16,wpnY+52,wpnX+44,wpnY+16); SDL_RenderDrawLine(renderer,wpnX+36,wpnY+28,wpnX+52,wpnY+28);
    SDL_Rect rangedBg = {wpnX+204,wpnY+4,192,96}; SDL_SetRenderDrawColor(renderer,60,60,180,60); SDL_RenderFillRect(renderer,&rangedBg);
    SDL_SetRenderDrawColor(renderer,100,180,220,200); SDL_Rect gunBody={wpnX+220,wpnY+24,28,16}; SDL_RenderFillRect(renderer,&gunBody); SDL_RenderDrawLine(renderer,wpnX+248,wpnY+30,wpnX+260,wpnY+30);

    if (font) {
        const char* mName = WeaponSystem::getWeaponName(combat.currentMelee);
        const char* rName = WeaponSystem::getWeaponName(combat.currentRanged);
        renderText(renderer, font, mName, wpnX+60, wpnY+16, {220,180,100,220});
        renderText(renderer, font, rName, wpnX+268, wpnY+16, {100,180,220,220});
        renderText(renderer, font, "[Q]", wpnX+4, wpnY-28, {150,150,160,120});
        renderText(renderer, font, "[R]", wpnX+344, wpnY-28, {150,150,160,120});

        if (m_combatSystem) {
            int mIdx=static_cast<int>(combat.currentMelee), rIdx=static_cast<int>(combat.currentRanged);
            int mKills=m_combatSystem->weaponKills[mIdx], rKills=m_combatSystem->weaponKills[rIdx];
            MasteryTier mTier=WeaponSystem::getMasteryTier(mKills), rTier=WeaponSystem::getMasteryTier(rKills);
            auto tierColor=[](MasteryTier t)->SDL_Color{switch(t){case MasteryTier::Mastered:return{255,220,80,255};case MasteryTier::Proficient:return{180,100,255,255};case MasteryTier::Familiar:return{80,200,180,255};default:return{80,80,90,120};}};
            MasteryTier tiers[2]={mTier,rTier};
            for(int w=0;w<2;w++){if(tiers[w]!=MasteryTier::None&&tiers[w]!=m_prevMasteryTier[w])m_masteryFlash[w]=0.6f;m_prevMasteryTier[w]=tiers[w];}
            auto masteryProgress=[](int kills)->float{int next=WeaponSystem::getNextTierThreshold(kills);if(next==0)return 1.0f;int prev=0;if(next==25)prev=10;else if(next==50)prev=25;float range=static_cast<float>(next-prev);return(range>0)?static_cast<float>(kills-prev)/range:1.0f;};
            auto renderMastery=[&](int kills,MasteryTier tier,int baseX,int barY,int slotIdx,SDL_Color barColor){
                const int bW=80,bH=8;
                int next=WeaponSystem::getNextTierThreshold(kills);
                if(tier!=MasteryTier::None){const char*tName=WeaponSystem::getMasteryTierName(tier);SDL_Color col=tierColor(tier);if(m_masteryFlash[slotIdx]>0){float t2=m_masteryFlash[slotIdx]/0.6f;col={255,220,80,static_cast<Uint8>(200*t2)};}renderText(renderer,font,tName,baseX,barY-14,col);if(next>0){char prog[16];std::snprintf(prog,sizeof(prog),"%d/%d",kills,next);renderText(renderer,font,prog,baseX+44,barY-14,{100,100,110,140});}}
                else{char prog[16];std::snprintf(prog,sizeof(prog),"%d/%d",kills,next);renderText(renderer,font,prog,baseX,barY-14,{100,100,110,140});}
                renderBar(renderer,baseX,barY,bW,bH,masteryProgress(kills),barColor,{30,30,40,160});
                SDL_SetRenderDrawColor(renderer,60,60,80,100);SDL_Rect barRect={baseX,barY,bW,bH};SDL_RenderDrawRect(renderer,&barRect);
                if(m_masteryFlash[slotIdx]>0){float t2=m_masteryFlash[slotIdx]/0.6f;SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer,255,220,80,static_cast<Uint8>(120*t2));SDL_Rect glow={baseX-1,barY-1,bW+2,bH+2};SDL_RenderDrawRect(renderer,&glow);}
            };
            renderMastery(mKills,mTier,wpnX+60,wpnY+76,0,tierColor(mTier));
            renderMastery(rKills,rTier,wpnX+268,wpnY+76,1,tierColor(rTier));
        }

        if (player->getEntity()->hasComponent<RelicComponent>()) {
            auto& relics=player->getEntity()->getComponent<RelicComponent>();
            int synergyY=wpnY+108;
            for(int i=0;i<static_cast<int>(SynergyID::COUNT);i++){
                auto sid=static_cast<SynergyID>(i);
                if(RelicSynergy::isWeaponSynergyActive(relics,sid,combat.currentMelee,combat.currentRanged)){
                    const auto&sData=RelicSynergy::getData(sid);
                    renderText(renderer,font,sData.name,wpnX+20,synergyY,{255,200,80,220});
                    renderText(renderer,font,sData.description,wpnX+20,synergyY+28,{200,180,120,160});
                    synergyY+=56;
                }
            }
        }
    }
}

void HUD::renderBar(SDL_Renderer* renderer, int x, int y, int w, int h,
                     float percent, SDL_Color fillColor, SDL_Color bgColor) {
    // Outer frame (dark beveled border)
    SDL_SetRenderDrawColor(renderer, 15, 15, 20, bgColor.a);
    SDL_Rect outerFrame = {x - 1, y - 1, w + 2, h + 2};
    SDL_RenderFillRect(renderer, &outerFrame);

    // Background with subtle gradient (darker at bottom)
    SDL_Rect bgTop = {x, y, w, h / 2};
    SDL_SetRenderDrawColor(renderer, bgColor.r + 8, bgColor.g + 8, bgColor.b + 10, bgColor.a);
    SDL_RenderFillRect(renderer, &bgTop);
    SDL_Rect bgBot = {x, y + h / 2, w, h - h / 2};
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(renderer, &bgBot);

    int fillW = static_cast<int>(w * percent);
    if (fillW > 0) {
        // Main fill with 3-band vertical gradient
        int bandH = h / 3;
        // Top band: bright
        Uint8 hiR = clampU8(fillColor.r + 40);
        Uint8 hiG = clampU8(fillColor.g + 40);
        Uint8 hiB = clampU8(fillColor.b + 35);
        SDL_SetRenderDrawColor(renderer, hiR, hiG, hiB, fillColor.a);
        SDL_Rect topBand = {x, y, fillW, bandH};
        SDL_RenderFillRect(renderer, &topBand);
        // Mid band: base color
        SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        SDL_Rect midBand = {x, y + bandH, fillW, bandH};
        SDL_RenderFillRect(renderer, &midBand);
        // Bottom band: darker
        Uint8 shR = fillColor.r > 50 ? static_cast<Uint8>(fillColor.r - 50) : 0;
        Uint8 shG = fillColor.g > 50 ? static_cast<Uint8>(fillColor.g - 50) : 0;
        Uint8 shB = fillColor.b > 50 ? static_cast<Uint8>(fillColor.b - 50) : 0;
        SDL_SetRenderDrawColor(renderer, shR, shG, shB, fillColor.a);
        SDL_Rect botBand = {x, y + bandH * 2, fillW, h - bandH * 2};
        SDL_RenderFillRect(renderer, &botBand);

        // Glass-like specular highlight (1px bright line near top)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
        SDL_Rect specular = {x + 1, y + 1, fillW - 2, 1};
        SDL_RenderFillRect(renderer, &specular);

        // Animated shimmer (subtle moving highlight)
        Uint32 ticks = SDL_GetTicks();
        int shimmerX = x + static_cast<int>((ticks % 3000) * fillW / 3000.0f);
        if (shimmerX < x + fillW - 16) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 25);
            SDL_Rect shimmer = {shimmerX, y + 1, 16, h - 2};
            SDL_RenderFillRect(renderer, &shimmer);
        }
    }

    // Inner border highlight (top-left bright, bottom-right dark)
    SDL_SetRenderDrawColor(renderer, 160, 155, 180, 50);
    SDL_RenderDrawLine(renderer, x, y, x + w - 1, y); // top
    SDL_RenderDrawLine(renderer, x, y, x, y + h - 1); // left
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 80);
    SDL_RenderDrawLine(renderer, x, y + h - 1, x + w - 1, y + h - 1); // bottom
    SDL_RenderDrawLine(renderer, x + w - 1, y, x + w - 1, y + h - 1); // right

    // Quarter notch marks (subtle)
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 40);
    for (int i = 1; i < 4; i++) {
        int nx = x + (w * i) / 4;
        SDL_RenderDrawLine(renderer, nx, y + 1, nx, y + h - 1);
    }
}

void HUD::renderText(SDL_Renderer* renderer, TTF_Font* font,
                      const char* text, int x, int y, SDL_Color color) {
    if (!font || !text) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        int sw = static_cast<int>(surface->w * g_hudScale);
        int sh = static_cast<int>(surface->h * g_hudScale);
        SDL_Rect rect = {x, y, sw, sh};
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}
