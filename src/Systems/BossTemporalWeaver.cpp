#include "BossAI.h"

void AISystem::updateTemporalWeaver(Entity& entity, float dt, const Vec2& playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& t = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    auto& hp = entity.getComponent<HealthComponent>();
    Vec2 center = t.getCenter();

    // Phase determination based on HP
    float hpPct = hp.getPercent();
    bool extraPhase = (AscensionSystem::currentLevel > 0 &&
        AscensionSystem::getLevel(AscensionSystem::currentLevel).bossExtraPhase);
    if (extraPhase && hpPct <= 0.15f) ai.bossPhase = 4;
    else if (hpPct > 0.66f) ai.bossPhase = 1;
    else if (hpPct > 0.33f) ai.bossPhase = 2;
    else ai.bossPhase = 3;

    // Hover motion (sine wave)
    ai.twHoverY += dt;
    float hoverOffset = std::sin(ai.twHoverY * 2.0f) * 15.0f;
    phys.velocity.y = hoverOffset * 3.0f;

    // Face player
    ai.facingRight = playerPos.x > center.x;

    // Move toward preferred range
    float dist = distanceTo(center, playerPos);
    float preferredDist = 200.0f;
    if (dist > preferredDist + 50.0f) {
        float dx = playerPos.x - center.x;
        phys.velocity.x = (dx > 0 ? 1.0f : -1.0f) * ai.chaseSpeed;
    } else if (dist < preferredDist - 50.0f) {
        float dx = playerPos.x - center.x;
        phys.velocity.x = (dx > 0 ? -1.0f : 1.0f) * ai.chaseSpeed * 0.7f;
    } else {
        phys.velocity.x *= 0.9f;
    }

    // Attack pattern timer
    ai.bossAttackTimer += dt;
    float twTelegraphTime = 0.5f - (ai.bossPhase - 1) * 0.1f; // 0.5, 0.4, 0.3

    // --- Temporal Weaver Telegraph Processing ---
    if (ai.bossTelegraphTimer > 0) {
        ai.bossTelegraphTimer -= dt;
        phys.velocity.x *= 0.8f;
        phys.velocity.y *= 0.8f;

        float telegraphMax = std::max(0.1f, 0.5f - (ai.bossPhase - 1) * 0.1f);
        float flashRate = 15.0f + (1.0f - ai.bossTelegraphTimer / telegraphMax) * 20.0f;
        float flash = std::sin(ai.bossTelegraphTimer * flashRate) * 0.5f + 0.5f;

        if (entity.hasComponent<SpriteComponent>()) {
            auto& sprite = entity.getComponent<SpriteComponent>();
            switch (ai.bossTelegraphAttack) {
                case 1: sprite.setColor(static_cast<Uint8>(220 + 35 * flash), static_cast<Uint8>(180 + 40 * flash), static_cast<Uint8>(40 * flash)); break; // sweep: golden
                case 2: sprite.setColor(static_cast<Uint8>(80 * flash), static_cast<Uint8>(180 + 75 * flash), 255); break; // rewind: cyan
                case 3: sprite.setColor(static_cast<Uint8>(220 + 35 * flash), static_cast<Uint8>(100 * flash), static_cast<Uint8>(50 * flash)); break; // time stop: red-gold
                default: break;
            }
        }

        if (m_particles) {
            SDL_Color warnColor;
            switch (ai.bossTelegraphAttack) {
                case 1: warnColor = {255, 200, 60, 200}; break;
                case 2: warnColor = {80, 200, 255, 200}; break;
                case 3: warnColor = {255, 120, 50, 200}; break;
                default: warnColor = {255, 255, 255, 200};
            }
            for (int i = 0; i < 2; i++) {
                float angle = (float)(rand() % 628) / 100.0f;
                Vec2 offset = {std::cos(angle) * 35.0f, std::sin(angle) * 35.0f};
                m_particles->burst(center + offset, 1, warnColor, 60.0f, 3.0f);
            }
        }

        if (ai.bossTelegraphTimer <= 0) {
            switch (ai.bossTelegraphAttack) {
                case 1: {
                    // Clock Hand Sweep - execute
                    if (m_combatSystem) {
                        Vec2 dir = ai.bossTelegraphDir;
                        int shotCount = ai.bossPhase >= 3 ? 7 : (ai.bossPhase >= 2 ? 5 : 3);
                        float spread = 0.4f;
                        for (int i = 0; i < shotCount; i++) {
                            float angleOff = (i - shotCount / 2.0f + 0.5f) * spread;
                            Vec2 shotDir = {
                                dir.x * std::cos(angleOff) - dir.y * std::sin(angleOff),
                                dir.x * std::sin(angleOff) + dir.y * std::cos(angleOff)
                            };
                            m_combatSystem->createProjectile(entities, center, shotDir,
                                entity.getComponent<CombatComponent>().rangedAttack.damage, 300.0f, entity.dimension);
                        }
                    }
                    AudioManager::instance().play(SFX::BossMultiShot);
                    if (m_particles) {
                        m_particles->burst(center, 20, {255, 200, 80, 255}, 180.0f, 3.0f);
                    }
                    ai.twSweepTimer = (ai.bossPhase >= 2) ? 4.0f : 6.0f;
                    break;
                }
                case 2: {
                    // Time Rewind - execute
                    float healAmount = hp.maxHP * 0.08f;
                    hp.currentHP = std::min(hp.maxHP, hp.currentHP + healAmount);
                    AudioManager::instance().play(SFX::RiftShieldActivate);
                    if (m_particles) {
                        m_particles->burst(center, 25, {100, 200, 255, 255}, 200.0f, 4.0f);
                    }
                    if (m_camera) m_camera->shake(6.0f, 0.3f);
                    ai.twRewindTimer = 15.0f;
                    break;
                }
                case 3: {
                    // Time Stop - execute
                    if (m_combatSystem) {
                        for (int i = 0; i < 12; i++) {
                            float angle = i * 6.283185f / 12.0f;
                            Vec2 dir = {std::cos(angle), std::sin(angle)};
                            m_combatSystem->createProjectile(entities, center, dir,
                                entity.getComponent<CombatComponent>().rangedAttack.damage * 1.5f,
                                200.0f, entity.dimension);
                        }
                    }
                    AudioManager::instance().play(SFX::BossShieldBurst);
                    if (m_particles) {
                        m_particles->burst(center, 40, {255, 200, 50, 255}, 300.0f, 5.0f);
                    }
                    if (m_camera) m_camera->shake(12.0f, 0.4f);
                    ai.twStopTimer = 8.0f;
                    break;
                }
                default: break;
            }
            ai.bossTelegraphAttack = -1;
        }
        return; // Skip normal AI during telegraph
    }

    // Phase 1: Time Slow Zones (instant, no telegraph) + Clock Hand Sweep + Chrono Shards
    ai.twSlowZoneTimer -= dt;
    ai.twSweepTimer -= dt;

    if (ai.twSlowZoneTimer <= 0) {
        ai.twSlowZoneTimer = (ai.bossPhase >= 3) ? 3.0f : 5.0f;
        if (m_combatSystem) {
            Vec2 zonePos = playerPos;
            for (int i = 0; i < 4; i++) {
                float angle = i * 6.283185f / 4.0f;
                Vec2 dir = {std::cos(angle), std::sin(angle)};
                m_combatSystem->createProjectile(entities, zonePos, dir,
                    ai.bossPhase >= 2 ? 12.0f : 8.0f, 150.0f, entity.dimension);
            }
        }
        if (m_particles) {
            m_particles->burst(playerPos, 15, {180, 160, 80, 200}, 100.0f, 3.0f);
        }
    }

    // Clock Hand Sweep (telegraphed)
    if (ai.twSweepTimer <= 0 && ai.bossTelegraphAttack < 0) {
        Vec2 dir = {playerPos.x - center.x, playerPos.y - center.y};
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 0) dir = dir * (1.0f / len);
        ai.bossTelegraphTimer = twTelegraphTime;
        ai.bossTelegraphAttack = 1;
        ai.bossTelegraphDir = dir;
        AudioManager::instance().play(SFX::SniperTelegraph);
    }

    // Phase 2+: Time Rewind (telegraphed)
    if (ai.bossPhase >= 2) {
        ai.twRewindTimer -= dt;
        if (ai.twRewindTimer <= 0 && ai.bossTelegraphAttack < 0) {
            ai.bossTelegraphTimer = twTelegraphTime;
            ai.bossTelegraphAttack = 2;
            AudioManager::instance().play(SFX::CollapseWarning);
        }
    }

    // Phase 3: Time Stop (telegraphed)
    if (ai.bossPhase >= 3) {
        ai.twStopTimer -= dt;
        if (ai.twStopTimer <= 0 && ai.bossTelegraphAttack < 0) {
            ai.bossTelegraphTimer = twTelegraphTime;
            ai.bossTelegraphAttack = 3;
            AudioManager::instance().play(SFX::CollapseWarning);
            if (m_camera) m_camera->shake(4.0f, 0.2f); // Pre-warning shake
        }
    }

    // Visual: clockwork golden color with phase-based intensity
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        float pulse = (std::sin(SDL_GetTicks() * 0.004f * ai.bossPhase) + 1.0f) * 0.5f;
        switch (ai.bossPhase) {
            case 1: sprite.setColor(180, static_cast<Uint8>(160 + 40 * pulse), 80); break;
            case 2: sprite.setColor(200, static_cast<Uint8>(140 + 60 * pulse), static_cast<Uint8>(60 + 40 * pulse)); break;
            case 3: sprite.setColor(static_cast<Uint8>(220 + 35 * pulse), static_cast<Uint8>(100 * pulse), static_cast<Uint8>(40 * pulse)); break;
            case 4: sprite.setColor(255, static_cast<Uint8>(60 + 50 * pulse), static_cast<Uint8>(20 * pulse)); break; // Blazing gold-red
        }
    }
}
