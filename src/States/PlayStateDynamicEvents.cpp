// PlayStateDynamicEvents.cpp -- Dynamic level events (dimension storm, elite invasion, time dilation)
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

void PlayState::triggerDynamicEvent() {
    // Pick a random event type
    auto type = static_cast<DynamicEventType>(std::rand() % static_cast<int>(DynamicEventType::COUNT));

    m_dynamicEvent = {};
    m_dynamicEvent.type = type;
    m_dynamicEvent.active = true;

    switch (type) {
        case DynamicEventType::DimensionStorm:
            m_dynamicEvent.duration = 15.0f;
            m_dynamicEvent.timer = 15.0f;
            m_dynamicEvent.effectTimer = 0;
            m_dynamicEvent.name = LOC("event.dim_storm");
            m_dynamicEvent.color = {180, 80, 255, 255}; // purple
            break;
        case DynamicEventType::EliteInvasion:
            m_dynamicEvent.duration = 0; // one-shot
            m_dynamicEvent.timer = 0;
            m_dynamicEvent.name = LOC("event.elite_invasion");
            m_dynamicEvent.color = {255, 60, 60, 255}; // red
            break;
        case DynamicEventType::TimeDilation:
            m_dynamicEvent.duration = 10.0f;
            m_dynamicEvent.timer = 10.0f;
            m_dynamicEvent.effectTimer = 0;
            m_dynamicEvent.name = LOC("event.time_dilation");
            m_dynamicEvent.color = {255, 215, 0, 255}; // golden
            break;
        default:
            m_dynamicEvent.active = false;
            return;
    }

    m_dynamicEventAnnouncementTimer = 2.5f;
    m_dynamicEventsTriggered++;

    // Announcement SFX + camera shake
    AudioManager::instance().play(SFX::CollapseWarning);
    m_camera.shake(8.0f, 0.3f);

    // Announcement particles at player position
    if (m_player) {
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_particles.burst(pPos, 25, m_dynamicEvent.color, 200.0f, 4.0f);
    }

    // Elite Invasion: one-shot spawn logic
    if (type == DynamicEventType::EliteInvasion) {
        if (m_player && m_level) {
            Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            int tileSize = m_level->getTileSize();
            int curDim = m_dimManager.getCurrentDimension();

            // Spawn 2-3 elite enemies at walkable positions near the player
            int spawnCount = 2 + std::rand() % 2;
            int spawned = 0;
            int attempts = 0;

            // Zone-based enemy type cap for dynamic events
            int dynZone = getZone(m_currentDifficulty);
            int maxType = std::min(static_cast<int>(EnemyType::Leech),
                                   std::max(2, dynZone * 3 + 2));
            // Advanced types (Teleporter, Reflector, Leech) require Zone 2+
            if (dynZone < 2) maxType = std::min(maxType, static_cast<int>(EnemyType::Sniper));

            while (spawned < spawnCount && attempts < 40) {
                attempts++;

                // Random offset 100-300px from player
                float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
                float dist = 100.0f + static_cast<float>(std::rand() % 200);
                float spawnX = playerPos.x + std::cos(angle) * dist;
                float spawnY = playerPos.y + std::sin(angle) * dist;

                // Check tile is walkable (not solid)
                int tileX = static_cast<int>(spawnX) / tileSize;
                int tileY = static_cast<int>(spawnY) / tileSize;

                // Ensure tile below is solid (enemy stands on something)
                // and current tile is not solid (enemy has room)
                if (m_level->isSolid(tileX, tileY, curDim)) continue;
                if (!m_level->isSolid(tileX, tileY + 1, curDim)) continue;

                // Pick a random enemy type (difficulty-capped)
                int enemyType = std::rand() % (maxType + 1);
                Vec2 spawnPos = {spawnX, spawnY - static_cast<float>(tileSize) / 2.0f};

                auto& e = Enemy::createByType(m_entities, enemyType, spawnPos, 0);

                // Make elite with random modifier
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 9);
                Enemy::makeElite(e, mod);

                // Apply theme variant if applicable
                applyThemeVariant(e, 0);

                // NG+ scaling
                applyNGPlusModifiers(e);

                // Set spawn animation timer
                if (e.hasComponent<AIComponent>()) {
                    e.getComponent<AIComponent>().spawnTimer = 0.4f;
                    e.getComponent<AIComponent>().spawnTimerInitial = 0.4f;
                }

                // Flash particles at spawn location
                m_particles.burst(spawnPos, 15, {255, 255, 200, 255}, 150.0f, 3.0f);

                spawned++;
            }
        }
        // Elite Invasion is one-shot: end immediately after spawning
        m_dynamicEvent.active = false;
    }
}

void PlayState::updateDynamicEvent(float dt) {
    if (!m_dynamicEventsEnabled) return;
    if (m_playerDying || m_levelComplete) return;

    // Announcement timer (visual display only)
    if (m_dynamicEventAnnouncementTimer > 0) {
        m_dynamicEventAnnouncementTimer -= dt;
    }

    // If an event is active, run its per-frame logic
    if (m_dynamicEvent.active) {
        m_dynamicEvent.timer -= dt;
        m_dynamicEvent.effectTimer += dt;

        switch (m_dynamicEvent.type) {
            case DynamicEventType::DimensionStorm: {
                // Force dimension switch every 2 seconds
                float interval = 2.0f;
                int prevSwitchCount = static_cast<int>((m_dynamicEvent.effectTimer - dt) / interval);
                int curSwitchCount = static_cast<int>(m_dynamicEvent.effectTimer / interval);
                if (curSwitchCount > prevSwitchCount) {
                    m_dimManager.switchDimension(true); // force=true bypasses cooldown
                    m_dimensionSwitches++;
                    AudioManager::instance().play(SFX::DimensionSwitch);
                    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
                    m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
                    m_screenEffects.triggerDimensionRipple();

                    // Relic dim-switch effects
                    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
                        auto& relicComp = m_player->getEntity()->getComponent<RelicComponent>();
                        auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                        relicComp.lastSwitchDimension = m_dimManager.getCurrentDimension();
                        RelicSystem::onDimensionSwitch(relicComp, &playerHP);
                    }

                    // Entropy from forced switch
                    m_entropy.onDimensionSwitch();
                }

                // Purple lightning particles each frame
                if (m_player) {
                    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                    float offsetX = static_cast<float>((std::rand() % 200) - 100);
                    float offsetY = static_cast<float>((std::rand() % 120) - 60);
                    SDL_Color purple = {160, 60, 255, 180};
                    m_particles.burst({pPos.x + offsetX, pPos.y + offsetY}, 1, purple, 80.0f, 1.0f);
                    // Occasional bright flash
                    if (std::rand() % 10 == 0) {
                        SDL_Color white = {220, 180, 255, 220};
                        m_particles.burst({pPos.x + offsetX * 0.5f, pPos.y + offsetY * 0.5f},
                                          3, white, 120.0f, 1.5f);
                    }
                }

                if (m_dynamicEvent.timer <= 0) {
                    endDynamicEvent();
                }
                break;
            }

            case DynamicEventType::TimeDilation: {
                // Golden time-distortion particles each frame
                if (m_player) {
                    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                    float offsetX = static_cast<float>((std::rand() % 300) - 150);
                    float offsetY = static_cast<float>((std::rand() % 160) - 80);
                    SDL_Color gold = {255, 215, 0, 140};
                    m_particles.burst({pPos.x + offsetX, pPos.y + offsetY}, 1, gold, 40.0f, 1.5f);
                    // Subtle clock-like spinning particles
                    if (std::rand() % 15 == 0) {
                        SDL_Color brightGold = {255, 240, 140, 200};
                        m_particles.burst({pPos.x + offsetX * 0.3f, pPos.y + offsetY * 0.3f},
                                          2, brightGold, 60.0f, 2.0f);
                    }
                }

                if (m_dynamicEvent.timer <= 0) {
                    endDynamicEvent();
                }
                break;
            }

            default:
                break;
        }
        return; // Don't tick cooldown while event is active
    }

    // Cooldown management: tick down and trigger when ready
    m_dynamicEventCooldown -= dt;
    if (m_dynamicEventCooldown <= 0) {
        triggerDynamicEvent();
    }
}

void PlayState::endDynamicEvent() {
    m_dynamicEvent.active = false;

    // End-burst particles at player position
    if (m_player) {
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_particles.burst(pPos, 20, m_dynamicEvent.color, 180.0f, 3.0f);
        m_particles.burst(pPos, 10, {255, 255, 255, 200}, 120.0f, 2.0f);
    }

    // Set new cooldown: 30-60 seconds
    m_dynamicEventCooldown = 30.0f + static_cast<float>(std::rand() % 31);
}

void PlayState::renderDynamicEventOverlay(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Announcement text (slides in from top, fades out)
    if (m_dynamicEventAnnouncementTimer > 0) {
        float t = m_dynamicEventAnnouncementTimer / 2.5f; // 1.0 at start -> 0.0 at end

        // Slide-in from top (first 0.3s) then hold, then fade out (last 0.5s)
        float slideProgress = std::min(1.0f, (1.0f - t) / 0.12f); // 0->1 in first 12%
        float alpha;
        if (t > 0.88f) alpha = (1.0f - t) / 0.12f; // fade in
        else if (t < 0.20f) alpha = t / 0.20f;       // fade out
        else alpha = 1.0f;                             // hold

        Uint8 a = static_cast<Uint8>(std::min(255.0f, alpha * 255.0f));
        if (a < 5) return;

        // Vertical slide: starts 40px above, settles at center-top
        int slideOffset = static_cast<int>((1.0f - slideProgress) * -40.0f);
        int baseY = SCREEN_HEIGHT / 4 + slideOffset;

        // Background bar
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, m_dynamicEvent.color.r / 4,
                               m_dynamicEvent.color.g / 4,
                               m_dynamicEvent.color.b / 4,
                               static_cast<Uint8>(a * 0.6f));
        SDL_Rect bgBar = {SCREEN_WIDTH / 2 - 720, baseY - 12, 1440, 80};
        SDL_RenderFillRect(renderer, &bgBar);

        // Event-colored border
        SDL_SetRenderDrawColor(renderer, m_dynamicEvent.color.r,
                               m_dynamicEvent.color.g,
                               m_dynamicEvent.color.b, a);
        SDL_RenderDrawRect(renderer, &bgBar);

        // Event name text
        SDL_Color textColor = {m_dynamicEvent.color.r, m_dynamicEvent.color.g,
                               m_dynamicEvent.color.b, a};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, m_dynamicEvent.name, textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                float scale = 1.8f;
                int w = static_cast<int>(surf->w * scale);
                int h = static_cast<int>(surf->h * scale);
                SDL_Rect dst = {SCREEN_WIDTH / 2 - w / 2, baseY + 20 / 2 - h / 2, w, h};
                SDL_SetTextureAlphaMod(tex, a);
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Active event indicator bar (small bar at top of screen showing timer)
    if (m_dynamicEvent.active && m_dynamicEvent.duration > 0) {
        float pct = m_dynamicEvent.timer / m_dynamicEvent.duration;
        int barW = 200;
        int barH = 6;
        int barX = SCREEN_WIDTH / 2 - barW / 2;
        int barY = 4;

        // Background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 160);
        SDL_Rect bgRect = {barX - 2, barY - 2, barW + 4, barH + 4};
        SDL_RenderFillRect(renderer, &bgRect);

        // Fill (event-colored, shrinks as timer depletes)
        int fillW = static_cast<int>(barW * pct);
        SDL_SetRenderDrawColor(renderer, m_dynamicEvent.color.r,
                               m_dynamicEvent.color.g,
                               m_dynamicEvent.color.b, 220);
        SDL_Rect fillRect = {barX, barY, fillW, barH};
        SDL_RenderFillRect(renderer, &fillRect);

        // Label text (small, above the bar)
        SDL_Color labelColor = {m_dynamicEvent.color.r, m_dynamicEvent.color.g,
                                m_dynamicEvent.color.b, 200};
        SDL_Surface* labelSurf = TTF_RenderUTF8_Blended(font, m_dynamicEvent.name, labelColor);
        if (labelSurf) {
            SDL_Texture* labelTex = SDL_CreateTextureFromSurface(renderer, labelSurf);
            if (labelTex) {
                SDL_Rect labelDst = {SCREEN_WIDTH / 2 - labelSurf->w / 2,
                                     barY + barH + 2,
                                     labelSurf->w, labelSurf->h};
                SDL_SetTextureAlphaMod(labelTex, 200);
                SDL_RenderCopy(renderer, labelTex, nullptr, &labelDst);
                SDL_DestroyTexture(labelTex);
            }
            SDL_FreeSurface(labelSurf);
        }
    }
}
