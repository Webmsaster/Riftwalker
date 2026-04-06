// PlayStateRenderHUD.cpp -- Split from PlayStateRender.cpp (HUD panels, status displays, combat feedback)
#include "PlayState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
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

void PlayState::renderCollapseWarning(SDL_Renderer* renderer) {
    if (!m_collapsing) return;

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
        std::snprintf(buf, sizeof(buf), LOC("hud.collapse"), secs, tenths);

        // Pulsing red-white text
        Uint8 pulse = static_cast<Uint8>(200 + 55 * std::sin(SDL_GetTicks() * 0.01f));
        SDL_Color tc = {pulse, static_cast<Uint8>(pulse / 4), 0, 255};
        SDL_Surface* ts = TTF_RenderText_Blended(font, buf, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {SCREEN_WIDTH / 2 - ts->w / 2, 140, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }
}

void PlayState::renderRiftProgress(SDL_Renderer* renderer) {
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
                        SDL_Rect rr = {SCREEN_WIDTH / 2 - rs->w / 2, 60, rs->w, rs->h};
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
                    SDL_Rect hr = {SCREEN_WIDTH / 2 - hs->w / 2, 920, hs->w, hs->h};
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
                    SDL_Rect hr = {SCREEN_WIDTH / 2 - hs->w / 2, 964, hs->w, hs->h};
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
                    SDL_Rect r = {SCREEN_WIDTH / 2 - s->w / 2, 1000, s->w, s->h};
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
        float halfW = SCREEN_WIDTH / 2.0f, halfH = SCREEN_HEIGHT / 2.0f;

        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            // Skip already-repaired rifts
            if (m_repairedRiftIndices.count(i)) continue;

            float sx = (rifts[i].x - camPos.x) * m_camera.zoom + halfW;
            float sy = (rifts[i].y - camPos.y) * m_camera.zoom + halfH;

            // Only show if off-screen
            if (sx >= -10 && sx <= SCREEN_WIDTH + 10 && sy >= -10 && sy <= SCREEN_HEIGHT + 10) continue;

            // Clamp to screen edge with margin
            float cx = std::max(25.0f, std::min(static_cast<float>(SCREEN_WIDTH - 25), sx));
            float cy = std::max(25.0f, std::min(static_cast<float>(SCREEN_HEIGHT - 25), sy));

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
        float halfW = SCREEN_WIDTH / 2.0f, halfH = SCREEN_HEIGHT / 2.0f;

        float sx = (exitPos.x - camPos.x) * m_camera.zoom + halfW;
        float sy = (exitPos.y - camPos.y) * m_camera.zoom + halfH;

        // Only show if exit is off-screen
        if (sx < -10 || sx > SCREEN_WIDTH + 10 || sy < -10 || sy > SCREEN_HEIGHT + 10) {
            // Clamp to screen edge with margin
            float cx = std::max(30.0f, std::min(static_cast<float>(SCREEN_WIDTH - 30), sx));
            float cy = std::max(30.0f, std::min(static_cast<float>(SCREEN_HEIGHT - 30), sy));

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
                SDL_Surface* ls = TTF_RenderText_Blended(font, LOC("hud.exit"), labelColor);
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
}

void PlayState::renderDebugOverlay(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || !m_player) return;

    // --- Performance & Gameplay section (always available) ---
    int fps = game ? game->getFPS() : 0;

    // Track average FPS (rolling)
    static float s_fpsAccum = 0;
    static int s_fpsFrames = 0;
    static int s_fpsAvg = 0;
    s_fpsAccum += static_cast<float>(fps);
    s_fpsFrames++;
    if (s_fpsFrames >= 60) {
        s_fpsAvg = static_cast<int>(s_fpsAccum / static_cast<float>(s_fpsFrames));
        s_fpsAccum = 0;
        s_fpsFrames = 0;
    }

    // Entity counts
    int totalEntities = static_cast<int>(m_entities.size());
    int aliveEntities = 0;
    int enemyCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.isAlive()) {
            aliveEntities++;
            auto& tag = e.getTag();
            if (tag.rfind("enemy", 0) == 0) enemyCount++;
        }
    });

    // Particle counts
    int activeParticles = m_particles.getActiveCount();
    int particlePool = m_particles.getPoolSize();

    // Player stats
    auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
    float entropyPct = m_entropy.getPercent() * 100.0f;
    int curDim = m_dimManager.getCurrentDimension();

    // Floor/Zone info
    int floor = m_currentDifficulty;
    int zone = getZone(floor);
    int floorInZone = getFloorInZone(floor);
    bool bossFloor = isBossFloor(floor);

    // Build lines into stack buffer
    struct Line { char text[96]; SDL_Color color; };
    Line lines[28];
    int lineCount = 0;

    auto addLine = [&](SDL_Color c, const char* fmt, ...) {
        if (lineCount >= 28) return;
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
    SDL_Color cDim    = {140, 140, 160, 255};

    // --- Performance block ---
    addLine(cHeader, "=== DEBUG OVERLAY (F3) ===");
    addLine(fps < 30 ? cWarn : fps < 55 ? cNormal : cGreen,
            "FPS: %d  (avg: %d)", fps, s_fpsAvg);
    addLine(cNormal, "Entities: %d alive / %d pool", aliveEntities, totalEntities);
    addLine(enemyCount > 0 ? cNormal : cDim,
            "Enemies: %d active", enemyCount);
    addLine(activeParticles > 1500 ? cWarn : cNormal,
            "Particles: %d / %d pool", activeParticles, particlePool);

    // --- Gameplay block ---
    addLine(cHeader, "--- Gameplay ---");
    addLine(bossFloor ? cWarn : cNormal,
            "Floor %d  (Zone %d-%d)%s", floor, zone + 1, floorInZone,
            bossFloor ? "  BOSS" : "");
    addLine(playerHP.getPercent() < 0.3f ? cWarn : cGreen,
            "HP: %.0f / %.0f (%.0f%%)", playerHP.currentHP, playerHP.maxHP,
            playerHP.getPercent() * 100.0f);
    addLine(m_entropy.isCritical() ? cWarn : entropyPct > 70.0f ? cNormal : cGreen,
            "Entropy: %.0f%%", entropyPct);
    addLine(cNormal, "Dim: %d | Run: %.0fs | Kills: %d",
            curDim, m_runTime, enemiesKilled);

    // --- Relic Safety Rails (only if relics available) ---
    if (m_player->getEntity()->hasComponent<RelicComponent>()) {
        auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
        float hpPct = playerHP.getPercent();

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
        int residueCount = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "dim_residue" && e.isAlive()) residueCount++;
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

        addLine(cHeader, "--- Relic Rails ---");
        addLine(gameplayPaused ? cWarn : cGreen, "Paused: %s", gameplayPaused ? "YES" : "NO");
        addLine(rawDmg > clampedDmg + 0.001f ? cWarn : cNormal,
                "DMG: %.2fx raw -> %.2fx clamp", rawDmg, clampedDmg);
        addLine(rawSpd > clampedSpd + 0.001f ? cWarn : cNormal,
                "SPD: %.2fx raw -> %.2fx clamp", rawSpd, clampedSpd);
        addLine(dmgTakenMult > 1.001f ? cWarn : dmgTakenMult < 0.999f ? cGreen : cNormal,
                "DmgTaken: %.2fx", dmgTakenMult);
        if (cdFloored)
            addLine(cWarn, "SwitchCD: %.2fs x%.2f = %.2fs FLOOR", baseCD, cdMult, finalCD);
        else
            addLine(cNormal, "SwitchCD: %.2fs x%.2f = %.2fs", baseCD, cdMult, finalCD);
        addLine(cNormal, "VoidHunger: %.0f%%/40%% | Res: %d/%d",
                relics.voidHungerBonus * 100.0f, residueCount, RelicSystem::getMaxResidueZones());
        addLine(cNormal, "Stab: %.1fs (%.0f%%) | Cond: %d stk",
                relics.stabilityTimer, stabPct, relics.riftConduitStacks);
        addLine(cNormal, "Synergies: %d | Seed: %d", synergyCount, m_runSeed);
    }

    addLine(cDim, "F4: snapshot to balance_snapshots.csv");

    // Render panel
    int panelX = 20, panelY = 120;
    int lineH = 32, padX = 16, padY = 8;
    int panelW = 760;
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
    int barW = 800;
    int barH = 24;
    int barX = SCREEN_WIDTH / 2 - barW / 2;
    int barY = 40;

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
    if (bt == 5) {
        // Entropy Incarnate: sickly green/purple tones
        switch (bossPhase) {
            case 1: r = 60; g = 180; b = 60; break;
            case 2: r = 100; g = 200; b = 40; break;
            case 3: r = 140; g = 60; b = 180; break;
            default: r = 60; g = 180; b = 60; break;
        }
    } else if (bt == 4) {
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
        const char* bossName = (bt == 5) ? LOC("boss.entropy_incarnate") : (bt == 4) ? LOC("boss.void_sovereign") : (bt == 3) ? LOC("boss.temporal_weaver") : (bt == 2) ? LOC("boss.dim_architect") : (bt == 1) ? LOC("boss.void_wyrm") : LOC("boss.rift_guardian");
        SDL_Color tc = (bt == 5) ? SDL_Color{80, 220, 80, 220} : (bt == 4) ? SDL_Color{180, 80, 255, 220} : (bt == 3) ? SDL_Color{220, 190, 100, 220} : (bt == 2) ? SDL_Color{160, 180, 255, 220} : (bt == 1) ? SDL_Color{180, 255, 200, 220} : SDL_Color{220, 180, 255, 220};
        SDL_Surface* ts = TTF_RenderText_Blended(font, bossName, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {SCREEN_WIDTH / 2 - ts->w / 2, barY + barH + 8, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
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

void PlayState::renderKillFeed(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    int x = SCREEN_WIDTH - 40;
    int y = SCREEN_HEIGHT - 280;

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
        float alpha = std::min(display[i].timer / 0.5f, 1.0f);
        // Slide-in animation: new entries slide from right
        float slideProgress = std::min(display[i].timer / 0.15f, 1.0f);
        float eased = 1.0f - (1.0f - slideProgress) * (1.0f - slideProgress); // ease-out
        int slideOffset = static_cast<int>((1.0f - eased) * 120);

        SDL_Color c = display[i].color;
        Uint8 a = static_cast<Uint8>(c.a * alpha);

        SDL_Surface* surf = TTF_RenderText_Blended(font, display[i].text, {c.r, c.g, c.b, 255});
        if (surf) {
            int entryX = x - surf->w + slideOffset;
            int entryW = surf->w + 32;

            // Background panel with gradient
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(70 * alpha));
            SDL_Rect bgRect = {entryX - 20, y - 2, entryW + 20, surf->h + 4};
            SDL_RenderFillRect(renderer, &bgRect);
            // Left accent bar (colored)
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, static_cast<Uint8>(180 * alpha));
            SDL_Rect accentBar = {entryX - 24, y - 2, 4, surf->h + 4};
            SDL_RenderFillRect(renderer, &accentBar);

            // Kill dot (small colored circle)
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, a);
            SDL_Rect dot = {entryX - 16, y + surf->h / 2 - 4, 8, 8};
            SDL_RenderFillRect(renderer, &dot);

            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect r = {entryX, y, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
        y += 40;
    }
}

void PlayState::renderDamageNumbers(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    for (auto& dn : m_damageNumbers) {
        float t = dn.lifetime / dn.maxLifetime; // 1.0 -> 0.0
        Uint8 alpha = static_cast<Uint8>(255 * t);

        // Lazy-init: create and cache textures on first render frame
        if (!dn.cachedText) {
            // Determine base color (full alpha — modulated per frame)
            SDL_Color color;
            if (dn.isShard) {
                color = {200, 150, 255, 255};
            } else if (dn.isBuff && dn.buffText && std::strcmp(dn.buffText, "PARRY!") == 0) {
                color = {255, 225, 80, 255};
            } else if (dn.isBuff) {
                color = {100, 220, 255, 255};
            } else if (dn.isHeal) {
                color = {50, 255, 80, 255};
            } else if (dn.isPlayerDamage) {
                color = {255, 60, 40, 255};
            } else if (dn.isCritical) {
                color = {255, 215, 50, 255};
            } else {
                color = {255, 240, 100, 255};
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

            // Compute base scale once
            if (dn.isBuff && dn.buffText && std::strcmp(dn.buffText, "PARRY!") == 0) {
                dn.baseScale = 1.8f;
            } else if (dn.isCritical) {
                dn.baseScale = 1.8f;
            } else if (dn.value > 20) {
                dn.baseScale = 1.3f;
            }

            // Create and cache text texture
            SDL_Surface* surface = TTF_RenderText_Blended(font, buf, color);
            if (surface) {
                dn.texW = surface->w;
                dn.texH = surface->h;
                dn.cachedText = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
            }
            // Create and cache shadow texture
            SDL_Color black = {0, 0, 0, 255};
            SDL_Surface* shadowSurf = TTF_RenderText_Blended(font, buf, black);
            if (shadowSurf) {
                dn.cachedShadow = SDL_CreateTextureFromSurface(renderer, shadowSurf);
                SDL_FreeSurface(shadowSurf);
            }
        }

        if (!dn.cachedText) continue;

        // Convert world position to screen position
        SDL_FRect worldRect = {dn.position.x - 10, dn.position.y - 8, 20, 16};
        SDL_Rect screenRect = m_camera.worldToScreen(worldRect);

        // Pop-in scale animation: starts large, settles to target scale
        float lifeProgress = 1.0f - t;
        float popScale = 1.0f;
        if (lifeProgress < 0.15f) {
            float popT = lifeProgress / 0.15f;
            popScale = 1.6f - 0.6f * popT;
        }
        float finalScale = dn.baseScale * popScale;

        int sw = static_cast<int>(dn.texW * finalScale);
        int sh = static_cast<int>(dn.texH * finalScale);
        int dx = screenRect.x - sw / 2;
        int dy = screenRect.y;

        // Shadow outline (8 offset copies) — reuse cached shadow texture
        if (dn.cachedShadow) {
            SDL_SetTextureAlphaMod(dn.cachedShadow, static_cast<Uint8>(alpha * 0.7f));
            for (int ox = -1; ox <= 1; ox++) {
                for (int oy = -1; oy <= 1; oy++) {
                    if (ox == 0 && oy == 0) continue;
                    SDL_Rect sdst = {dx + ox, dy + oy, sw, sh};
                    SDL_RenderCopy(renderer, dn.cachedShadow, nullptr, &sdst);
                }
            }
        }

        // Main text — reuse cached text texture
        SDL_SetTextureAlphaMod(dn.cachedText, alpha);
        SDL_Rect dst = {dx, dy, sw, sh};
        SDL_RenderCopy(renderer, dn.cachedText, nullptr, &dst);
    }
}

// --- NPC Quest HUD (bottom-right corner, above kill feed) ---

void PlayState::renderQuestHUD(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Show active quest progress
    if (m_activeQuest.active) {
        char buf[128];
        if (m_activeQuest.targetKills > 0) {
            int cur = std::min(m_activeQuest.currentKills, m_activeQuest.targetKills);
            std::snprintf(buf, sizeof(buf), "Quest: %s (%d/%d)",
                          m_activeQuest.description.c_str(), cur, m_activeQuest.targetKills);
        } else if (m_activeQuest.targetRifts > 0) {
            int cur = std::min(m_activeQuest.currentRifts, m_activeQuest.targetRifts);
            std::snprintf(buf, sizeof(buf), "Quest: %s (%d/%d)",
                          m_activeQuest.description.c_str(), cur, m_activeQuest.targetRifts);
        } else {
            return;
        }

        // Background panel
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        int textW = static_cast<int>(std::strlen(buf) * 7.5f); // Approximate text width
        int panelX = SCREEN_WIDTH - textW - 30;
        int panelY = SCREEN_HEIGHT - 85;
        SDL_Rect bg = {panelX - 8, panelY - 4, textW + 16, 24};
        SDL_SetRenderDrawColor(renderer, 15, 12, 30, 180);
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 100, 180, 255, 150);
        SDL_RenderDrawRect(renderer, &bg);

        // Text
        SDL_Color questColor = {140, 200, 255, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, buf, questColor);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {panelX, panelY, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Show quest completion notification
    if (m_questCompleteTimer > 0 && m_activeQuest.completed) {
        float alpha = std::min(m_questCompleteTimer / 0.5f, 1.0f); // Fade out in last 0.5s
        Uint8 a = static_cast<Uint8>(alpha * 255);

        char buf[128];
        std::snprintf(buf, sizeof(buf), "Quest Complete! +%d Shards", m_activeQuest.shardReward);

        int textW = static_cast<int>(std::strlen(buf) * 7.5f);
        int panelX = SCREEN_WIDTH / 2 - textW / 2;
        int panelY = SCREEN_HEIGHT / 3;

        // Golden background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect bg = {panelX - 12, panelY - 6, textW + 24, 30};
        SDL_SetRenderDrawColor(renderer, 30, 25, 10, static_cast<Uint8>(alpha * 200));
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 255, 215, 80, static_cast<Uint8>(alpha * 200));
        SDL_RenderDrawRect(renderer, &bg);

        SDL_Color gold = {255, 215, 80, a};
        SDL_Surface* s = TTF_RenderText_Blended(font, buf, gold);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_SetTextureAlphaMod(t, a);
                SDL_Rect r = {panelX, panelY, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }
}

void PlayState::renderWeaponSwitchDisplay(SDL_Renderer* renderer, TTF_Font* font) {
    if (m_weaponSwitchDisplayTimer <= 0 || !font || !m_player) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 center = pt.getCenter();
    Vec2 camPos = m_camera.getPosition();

    int screenX = static_cast<int>(center.x - camPos.x);
    int screenY = static_cast<int>(center.y - camPos.y) + 32; // Below player sprite

    // Fade out during the last 0.5s
    float alpha = 1.0f;
    if (m_weaponSwitchDisplayTimer < 0.5f) {
        alpha = m_weaponSwitchDisplayTimer / 0.5f;
    }
    // Slight upward float over time
    float elapsed = 1.5f - m_weaponSwitchDisplayTimer;
    screenY -= static_cast<int>(elapsed * 12.0f);

    Uint8 a = static_cast<Uint8>(alpha * 230);

    // Weapon name color: orange for melee, cyan for ranged
    SDL_Color textColor = m_weaponSwitchIsMelee
        ? SDL_Color{255, 200, 80, a}
        : SDL_Color{80, 200, 255, a};

    SDL_Surface* surf = TTF_RenderText_Blended(font, m_weaponSwitchName.c_str(), textColor);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            SDL_SetTextureAlphaMod(tex, a);
            SDL_Rect dst = {screenX - surf->w / 2, screenY, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }

    // Small category label ("MELEE" / "RANGED") above the weapon name
    const char* category = m_weaponSwitchIsMelee ? "MELEE" : "RANGED";
    Uint8 catAlpha = static_cast<Uint8>(alpha * 140);
    SDL_Color catColor = {200, 200, 200, catAlpha};
    SDL_Surface* catSurf = TTF_RenderText_Blended(font, category, catColor);
    if (catSurf) {
        SDL_Texture* catTex = SDL_CreateTextureFromSurface(renderer, catSurf);
        if (catTex) {
            SDL_SetTextureAlphaMod(catTex, catAlpha);
            // Render at half scale for a smaller label
            int catW = catSurf->w / 2;
            int catH = catSurf->h / 2;
            SDL_Rect catDst = {screenX - catW / 2, screenY - catH - 2, catW, catH};
            SDL_RenderCopy(renderer, catTex, nullptr, &catDst);
            SDL_DestroyTexture(catTex);
        }
        SDL_FreeSurface(catSurf);
    }
}
