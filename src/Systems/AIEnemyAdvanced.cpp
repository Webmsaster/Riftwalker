// AIEnemyAdvanced.cpp -- Advanced enemy AI updates (Shielder through Mimic)
// Split from AISystem.cpp for maintainability
#include "AISystem.h"
#include "Components/AIComponent.h"
#include "Components/TransformComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/HealthComponent.h"
#include "Components/ColliderComponent.h"
#include "Systems/ParticleSystem.h"
#include "Systems/CombatSystem.h"
#include "Core/AudioManager.h"
#include "Game/Level.h"
#include "Game/Player.h"
#include <cmath>
#include <cstdlib>

void AISystem::updateShielder(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * 0.6f * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) {
                ai.state = AIState::Chase;
                ai.shieldUp = true;
            }
            break;
        }
        case AIState::Chase: {
            // Slow advance with shield raised, always face player
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
            ai.facingRight = dirX > 0;
            ai.shieldUp = true;

            // Shielder Bash: when player is very close, push with shield
            if (ai.shielderBashCooldown > 0) ai.shielderBashCooldown -= dt;
            if (dist < 25.0f && ai.shielderBashCooldown <= 0 && m_player) {
                auto* playerEntity = m_player->getEntity();
                if (playerEntity && playerEntity->hasComponent<PhysicsBody>()) {
                    auto& playerPhys = playerEntity->getComponent<PhysicsBody>();
                    float pushDir = (playerPos.x > pos.x) ? 1.0f : -1.0f;
                    playerPhys.velocity.x += pushDir * 500.0f;
                    playerPhys.velocity.y -= 100.0f;
                    ai.shielderBashCooldown = 3.0f;
                    if (m_particles) {
                        m_particles->burst(pos, 6, {80, 180, 220, 200}, 80.0f, 2.0f);
                    }
                    if (m_camera) m_camera->shake(4.0f, 0.12f);
                    AudioManager::instance().play(SFX::GroundSlam);
                }
            }

            if (dist < ai.attackRange) {
                ai.state = AIState::Attack;
                ai.attackTimer = ai.attackWindup; // wind-up before first attack
            }
            if (dist > ai.loseRange) {
                ai.state = AIState::Patrol;
                ai.shieldUp = false;
            }
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;

            // Visual wind-up telegraph (enrage shortens telegraph)
            float effectiveWindup = ai.attackWindup * (ai.isEnraged ? 0.75f : 1.0f);
            if (ai.attackTimer > 0 && ai.attackTimer < effectiveWindup) {
                attackWindupEffect(entity, ai.attackTimer, effectiveWindup);
            }

            if (ai.attackTimer <= 0) {
                // Lower shield briefly to attack
                ai.shieldUp = false;

                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                float cooldownMult = ai.isEnraged ? 0.7f : 1.0f;
                ai.attackTimer = ai.attackCooldown * cooldownMult;
                if (entity.hasComponent<SpriteComponent>())
                    entity.getComponent<SpriteComponent>().restoreColor();
            }

            // Re-raise shield after attack animation ends
            if (entity.hasComponent<CombatComponent>()) {
                auto& combat = entity.getComponent<CombatComponent>();
                if (!combat.isAttacking && ai.attackTimer < ai.attackCooldown - 0.3f) {
                    ai.shieldUp = true;
                }
            }

            if (dist > ai.attackRange * 1.5f) ai.state = AIState::Chase;
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updateCrawler(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    float effectiveDetect = ai.detectRange * ai.dimDetectMod;

    if (ai.onCeiling && !ai.dropping) {
        // Patrol on ceiling
        Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
        float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
        phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
        phys.velocity.y = 0;
        ai.facingRight = dirX > 0;

        if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;

        // Drop when player is directly below and close
        float horizDist = std::abs(pos.x - playerPos.x);
        if (horizDist < 30.0f && playerPos.y > pos.y && dist < effectiveDetect) {
            ai.dropping = true;
            ai.onCeiling = false;
            phys.useGravity = true;
            phys.velocity.y = ai.dropSpeed;
            AudioManager::instance().play(SFX::CrawlerDrop);
        }
    } else if (ai.dropping) {
        // Falling toward player
        if (phys.onGround) {
            ai.dropping = false;
            ai.state = AIState::Chase;
            // Impact particles
            if (m_particles) {
                Vec2 feet = {pos.x, transform.position.y + transform.height};
                m_particles->burst(feet, 8, {60, 160, 80, 200}, 100.0f, 3.0f);
            }
        }
    } else {
        // On ground: chase like a walker
        if (dist < effectiveDetect) {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.chaseSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (dist < ai.attackRange) {
                ai.attackTimer -= dt;

                // Visual wind-up telegraph
                if (ai.attackTimer > 0 && ai.attackTimer < ai.attackWindup) {
                    attackWindupEffect(entity, ai.attackTimer, ai.attackWindup);
                }

                if (ai.attackTimer <= 0) {
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                    combat.startAttack(AttackType::Melee, dir);
                    ai.attackTimer = ai.attackCooldown / std::max(0.1f, ai.dimSpeedMod);
                    if (entity.hasComponent<SpriteComponent>())
                        entity.getComponent<SpriteComponent>().restoreColor();
                }
            } else {
                // Reset timer for next approach wind-up
                if (ai.attackTimer <= 0) ai.attackTimer = ai.attackWindup;
            }
        } else {
            // Try to climb back to ceiling (for simplicity, just patrol on ground)
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;
            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
        }
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updateSummoner(Entity& entity, float dt, const Vec2& playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    // Count active minions
    ai.activeMinions = 0;
    entities.forEach([&](Entity& e) {
        if (e.isAlive() && e.getTag() == "enemy_minion" && e.dimension == entity.dimension) {
            ai.activeMinions++;
        }
    });

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < ai.detectRange) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            // Keep distance from player, retreat if too close
            if (dist < 100.0f) {
                float dirX = (playerPos.x > pos.x) ? -1.0f : 1.0f;
                phys.velocity.x = dirX * ai.chaseSpeed;
            } else if (dist > ai.attackRange) {
                float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
                phys.velocity.x = dirX * ai.patrolSpeed;
            } else {
                phys.velocity.x *= 0.8f;
            }
            ai.facingRight = playerPos.x > pos.x;

            // Summon minions
            ai.summonTimer -= dt;
            if (ai.summonTimer <= 0 && ai.activeMinions < ai.maxMinions) {
                float offsetX = (ai.facingRight ? 1.0f : -1.0f) * 40.0f;
                Vec2 spawnPos = {pos.x + offsetX, pos.y};

                // Spawn via Enemy factory (uses Walker AI)
                auto& minion = entities.addEntity("enemy_minion");
                minion.dimension = entity.dimension;
                auto& mt = minion.addComponent<TransformComponent>(spawnPos.x, spawnPos.y, 16, 16);
                (void)mt;
                auto& ms = minion.addComponent<SpriteComponent>();
                ms.setColor(200, 100, 255);
                ms.renderLayer = 2;
                auto& mp = minion.addComponent<PhysicsBody>();
                mp.gravity = 980.0f;
                mp.friction = 600.0f;
                auto& mc = minion.addComponent<ColliderComponent>();
                mc.width = 14; mc.height = 14; mc.offset = {1, 1};
                mc.layer = LAYER_ENEMY;
                mc.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;
                auto& mh = minion.addComponent<HealthComponent>();
                mh.maxHP = 10.0f; mh.currentHP = 10.0f;
                auto& mcb = minion.addComponent<CombatComponent>();
                mcb.meleeAttack.damage = 8.0f;
                mcb.meleeAttack.knockback = 100.0f;
                mcb.meleeAttack.cooldown = 1.5f;
                auto& mai = minion.addComponent<AIComponent>();
                mai.enemyType = EnemyType::Walker;
                mai.detectRange = 150.0f;
                mai.attackRange = 20.0f;
                mai.chaseSpeed = 130.0f;
                mai.patrolSpeed = 70.0f;
                mai.patrolStart = spawnPos;
                mai.patrolEnd = {spawnPos.x + 60.0f, spawnPos.y};

                ai.summonTimer = ai.summonCooldown;

                // Summon particles
                if (m_particles) {
                    m_particles->burst(spawnPos, 10, {180, 50, 220, 200}, 80.0f, 4.0f);
                }
                AudioManager::instance().play(SFX::SummonerSummon);
            }

            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        default: break;
    }

    // Pulsing glow when summoning
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        if (ai.summonTimer < 1.0f && ai.activeMinions < ai.maxMinions) {
            float pulse = std::sin(SDL_GetTicks() * 0.01f) * 0.5f + 0.5f;
            sprite.setColor(180 + static_cast<int>(pulse * 75), 50, 220);
        }
    }
}

void AISystem::updateSniper(Entity& entity, float dt, const Vec2& playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    float effectiveDetect = ai.detectRange * ai.dimDetectMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            ai.facingRight = playerPos.x > pos.x;

            // Maintain preferred distance
            if (dist < ai.preferredRange - 50.0f) {
                // Too close, retreat
                float dirX = (playerPos.x > pos.x) ? -1.0f : 1.0f;
                phys.velocity.x = dirX * ai.retreatSpeed * ai.dimSpeedMod;
            } else if (dist > ai.preferredRange + 50.0f) {
                // Too far, approach
                float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
                phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            } else {
                // In sweet spot, stop and aim
                phys.velocity.x *= 0.8f;
            }

            // Start telegraph when in range
            ai.attackTimer -= dt;
            if (ai.attackTimer <= 0 && dist < ai.sniperRange * ai.dimDetectMod && !ai.isTelegraphing) {
                ai.isTelegraphing = true;
                ai.telegraphTimer = ai.telegraphDuration / std::max(0.1f, ai.dimSpeedMod);
                AudioManager::instance().play(SFX::SniperTelegraph);
            }

            // Telegraph countdown
            if (ai.isTelegraphing) {
                ai.telegraphTimer -= dt;
                phys.velocity.x *= 0.3f; // Slow down while aiming

                if (ai.telegraphTimer <= 0) {
                    // Fire high-damage shot
                    ai.isTelegraphing = false;
                    ai.attackTimer = entity.getComponent<CombatComponent>().rangedAttack.cooldown;

                    // Create sniper projectile
                    Vec2 dir = (playerPos - pos).normalized();
                    auto& proj = entities.addEntity("projectile");
                    proj.dimension = entity.dimension;
                    auto& pt = proj.addComponent<TransformComponent>(pos.x, pos.y, 12, 4);
                    (void)pt;
                    auto& ps = proj.addComponent<SpriteComponent>();
                    ps.setColor(255, 220, 50);
                    ps.renderLayer = 3;
                    auto& pp = proj.addComponent<PhysicsBody>();
                    pp.useGravity = false;
                    pp.velocity = dir * 600.0f;
                    auto& pc = proj.addComponent<ColliderComponent>();
                    pc.width = 10; pc.height = 4;
                    pc.layer = LAYER_PROJECTILE;
                    pc.mask = LAYER_TILE | LAYER_PLAYER;
                    pc.type = ColliderType::Trigger;
                    float damage = entity.getComponent<CombatComponent>().rangedAttack.damage;
                    auto* cs = m_combatSystem;
                    pc.onTrigger = [damage, cs](Entity* self, Entity* other) {
                        if (other->hasComponent<HealthComponent>()) {
                            auto& hp = other->getComponent<HealthComponent>();
                            if (!hp.isInvincible()) {
                                hp.takeDamage(damage);
                                if (cs && other->hasComponent<TransformComponent>()) {
                                    bool isPlayer = other->isPlayer;
                                    Vec2 srcPos = self->hasComponent<TransformComponent>()
                                        ? self->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
                                    cs->addDamageEvent(other->getComponent<TransformComponent>().getCenter(), damage, isPlayer, false, false, srcPos);
                                }
                            }
                        }
                        self->destroy();
                    };
                    auto& ph = proj.addComponent<HealthComponent>();
                    ph.maxHP = 1; ph.currentHP = 1;
                    Entity* projPtr = &proj;
                    ph.onDamage = [projPtr](float) { projPtr->destroy(); };

                    AudioManager::instance().play(SFX::RangedShot);
                    if (m_camera) m_camera->shake(3.0f, 0.1f);
                }
            }

            if (dist > ai.loseRange) {
                ai.state = AIState::Patrol;
                ai.isTelegraphing = false;
            }
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        // Flash red during telegraph
        if (ai.isTelegraphing) {
            float flash = std::sin(ai.telegraphTimer * 15.0f) * 0.5f + 0.5f;
            sprite.setColor(200 + static_cast<int>(flash * 55), static_cast<int>(180 * (1.0f - flash)), 40);
        }
    }
}

void AISystem::updateTeleporter(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    // Teleport cooldown
    ai.teleportTimer -= dt;
    ai.afterimageTimer -= dt;
    if (ai.justTeleported) ai.justTeleported = false;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
            ai.facingRight = dirX > 0;

            // Teleport behind player when cooldown ready
            if (ai.teleportTimer <= 0 && dist < ai.detectRange * 1.5f) {
                // Save afterimage position for rendering
                ai.afterimagePos = pos;
                ai.afterimageTimer = 0.3f;

                // Teleport particles at origin
                if (m_particles) {
                    m_particles->burst(pos, 12, {160, 60, 220, 255}, 120.0f, 2.0f);
                }

                // Teleport behind player
                float behindX = playerPos.x + (dirX > 0 ? -60.0f : 60.0f);
                transform.position.x = behindX;
                transform.position.y = playerPos.y - transform.height / 2;
                transform.prevPosition = transform.position;

                // Teleport particles at destination
                if (m_particles) {
                    m_particles->burst(transform.getCenter(), 12, {160, 60, 220, 255}, 120.0f, 2.0f);
                }

                ai.teleportTimer = ai.teleportCooldown;
                ai.justTeleported = true;
                ai.facingRight = playerPos.x > transform.getCenter().x;
                AudioManager::instance().play(SFX::DimensionSwitch);

                // Immediately attempt attack after teleport
                ai.state = AIState::Attack;
                float cooldownMult = ai.isEnraged ? 0.7f : 1.0f;
                ai.attackTimer = 0.15f * cooldownMult; // very short wind-up after teleport (surprise attack)
            }

            if (dist < ai.attackRange) {
                ai.state = AIState::Attack;
                float windupMult = ai.isEnraged ? 0.75f : 1.0f;
                ai.attackTimer = ai.attackWindup * windupMult;
            }
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;

            if (ai.attackTimer > 0) {
                float windupMult = ai.isEnraged ? 0.75f : 1.0f;
                attackWindupEffect(entity, ai.attackTimer, ai.attackWindup * windupMult);
            }

            if (ai.attackTimer <= 0) {
                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                float cooldownMult = ai.isEnraged ? 0.7f : 1.0f;
                ai.attackTimer = ai.attackCooldown * cooldownMult / std::max(0.1f, ai.dimSpeedMod);
                if (entity.hasComponent<SpriteComponent>())
                    entity.getComponent<SpriteComponent>().restoreColor();
            }
            if (dist > ai.attackRange * 1.5f) ai.state = AIState::Chase;
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        // Flicker effect when teleport is nearly ready
        if (ai.teleportTimer < 0.5f && ai.teleportTimer > 0) {
            Uint32 t = SDL_GetTicks();
            sprite.color.a = ((t / 50) % 2 == 0) ? 255 : 128;
        } else {
            sprite.color.a = 255;
        }
    }
}

void AISystem::updateReflector(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    // Shield cycle: 4s up / 1s down
    ai.reflectorShieldTimer += dt;
    if (ai.reflectorShieldUp) {
        if (ai.reflectorShieldTimer >= ai.reflectorShieldCooldown) {
            ai.reflectorShieldUp = false;
            ai.reflectorShieldTimer = 0;
            AudioManager::instance().play(SFX::RiftShieldBurst);
            // Shield drops — visual feedback
            if (m_particles) {
                m_particles->burst(pos, 6, {180, 190, 220, 200}, 60.0f, 1.5f);
            }
        }
    } else {
        if (ai.reflectorShieldTimer >= ai.reflectorShieldDownTime) {
            ai.reflectorShieldUp = true;
            ai.reflectorShieldTimer = 0;
            AudioManager::instance().play(SFX::RiftShieldActivate);
            // Shield raises — visual feedback
            if (m_particles) {
                m_particles->burst(pos, 8, {200, 210, 240, 230}, 80.0f, 2.0f);
            }
        }
    }

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            // Slow advance, always face player
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
            ai.facingRight = dirX > 0;

            if (dist < ai.attackRange) {
                ai.state = AIState::Attack;
                float windupMult = ai.isEnraged ? 0.75f : 1.0f;
                ai.attackTimer = ai.attackWindup * windupMult;
            }
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;
            // Always face player
            ai.facingRight = playerPos.x > pos.x;

            float windupMult = ai.isEnraged ? 0.75f : 1.0f;
            if (ai.attackTimer > 0) {
                attackWindupEffect(entity, ai.attackTimer, ai.attackWindup * windupMult);
            }

            if (ai.attackTimer <= 0) {
                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                float cooldownMult = ai.isEnraged ? 0.7f : 1.0f;
                ai.attackTimer = ai.attackCooldown * cooldownMult / std::max(0.1f, ai.dimSpeedMod);
                if (entity.hasComponent<SpriteComponent>())
                    entity.getComponent<SpriteComponent>().restoreColor();
            }
            if (dist > ai.attackRange * 1.5f) ai.state = AIState::Chase;
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        // Shield visual: brighter when shield is up
        if (ai.reflectorShieldUp) {
            float pulse = std::sin(SDL_GetTicks() * 0.005f) * 0.15f + 0.85f;
            sprite.color.r = static_cast<Uint8>(std::min(255.0f, 180.0f * pulse + 40.0f));
            sprite.color.g = static_cast<Uint8>(std::min(255.0f, 190.0f * pulse + 40.0f));
            sprite.color.b = static_cast<Uint8>(std::min(255.0f, 220.0f * pulse + 30.0f));
        } else {
            sprite.color = {140, 150, 170, 255}; // Dimmer when shield is down
        }
    }
}

void AISystem::updateLeech(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    // Leech gets faster when low HP (<50%)
    float hpRatio = 1.0f;
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        hpRatio = hp.currentHP / std::max(0.01f, hp.maxHP);
    }
    if (hpRatio < 0.5f) {
        effectiveChase *= 1.4f; // Desperate speed boost
    }

    // Drain timer
    ai.leechDrainTimer -= dt;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
            ai.facingRight = dirX > 0;

            // At drain range: drain HP from player, heal self
            if (dist < ai.attackRange && ai.leechDrainTimer <= 0 && m_player) {
                auto* playerEntity = m_player->getEntity();
                if (playerEntity && playerEntity->hasComponent<HealthComponent>()) {
                    auto& playerHP = playerEntity->getComponent<HealthComponent>();
                    if (!playerHP.isInvincible()) {
                        float drainAmount = ai.leechDrainRate;
                        playerHP.takeDamage(drainAmount);
                        // Heal self equal amount
                        if (entity.hasComponent<HealthComponent>()) {
                            entity.getComponent<HealthComponent>().heal(drainAmount);
                        }
                        ai.leechDrainTimer = 0.5f;

                        // Green drain particles from player to leech
                        if (m_particles) {
                            m_particles->burst(playerPos, 3, {50, 200, 60, 200}, 40.0f, 1.5f);
                            m_particles->burst(pos, 3, {80, 255, 80, 200}, 30.0f, 1.0f);
                        }

                        // Damage event for floating numbers (feedbackHandled to avoid extra shake)
                        if (m_combatSystem) {
                            m_combatSystem->addDamageEvent(playerPos, drainAmount, true, false, true, pos);
                        }
                    }
                }
            }

            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        // Pulsing green glow when draining
        if (dist < ai.attackRange && ai.state == AIState::Chase) {
            float pulse = std::sin(SDL_GetTicks() * 0.01f) * 0.3f + 0.7f;
            sprite.color.g = static_cast<Uint8>(std::min(255.0f, 140.0f + 80.0f * pulse));
        } else {
            sprite.color = {50, 140, 60, 255};
        }
    }
}

void AISystem::updateSwarmer(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    // Zigzag movement for unpredictability
    ai.swarmerZigzagPhase += dt * 8.0f;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            // Zigzag: adds vertical oscillation during chase
            float zigzag = std::sin(ai.swarmerZigzagPhase) * 60.0f;
            phys.velocity.x = dirX * effectiveChase + zigzag * 0.3f;
            ai.facingRight = dirX > 0;

            // Jump if below player
            if (playerPos.y < pos.y - 40.0f && phys.onGround) {
                phys.velocity.y = -350.0f;
            }

            if (dist < ai.attackRange) ai.state = AIState::Attack;
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;
            if (ai.attackTimer <= 0) {
                ai.attackTimer = ai.attackCooldown;
                ai.state = AIState::Chase;
            }
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updateGravityWell(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;

    // Hover animation
    ai.gravHoverY += dt * 2.0f;
    float hoverOffset = std::sin(ai.gravHoverY) * 8.0f;
    phys.velocity.y = hoverOffset * 30.0f;

    // Pulse timer for visual effect
    ai.gravPulseTimer += dt;

    switch (ai.state) {
        case AIState::Patrol: {
            // Slow drift
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            // Slowly approach player but maintain distance
            float desiredDist = ai.gravPullRadius * 0.8f;
            if (dist > desiredDist + 20.0f) {
                float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
                phys.velocity.x = dirX * ai.chaseSpeed * ai.dimSpeedMod;
                ai.facingRight = dirX > 0;
            } else {
                phys.velocity.x *= 0.9f;
            }

            // Apply gravity pull to player when in range
            if (dist < ai.gravPullRadius && dist > 10.0f && m_player) {
                auto* playerEntity = m_player->getEntity();
                if (playerEntity && playerEntity->hasComponent<PhysicsBody>()) {
                    auto& playerPhys = playerEntity->getComponent<PhysicsBody>();
                    float pullStrength = ai.gravPullForce * (1.0f - dist / ai.gravPullRadius) * dt;
                    float dx = pos.x - playerPos.x;
                    float dy = pos.y - playerPos.y;
                    float mag = std::sqrt(dx * dx + dy * dy);
                    if (mag > 0.01f) {
                        playerPhys.velocity.x += (dx / mag) * pullStrength;
                        playerPhys.velocity.y += (dy / mag) * pullStrength * 0.5f;
                    }
                }

                // Pull particles + audio feedback
                if (m_particles && ai.gravPulseTimer > 0.3f) {
                    ai.gravPulseTimer = 0;
                    AudioManager::instance().play(SFX::EntropyTendril);
                    m_particles->burst(pos, 4, {120, 60, 200, 150}, ai.gravPullRadius * 0.5f, 0.5f);
                }
            }

            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        default: break;
    }

    // Pulsing purple glow
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        float pulse = std::sin(SDL_GetTicks() * 0.005f) * 0.3f + 0.7f;
        sprite.color.r = static_cast<Uint8>(100 * pulse);
        sprite.color.g = static_cast<Uint8>(50 * pulse);
        sprite.color.b = static_cast<Uint8>(180 * pulse);
    }
}

void AISystem::updateMimic(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    if (!ai.mimicRevealed) {
        // Disguised as crate — completely still
        phys.velocity.x = 0;
        phys.velocity.y = 0;

        // Reveal when player gets close
        if (dist < ai.mimicRevealRange) {
            ai.mimicRevealed = true;
            ai.state = AIState::Chase;
            AudioManager::instance().play(SFX::AnomalySpawn);

            // Surprise burst particles
            if (m_particles) {
                m_particles->burst(pos, 12, {200, 60, 60, 255}, 50.0f, 0.6f);
            }

            // Change color to red to signal danger
            if (entity.hasComponent<SpriteComponent>()) {
                auto& sprite = entity.getComponent<SpriteComponent>();
                sprite.setColor(220, 50, 50);
                sprite.renderLayer = 2; // Move to enemy layer
            }

            // Initial lunge at player
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * 250.0f;
            phys.velocity.y = -200.0f;
            phys.friction = 600.0f; // Now movable
        }
        return;
    }

    // Once revealed, behaves like aggressive walker with lunge attacks
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;
    ai.mimicLungeTimer -= dt;

    switch (ai.state) {
        case AIState::Chase: {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
            ai.facingRight = dirX > 0;

            // Periodic lunge attack
            if (dist < 80.0f && ai.mimicLungeTimer <= 0 && phys.onGround) {
                ai.mimicLungeTimer = 2.0f;
                phys.velocity.x = dirX * 300.0f;
                phys.velocity.y = -180.0f;
            }

            if (dist < ai.attackRange) ai.state = AIState::Attack;
            if (dist > ai.loseRange * 1.5f) ai.state = AIState::Chase; // Never goes back to idle
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;
            if (ai.attackTimer <= 0) {
                ai.attackTimer = ai.attackCooldown;
                ai.state = AIState::Chase;
            }
            break;
        }
        default:
            if (ai.mimicRevealed) ai.state = AIState::Chase;
            break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
    }
}

// Boss update implementations have been moved to separate files:
// BossRiftGuardian.cpp, BossVoidWyrm.cpp, BossDimensionalArchitect.cpp,
// BossTemporalWeaver.cpp, BossVoidSovereign.cpp, BossEntropyIncarnate.cpp
