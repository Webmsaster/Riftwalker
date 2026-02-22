#include "HUD.h"
#include "Game/Player.h"
#include "Game/SuitEntropy.h"
#include "Game/DimensionManager.h"
#include "Game/Level.h"
#include "ECS/EntityManager.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/TransformComponent.h"
#include <cstdio>
#include <cmath>

void HUD::updateFlash(float dt) {
    if (m_damageFlash > 0) m_damageFlash -= dt;
}

void HUD::renderFlash(SDL_Renderer* renderer, int screenW, int screenH) {
    if (m_damageFlash <= 0) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    Uint8 alpha = static_cast<Uint8>(m_damageFlash / 0.3f * 80);
    // Red vignette flash
    for (int i = 0; i < 3; i++) {
        SDL_SetRenderDrawColor(renderer, 255, 20, 0, static_cast<Uint8>(alpha / (i + 1)));
        SDL_Rect border = {i * 4, i * 4, screenW - i * 8, screenH - i * 8};
        SDL_RenderDrawRect(renderer, &border);
    }
    // Top/bottom red bars
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, static_cast<Uint8>(alpha * 0.5f));
    SDL_Rect top = {0, 0, screenW, 8};
    SDL_Rect bot = {0, screenH - 8, screenW, 8};
    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bot);
}

void HUD::renderMinimap(SDL_Renderer* renderer, const Level* level,
                         const Player* player, const DimensionManager* dimMgr,
                         int screenW, int screenH,
                         EntityManager* entities) {
    if (!level) return;

    int mapW = 140;
    int mapH = 100;
    int mapX = screenW - mapW - 15;
    int mapY = screenH - mapH - 30;

    // Background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect bg = {mapX - 2, mapY - 2, mapW + 4, mapH + 4};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 100);
    SDL_RenderDrawRect(renderer, &bg);

    // Scale level to minimap
    int levelW = level->getWidth();
    int levelH = level->getHeight();
    float scaleX = static_cast<float>(mapW) / levelW;
    float scaleY = static_cast<float>(mapH) / levelH;
    float scale = std::min(scaleX, scaleY);

    int offsetX = mapX + (mapW - static_cast<int>(levelW * scale)) / 2;
    int offsetY = mapY + (mapH - static_cast<int>(levelH * scale)) / 2;

    int dim = dimMgr ? dimMgr->getCurrentDimension() : 1;

    // Draw tiles (sample every few tiles for performance)
    int step = std::max(1, levelW / 70);
    for (int ty = 0; ty < levelH; ty += step) {
        for (int tx = 0; tx < levelW; tx += step) {
            const auto& tile = level->getTile(tx, ty, dim);
            if (tile.type == TileType::Empty) continue;

            int px = offsetX + static_cast<int>(tx * scale);
            int py = offsetY + static_cast<int>(ty * scale);
            int pw = std::max(1, static_cast<int>(step * scale));
            int ph = std::max(1, static_cast<int>(step * scale));

            if (tile.type == TileType::Solid) {
                SDL_SetRenderDrawColor(renderer, tile.color.r / 2, tile.color.g / 2, tile.color.b / 2, 180);
            } else if (tile.type == TileType::Spike) {
                SDL_SetRenderDrawColor(renderer, 200, 60, 60, 180);
            } else if (tile.type == TileType::OneWay) {
                SDL_SetRenderDrawColor(renderer, 100, 100, 60, 120);
            } else {
                continue;
            }
            SDL_Rect r = {px, py, pw, ph};
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // Draw rift positions
    auto rifts = level->getRiftPositions();
    int tileSize = level->getTileSize();
    for (auto& rp : rifts) {
        int rx = offsetX + static_cast<int>((rp.x / tileSize) * scale);
        int ry = offsetY + static_cast<int>((rp.y / tileSize) * scale);
        float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.006f);
        SDL_SetRenderDrawColor(renderer, 180, 100, 255, static_cast<Uint8>(150 + 80 * pulse));
        SDL_Rect rr = {rx - 2, ry - 2, 4, 4};
        SDL_RenderFillRect(renderer, &rr);
    }

    // Draw exit
    Vec2 exitPt = level->getExitPoint();
    int ex = offsetX + static_cast<int>((exitPt.x / tileSize) * scale);
    int ey = offsetY + static_cast<int>((exitPt.y / tileSize) * scale);
    float ePulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.005f);
    SDL_SetRenderDrawColor(renderer, 100, 255, 100, static_cast<Uint8>(150 + 100 * ePulse));
    SDL_Rect er = {ex - 2, ey - 2, 5, 5};
    SDL_RenderFillRect(renderer, &er);

    // Draw player position
    if (player && player->getEntity()->hasComponent<TransformComponent>()) {
        auto& pt = player->getEntity()->getComponent<TransformComponent>();
        int playerMX = offsetX + static_cast<int>((pt.getCenter().x / tileSize) * scale);
        int playerMY = offsetY + static_cast<int>((pt.getCenter().y / tileSize) * scale);
        // White blinking dot
        Uint32 t = SDL_GetTicks();
        if ((t / 300) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
        }
        SDL_Rect pr = {playerMX - 2, playerMY - 2, 4, 4};
        SDL_RenderFillRect(renderer, &pr);
    }

    // Enemy dots
    if (entities) {
        int tileSize = level->getTileSize();
        entities->forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.hasComponent<TransformComponent>()) return;

            auto& et = e.getComponent<TransformComponent>();
            int emx = offsetX + static_cast<int>((et.getCenter().x / tileSize) * scale);
            int emy = offsetY + static_cast<int>((et.getCenter().y / tileSize) * scale);

            // Boss: larger orange dot, regular enemies: small red dot
            if (e.getTag() == "enemy_boss") {
                float bPulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.008f);
                SDL_SetRenderDrawColor(renderer, 255, 140, 40, static_cast<Uint8>(200 + 55 * bPulse));
                SDL_Rect er = {emx - 3, emy - 3, 6, 6};
                SDL_RenderFillRect(renderer, &er);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 60, 60, 180);
                SDL_Rect er = {emx - 1, emy - 1, 3, 3};
                SDL_RenderFillRect(renderer, &er);
            }
        });
    }
}

void HUD::render(SDL_Renderer* renderer, TTF_Font* font,
                 const Player* player, const SuitEntropy* entropy,
                 const DimensionManager* dimMgr,
                 int screenW, int screenH, int fps, int riftShards) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int margin = 15;
    int barW = 220;
    int barH = 18;

    // Semi-transparent HUD backing panel
    SDL_Rect hudBg = {margin - 5, margin - 5, barW + 20, barH * 4 + 70};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 80);
    SDL_RenderFillRect(renderer, &hudBg);
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 60);
    SDL_RenderDrawRect(renderer, &hudBg);

    // HP Bar
    if (player && player->getEntity()->hasComponent<HealthComponent>()) {
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

        renderBar(renderer, margin, margin, barW, barH, pct, hpColor, {20, 20, 25, 220});

        if (font) {
            char hpText[32];
            std::snprintf(hpText, sizeof(hpText), "HP %.0f/%.0f", hp.currentHP, hp.maxHP);
            renderText(renderer, font, hpText, margin + 5, margin + 2, {255, 255, 255, 220});
        }
    }

    // Entropy Bar
    if (entropy) {
        int ey = margin + barH + 6;
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
            renderText(renderer, font, entText, margin + 5, ey + 2, {255, 255, 255, 220});
        }
    }

    // Dimension indicator
    if (dimMgr) {
        int dimY = margin + (barH + 6) * 2;
        int dim = dimMgr->getCurrentDimension();
        SDL_Color dimColorA = dimMgr->getDimColorA();
        SDL_Color dimColorB = dimMgr->getDimColorB();
        SDL_Color activeColor = (dim == 1) ? dimColorA : dimColorB;

        SDL_Rect dimBg = {margin, dimY, 50, 20};
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 200);
        SDL_RenderFillRect(renderer, &dimBg);

        SDL_Rect dimA = {margin + 1, dimY + 1, 23, 18};
        SDL_Rect dimB = {margin + 26, dimY + 1, 23, 18};
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
            renderText(renderer, font, dimText, margin + 56, dimY + 3,
                       {activeColor.r, activeColor.g, activeColor.b, 255});

            char switchText[32];
            std::snprintf(switchText, sizeof(switchText), "[E] x%d", dimMgr->getSwitchCount());
            renderText(renderer, font, switchText, margin + 110, dimY + 3, {150, 150, 170, 180});
        }
    }

    // Ability bar with cooldown indicators
    if (player && player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = player->getEntity()->getComponent<CombatComponent>();
        int abY = margin + (barH + 6) * 3;
        int iconSize = 22;
        int iconGap = 6;

        struct AbilityInfo {
            float cooldownPct; // 0=on cooldown, 1=ready
            SDL_Color readyColor;
            const char* label;
        };

        // Calculate cooldown percentages
        float meleePct = 1.0f - (combat.cooldownTimer / std::max(0.01f, combat.meleeAttack.cooldown));
        if (!combat.isAttacking || combat.currentAttack != AttackType::Melee) {
            if (combat.cooldownTimer <= 0) meleePct = 1.0f;
        }
        float rangedPct = 1.0f - (combat.cooldownTimer / std::max(0.01f, combat.rangedAttack.cooldown));
        if (!combat.isAttacking || combat.currentAttack != AttackType::Ranged) {
            if (combat.cooldownTimer <= 0) rangedPct = 1.0f;
        }
        float dashPct = 1.0f - (player->dashCooldownTimer / player->dashCooldown);
        if (dashPct > 1.0f) dashPct = 1.0f;
        float dimPct = dimMgr ? 1.0f - (dimMgr->getCooldownTimer() / dimMgr->switchCooldown) : 1.0f;
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
            SDL_Color c = ready ? abilities[i].readyColor : SDL_Color{50, 50, 60, 200};

            // Icon background
            SDL_SetRenderDrawColor(renderer, 15, 15, 25, 180);
            SDL_Rect bg = {ix, abY, iconSize, iconSize};
            SDL_RenderFillRect(renderer, &bg);

            // Draw procedural icon
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
            int cx = ix + iconSize / 2;
            int cy = abY + iconSize / 2;

            switch (i) {
                case 0: // Melee: sword (diagonal line + crossguard)
                    SDL_RenderDrawLine(renderer, cx - 5, cy + 5, cx + 5, cy - 5);
                    SDL_RenderDrawLine(renderer, cx - 5, cy + 4, cx + 5, cy - 6);
                    SDL_RenderDrawLine(renderer, cx - 3, cy - 1, cx + 3, cy + 1);
                    break;
                case 1: // Ranged: circle projectile with trail
                    SDL_RenderDrawLine(renderer, cx - 6, cy, cx - 2, cy);
                    SDL_RenderDrawLine(renderer, cx - 5, cy - 1, cx - 2, cy - 1);
                    {
                        SDL_Rect dot = {cx, cy - 3, 6, 6};
                        SDL_RenderFillRect(renderer, &dot);
                    }
                    break;
                case 2: // Dash: arrow
                    SDL_RenderDrawLine(renderer, cx - 6, cy, cx + 4, cy);
                    SDL_RenderDrawLine(renderer, cx + 4, cy, cx, cy - 4);
                    SDL_RenderDrawLine(renderer, cx + 4, cy, cx, cy + 4);
                    SDL_RenderDrawLine(renderer, cx - 6, cy - 1, cx + 4, cy - 1);
                    break;
                case 3: // Dim switch: two overlapping squares
                    {
                        SDL_Rect sq1 = {cx - 5, cy - 5, 7, 7};
                        SDL_Rect sq2 = {cx - 1, cy - 1, 7, 7};
                        SDL_RenderDrawRect(renderer, &sq1);
                        SDL_RenderDrawRect(renderer, &sq2);
                    }
                    break;
            }

            // Cooldown overlay (dark sweep from top)
            if (!ready) {
                int coverH = static_cast<int>(iconSize * (1.0f - abilities[i].cooldownPct));
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
                SDL_Rect cover = {ix, abY, iconSize, coverH};
                SDL_RenderFillRect(renderer, &cover);
            }

            // Border (brighter when ready)
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, static_cast<Uint8>(ready ? 180 : 60));
            SDL_RenderDrawRect(renderer, &bg);

            // Key label below
            if (font) {
                renderText(renderer, font, abilities[i].label, ix + 4, abY + iconSize + 1,
                           {c.r, c.g, c.b, static_cast<Uint8>(ready ? 200 : 80)});
            }
        }
    }

    // Active buff indicators (below ability bar)
    if (player) {
        int buffY = margin + (barH + 6) * 3 + 36;
        int buffX = margin;
        int buffSize = 16;
        int buffGap = 4;

        auto drawBuff = [&](const char* label, float timer, float maxTime, SDL_Color color) {
            if (timer <= 0) return;
            float pct = timer / maxTime;
            // Background
            SDL_SetRenderDrawColor(renderer, 10, 10, 20, 180);
            SDL_Rect bg = {buffX, buffY, buffSize, buffSize};
            SDL_RenderFillRect(renderer, &bg);
            // Fill based on remaining time
            int fillH = static_cast<int>(buffSize * pct);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 180);
            SDL_Rect fill = {buffX, buffY + buffSize - fillH, buffSize, fillH};
            SDL_RenderFillRect(renderer, &fill);
            // Border
            float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.008f);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, static_cast<Uint8>(120 * pulse));
            SDL_RenderDrawRect(renderer, &bg);
            // Label
            if (font) {
                renderText(renderer, font, label, buffX + 2, buffY + buffSize + 1,
                           {color.r, color.g, color.b, 180});
            }
            buffX += buffSize + buffGap;
        };

        drawBuff("SP", player->speedBoostTimer, 6.0f, {255, 255, 80, 255});
        drawBuff("DM", player->damageBoostTimer, 8.0f, {255, 80, 80, 255});
        if (player->hasShield) {
            drawBuff("SH", player->shieldTimer, 8.0f, {100, 180, 255, 255});
        }
    }

    // Combo counter (center top, only when combo > 1)
    if (player && player->getEntity()->hasComponent<CombatComponent>() && font) {
        auto& combat = player->getEntity()->getComponent<CombatComponent>();
        if (combat.comboCount > 1 && combat.comboTimer > 0) {
            char comboText[32];
            std::snprintf(comboText, sizeof(comboText), "%dx COMBO", combat.comboCount);

            // Pulsing scale effect
            float pulse = 0.8f + 0.2f * std::sin(SDL_GetTicks() * 0.012f);

            // Color shifts from yellow to orange to red based on combo count
            SDL_Color comboColor;
            if (combat.comboCount < 4) {
                comboColor = {255, 220, 50, 255};
            } else if (combat.comboCount < 7) {
                comboColor = {255, 160, 30, 255};
            } else {
                comboColor = {255, 60, 30, 255};
            }

            // Damage bonus text
            char bonusText[32];
            std::snprintf(bonusText, sizeof(bonusText), "+%d%% DMG", static_cast<int>(combat.comboCount * 15));

            int comboY = 60;
            renderText(renderer, font, comboText, screenW / 2 - 40, comboY, comboColor);
            renderText(renderer, font, bonusText, screenW / 2 - 30, comboY + 22,
                       {comboColor.r, comboColor.g, comboColor.b, 180});

            // Combo timer bar (shrinking)
            float timerPct = combat.comboTimer / combat.comboWindow;
            int barW = 80;
            int barX = screenW / 2 - barW / 2;
            SDL_Rect timerBg = {barX, comboY + 42, barW, 4};
            SDL_SetRenderDrawColor(renderer, 40, 40, 50, 150);
            SDL_RenderFillRect(renderer, &timerBg);
            SDL_Rect timerFill = {barX, comboY + 42, static_cast<int>(barW * timerPct), 4};
            SDL_SetRenderDrawColor(renderer, comboColor.r, comboColor.g, comboColor.b, 200);
            SDL_RenderFillRect(renderer, &timerFill);
        }
    }

    // Rift Shards (top right)
    {
        int shardX = screenW - 170;
        int shardY = margin;

        SDL_Rect shardBg = {shardX - 5, shardY - 3, 165, 26};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 80);
        SDL_RenderFillRect(renderer, &shardBg);

        // Diamond shard icon
        int ix = shardX + 6, iy = shardY + 10;
        SDL_SetRenderDrawColor(renderer, 180, 130, 255, 255);
        SDL_RenderDrawLine(renderer, ix, iy - 6, ix + 6, iy);
        SDL_RenderDrawLine(renderer, ix + 6, iy, ix, iy + 6);
        SDL_RenderDrawLine(renderer, ix, iy + 6, ix - 6, iy);
        SDL_RenderDrawLine(renderer, ix - 6, iy, ix, iy - 6);
        SDL_SetRenderDrawColor(renderer, 150, 100, 230, 180);
        SDL_Rect diamondInner = {ix - 3, iy - 3, 6, 6};
        SDL_RenderFillRect(renderer, &diamondInner);

        if (font) {
            char shardText[32];
            std::snprintf(shardText, sizeof(shardText), "%d Shards", riftShards);
            renderText(renderer, font, shardText, shardX + 20, shardY, {200, 170, 255, 255});
        }
    }

    // FPS counter (bottom right, above minimap)
    if (font) {
        char fpsText[16];
        std::snprintf(fpsText, sizeof(fpsText), "%d FPS", fps);
        renderText(renderer, font, fpsText, screenW - 80, screenH - 155, {80, 80, 90, 150});
    }

    // Controls hint (bottom left)
    if (font) {
        renderText(renderer, font,
                   "WASD Move  SPACE Jump  SHIFT Dash  J Melee  K Ranged  E Dimension  F Interact",
                   15, screenH - 22, {60, 60, 80, 100});
    }
}

void HUD::renderBar(SDL_Renderer* renderer, int x, int y, int w, int h,
                     float percent, SDL_Color fillColor, SDL_Color bgColor) {
    SDL_Rect bg = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(renderer, &bg);

    int fillW = static_cast<int>(w * percent);
    if (fillW > 0) {
        SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        SDL_Rect fill = {x, y, fillW, h};
        SDL_RenderFillRect(renderer, &fill);

        Uint8 hiR = static_cast<Uint8>(std::min(255, fillColor.r + 50));
        Uint8 hiG = static_cast<Uint8>(std::min(255, fillColor.g + 50));
        Uint8 hiB = static_cast<Uint8>(std::min(255, fillColor.b + 50));
        SDL_SetRenderDrawColor(renderer, hiR, hiG, hiB, static_cast<Uint8>(fillColor.a * 0.6f));
        SDL_Rect highlight = {x, y, fillW, h / 3};
        SDL_RenderFillRect(renderer, &highlight);

        Uint8 shR = fillColor.r > 40 ? fillColor.r - 40 : 0;
        Uint8 shG = fillColor.g > 40 ? fillColor.g - 40 : 0;
        Uint8 shB = fillColor.b > 40 ? fillColor.b - 40 : 0;
        SDL_SetRenderDrawColor(renderer, shR, shG, shB, static_cast<Uint8>(fillColor.a * 0.4f));
        SDL_Rect shadow = {x, y + h - h / 4, fillW, h / 4};
        SDL_RenderFillRect(renderer, &shadow);
    }

    SDL_SetRenderDrawColor(renderer, 120, 120, 140, 160);
    SDL_RenderDrawRect(renderer, &bg);

    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 80);
    for (int i = 1; i < 4; i++) {
        int nx = x + (w * i) / 4;
        SDL_RenderDrawLine(renderer, nx, y, nx, y + h);
    }
}

void HUD::renderText(SDL_Renderer* renderer, TTF_Font* font,
                      const char* text, int x, int y, SDL_Color color) {
    if (!font || !text) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect rect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}
