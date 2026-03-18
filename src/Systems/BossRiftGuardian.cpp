#include "BossAI.h"

void AISystem::updateBoss(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    // Determine phase based on HP
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        float hpPct = hp.getPercent();
        // Phase 4 (Ascension 7+): boss extra phase at 15% HP
        bool extraPhase = (AscensionSystem::currentLevel > 0 &&
            AscensionSystem::getLevel(AscensionSystem::currentLevel).bossExtraPhase);
        if (extraPhase && hpPct < 0.15f && ai.bossPhase < 4) {
            ai.bossPhase = 4;
            ai.bossEnraged = true;
            if (m_camera) m_camera->shake(20.0f, 0.8f);
            if (m_particles) m_particles->burst(pos, 70, {255, 30, 30, 255}, 400.0f, 6.0f);
            AudioManager::instance().play(SFX::SuitEntropyCritical);
        } else if (hpPct < 0.33f && ai.bossPhase < 3) {
            ai.bossPhase = 3;
            ai.bossEnraged = true;
            if (m_camera) m_camera->shake(15.0f, 0.5f);
            if (m_particles) m_particles->burst(pos, 50, {255, 50, 50, 255}, 300.0f, 5.0f);
            AudioManager::instance().play(SFX::SuitEntropyCritical);
        } else if (hpPct < 0.66f && ai.bossPhase < 2) {
            ai.bossPhase = 2;
            if (m_camera) m_camera->shake(10.0f, 0.3f);
            if (m_particles) m_particles->burst(pos, 30, {255, 150, 50, 255}, 250.0f, 4.0f);
            AudioManager::instance().play(SFX::CollapseWarning);
        }
    }

    float phaseSpeedMult = 1.0f + (ai.bossPhase - 1) * 0.25f;
    float phaseCooldownMult = 1.0f - (ai.bossPhase - 1) * 0.2f;

    ai.facingRight = playerPos.x > pos.x;
    ai.bossAttackTimer -= dt;
    ai.bossLeapTimer -= dt;
    ai.bossShieldTimer -= dt;
    ai.bossTeleportTimer -= dt;

    // Shield burst end: release invulnerability and do AoE knockback
    if (ai.bossShieldActiveTimer > 0) {
        ai.bossShieldActiveTimer -= dt;
        if (ai.bossShieldActiveTimer <= 0) {
            if (entity.hasComponent<HealthComponent>()) {
                entity.getComponent<HealthComponent>().invulnerable = false;
            }
            // AoE knockback burst on shield end
            if (m_camera) m_camera->shake(12.0f, 0.3f);
            if (m_combatSystem) m_combatSystem->addHitFreeze(0.06f);
            AudioManager::instance().play(SFX::BossShieldBurst);
            if (m_particles) {
                m_particles->burst(pos, 40, {100, 200, 255, 255}, 350.0f, 4.0f);
            }
            entities.forEach([&](Entity& target) {
                if (&target == &entity) return;
                if (target.getTag() != "player") return;
                if (!target.hasComponent<TransformComponent>() || !target.hasComponent<PhysicsBody>()) return;
                Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                float d = distanceTo(pos, tPos);
                if (d < 150.0f) {
                    Vec2 knockDir = (tPos - pos).normalized();
                    target.getComponent<PhysicsBody>().velocity += knockDir * 400.0f;
                    target.getComponent<PhysicsBody>().velocity.y = -250.0f;
                    if (target.hasComponent<HealthComponent>()) {
                        auto& hp = target.getComponent<HealthComponent>();
                        if (!hp.isInvincible()) {
                            hp.takeDamage(15.0f);
                            if (m_combatSystem)
                                m_combatSystem->addDamageEvent(tPos, 15.0f, true);
                        }
                    }
                }
            });
        }
    }

    // --- Boss Telegraph Processing ---
    if (ai.bossTelegraphTimer > 0) {
        ai.bossTelegraphTimer -= dt;
        phys.velocity.x = 0; // Boss stops during telegraph
        ai.facingRight = playerPos.x > pos.x;

        // Visual warning: pulsing color + particles
        float telegraphMax = std::max(0.1f, 0.5f - (ai.bossPhase - 1) * 0.1f);
        float flashRate = 15.0f + (1.0f - ai.bossTelegraphTimer / telegraphMax) * 20.0f;
        float flash = std::sin(ai.bossTelegraphTimer * flashRate) * 0.5f + 0.5f;

        if (entity.hasComponent<SpriteComponent>()) {
            auto& sprite = entity.getComponent<SpriteComponent>();
            switch (ai.bossTelegraphAttack) {
                case 3: sprite.setColor(255, 100 + (int)(flash * 155), (int)(flash * 100)); break;
                case 4: sprite.setColor(255, (int)(flash * 100), 200); break;
                case 5: sprite.setColor(100 + (int)(flash * 155), 200 + (int)(flash * 55), 255); break;
                case 6: sprite.setColor(150 + (int)(flash * 105), 50, 200 + (int)(flash * 55)); break;
                default: break;
            }
        }

        if (m_particles) {
            SDL_Color warnColor;
            switch (ai.bossTelegraphAttack) {
                case 3: warnColor = {255, 200, 50, 200}; break;
                case 4: warnColor = {255, 80, 200, 200}; break;
                case 5: warnColor = {100, 200, 255, 200}; break;
                case 6: warnColor = {180, 50, 255, 200}; break;
                default: warnColor = {255, 255, 255, 200};
            }
            for (int i = 0; i < 2; i++) {
                float angle = (float)(rand() % 628) / 100.0f;
                Vec2 offset = {std::cos(angle) * 30.0f, std::sin(angle) * 30.0f};
                m_particles->burst(pos + offset, 1, warnColor, 60.0f, 3.0f);
            }
        }

        // Execute attack when telegraph expires
        if (ai.bossTelegraphTimer <= 0) {
            float dirX = ai.facingRight ? 1.0f : -1.0f;
            switch (ai.bossTelegraphAttack) {
                case 3: {
                    // Ground slam leap - execute
                    phys.velocity.y = -500.0f;
                    phys.velocity.x = dirX * 250.0f;
                    ai.bossLeapTimer = 3.0f * phaseCooldownMult;
                    ai.bossAttackTimer = 0.8f;
                    if (m_particles) m_particles->burst(pos, 15, {200, 40, 180, 255}, 200.0f, 3.0f);
                    AudioManager::instance().play(SFX::PlayerDash);
                    break;
                }
                case 4: {
                    // Multi-projectile fan - execute
                    if (m_combatSystem) {
                        auto& combat = entity.getComponent<CombatComponent>();
                        int projCount = 2 + ai.bossPhase;
                        float spreadAngle = 0.4f + ai.bossPhase * 0.15f;
                        Vec2 baseDir = ai.bossTelegraphDir;
                        for (int p = 0; p < projCount; p++) {
                            float angle = -spreadAngle / 2.0f + spreadAngle * p / std::max(1, projCount - 1);
                            Vec2 dir = {
                                baseDir.x * std::cos(angle) - baseDir.y * std::sin(angle),
                                baseDir.x * std::sin(angle) + baseDir.y * std::cos(angle)
                            };
                            m_combatSystem->createProjectile(entities, pos, dir,
                                combat.rangedAttack.damage, 400.0f, entity.dimension);
                        }
                        if (m_particles) m_particles->burst(pos, 20, {255, 100, 200, 255}, 200.0f, 3.0f);
                        AudioManager::instance().play(SFX::BossMultiShot);
                    }
                    ai.bossAttackTimer = 2.0f * phaseCooldownMult;
                    break;
                }
                case 5: {
                    // Shield burst - execute
                    if (entity.hasComponent<HealthComponent>()) {
                        entity.getComponent<HealthComponent>().invulnerable = true;
                    }
                    ai.bossShieldActiveTimer = 1.2f;
                    ai.bossShieldTimer = 5.0f * phaseCooldownMult;
                    ai.bossAttackTimer = 1.5f;
                    if (m_camera) m_camera->shake(8.0f, 0.3f);
                    if (m_particles) m_particles->burst(pos, 35, {100, 200, 255, 255}, 250.0f, 4.0f);
                    AudioManager::instance().play(SFX::BossShieldBurst);
                    break;
                }
                case 6: {
                    // Teleport strike - execute
                    if (m_particles) m_particles->burst(pos, 25, {150, 50, 200, 255}, 200.0f, 3.0f);
                    float behindDir = (playerPos.x > pos.x) ? 1.0f : -1.0f;
                    transform.position.x = playerPos.x + behindDir * 80.0f;
                    transform.position.y = playerPos.y - 20.0f;
                    transform.prevPosition = transform.position; // Sync for interpolation
                    ai.facingRight = behindDir < 0;
                    Vec2 newPos = transform.getCenter();
                    if (m_particles) m_particles->burst(newPos, 25, {200, 50, 150, 255}, 200.0f, 3.0f);
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                    combat.cooldownTimer = 0;
                    combat.startAttack(AttackType::Melee, dir);
                    ai.bossTeleportTimer = 5.0f * phaseCooldownMult;
                    ai.bossAttackTimer = 0.8f;
                    if (m_camera) m_camera->shake(6.0f, 0.2f);
                    if (m_combatSystem) m_combatSystem->addHitFreeze(0.05f);
                    AudioManager::instance().play(SFX::BossTeleport);
                    break;
                }
                default: break;
            }
            ai.bossTelegraphAttack = -1;
            // Reset sprite color
            if (entity.hasComponent<SpriteComponent>()) {
                entity.getComponent<SpriteComponent>().setColor(255, 255, 255);
            }
        }
        return; // Skip normal AI during telegraph
    }

    if (dist < ai.detectRange) {
        float dirX = ai.facingRight ? 1.0f : -1.0f;
        float speed = ai.chaseSpeed * phaseSpeedMult;

        if (ai.bossAttackTimer <= 0) {
            // More patterns available in higher phases
            ai.bossAttackPattern = (ai.bossAttackPattern + 1) % (ai.bossPhase + 4);
            float telegraphTime = 0.5f - (ai.bossPhase - 1) * 0.1f; // 0.5s, 0.4s, 0.3s per phase

            switch (ai.bossAttackPattern) {
                case 0:
                case 1: {
                    // Melee attack - no telegraph (fast, short range)
                    if (dist < ai.attackRange * 2.0f) {
                        auto& combat = entity.getComponent<CombatComponent>();
                        Vec2 dir = {dirX, 0.0f};
                        combat.startAttack(AttackType::Melee, dir);
                        ai.bossAttackTimer = ai.attackCooldown * phaseCooldownMult;
                    } else {
                        ai.bossAttackTimer = 0.3f;
                    }
                    break;
                }
                case 2: {
                    // Single ranged projectile - no telegraph (weak)
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = (playerPos - pos).normalized();
                    combat.startAttack(AttackType::Ranged, dir);
                    ai.bossAttackTimer = 1.5f * phaseCooldownMult;
                    break;
                }
                case 3: {
                    // Ground slam leap - TELEGRAPH
                    if (ai.bossLeapTimer <= 0 && phys.onGround) {
                        ai.bossTelegraphTimer = telegraphTime;
                        ai.bossTelegraphAttack = 3;
                        ai.bossTelegraphDir = {dirX, 0.0f};
                        ai.bossAttackTimer = 0.3f;
                        AudioManager::instance().play(SFX::CollapseWarning);
                    } else {
                        ai.bossAttackTimer = 0.5f;
                    }
                    break;
                }
                case 4: {
                    // Multi-projectile fan - TELEGRAPH
                    if (m_combatSystem) {
                        ai.bossTelegraphTimer = telegraphTime;
                        ai.bossTelegraphAttack = 4;
                        ai.bossTelegraphDir = (playerPos - pos).normalized();
                        ai.bossAttackTimer = 0.3f;
                        AudioManager::instance().play(SFX::SniperTelegraph);
                    } else {
                        ai.bossAttackTimer = 0.5f;
                    }
                    break;
                }
                case 5: {
                    // Shield burst (Phase 2+) - TELEGRAPH
                    if (ai.bossPhase >= 2 && ai.bossShieldTimer <= 0) {
                        ai.bossTelegraphTimer = telegraphTime;
                        ai.bossTelegraphAttack = 5;
                        ai.bossAttackTimer = 0.3f;
                        AudioManager::instance().play(SFX::CollapseWarning);
                    } else {
                        ai.bossAttackTimer = 0.5f;
                    }
                    break;
                }
                case 6: {
                    // Teleport strike (Phase 3) - TELEGRAPH
                    if (ai.bossPhase >= 3 && ai.bossTeleportTimer <= 0) {
                        ai.bossTelegraphTimer = telegraphTime;
                        ai.bossTelegraphAttack = 6;
                        ai.bossAttackTimer = 0.3f;
                        AudioManager::instance().play(SFX::SniperTelegraph);
                    } else {
                        if (dist < ai.attackRange * 1.5f) {
                            auto& combat = entity.getComponent<CombatComponent>();
                            Vec2 dir = {dirX, 0.0f};
                            combat.startAttack(AttackType::Melee, dir);
                        }
                        ai.bossAttackTimer = 0.6f * phaseCooldownMult;
                    }
                    break;
                }
                default: {
                    if (dist < ai.attackRange * 1.5f) {
                        auto& combat = entity.getComponent<CombatComponent>();
                        Vec2 dir = {dirX, 0.0f};
                        combat.startAttack(AttackType::Melee, dir);
                    }
                    ai.bossAttackTimer = 0.6f * phaseCooldownMult;
                    break;
                }
            }
        }

        // Ground slam landing AoE
        if (phys.onGround && ai.bossLeapTimer > 2.0f * phaseCooldownMult) {
            ai.bossLeapTimer = 1.5f * phaseCooldownMult;
            if (m_camera) m_camera->shake(10.0f, 0.3f);
            AudioManager::instance().play(SFX::SpikeDamage);
            if (m_particles) {
                m_particles->burst(pos, 30, {180, 60, 255, 255}, 300.0f, 4.0f);
            }
            entities.forEach([&](Entity& target) {
                if (&target == &entity) return;
                if (target.getTag() != "player") return;
                if (!target.hasComponent<HealthComponent>() || !target.hasComponent<TransformComponent>()) return;
                Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                float d = distanceTo(pos, tPos);
                if (d < 120.0f) {
                    float falloff = 1.0f - (d / 120.0f);
                    auto& hp = target.getComponent<HealthComponent>();
                    if (!hp.isInvincible()) {
                        float dmg = 20.0f * falloff;
                        hp.takeDamage(dmg);
                        if (m_combatSystem)
                            m_combatSystem->addDamageEvent(tPos, dmg, true);
                    }
                    if (target.hasComponent<PhysicsBody>()) {
                        target.getComponent<PhysicsBody>().velocity.y = -300.0f * falloff;
                    }
                }
            });
        }

        phys.velocity.x = dirX * speed;
    } else {
        Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
        float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
        phys.velocity.x = dirX * ai.patrolSpeed;
        if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        float pulse = (std::sin(SDL_GetTicks() * 0.005f * ai.bossPhase) + 1.0f) * 0.5f;
        switch (ai.bossPhase) {
            case 1: sprite.setColor(200, static_cast<Uint8>(40 + 40 * pulse), 180); break;
            case 2: sprite.setColor(220, static_cast<Uint8>(100 * pulse), 100); break;
            case 3: sprite.setColor(255, static_cast<Uint8>(40 * pulse), static_cast<Uint8>(40 * pulse)); break;
            case 4: sprite.setColor(255, static_cast<Uint8>(20 * pulse), static_cast<Uint8>(60 + 40 * pulse)); break; // Deep crimson-magenta
        }
    }
}
