#include "BossAI.h"

void AISystem::updateVoidWyrm(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    // Phase transitions based on HP
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        float hpPct = hp.getPercent();
        if (hpPct < 0.33f && ai.bossPhase < 3) {
            ai.bossPhase = 3;
            ai.bossEnraged = true;
            if (m_camera) m_camera->shake(15.0f, 0.5f);
            if (m_particles) m_particles->burst(pos, 50, {80, 255, 150, 255}, 300.0f, 5.0f);
            AudioManager::instance().play(SFX::SuitEntropyCritical);
        } else if (hpPct < 0.66f && ai.bossPhase < 2) {
            ai.bossPhase = 2;
            if (m_camera) m_camera->shake(10.0f, 0.3f);
            if (m_particles) m_particles->burst(pos, 30, {60, 220, 100, 255}, 250.0f, 4.0f);
            AudioManager::instance().play(SFX::CollapseWarning);
        }
    }

    float phaseSpeedMult = 1.0f + (ai.bossPhase - 1) * 0.3f;
    float phaseCooldownMult = 1.0f - (ai.bossPhase - 1) * 0.2f;

    ai.facingRight = playerPos.x > pos.x;
    ai.bossAttackTimer -= dt;
    ai.wyrmDiveTimer -= dt;
    ai.wyrmPoisonTimer -= dt;
    ai.wyrmBarrageTimer -= dt;

    // --- VoidWyrm Telegraph Processing ---
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
                case 2: sprite.setColor(255, 150 + (int)(flash * 105), (int)(flash * 50)); break; // divebomb: orange
                case 3: sprite.setColor((int)(flash * 100), 200 + (int)(flash * 55), (int)(flash * 60)); break; // poison: green
                case 4: sprite.setColor((int)(flash * 80), 200 + (int)(flash * 55), 200 + (int)(flash * 55)); break; // spiral: cyan
                case 5: sprite.setColor(200 + (int)(flash * 55), (int)(flash * 80), (int)(flash * 80)); break; // barrage: red
                default: break;
            }
        }

        if (m_particles) {
            SDL_Color warnColor;
            switch (ai.bossTelegraphAttack) {
                case 2: warnColor = {255, 200, 50, 200}; break;
                case 3: warnColor = {100, 255, 80, 200}; break;
                case 4: warnColor = {80, 255, 220, 200}; break;
                case 5: warnColor = {255, 80, 80, 200}; break;
                default: warnColor = {255, 255, 255, 200};
            }
            for (int i = 0; i < 2; i++) {
                float angle = (float)(rand() % 628) / 100.0f;
                Vec2 offset = {std::cos(angle) * 35.0f, std::sin(angle) * 35.0f};
                m_particles->burst(pos + offset, 1, warnColor, 60.0f, 3.0f);
            }
        }

        if (ai.bossTelegraphTimer <= 0) {
            switch (ai.bossTelegraphAttack) {
                case 2: {
                    // Divebomb - execute
                    ai.wyrmDiving = true;
                    ai.wyrmDiveTarget = ai.bossTelegraphDir; // stored target pos
                    ai.wyrmDiveTimer = 4.0f * phaseCooldownMult;
                    ai.bossAttackTimer = 1.0f;
                    AudioManager::instance().play(SFX::WyrmDive);
                    if (m_particles) m_particles->burst(pos, 15, {40, 255, 140, 255}, 200.0f, 3.0f);
                    break;
                }
                case 3: {
                    // Poison cloud - execute
                    ai.wyrmPoisonTimer = 5.0f * phaseCooldownMult;
                    ai.bossAttackTimer = 2.0f * phaseCooldownMult;
                    AudioManager::instance().play(SFX::WyrmPoison);
                    Vec2 targetPos = ai.bossTelegraphDir; // stored player pos
                    if (m_particles) m_particles->burst(targetPos, 30, {100, 220, 60, 180}, 120.0f, 5.0f);
                    if (m_camera) m_camera->shake(4.0f, 0.15f);
                    entities.forEach([&](Entity& target) {
                        if (target.getTag() != "player") return;
                        if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                        Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                        if (distanceTo(targetPos, tPos) < 100.0f) {
                            auto& hp = target.getComponent<HealthComponent>();
                            if (!hp.isInvincible()) {
                                hp.takeDamage(8.0f);
                                if (m_combatSystem)
                                    m_combatSystem->addDamageEvent(tPos, 8.0f, true);
                            }
                        }
                    });
                    break;
                }
                case 4: {
                    // Spiral projectile ring - execute
                    if (m_combatSystem) {
                        int projCount = 5 + ai.bossPhase * 2;
                        float projDmg = entity.getComponent<CombatComponent>().rangedAttack.damage;
                        for (int p = 0; p < projCount; p++) {
                            float angle = (6.283185f / projCount) * p + ai.wyrmOrbitAngle;
                            Vec2 shotDir = {std::cos(angle), std::sin(angle)};
                            m_combatSystem->createProjectile(entities, pos, shotDir, projDmg, 280.0f, entity.dimension);
                        }
                        ai.bossAttackTimer = 2.5f * phaseCooldownMult;
                        AudioManager::instance().play(SFX::BossMultiShot);
                        if (m_particles) m_particles->burst(pos, 20, {60, 255, 180, 255}, 200.0f, 3.0f);
                    }
                    break;
                }
                case 5: {
                    // Rapid barrage - execute
                    ai.wyrmBarrageCount = 5 + ai.bossPhase * 2;
                    ai.wyrmBarrageTimer = 0;
                    AudioManager::instance().play(SFX::WyrmBarrage);
                    break;
                }
                default: break;
            }
            ai.bossTelegraphAttack = -1;
        }
        return; // Skip normal AI during telegraph
    }

    if (dist < ai.detectRange) {
        // Handle active divebomb
        if (ai.wyrmDiving) {
            Vec2 dir = (ai.wyrmDiveTarget - pos).normalized();
            phys.velocity = dir * 450.0f * phaseSpeedMult;

            // Check if reached target or passed it
            if (distanceTo(pos, ai.wyrmDiveTarget) < 30.0f || pos.y > ai.wyrmDiveTarget.y + 50.0f) {
                ai.wyrmDiving = false;
                // Impact AoE
                if (m_camera) m_camera->shake(10.0f, 0.25f);
                if (m_particles) {
                    m_particles->burst(pos, 25, {80, 255, 120, 255}, 250.0f, 4.0f);
                }
                AudioManager::instance().play(SFX::SpikeDamage);
                // Damage nearby player
                entities.forEach([&](Entity& target) {
                    if (target.getTag() != "player") return;
                    if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                    Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                    float d = distanceTo(pos, tPos);
                    if (d < 80.0f) {
                        float falloff = 1.0f - (d / 80.0f);
                        auto& hp = target.getComponent<HealthComponent>();
                        if (!hp.isInvincible()) {
                            float dmg = 18.0f * falloff;
                            hp.takeDamage(dmg);
                            if (m_combatSystem)
                                m_combatSystem->addDamageEvent(tPos, dmg, true);
                        }
                        if (target.hasComponent<PhysicsBody>()) {
                            Vec2 knockDir = (tPos - pos).normalized();
                            knockDir.y = -0.5f;
                            target.getComponent<PhysicsBody>().velocity += knockDir * 350.0f * falloff;
                        }
                    }
                });
                // Return to orbit
                phys.velocity.y = -200.0f;
            }
        }
        // Handle active barrage
        else if (ai.wyrmBarrageCount > 0) {
            // Hover in place while barraging
            phys.velocity.x *= 0.85f;
            phys.velocity.y *= 0.85f;
            ai.wyrmBarrageTimer -= dt;
            if (ai.wyrmBarrageTimer <= 0 && m_combatSystem) {
                Vec2 dir = (playerPos - pos).normalized();
                // Slight spread
                float spread = (std::rand() % 100 - 50) * 0.005f;
                Vec2 shotDir = {dir.x * std::cos(spread) - dir.y * std::sin(spread),
                                dir.x * std::sin(spread) + dir.y * std::cos(spread)};
                float projDmg = entity.getComponent<CombatComponent>().rangedAttack.damage * 0.7f;
                m_combatSystem->createProjectile(entities, pos, shotDir, projDmg, 350.0f, entity.dimension);
                ai.wyrmBarrageCount--;
                ai.wyrmBarrageTimer = 0.15f;
                AudioManager::instance().play(SFX::RangedShot);
            }
            if (ai.wyrmBarrageCount <= 0) {
                ai.bossAttackTimer = 1.5f * phaseCooldownMult;
            }
        }
        // Normal orbit behavior
        else {
            // Orbit around player
            ai.wyrmOrbitAngle += dt * 1.8f * phaseSpeedMult;
            float targetX = playerPos.x + std::cos(ai.wyrmOrbitAngle) * ai.wyrmOrbitRadius;
            float targetY = playerPos.y - 80.0f + std::sin(ai.wyrmOrbitAngle * 0.7f) * 40.0f;
            Vec2 target = {targetX, targetY};
            Vec2 dir = (target - pos).normalized();
            float speed = ai.chaseSpeed * phaseSpeedMult;
            phys.velocity = dir * speed;

            // Attack patterns
            if (ai.bossAttackTimer <= 0) {
                ai.bossAttackPattern = (ai.bossAttackPattern + 1) % (ai.bossPhase + 4);

                switch (ai.bossAttackPattern) {
                    case 0:
                    case 1: {
                        // Ranged shot toward player
                        auto& combat = entity.getComponent<CombatComponent>();
                        Vec2 shotDir = (playerPos - pos).normalized();
                        combat.startAttack(AttackType::Ranged, shotDir);
                        ai.bossAttackTimer = 1.5f * phaseCooldownMult;
                        break;
                    }
                    case 2: {
                        // Divebomb - TELEGRAPH
                        if (ai.wyrmDiveTimer <= 0) {
                            float telegraphTime = 0.5f - (ai.bossPhase - 1) * 0.1f;
                            ai.bossTelegraphTimer = telegraphTime;
                            ai.bossTelegraphAttack = 2;
                            ai.bossTelegraphDir = playerPos; // store target position
                            ai.bossAttackTimer = 0.3f;
                            AudioManager::instance().play(SFX::CollapseWarning);
                        } else {
                            ai.bossAttackTimer = 0.5f;
                        }
                        break;
                    }
                    case 3: {
                        // Poison cloud - TELEGRAPH
                        if (ai.wyrmPoisonTimer <= 0 && m_combatSystem) {
                            float telegraphTime = 0.5f - (ai.bossPhase - 1) * 0.1f;
                            ai.bossTelegraphTimer = telegraphTime;
                            ai.bossTelegraphAttack = 3;
                            ai.bossTelegraphDir = playerPos; // store target position
                            ai.bossAttackTimer = 0.3f;
                            AudioManager::instance().play(SFX::SniperTelegraph);
                        } else {
                            ai.bossAttackTimer = 0.5f;
                        }
                        break;
                    }
                    case 4: {
                        // Spiral projectile ring - TELEGRAPH
                        if (m_combatSystem) {
                            float telegraphTime = 0.5f - (ai.bossPhase - 1) * 0.1f;
                            ai.bossTelegraphTimer = telegraphTime;
                            ai.bossTelegraphAttack = 4;
                            ai.bossAttackTimer = 0.3f;
                            AudioManager::instance().play(SFX::CollapseWarning);
                        } else {
                            ai.bossAttackTimer = 0.5f;
                        }
                        break;
                    }
                    case 5: {
                        // Rapid barrage (Phase 2+) - TELEGRAPH
                        if (ai.bossPhase >= 2) {
                            float telegraphTime = 0.5f - (ai.bossPhase - 1) * 0.1f;
                            ai.bossTelegraphTimer = telegraphTime;
                            ai.bossTelegraphAttack = 5;
                            ai.bossAttackTimer = 0.3f;
                            AudioManager::instance().play(SFX::SniperTelegraph);
                        } else {
                            ai.bossAttackTimer = 0.5f;
                        }
                        break;
                    }
                    case 6: {
                        // Summon poison minions (Phase 3)
                        if (ai.bossPhase >= 3) {
                            for (int i = 0; i < 2; i++) {
                                float offsetX = (i == 0 ? -60.0f : 60.0f);
                                Vec2 spawnPos = {pos.x + offsetX, pos.y + 30.0f};
                                auto& minion = entities.addEntity("enemy_minion");
                                minion.dimension = entity.dimension;
                                minion.addComponent<TransformComponent>(spawnPos.x, spawnPos.y, 16, 16);
                                auto& ms = minion.addComponent<SpriteComponent>();
                                ms.setColor(80, 200, 100);
                                ms.renderLayer = 2;
                                auto& mp = minion.addComponent<PhysicsBody>();
                                mp.gravity = 980.0f;
                                mp.friction = 600.0f;
                                auto& mc = minion.addComponent<ColliderComponent>();
                                mc.width = 14; mc.height = 14; mc.offset = {1, 1};
                                mc.layer = LAYER_ENEMY;
                                mc.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;
                                auto& mh = minion.addComponent<HealthComponent>();
                                mh.maxHP = 12.0f; mh.currentHP = 12.0f;
                                auto& mcb = minion.addComponent<CombatComponent>();
                                mcb.meleeAttack.damage = 10.0f;
                                mcb.meleeAttack.knockback = 120.0f;
                                mcb.meleeAttack.cooldown = 1.5f;
                                auto& mai = minion.addComponent<AIComponent>();
                                mai.enemyType = EnemyType::Walker;
                                mai.detectRange = 150.0f;
                                mai.attackRange = 20.0f;
                                mai.chaseSpeed = 140.0f;
                                mai.patrolSpeed = 70.0f;
                                mai.patrolStart = spawnPos;
                                mai.patrolEnd = {spawnPos.x + 60.0f, spawnPos.y};
                                if (m_particles) {
                                    m_particles->burst(spawnPos, 10, {80, 220, 100, 200}, 80.0f, 4.0f);
                                }
                            }
                            AudioManager::instance().play(SFX::SummonerSummon);
                            ai.bossAttackTimer = 4.0f * phaseCooldownMult;
                        } else {
                            ai.bossAttackTimer = 0.5f;
                        }
                        break;
                    }
                    default:
                        ai.bossAttackTimer = 0.5f;
                        break;
                }
            }
        }
    } else {
        // Out of range: figure-8 patrol
        ai.wyrmOrbitAngle += dt * 1.2f;
        float targetX = ai.patrolStart.x + std::cos(ai.wyrmOrbitAngle) * 100.0f;
        float targetY = ai.patrolStart.y + std::sin(ai.wyrmOrbitAngle * 2.0f) * 40.0f;
        Vec2 dir = (Vec2{targetX, targetY} - pos).normalized();
        phys.velocity = dir * ai.patrolSpeed;
    }

    // Visual color
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        float pulse = (std::sin(SDL_GetTicks() * 0.006f * ai.bossPhase) + 1.0f) * 0.5f;
        switch (ai.bossPhase) {
            case 1: sprite.setColor(40, static_cast<Uint8>(160 + 40 * pulse), 120); break;
            case 2: sprite.setColor(60, static_cast<Uint8>(200 + 55 * pulse), 80); break;
            case 3: sprite.setColor(static_cast<Uint8>(100 + 80 * pulse), 255, static_cast<Uint8>(60 * pulse)); break;
        }
    }
}
