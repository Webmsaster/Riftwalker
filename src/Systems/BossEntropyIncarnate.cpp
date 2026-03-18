#include "BossAI.h"

void AISystem::updateEntropyIncarnate(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
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
        if (m_particles) m_particles->burst(pos, 80, {180, 0, 255, 255}, 400.0f, 7.0f);
        AudioManager::instance().play(SFX::SuitEntropyCritical);
    } else if (hpPct <= 0.33f && ai.bossPhase < 3) {
        ai.bossPhase = 3;
        if (m_camera) m_camera->shake(15.0f, 0.8f);
        if (m_particles) m_particles->burst(pos, 60, {140, 0, 200, 255}, 350.0f, 6.0f);
        AudioManager::instance().play(SFX::SuitEntropyCritical);
    } else if (hpPct <= 0.66f && ai.bossPhase < 2) {
        ai.bossPhase = 2;
        if (m_camera) m_camera->shake(10.0f, 0.5f);
        if (m_particles) m_particles->burst(pos, 40, {120, 0, 180, 255}, 280.0f, 5.0f);
        AudioManager::instance().play(SFX::CollapseWarning);
    }

    // Phase 3: 0.7x cooldown multiplier
    float cooldownMult = (ai.bossPhase >= 3) ? 0.7f : 1.0f;
    float dist = distanceTo(pos, playerPos);
    ai.eiCorePulse = std::fmod(ai.eiCorePulse + dt * (2.0f + ai.bossPhase), 6.283185f);

    // --- Telegraph Processing ---
    if (ai.bossTelegraphTimer > 0) {
        ai.bossTelegraphTimer -= dt;
        phys.velocity.x *= 0.7f;
        phys.velocity.y *= 0.7f;

        float telegraphMax = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
        float flashRate = 12.0f + (1.0f - ai.bossTelegraphTimer / telegraphMax) * 25.0f;
        float flash = std::sin(ai.bossTelegraphTimer * flashRate) * 0.5f + 0.5f;

        // Color flash based on attack type
        if (entity.hasComponent<SpriteComponent>()) {
            auto& sprite = entity.getComponent<SpriteComponent>();
            switch (ai.bossTelegraphAttack) {
                case 1: sprite.setColor(140 + (int)(flash * 115), 0, 200 + (int)(flash * 55)); break; // entropy pulse: deep purple
                case 2: sprite.setColor(200 + (int)(flash * 55), 0, 200 + (int)(flash * 55)); break; // dim lock: magenta
                case 3: sprite.setColor(100 + (int)(flash * 80), 0, 160 + (int)(flash * 95)); break; // missiles: dark purple
                case 4: sprite.setColor(180 + (int)(flash * 75), (int)(flash * 40), 100 + (int)(flash * 60)); break; // shatter: red-purple
                default: break;
            }
        }

        // Warning particles
        if (m_particles) {
            SDL_Color warnColor;
            switch (ai.bossTelegraphAttack) {
                case 1: warnColor = {160, 0, 200, 200}; break;
                case 2: warnColor = {255, 0, 255, 200}; break;
                case 3: warnColor = {120, 0, 180, 200}; break;
                case 4: warnColor = {255, 40, 120, 200}; break;
                default: warnColor = {255, 255, 255, 200};
            }
            for (int i = 0; i < 3; i++) {
                float angle = (float)(rand() % 628) / 100.0f;
                Vec2 offset = {std::cos(angle) * 40.0f, std::sin(angle) * 40.0f};
                m_particles->burst(pos + offset, 1, warnColor, 70.0f, 3.0f);
            }
        }

        // Execute attack when telegraph finishes
        if (ai.bossTelegraphTimer <= 0) {
            switch (ai.bossTelegraphAttack) {
                case 1: {
                    // Entropy Pulse: AoE burst adding entropy to player
                    ai.eiPulseTimer = 6.0f * cooldownMult;
                    ai.eiPulseFired = true; // PlayState will consume this and add entropy
                    if (m_camera) m_camera->shake(12.0f, 0.4f);
                    AudioManager::instance().play(SFX::EntropyPulse);

                    // AoE damage + entropy (handled via flag for PlayState)
                    entities.forEach([&](Entity& target) {
                        if (target.getTag() != "player") return;
                        if (!target.hasComponent<TransformComponent>()) return;
                        Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                        float d = distanceTo(pos, tPos);
                        if (d < 200.0f) {
                            if (target.hasComponent<HealthComponent>()) {
                                auto& thp = target.getComponent<HealthComponent>();
                                if (!thp.isInvincible()) {
                                    float dmg = 10.0f + ai.bossPhase * 3.0f;
                                    thp.takeDamage(dmg);
                                    if (m_combatSystem)
                                        m_combatSystem->addDamageEvent(tPos, dmg, true);
                                }
                            }
                            // Knockback
                            if (target.hasComponent<PhysicsBody>()) {
                                Vec2 knockDir = (tPos - pos).normalized();
                                target.getComponent<PhysicsBody>().velocity += knockDir * 350.0f;
                            }
                        }
                    });

                    // Expanding purple ring particles
                    if (m_particles) {
                        m_particles->burst(pos, 35, {140, 0, 200, 255}, 250.0f, 5.0f);
                        for (int i = 0; i < 20; i++) {
                            float angle = i * 6.283185f / 20.0f;
                            Vec2 ring = {pos.x + std::cos(angle) * 100.0f, pos.y + std::sin(angle) * 100.0f};
                            m_particles->burst(ring, 2, {120, 20, 180, 200}, 60.0f, 2.0f);
                        }
                    }
                    break;
                }
                case 2: {
                    // Forced Dimension Lock
                    ai.eiDimLockTimer = 15.0f * cooldownMult;
                    ai.eiDimLockActive = 5.0f;
                    if (m_particles) m_particles->burst(pos, 25, {200, 0, 200, 255}, 180.0f, 5.0f);
                    if (m_camera) m_camera->shake(8.0f, 0.3f);
                    AudioManager::instance().play(SFX::VoidSovereignDimLock);
                    break;
                }
                case 3: {
                    // Entropy Missiles: 5 fast projectiles in spread
                    ai.eiMissileTimer = 8.0f * cooldownMult;
                    if (m_combatSystem) {
                        float baseAngle = std::atan2(playerPos.y - pos.y, playerPos.x - pos.x);
                        int missileCount = 5 + (ai.bossPhase - 1);
                        float spread = 0.3f;
                        for (int i = 0; i < missileCount; i++) {
                            float angle = baseAngle + (i - missileCount / 2.0f) * spread;
                            Vec2 dir = {std::cos(angle), std::sin(angle)};
                            Vec2 spawnPos = {pos.x + dir.x * 35.0f, pos.y + dir.y * 35.0f};
                            m_combatSystem->createProjectile(entities, spawnPos, dir,
                                15.0f + ai.bossPhase * 2.0f, 350.0f, entity.dimension);
                            if (m_particles)
                                m_particles->burst(spawnPos, 4, {140, 0, 200, 255}, 80.0f, 2.0f);
                        }
                    }
                    if (m_camera) m_camera->shake(8.0f, 0.3f);
                    AudioManager::instance().play(SFX::EntropyMissile);
                    break;
                }
                case 4: {
                    // Dimension Shatter: force random dimension switch
                    ai.eiShatterTimer = 6.0f * cooldownMult;
                    ai.eiForceDimSwitch = true;
                    if (m_particles) {
                        m_particles->burst(pos, 40, {200, 40, 120, 255}, 300.0f, 6.0f);
                        // Shatter crack particles
                        for (int i = 0; i < 12; i++) {
                            float angle = i * 6.283185f / 12.0f;
                            Vec2 crack = {pos.x + std::cos(angle) * 120.0f, pos.y + std::sin(angle) * 120.0f};
                            m_particles->burst(crack, 3, {255, 50, 150, 200}, 100.0f, 3.0f);
                        }
                    }
                    if (m_camera) m_camera->shake(12.0f, 0.5f);
                    AudioManager::instance().play(SFX::EntropyShatter);
                    break;
                }
                default: break;
            }
            ai.bossTelegraphAttack = -1;
        }
        return; // Skip normal AI during telegraph
    }

    // --- Hover movement toward player ---
    float targetX = playerPos.x;
    float targetY = playerPos.y - 100.0f; // hover above
    ai.eiHoverY = std::sin(ai.eiCorePulse * 0.8f) * 15.0f; // gentle hover bob
    targetY += ai.eiHoverY;
    float dx = targetX - pos.x;
    float dy = targetY - pos.y;
    float moveSpeed = 80.0f + ai.bossPhase * 25.0f;
    if (dist > 50.0f) {
        float nd = std::sqrt(dx * dx + dy * dy);
        if (nd > 0) {
            phys.velocity.x = (dx / nd) * moveSpeed;
            phys.velocity.y = (dy / nd) * moveSpeed;
        }
    }
    ai.facingRight = dx > 0;
    ai.state = AIState::Chase;

    // ========================================
    // Phase 1 attacks (100-66% HP)
    // ========================================

    // Entropy Pulse AoE - TELEGRAPH
    ai.eiPulseTimer -= dt;
    if (ai.eiPulseTimer <= 0 && dist < 250.0f) {
        float telegraphTime = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
        ai.bossTelegraphTimer = telegraphTime;
        ai.bossTelegraphAttack = 1;
        AudioManager::instance().play(SFX::CollapseWarning);
    }

    // Void Tendrils: 3 slow-moving homing projectiles
    ai.eiTendrilTimer -= dt;
    if (ai.eiTendrilTimer <= 0 && dist < 400.0f) {
        ai.eiTendrilTimer = 5.0f * cooldownMult;
        if (m_combatSystem) {
            int tendrilCount = 3 + (ai.bossPhase >= 3 ? 1 : 0);
            for (int i = 0; i < tendrilCount; i++) {
                float angle = std::atan2(playerPos.y - pos.y, playerPos.x - pos.x) +
                              (i - tendrilCount / 2.0f) * 0.5f;
                Vec2 dir = {std::cos(angle), std::sin(angle)};
                Vec2 spawnPos = {pos.x + dir.x * 30.0f, pos.y + dir.y * 30.0f};
                // Slow homing projectiles (low speed, 12 dmg)
                m_combatSystem->createProjectile(entities, spawnPos, dir,
                    12.0f, 120.0f, entity.dimension);
                if (m_particles)
                    m_particles->burst(spawnPos, 5, {100, 0, 160, 255}, 60.0f, 3.0f);
            }
        }
        AudioManager::instance().play(SFX::EntropyTendril);
    }

    // Teleport every 8s
    ai.eiTeleportTimer -= dt;
    if (ai.eiTeleportTimer <= 0) {
        ai.eiTeleportTimer = 8.0f * cooldownMult;
        if (m_particles)
            m_particles->burst(pos, 20, {100, 20, 140, 255}, 120.0f, 4.0f);

        // Teleport to random offset from player
        float tAngle = static_cast<float>(std::rand() % 628) / 100.0f;
        float tDist = 150.0f + static_cast<float>(std::rand() % 100);
        transform.position.x = playerPos.x + std::cos(tAngle) * tDist - transform.width / 2;
        transform.position.y = playerPos.y + std::sin(tAngle) * tDist - transform.height / 2;
        transform.prevPosition = transform.position; // Sync for interpolation

        if (m_particles)
            m_particles->burst(transform.getCenter(), 20, {100, 20, 140, 255}, 120.0f, 4.0f);
        AudioManager::instance().play(SFX::BossTeleport);
    }

    // ========================================
    // Phase 2 attacks (66-33% HP)
    // ========================================
    if (ai.bossPhase >= 2) {
        // Entropy Storm: continuous entropy drain zone around boss
        ai.eiStormTimer -= dt;
        if (ai.eiStormTimer <= 0 && ai.eiStormActive <= 0) {
            ai.eiStormTimer = 12.0f * cooldownMult;
            ai.eiStormActive = 5.0f; // 5s duration
            if (m_camera) m_camera->shake(6.0f, 0.3f);
            AudioManager::instance().play(SFX::VoidSovereignStorm);
        }
        if (ai.eiStormActive > 0) {
            ai.eiStormActive -= dt;
            // Visual: swirling entropy particles around boss
            if (m_particles) {
                for (int s = 0; s < 4; s++) {
                    float sAngle = ai.eiCorePulse + s * 1.5708f;
                    float sRadius = 120.0f + std::sin(ai.eiCorePulse * 2.0f) * 30.0f;
                    Vec2 sp = {pos.x + std::cos(sAngle) * sRadius,
                               pos.y + std::sin(sAngle) * sRadius};
                    m_particles->burst(sp, 1, {140, 0, 200, 180}, 40.0f, 1.5f);
                }
            }
            // Entropy drain damage to player if in range (150px)
            // Note: actual entropy addition handled by PlayState reading eiStormActive
            entities.forEach([&](Entity& target) {
                if (target.getTag() != "player") return;
                if (!target.hasComponent<TransformComponent>()) return;
                Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                float d = distanceTo(pos, tPos);
                if (d < 150.0f && target.hasComponent<HealthComponent>()) {
                    auto& thp = target.getComponent<HealthComponent>();
                    if (!thp.isInvincible()) {
                        // Light tick damage while in entropy storm zone
                        thp.takeDamage(5.0f * dt);
                    }
                }
            });
        }

        // Forced Dimension Lock - TELEGRAPH
        ai.eiDimLockTimer -= dt;
        if (ai.eiDimLockTimer <= 0 && ai.eiDimLockActive <= 0) {
            float telegraphTime = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 2;
            AudioManager::instance().play(SFX::SniperTelegraph);
        }
        if (ai.eiDimLockActive > 0) {
            ai.eiDimLockActive -= dt;
            // Magenta visual: pulsing particles on player while locked
            if (m_particles) {
                entities.forEach([&](Entity& target) {
                    if (target.getTag() != "player") return;
                    if (!target.hasComponent<TransformComponent>()) return;
                    Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                    float pAngle = ai.eiCorePulse * 3.0f + static_cast<float>(std::rand() % 10) * 0.6f;
                    m_particles->burst({tPos.x + std::cos(pAngle) * 20.0f,
                                        tPos.y + std::sin(pAngle) * 20.0f},
                                       1, {200, 0, 200, 150}, 30.0f, 0.5f);
                });
            }
        }

        // Entropy Missiles - TELEGRAPH
        ai.eiMissileTimer -= dt;
        if (ai.eiMissileTimer <= 0 && dist < 350.0f) {
            float telegraphTime = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 3;
            AudioManager::instance().play(SFX::SniperTelegraph);
        }

        // Spawn entropy minions (max 2)
        int activeMinions = 0;
        entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_entropy_minion" && e.hasComponent<HealthComponent>()) {
                if (e.getComponent<HealthComponent>().currentHP > 0) activeMinions++;
            }
        });
        ai.eiMinionCount = activeMinions;

        if (ai.eiMinionCount < 2) {
            ai.eiMinionTimer -= dt;
            if (ai.eiMinionTimer <= 0) {
                ai.eiMinionTimer = 15.0f * cooldownMult;
                int toSpawn = 2 - ai.eiMinionCount;
                for (int c = 0; c < toSpawn; c++) {
                    float cAngle = static_cast<float>(std::rand() % 628) / 100.0f;
                    Vec2 minionPos = {pos.x + std::cos(cAngle) * 100.0f,
                                      pos.y + std::sin(cAngle) * 60.0f};

                    auto& minion = entities.addEntity("enemy_entropy_minion");
                    minion.addComponent<TransformComponent>(minionPos.x, minionPos.y, 20, 20);
                    auto& ms = minion.addComponent<SpriteComponent>();
                    ms.setColor(120, 0, 160); // Dark purple
                    ms.renderLayer = 2;
                    auto& mp = minion.addComponent<PhysicsBody>();
                    mp.useGravity = true;
                    mp.gravity = 980.0f;
                    mp.friction = 600.0f;
                    auto& mc = minion.addComponent<ColliderComponent>();
                    mc.width = 18; mc.height = 18; mc.offset = {1, 1};
                    mc.layer = LAYER_ENEMY;
                    mc.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;
                    auto& mhp = minion.addComponent<HealthComponent>();
                    mhp.maxHP = 30.0f;
                    mhp.currentHP = 30.0f;
                    auto& mcombat = minion.addComponent<CombatComponent>();
                    mcombat.meleeAttack.damage = 8.0f;
                    mcombat.meleeAttack.range = 25.0f;
                    mcombat.meleeAttack.knockback = 150.0f;
                    auto& mai = minion.addComponent<AIComponent>();
                    mai.enemyType = EnemyType::Walker; // Simple walker AI
                    mai.detectRange = 300.0f;
                    mai.chaseSpeed = 140.0f;
                    mai.attackRange = 25.0f;
                    mai.attackCooldown = 1.5f;
                    mai.patrolStart = {minionPos.x - 80, minionPos.y};
                    mai.patrolEnd = {minionPos.x + 80, minionPos.y};
                    minion.dimension = entity.dimension;

                    ai.eiMinionCount++;
                    if (m_particles)
                        m_particles->burst(minionPos, 15, {120, 0, 180, 255}, 100.0f, 3.0f);
                }
                AudioManager::instance().play(SFX::SummonerSummon);
            }
        }
    }

    // ========================================
    // Phase 3 attacks (<33% HP)
    // ========================================
    if (ai.bossPhase >= 3) {
        // Dimension Shatter: force random dimension switch every 6s - TELEGRAPH
        ai.eiShatterTimer -= dt;
        if (ai.eiShatterTimer <= 0) {
            float telegraphTime = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 4;
            AudioManager::instance().play(SFX::CollapseWarning);
        }

        // Entropy Overload: if player entropy > 70, boss heals 5% max HP per second
        // Note: actual entropy check done by PlayState, boss signals heal via eiOverloadHealAccum
        ai.eiOverloadHealAccum += dt; // PlayState checks entropy and applies healing

        // Final Stand: at 10% HP, massive entropy pulse + all minions explode
        if (hpPct <= 0.10f && !ai.eiFinalStandTriggered) {
            ai.eiFinalStandTriggered = true;

            // Massive visual explosion
            if (m_camera) m_camera->shake(20.0f, 1.0f);
            if (m_combatSystem) m_combatSystem->addHitFreeze(0.2f);
            AudioManager::instance().play(SFX::EntropyPulse);
            AudioManager::instance().play(SFX::SuitEntropyCritical);

            // Massive AoE damage
            entities.forEach([&](Entity& target) {
                if (target.getTag() != "player") return;
                if (!target.hasComponent<TransformComponent>()) return;
                Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                float d = distanceTo(pos, tPos);
                if (d < 300.0f && target.hasComponent<HealthComponent>()) {
                    auto& thp = target.getComponent<HealthComponent>();
                    if (!thp.isInvincible()) {
                        thp.takeDamage(20.0f);
                        if (m_combatSystem)
                            m_combatSystem->addDamageEvent(tPos, 20.0f, true);
                    }
                }
                if (target.hasComponent<PhysicsBody>()) {
                    Vec2 knockDir = (tPos - pos).normalized();
                    target.getComponent<PhysicsBody>().velocity += knockDir * 500.0f;
                }
            });

            // Explode all entropy minions (adds entropy via PlayState)
            entities.forEach([&](Entity& e) {
                if (e.getTag() == "enemy_entropy_minion" && e.hasComponent<HealthComponent>()) {
                    auto& mhp = e.getComponent<HealthComponent>();
                    if (mhp.currentHP > 0) {
                        if (e.hasComponent<TransformComponent>() && m_particles) {
                            Vec2 mPos = e.getComponent<TransformComponent>().getCenter();
                            m_particles->burst(mPos, 25, {140, 0, 200, 255}, 200.0f, 5.0f);
                        }
                        mhp.currentHP = 0;
                        e.destroy();
                    }
                }
            });

            // Massive particle explosion
            if (m_particles) {
                m_particles->burst(pos, 80, {160, 0, 220, 255}, 400.0f, 7.0f);
                for (int i = 0; i < 24; i++) {
                    float angle = i * 6.283185f / 24.0f;
                    Vec2 ring = {pos.x + std::cos(angle) * 150.0f, pos.y + std::sin(angle) * 150.0f};
                    m_particles->burst(ring, 4, {200, 50, 255, 255}, 120.0f, 3.0f);
                }
            }
        }
    }

    // --- Entropy aura visual: persistent pulsing particles around boss ---
    if (m_particles) {
        float auraIntensity = 1.0f + (ai.bossPhase - 1) * 0.5f;
        int particleCount = static_cast<int>(2 * auraIntensity);
        for (int i = 0; i < particleCount; i++) {
            float aAngle = ai.eiCorePulse + i * 3.14159f / particleCount;
            float aRadius = 30.0f + std::sin(ai.eiCorePulse * 1.5f + i) * 10.0f;
            Vec2 ap = {pos.x + std::cos(aAngle) * aRadius, pos.y + std::sin(aAngle) * aRadius};
            Uint8 r = static_cast<Uint8>(100 + 40 * std::sin(ai.eiCorePulse));
            Uint8 b = static_cast<Uint8>(160 + 40 * std::sin(ai.eiCorePulse + 1.0f));
            m_particles->burst(ap, 1, {r, 0, b, 150}, 20.0f, 1.0f);
        }

        // Glowing core effect
        float coreGlow = 0.5f + 0.5f * std::sin(ai.eiCorePulse * 2.0f);
        m_particles->burst(pos, 1, {static_cast<Uint8>(180 * coreGlow), 0,
                                     static_cast<Uint8>(220 * coreGlow), static_cast<Uint8>(100 * coreGlow)},
                          15.0f, 0.5f);
    }

    // Visual: pulsing color per phase
    auto& sprite = entity.getComponent<SpriteComponent>();
    float pulse = 0.5f + 0.5f * std::sin(ai.eiCorePulse);
    switch (ai.bossPhase) {
        case 1:
            sprite.setColor(static_cast<Uint8>(90 + 30 * pulse), static_cast<Uint8>(10 * pulse),
                           static_cast<Uint8>(130 + 30 * pulse));
            break;
        case 2:
            sprite.setColor(static_cast<Uint8>(110 + 50 * pulse), static_cast<Uint8>(15 * pulse),
                           static_cast<Uint8>(150 + 50 * pulse));
            break;
        case 3:
            sprite.setColor(static_cast<Uint8>(140 + 80 * pulse), static_cast<Uint8>(20 * pulse),
                           static_cast<Uint8>(180 + 60 * pulse));
            break;
        case 4:
            sprite.setColor(static_cast<Uint8>(200 + 55 * pulse), static_cast<Uint8>(30 * pulse),
                           static_cast<Uint8>(220 + 35 * pulse));
            break; // Blazing entropy purple
    }
}
