#include "BossAI.h"

void AISystem::updateVoidSovereign(Entity& entity, float dt, const Vec2& playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    auto& hp = entity.getComponent<HealthComponent>();
    Vec2 pos = transform.getCenter();

    if (ai.state == AIState::Dead) return;
    if (ai.state == AIState::Stunned) {
        ai.stunTimer -= dt;
        if (ai.stunTimer <= 0) ai.state = AIState::Chase;
        return;
    }

    // Phase transitions
    float hpPct = hp.getPercent();
    bool extraPhase = (AscensionSystem::currentLevel > 0 &&
        AscensionSystem::getLevel(AscensionSystem::currentLevel).bossExtraPhase);
    if (extraPhase && hpPct <= 0.15f && ai.bossPhase < 4) {
        ai.bossPhase = 4;
        if (m_camera) m_camera->shake(20.0f, 1.0f);
        if (m_particles) m_particles->burst(pos, 70, {200, 0, 255, 255}, 400.0f, 7.0f);
        AudioManager::instance().play(SFX::SuitEntropyCritical);
    } else if (hpPct <= 0.4f && ai.bossPhase < 3) {
        ai.bossPhase = 3;
        if (m_camera) m_camera->shake(15.0f, 0.8f);
    } else if (hpPct <= 0.7f && ai.bossPhase < 2) {
        ai.bossPhase = 2;
        if (m_camera) m_camera->shake(10.0f, 0.5f);
    }

    float dist = distanceTo(pos, playerPos);
    ai.vsVoidKernPulse = std::fmod(ai.vsVoidKernPulse + dt * (2.0f + ai.bossPhase), 6.283185f);

    // --- VoidSovereign Telegraph Processing ---
    if (ai.bossTelegraphTimer > 0) {
        ai.bossTelegraphTimer -= dt;
        phys.velocity.x *= 0.7f;
        phys.velocity.y *= 0.7f;

        float telegraphMax = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
        float flashRate = 12.0f + (1.0f - ai.bossTelegraphTimer / telegraphMax) * 25.0f;
        float flash = std::sin(ai.bossTelegraphTimer * flashRate) * 0.5f + 0.5f;

        if (entity.hasComponent<SpriteComponent>()) {
            auto& sprite = entity.getComponent<SpriteComponent>();
            switch (ai.bossTelegraphAttack) {
                case 1: sprite.setColor(200 + (int)(flash * 55), (int)(flash * 50), 150 + (int)(flash * 105)); break; // slam: red-purple
                case 2: sprite.setColor(200 + (int)(flash * 55), 0, 200 + (int)(flash * 55)); break; // dimlock: magenta
                case 3: sprite.setColor(150 + (int)(flash * 105), 0, 200 + (int)(flash * 55)); break; // laser: deep purple
                case 4: sprite.setColor(100 + (int)(flash * 60), 0, 180 + (int)(flash * 75)); break; // storm: void purple
                default: break;
            }
        }

        if (m_particles) {
            SDL_Color warnColor;
            switch (ai.bossTelegraphAttack) {
                case 1: warnColor = {255, 50, 150, 200}; break;
                case 2: warnColor = {255, 0, 255, 200}; break;
                case 3: warnColor = {200, 0, 255, 200}; break;
                case 4: warnColor = {160, 0, 200, 200}; break;
                default: warnColor = {255, 255, 255, 200};
            }
            for (int i = 0; i < 3; i++) {
                float angle = (float)(rand() % 628) / 100.0f;
                Vec2 offset = {std::cos(angle) * 40.0f, std::sin(angle) * 40.0f};
                m_particles->burst(pos + offset, 1, warnColor, 70.0f, 3.0f);
            }
        }

        if (ai.bossTelegraphTimer <= 0) {
            switch (ai.bossTelegraphAttack) {
                case 1: {
                    // Rift Slam - execute
                    ai.vsSlamTimer = 5.0f - ai.bossPhase * 0.6f;
                    if (m_camera) m_camera->shake(12.0f, 0.4f);
                    AudioManager::instance().play(SFX::VoidSovereignSlam);
                    entities.forEach([&](Entity& target) {
                        if (!target.isPlayer) return;
                        if (!target.hasComponent<TransformComponent>()) return;
                        Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                        float d = distanceTo(pos, tPos);
                        if (d < 150.0f) {
                            if (target.hasComponent<HealthComponent>()) {
                                auto& hp = target.getComponent<HealthComponent>();
                                if (!hp.isInvincible()) {
                                    hp.takeDamage(35.0f);
                                    if (m_combatSystem)
                                        m_combatSystem->addDamageEvent(tPos, 35.0f, true, false, false, pos);
                                }
                            }
                            if (target.hasComponent<PhysicsBody>()) {
                                Vec2 knockDir = (tPos - pos).normalized();
                                target.getComponent<PhysicsBody>().velocity += knockDir * 400.0f;
                                target.getComponent<PhysicsBody>().velocity.y = -300.0f;
                            }
                        }
                    });
                    if (m_particles) {
                        m_particles->burst(pos, 30, {150, 50, 200, 255}, 200.0f, 5.0f);
                        for (int i = 0; i < 16; i++) {
                            float angle = i * 6.283185f / 16.0f;
                            Vec2 ring = {pos.x + std::cos(angle) * 75.0f, pos.y + std::sin(angle) * 75.0f};
                            m_particles->burst(ring, 3, {100, 20, 160, 200}, 80.0f, 2.0f);
                        }
                    }
                    break;
                }
                case 2: {
                    // Dimension Lock - execute
                    ai.vsDimLockTimer = 15.0f;
                    ai.vsDimLockActive = 5.0f;
                    if (m_particles) m_particles->burst(pos, 25, {200, 0, 200, 255}, 180.0f, 5.0f);
                    if (m_camera) m_camera->shake(8.0f, 0.3f);
                    AudioManager::instance().play(SFX::VoidSovereignDimLock);
                    break;
                }
                case 3: {
                    // Reality Tear laser - execute
                    ai.vsLaserActive = true;
                    ai.vsLaserTimer = 3.0f;
                    ai.vsLaserAngle = std::atan2(playerPos.y - pos.y, playerPos.x - pos.x);
                    AudioManager::instance().play(SFX::VoidSovereignLaser);
                    break;
                }
                case 4: {
                    // Void Storm - execute
                    ai.vsStormTimer = 20.0f;
                    ai.vsStormActive = 5.0f;
                    ai.vsStormSafe1 = {playerPos.x + static_cast<float>((std::rand() % 200) - 100),
                                        playerPos.y + static_cast<float>((std::rand() % 80) - 40)};
                    ai.vsStormSafe2 = {playerPos.x + static_cast<float>((std::rand() % 200) - 100),
                                        playerPos.y + static_cast<float>((std::rand() % 80) - 40)};
                    if (m_camera) m_camera->shake(10.0f, 0.5f);
                    AudioManager::instance().play(SFX::VoidSovereignStorm);
                    break;
                }
                default: break;
            }
            ai.bossTelegraphAttack = -1;
        }
        return; // Skip normal AI during telegraph
    }

    // Hover movement toward player
    float targetX = playerPos.x;
    float targetY = playerPos.y - 120.0f; // hover above
    float dx = targetX - pos.x;
    float dy = targetY - pos.y;
    float moveSpeed = 80.0f + ai.bossPhase * 30.0f;
    if (dist > 60.0f) {
        float nd = std::sqrt(dx * dx + dy * dy);
        if (nd > 0) {
            phys.velocity.x = (dx / nd) * moveSpeed;
            phys.velocity.y = (dy / nd) * moveSpeed;
        }
    }

    ai.facingRight = dx > 0;
    ai.state = AIState::Chase;

    // --- Phase 1 attacks: Void Orbs + Rift Slam + Teleport ---

    // Void Orbs (3 projectiles - no telegraph, frequent weak attack)
    ai.vsOrbTimer -= dt;
    if (ai.vsOrbTimer <= 0 && dist < 500.0f) {
        ai.vsOrbTimer = 3.0f - ai.bossPhase * 0.3f;
        int orbCount = 3 + (ai.bossPhase - 1);
        float spreadAngle = 0.4f;
        float baseAngle = std::atan2(playerPos.y - pos.y, playerPos.x - pos.x);

        for (int i = 0; i < orbCount; i++) {
            float angle = baseAngle + (i - orbCount / 2.0f) * spreadAngle;
            Vec2 dir = {std::cos(angle), std::sin(angle)};
            Vec2 spawnPos = {pos.x + dir.x * 40.0f, pos.y + dir.y * 40.0f};
            if (m_combatSystem) {
                m_combatSystem->createProjectile(entities, spawnPos, dir, 18.0f, 250.0f, entity.dimension);
            }
            if (m_particles) {
                m_particles->burst(spawnPos, 6, {120, 40, 200, 255}, 100.0f, 3.0f);
            }
        }
        if (m_camera) m_camera->shake(5.0f, 0.2f);
        AudioManager::instance().play(SFX::VoidSovereignOrb);
    }

    // Rift Slam (AoE 150px) - TELEGRAPH
    ai.vsSlamTimer -= dt;
    if (ai.vsSlamTimer <= 0 && dist < 200.0f) {
        float telegraphTime = 0.6f - (ai.bossPhase - 1) * 0.1f;
        ai.bossTelegraphTimer = telegraphTime;
        ai.bossTelegraphAttack = 1;
        AudioManager::instance().play(SFX::CollapseWarning);
    }

    // Teleport
    ai.vsTeleportTimer -= dt;
    if (ai.vsTeleportTimer <= 0) {
        ai.vsTeleportTimer = 10.0f - ai.bossPhase * 1.5f;
        if (m_particles) {
            m_particles->burst(pos, 20, {80, 0, 120, 255}, 120.0f, 4.0f);
        }
        // Teleport to random offset from player
        float angle = static_cast<float>(std::rand() % 628) / 100.0f;
        float tdist = 150.0f + static_cast<float>(std::rand() % 100);
        transform.position.x = playerPos.x + std::cos(angle) * tdist - transform.width / 2;
        transform.position.y = playerPos.y + std::sin(angle) * tdist - transform.height / 2;
        transform.prevPosition = transform.position; // Sync for interpolation
        if (m_particles) {
            m_particles->burst(transform.getCenter(), 20, {80, 0, 120, 255}, 120.0f, 4.0f);
        }
    }

    // --- Phase 2 additions: Dimension Lock + Shadow Clones ---
    if (ai.bossPhase >= 2) {
        ai.vsDimLockTimer -= dt;
        if (ai.vsDimLockTimer <= 0 && ai.vsDimLockActive <= 0) {
            // Dimension Lock - TELEGRAPH
            float telegraphTime = 0.6f - (ai.bossPhase - 1) * 0.1f;
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 2;
            AudioManager::instance().play(SFX::SniperTelegraph);
        }
        if (ai.vsDimLockActive > 0) {
            ai.vsDimLockActive -= dt;
        }

        // Shadow Clones: count active, respawn if below 2
        int activeClones = 0;
        entities.forEach([&](Entity& e) {
            if (e.isShadowClone && e.hasComponent<HealthComponent>()) {
                if (e.getComponent<HealthComponent>().currentHP > 0) activeClones++;
            }
        });
        ai.vsCloneCount = activeClones;

        if (ai.vsCloneCount < 2) {
            ai.vsCloneSpawnTimer -= dt;
            if (ai.vsCloneSpawnTimer <= 0) {
                ai.vsCloneSpawnTimer = 18.0f;
                int toSpawn = 2 - ai.vsCloneCount;
                for (int c = 0; c < toSpawn; c++) {
                    float cAngle = static_cast<float>(std::rand() % 628) / 100.0f;
                    Vec2 clonePos = {pos.x + std::cos(cAngle) * 120.0f,
                                     pos.y + std::sin(cAngle) * 80.0f};

                    auto& clone = entities.addEntity("enemy_shadow_clone");
                    clone.addComponent<TransformComponent>(clonePos.x, clonePos.y, 48, 48);
                    auto& cs = clone.addComponent<SpriteComponent>();
                    cs.setColor(60, 0, 90);
                    cs.renderLayer = 2;
                    auto& cp = clone.addComponent<PhysicsBody>();
                    cp.useGravity = true;
                    auto& cc = clone.addComponent<ColliderComponent>();
                    cc.width = 40; cc.height = 40; cc.offset = {4, 4};
                    cc.layer = LAYER_ENEMY;
                    cc.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;
                    auto& chp = clone.addComponent<HealthComponent>();
                    chp.maxHP = hp.maxHP * 0.3f;
                    chp.currentHP = chp.maxHP;
                    auto& ccombat = clone.addComponent<CombatComponent>();
                    ccombat.meleeAttack.damage = 15.0f;
                    ccombat.meleeAttack.range = 50.0f;
                    ccombat.meleeAttack.knockback = 200.0f;
                    auto& cai = clone.addComponent<AIComponent>();
                    cai.enemyType = EnemyType::Walker;
                    cai.detectRange = 400.0f;
                    cai.chaseSpeed = 160.0f;
                    cai.attackRange = 50.0f;
                    cai.attackCooldown = 1.2f;
                    cai.patrolStart = {clonePos.x - 100, clonePos.y};
                    cai.patrolEnd = {clonePos.x + 100, clonePos.y};
                    clone.dimension = entity.dimension;

                    ai.vsCloneCount++;
                    if (m_particles) {
                        m_particles->burst(clonePos, 15, {80, 0, 120, 255}, 100.0f, 3.0f);
                    }
                }
                AudioManager::instance().play(SFX::VoidSovereignTeleport);
            }
        }
    }

    // --- Phase 3 additions: Auto Dim-Switch + Reality Tear + Void Storm ---
    if (ai.bossPhase >= 3) {
        // Auto dimension switch every 3s (flag consumed by PlayState)
        ai.vsAutoSwitchTimer -= dt;
        if (ai.vsAutoSwitchTimer <= 0) {
            ai.vsAutoSwitchTimer = 3.0f;
            ai.vsForceDimSwitch = true;
        }

        // Reality Tear laser - TELEGRAPH
        if (!ai.vsLaserActive) {
            ai.vsLaserTimer -= dt;
        }
        if (ai.vsLaserTimer <= 0 && !ai.vsLaserActive) {
            float telegraphTime = 0.6f - (ai.bossPhase - 1) * 0.1f;
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 3;
            ai.vsLaserTimer = 1.0f; // prevent re-trigger during telegraph
            AudioManager::instance().play(SFX::SniperTelegraph);
        }
        if (ai.vsLaserActive) {
            ai.vsLaserAngle += dt * 1.5f; // slow sweep
            ai.vsLaserTimer -= dt;
            if (ai.vsLaserTimer <= 0) {
                ai.vsLaserActive = false;
                ai.vsLaserTimer = 10.0f;
            }
            // Laser damage: check player proximity to beam
            float laserLen = 400.0f;
            Vec2 laserEnd = {pos.x + std::cos(ai.vsLaserAngle) * laserLen,
                             pos.y + std::sin(ai.vsLaserAngle) * laserLen};
            entities.forEach([&](Entity& target) {
                if (!target.isPlayer) return;
                if (!target.hasComponent<TransformComponent>()) return;
                Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                // Point-to-segment distance
                float lx = laserEnd.x - pos.x, ly = laserEnd.y - pos.y;
                float tx = tPos.x - pos.x, ty = tPos.y - pos.y;
                float lenSq = lx * lx + ly * ly;
                float proj = (lenSq > 0) ? (tx * lx + ty * ly) / lenSq : 0;
                proj = std::max(0.0f, std::min(1.0f, proj));
                float cx = pos.x + proj * lx, cy = pos.y + proj * ly;
                float ddx = tPos.x - cx, ddy = tPos.y - cy;
                if (ddx * ddx + ddy * ddy < 30.0f * 30.0f && target.hasComponent<HealthComponent>()) {
                    auto& hp = target.getComponent<HealthComponent>();
                    if (!hp.isInvincible()) {
                        float dmg = 40.0f * dt;
                        hp.takeDamage(dmg);
                        // No DamageEvent for continuous laser (would spam floating numbers)
                    }
                }
            });
            if (m_particles) {
                for (int i = 0; i < 5; i++) {
                    float t = static_cast<float>(i) / 5.0f;
                    Vec2 lp = {pos.x + (laserEnd.x - pos.x) * t,
                               pos.y + (laserEnd.y - pos.y) * t};
                    m_particles->burst(lp, 2, {200, 0, 255, 200}, 40.0f, 2.0f);
                }
            }
        }

        // Void Storm: periodic AoE with safe zones - TELEGRAPH
        ai.vsStormTimer -= dt;
        if (ai.vsStormTimer <= 0 && ai.vsStormActive <= 0) {
            float telegraphTime = 0.6f - (ai.bossPhase - 1) * 0.1f;
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 4;
            ai.vsStormTimer = 1.0f; // prevent re-trigger during telegraph
            AudioManager::instance().play(SFX::CollapseWarning);
        }
        if (ai.vsStormActive > 0) {
            ai.vsStormActive -= dt;
            // Visual: chaotic void particles
            if (m_particles) {
                for (int s = 0; s < 3; s++) {
                    float rx = pos.x + static_cast<float>((std::rand() % 600) - 300);
                    float ry = pos.y + static_cast<float>((std::rand() % 400) - 200);
                    m_particles->burst({rx, ry}, 1, {160, 0, 200, 150}, 30.0f, 1.0f);
                }
                // Safe zone indicators (green circles)
                for (int s = 0; s < 4; s++) {
                    float sa = s * 1.5708f + ai.vsVoidKernPulse;
                    m_particles->burst({ai.vsStormSafe1.x + std::cos(sa) * 40.0f,
                                        ai.vsStormSafe1.y + std::sin(sa) * 40.0f},
                                       1, {60, 255, 60, 200}, 20.0f, 0.5f);
                    m_particles->burst({ai.vsStormSafe2.x + std::cos(sa) * 40.0f,
                                        ai.vsStormSafe2.y + std::sin(sa) * 40.0f},
                                       1, {60, 255, 60, 200}, 20.0f, 0.5f);
                }
            }
            // Damage player if not in safe zones
            entities.forEach([&](Entity& target) {
                if (!target.isPlayer) return;
                if (!target.hasComponent<TransformComponent>()) return;
                Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                float d1 = distanceTo(tPos, ai.vsStormSafe1);
                float d2 = distanceTo(tPos, ai.vsStormSafe2);
                float safeRadius = 80.0f;
                if (d1 > safeRadius && d2 > safeRadius) {
                    if (target.hasComponent<HealthComponent>()) {
                        auto& hp = target.getComponent<HealthComponent>();
                        if (!hp.isInvincible()) {
                            hp.takeDamage(25.0f * dt);
                        }
                    }
                }
            });
        }
    }

    // Visual: pulsing color
    auto& sprite = entity.getComponent<SpriteComponent>();
    float pulse = 0.5f + 0.5f * std::sin(ai.vsVoidKernPulse);
    switch (ai.bossPhase) {
        case 1:
            sprite.setColor(static_cast<Uint8>(80 + 40 * pulse), 0, static_cast<Uint8>(120 + 40 * pulse));
            break;
        case 2:
            sprite.setColor(static_cast<Uint8>(100 + 60 * pulse), static_cast<Uint8>(20 * pulse), static_cast<Uint8>(140 + 50 * pulse));
            break;
        case 3:
            sprite.setColor(static_cast<Uint8>(140 + 80 * pulse), static_cast<Uint8>(40 * pulse), static_cast<Uint8>(180 + 60 * pulse));
            break;
        case 4:
            sprite.setColor(static_cast<Uint8>(200 + 55 * pulse), static_cast<Uint8>(30 * pulse), static_cast<Uint8>(220 + 35 * pulse));
            break; // Bright void purple
    }
}
