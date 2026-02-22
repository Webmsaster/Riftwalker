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
#include <cmath>

float AISystem::distanceTo(Vec2 a, Vec2 b) const {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

void AISystem::update(EntityManager& entities, float dt, Vec2 playerPos, int playerDimension) {
    auto ents = entities.getEntitiesWithComponent<AIComponent>();

    for (auto* e : ents) {
        if (!e->hasComponent<TransformComponent>()) continue;
        auto& ai = e->getComponent<AIComponent>();

        // Update stun
        if (ai.state == AIState::Stunned) {
            ai.stunTimer -= dt;
            if (ai.stunTimer <= 0) ai.state = AIState::Patrol;
            continue;
        }

        // Check if enemy is in the same dimension as player
        bool sameDim = (e->dimension == 0 || e->dimension == playerDimension);

        switch (ai.enemyType) {
            case EnemyType::Walker:  updateWalker(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Flyer:   updateFlyer(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Turret:  updateTurret(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Charger: updateCharger(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Phaser:   updatePhaser(*e, dt, playerPos, playerDimension); break;
            case EnemyType::Exploder: updateExploder(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities); break;
            case EnemyType::Shielder: updateShielder(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Crawler:  updateCrawler(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Summoner: updateSummoner(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities); break;
            case EnemyType::Sniper:   updateSniper(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities); break;
            case EnemyType::Boss: updateBoss(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities); break;
        }
    }
}

void AISystem::updateWalker(Entity& entity, float dt, Vec2 playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

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
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.chaseSpeed;
            ai.facingRight = dirX > 0;

            if (dist < ai.attackRange) ai.state = AIState::Attack;
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;
            if (ai.attackTimer <= 0) {
                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                ai.attackTimer = ai.attackCooldown;
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

void AISystem::updateFlyer(Entity& entity, float dt, Vec2 playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            Vec2 dir = (target - pos).normalized();
            phys.velocity = dir * ai.patrolSpeed;

            if (distanceTo(pos, target) < 10.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < ai.detectRange) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            // Hover above player, then swoop
            Vec2 hoverTarget = {playerPos.x, playerPos.y - ai.flyHeight};
            Vec2 dir = (hoverTarget - pos).normalized();
            phys.velocity = dir * ai.chaseSpeed;

            ai.attackTimer -= dt;
            if (ai.attackTimer <= 0 && dist < ai.detectRange) {
                ai.state = AIState::Attack;
                ai.targetPosition = playerPos;
            }
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            // Swoop down toward player
            Vec2 dir = (ai.targetPosition - pos).normalized();
            phys.velocity = dir * ai.swoopSpeed;

            if (distanceTo(pos, ai.targetPosition) < 20.0f || pos.y > ai.targetPosition.y + 30.0f) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.attackCooldown;
                auto& combat = entity.getComponent<CombatComponent>();
                combat.startAttack(AttackType::Melee, dir);
            }
            break;
        }
        default: break;
    }
}

void AISystem::updateTurret(Entity& entity, float dt, Vec2 playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    ai.facingRight = playerPos.x > pos.x;

    if (dist < ai.detectRange) {
        ai.attackTimer -= dt;
        if (ai.attackTimer <= 0) {
            auto& combat = entity.getComponent<CombatComponent>();
            Vec2 dir = (playerPos - pos).normalized();
            combat.startAttack(AttackType::Ranged, dir);
            ai.attackTimer = ai.attackCooldown;
        }
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updateCharger(Entity& entity, float dt, Vec2 playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < ai.detectRange) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.chargeWindup;
            }
            break;
        }
        case AIState::Chase: {
            // Wind up before charging
            phys.velocity.x *= 0.9f;
            ai.attackTimer -= dt;

            // Visual warning: flash
            auto& sprite = entity.getComponent<SpriteComponent>();
            sprite.setColor(255, static_cast<Uint8>(120 + 135 * (ai.attackTimer / ai.chargeWindup)), 40);

            if (ai.attackTimer <= 0) {
                ai.state = AIState::Attack;
                ai.attackTimer = ai.chargeTime;
                ai.facingRight = playerPos.x > pos.x;
            }
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            // CHARGE!
            phys.velocity.x = (ai.facingRight ? 1.0f : -1.0f) * ai.chargeSpeed;
            ai.attackTimer -= dt;

            auto& combat = entity.getComponent<CombatComponent>();
            if (!combat.isAttacking) {
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
            }

            if (ai.attackTimer <= 0 || phys.onWallLeft || phys.onWallRight) {
                ai.state = AIState::Stunned;
                ai.stunTimer = 1.0f;
                phys.velocity.x = 0;
                entity.getComponent<SpriteComponent>().setColor(220, 120, 40);
            }
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updatePhaser(Entity& entity, float dt, Vec2 playerPos, int playerDim) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();

    bool sameDim = (entity.dimension == 0 || entity.dimension == playerDim);
    float dist = distanceTo(pos, playerPos);

    // Phase ability
    ai.phaseTimer -= dt;
    if (ai.phaseTimer <= 0 && !sameDim) {
        // Switch to player's dimension
        entity.dimension = playerDim;
        ai.phaseTimer = ai.phaseCooldown;
    } else if (ai.phaseTimer <= 0 && sameDim && dist < ai.detectRange) {
        // Randomly phase out to confuse player
        if (std::rand() % 100 < 30) {
            entity.dimension = (entity.dimension == 1) ? 2 : 1;
            ai.phaseTimer = ai.phaseCooldown * 0.5f;
        }
    }

    // Visual: phaser flickers between colors
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        Uint32 t = SDL_GetTicks();
        if ((t / 200) % 2 == 0) {
            sprite.setColor(100, 50, 200);
        } else {
            sprite.setColor(200, 50, 100);
        }
    }

    // Movement (similar to walker when in same dimension)
    if (sameDim) {
        if (dist < ai.detectRange) {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.chaseSpeed;
            ai.facingRight = dirX > 0;

            if (dist < ai.attackRange) {
                ai.attackTimer -= dt;
                if (ai.attackTimer <= 0) {
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                    combat.startAttack(AttackType::Melee, dir);
                    ai.attackTimer = ai.attackCooldown;
                }
            }
        } else {
            // Patrol
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed;
            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
        }
    }
}

void AISystem::updateExploder(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    // Explode if HP depleted (killed by combat)
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.currentHP <= 0 && !ai.exploded) {
            explode(entity, entities);
            return;
        }
    }

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
            // Rush toward player at high speed
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.chaseSpeed;
            ai.facingRight = dirX > 0;

            // Explode on contact
            if (dist < ai.attackRange) {
                explode(entity, entities);
                return;
            }
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        default: break;
    }

    // Pulsing color when chasing
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        if (ai.state == AIState::Chase) {
            float pulse = (std::sin(SDL_GetTicks() * 0.015f) + 1.0f) * 0.5f;
            sprite.setColor(255, static_cast<Uint8>(60 + 80 * pulse), 20);
        }
    }
}

void AISystem::explode(Entity& entity, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    if (ai.exploded) return;
    ai.exploded = true;

    auto& transform = entity.getComponent<TransformComponent>();
    Vec2 center = transform.getCenter();
    int dim = entity.dimension;

    // AoE damage to all nearby entities
    entities.forEach([&](Entity& target) {
        if (&target == &entity) return;
        if (!target.hasComponent<HealthComponent>() || !target.hasComponent<TransformComponent>()) return;
        if (target.dimension != 0 && target.dimension != dim) return;

        Vec2 targetPos = target.getComponent<TransformComponent>().getCenter();
        float dist = distanceTo(center, targetPos);

        if (dist < ai.explodeRadius) {
            float falloff = 1.0f - (dist / ai.explodeRadius);
            target.getComponent<HealthComponent>().takeDamage(ai.explodeDamage * falloff);

            if (target.hasComponent<PhysicsBody>()) {
                Vec2 knockDir = (targetPos - center).normalized();
                knockDir.y = -0.5f;
                target.getComponent<PhysicsBody>().velocity += knockDir * 400.0f * falloff;
            }
        }
    });

    // Explosion particles
    if (m_particles) {
        m_particles->burst(center, 40, {255, 120, 30, 255}, 250.0f, 5.0f);
        m_particles->burst(center, 20, {255, 220, 50, 255}, 180.0f, 3.0f);
        m_particles->burst(center, 15, {255, 50, 20, 255}, 300.0f, 6.0f);
    }

    if (m_camera) {
        m_camera->shake(12.0f, 0.3f);
    }

    AudioManager::instance().play(SFX::EnemyDeath);
    entity.destroy();
}

void AISystem::updateShielder(Entity& entity, float dt, Vec2 playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * 0.6f;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < ai.detectRange) {
                ai.state = AIState::Chase;
                ai.shieldUp = true;
            }
            break;
        }
        case AIState::Chase: {
            // Slow advance with shield raised, always face player
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.chaseSpeed;
            ai.facingRight = dirX > 0;
            ai.shieldUp = true;

            if (dist < ai.attackRange) ai.state = AIState::Attack;
            if (dist > ai.loseRange) {
                ai.state = AIState::Patrol;
                ai.shieldUp = false;
            }
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;
            if (ai.attackTimer <= 0) {
                // Lower shield briefly to attack
                ai.shieldUp = false;

                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                ai.attackTimer = ai.attackCooldown;
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

void AISystem::updateCrawler(Entity& entity, float dt, Vec2 playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

    if (ai.onCeiling && !ai.dropping) {
        // Patrol on ceiling
        Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
        float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
        phys.velocity.x = dirX * ai.patrolSpeed;
        phys.velocity.y = 0;
        ai.facingRight = dirX > 0;

        if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;

        // Drop when player is directly below and close
        float horizDist = std::abs(pos.x - playerPos.x);
        if (horizDist < 30.0f && playerPos.y > pos.y && dist < ai.detectRange) {
            ai.dropping = true;
            ai.onCeiling = false;
            phys.useGravity = true;
            phys.velocity.y = ai.dropSpeed;
            AudioManager::instance().play(SFX::EnemyHit);
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
        if (dist < ai.detectRange) {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.chaseSpeed;
            ai.facingRight = dirX > 0;

            if (dist < ai.attackRange) {
                ai.attackTimer -= dt;
                if (ai.attackTimer <= 0) {
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                    combat.startAttack(AttackType::Melee, dir);
                    ai.attackTimer = ai.attackCooldown;
                }
            }
        } else {
            // Try to climb back to ceiling (for simplicity, just patrol on ground)
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed;
            ai.facingRight = dirX > 0;
            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
        }
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updateSummoner(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
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
                AudioManager::instance().play(SFX::RiftRepair);
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

void AISystem::updateSniper(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float dist = distanceTo(pos, playerPos);

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
            ai.facingRight = playerPos.x > pos.x;

            // Maintain preferred distance
            if (dist < ai.preferredRange - 50.0f) {
                // Too close, retreat
                float dirX = (playerPos.x > pos.x) ? -1.0f : 1.0f;
                phys.velocity.x = dirX * ai.retreatSpeed;
            } else if (dist > ai.preferredRange + 50.0f) {
                // Too far, approach
                float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
                phys.velocity.x = dirX * ai.patrolSpeed;
            } else {
                // In sweet spot, stop and aim
                phys.velocity.x *= 0.8f;
            }

            // Start telegraph when in range
            ai.attackTimer -= dt;
            if (ai.attackTimer <= 0 && dist < ai.sniperRange && !ai.isTelegraphing) {
                ai.isTelegraphing = true;
                ai.telegraphTimer = ai.telegraphDuration;
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
                    pc.onTrigger = [damage](Entity* self, Entity* other) {
                        if (other->hasComponent<HealthComponent>()) {
                            other->getComponent<HealthComponent>().takeDamage(damage);
                        }
                        self->destroy();
                    };
                    auto& ph = proj.addComponent<HealthComponent>();
                    ph.maxHP = 1; ph.currentHP = 1;
                    ph.onDamage = [&proj](float) { proj.destroy(); };

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
        if (hpPct < 0.33f && ai.bossPhase < 3) {
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
                        target.getComponent<HealthComponent>().takeDamage(15.0f);
                    }
                }
            });
        }
    }

    if (dist < ai.detectRange) {
        float dirX = ai.facingRight ? 1.0f : -1.0f;
        float speed = ai.chaseSpeed * phaseSpeedMult;

        if (ai.bossAttackTimer <= 0) {
            // More patterns available in higher phases
            ai.bossAttackPattern = (ai.bossAttackPattern + 1) % (ai.bossPhase + 4);

            switch (ai.bossAttackPattern) {
                case 0:
                case 1: {
                    // Melee attack
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
                    // Single ranged projectile
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = (playerPos - pos).normalized();
                    combat.startAttack(AttackType::Ranged, dir);
                    ai.bossAttackTimer = 1.5f * phaseCooldownMult;
                    break;
                }
                case 3: {
                    // Ground slam leap
                    if (ai.bossLeapTimer <= 0 && phys.onGround) {
                        phys.velocity.y = -500.0f;
                        phys.velocity.x = dirX * 250.0f;
                        ai.bossLeapTimer = 3.0f * phaseCooldownMult;
                        ai.bossAttackTimer = 0.8f;
                        if (m_particles) {
                            m_particles->burst(pos, 15, {200, 40, 180, 255}, 200.0f, 3.0f);
                        }
                        AudioManager::instance().play(SFX::PlayerDash);
                    } else {
                        ai.bossAttackTimer = 0.5f;
                    }
                    break;
                }
                case 4: {
                    // Multi-projectile fan
                    if (m_combatSystem) {
                        auto& combat = entity.getComponent<CombatComponent>();
                        int projCount = 2 + ai.bossPhase; // 3, 4, or 5
                        float spreadAngle = 0.4f + ai.bossPhase * 0.15f;
                        Vec2 baseDir = (playerPos - pos).normalized();

                        for (int p = 0; p < projCount; p++) {
                            float angle = -spreadAngle / 2.0f + spreadAngle * p / std::max(1, projCount - 1);
                            Vec2 dir = {
                                baseDir.x * std::cos(angle) - baseDir.y * std::sin(angle),
                                baseDir.x * std::sin(angle) + baseDir.y * std::cos(angle)
                            };
                            m_combatSystem->createProjectile(entities, pos, dir,
                                combat.rangedAttack.damage, 400.0f, entity.dimension);
                        }
                        ai.bossAttackTimer = 2.0f * phaseCooldownMult;
                        if (m_particles) {
                            m_particles->burst(pos, 20, {255, 100, 200, 255}, 200.0f, 3.0f);
                        }
                        AudioManager::instance().play(SFX::BossMultiShot);
                    } else {
                        ai.bossAttackTimer = 0.5f;
                    }
                    break;
                }
                case 5: {
                    // Shield burst (Phase 2+)
                    if (ai.bossPhase >= 2 && ai.bossShieldTimer <= 0) {
                        if (entity.hasComponent<HealthComponent>()) {
                            entity.getComponent<HealthComponent>().invulnerable = true;
                        }
                        ai.bossShieldActiveTimer = 1.2f;
                        ai.bossShieldTimer = 5.0f * phaseCooldownMult;
                        ai.bossAttackTimer = 1.5f;
                        if (m_camera) m_camera->shake(8.0f, 0.3f);
                        if (m_particles) {
                            m_particles->burst(pos, 35, {100, 200, 255, 255}, 250.0f, 4.0f);
                        }
                        AudioManager::instance().play(SFX::BossShieldBurst);
                    } else {
                        ai.bossAttackTimer = 0.5f;
                    }
                    break;
                }
                case 6: {
                    // Teleport strike (Phase 3)
                    if (ai.bossPhase >= 3 && ai.bossTeleportTimer <= 0) {
                        // Vanish particles at old position
                        if (m_particles) {
                            m_particles->burst(pos, 25, {150, 50, 200, 255}, 200.0f, 3.0f);
                        }

                        // Teleport behind player
                        float behindDir = (playerPos.x > pos.x) ? 1.0f : -1.0f;
                        transform.position.x = playerPos.x + behindDir * 80.0f;
                        transform.position.y = playerPos.y - 20.0f;
                        ai.facingRight = behindDir < 0;

                        // Appear particles at new position
                        Vec2 newPos = transform.getCenter();
                        if (m_particles) {
                            m_particles->burst(newPos, 25, {200, 50, 150, 255}, 200.0f, 3.0f);
                        }

                        // Immediate melee attack
                        auto& combat = entity.getComponent<CombatComponent>();
                        Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                        combat.cooldownTimer = 0;
                        combat.startAttack(AttackType::Melee, dir);

                        ai.bossTeleportTimer = 5.0f * phaseCooldownMult;
                        ai.bossAttackTimer = 0.8f;
                        if (m_camera) m_camera->shake(6.0f, 0.2f);
                        if (m_combatSystem) m_combatSystem->addHitFreeze(0.05f);
                        AudioManager::instance().play(SFX::BossTeleport);
                    } else {
                        // Fallback melee if teleport on cooldown
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
                    target.getComponent<HealthComponent>().takeDamage(20.0f * falloff);
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
        }
    }
}
