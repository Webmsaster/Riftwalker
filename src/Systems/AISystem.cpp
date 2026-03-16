#include "AISystem.h"
#include "Components/AIComponent.h"
#include "Components/TransformComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/ColliderComponent.h"
#include "Systems/ParticleSystem.h"
#include "Systems/CombatSystem.h"
#include "Core/AudioManager.h"
#include "Game/Level.h"
#include "Game/SpriteConfig.h"
#include <cmath>
#include <cstdlib>

void AISystem::attackWindupEffect(Entity& entity, float timer, float windupTime) {
    if (windupTime <= 0) return;
    if (!entity.hasComponent<TransformComponent>()) return;

    float progress = 1.0f - (timer / windupTime); // 0→1 as attack approaches

    // Color pulse: flash from base color toward bright red as attack nears
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        auto& base = sprite.baseColor;
        float flash = std::sin(progress * 15.0f) * 0.5f + 0.5f;
        float blend = progress * flash; // 0→1, pulsing
        Uint8 r = static_cast<Uint8>(base.r + (255 - base.r) * blend);
        Uint8 g = static_cast<Uint8>(base.g * (1.0f - blend * 0.7f));
        Uint8 b = static_cast<Uint8>(base.b * (1.0f - blend * 0.7f));
        sprite.color = {r, g, b, base.a};
    }

    // Warning particles: red-orange sparks above enemy, intensifying
    if (m_particles && progress > 0.3f) {
        auto& t = entity.getComponent<TransformComponent>();
        Vec2 aboveHead = {t.getCenter().x, t.position.y - 4.0f};
        float offsetX = (static_cast<float>(std::rand() % 20) - 10.0f);
        aboveHead.x += offsetX;
        SDL_Color warnColor = {255, static_cast<Uint8>(80 + 100 * (1.0f - progress)), 40, 200};
        float size = 1.5f + progress * 2.0f;
        m_particles->burst(aboveHead, 1, warnColor, 20.0f + progress * 40.0f, size);
    }
}

void AISystem::updateEnemyAnimation(Entity& entity) {
    if (!entity.hasComponent<SpriteComponent>() || !entity.hasComponent<AnimationComponent>())
        return;
    auto& sprite = entity.getComponent<SpriteComponent>();
    if (!sprite.texture) return; // No sprite loaded, skip

    auto& ai = entity.getComponent<AIComponent>();
    auto& anim = entity.getComponent<AnimationComponent>();

    // Map AIState to animation name
    const char* animName = Anim::Idle;
    bool isBoss = (ai.enemyType == EnemyType::Boss);

    if (entity.hasComponent<HealthComponent>() && entity.getComponent<HealthComponent>().currentHP <= 0) {
        animName = Anim::Dead;
    } else if (ai.state == AIState::Stunned || ai.state == AIState::Juggled) {
        animName = Anim::Hurt;
    } else if (ai.state == AIState::Attack) {
        animName = isBoss ? Anim::Attack1 : Anim::Attack;
    } else if (ai.state == AIState::Chase) {
        animName = isBoss ? Anim::Move : Anim::Walk;
    } else if (ai.state == AIState::Patrol) {
        animName = isBoss ? Anim::Move : Anim::Walk;
    } else if (ai.state == AIState::Flee) {
        animName = isBoss ? Anim::Move : Anim::Walk;
    } else {
        animName = Anim::Idle;
    }

    // Boss-specific: hit flash triggers hurt
    if (isBoss && entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.invincibilityTimer > hp.invincibilityTime - 0.12f && hp.invincibilityTimer > 0) {
            animName = Anim::Hurt;
        }
    }

    anim.play(animName);
}

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

        // Spawn animation: skip AI and keep invulnerable while spawning in
        if (ai.spawnTimer > 0) {
            ai.spawnTimer -= dt;
            if (e->hasComponent<HealthComponent>()) {
                e->getComponent<HealthComponent>().invincibilityTimer = 0.1f;
            }
            // Spawn particles (brief flash at start)
            if (ai.spawnTimer > 0 && m_particles) {
                auto& t = e->getComponent<TransformComponent>();
                if (static_cast<int>(ai.spawnTimer * 20) % 3 == 0) {
                    m_particles->burst(t.getCenter(), 2, {255, 255, 255, 150}, 40.0f, 1.5f);
                }
            }
            continue;
        }

        // Update stun
        if (ai.state == AIState::Stunned) {
            ai.stunTimer -= dt;
            if (ai.stunTimer <= 0) ai.state = AIState::Patrol;
            continue;
        }

        // Update juggle state
        if (ai.state == AIState::Juggled) {
            ai.juggleTimer -= dt;
            bool landed = false;
            if (e->hasComponent<PhysicsBody>()) {
                landed = e->getComponent<PhysicsBody>().onGround;
            }
            if (ai.juggleTimer <= 0 || landed) {
                // End juggle: brief stun on landing
                ai.stun(0.3f);
                ai.juggleHitCount = 0;
            }
            continue;
        }

        // Elite modifier updates
        if (ai.isElite) {
            ai.eliteGlowTimer += dt;

            // Shielded: regenerate shield (timer starts at 5s)
            if (ai.eliteMod == EliteModifier::Shielded) {
                if (ai.eliteShieldRegenTimer <= 0 && ai.eliteShieldHP >= 30.0f) {
                    ai.eliteShieldRegenTimer = 5.0f; // Initialize timer on first frame
                }
                ai.eliteShieldRegenTimer -= dt;
                if (ai.eliteShieldRegenTimer <= 0 && ai.eliteShieldHP < 30.0f) {
                    ai.eliteShieldHP = 30.0f;
                    ai.eliteShieldRegenTimer = 5.0f;
                    if (m_particles) {
                        auto& t = e->getComponent<TransformComponent>();
                        m_particles->burst(t.getCenter(), 10, {80, 150, 255, 200}, 80.0f, 2.0f);
                    }
                }
            }

            // Teleporter: teleport behind player
            if (ai.eliteMod == EliteModifier::Teleporter && e->hasComponent<TransformComponent>()) {
                ai.eliteTeleportTimer -= dt;
                if (ai.eliteTeleportTimer <= 0) {
                    ai.eliteTeleportTimer = 3.0f;
                    auto& t = e->getComponent<TransformComponent>();
                    float dx = playerPos.x - t.getCenter().x;
                    float behindX = playerPos.x + (dx > 0 ? -60.0f : 60.0f);
                    if (m_particles) {
                        m_particles->burst(t.getCenter(), 12, {150, 80, 220, 255}, 120.0f, 2.0f);
                    }
                    t.position.x = behindX;
                    t.position.y = playerPos.y - t.height / 2;
                    if (m_particles) {
                        m_particles->burst(t.getCenter(), 12, {150, 80, 220, 255}, 120.0f, 2.0f);
                    }
                }
            }

            // Vampiric: heal percentage of damage dealt is handled in CombatSystem
        }

        // Dimension behavior modifiers for dim-0 enemies
        if (e->dimension == 0) {
            if (playerDimension == 1) {
                // Dim A (blue/calm): enemies are calmer
                ai.dimSpeedMod = 0.75f;
                ai.dimDamageMod = 0.85f;
                ai.dimDetectMod = 1.3f;
            } else {
                // Dim B (red/hostile): enemies are aggressive
                ai.dimSpeedMod = 1.3f;
                ai.dimDamageMod = 1.25f;
                ai.dimDetectMod = 0.8f;
            }

            // Visual: periodic particle aura indicating dimension state
            if (m_particles && e->hasComponent<TransformComponent>()) {
                auto& t = e->getComponent<TransformComponent>();
                // Emit a small particle every ~2s (stagger by entity position)
                Uint32 ticks = SDL_GetTicks();
                int hash = static_cast<int>(t.position.x * 7 + t.position.y * 13) & 0x7FF;
                if (((ticks + hash) % 2000) < static_cast<Uint32>(dt * 1000)) {
                    SDL_Color auraColor = (playerDimension == 1)
                        ? SDL_Color{80, 120, 220, 120}   // Blue aura (calm)
                        : SDL_Color{220, 80, 60, 120};   // Red aura (aggressive)
                    m_particles->burst(t.getCenter(), 3, auraColor, 30.0f, 0.8f);
                }
            }
        } else {
            ai.dimSpeedMod = 1.0f;
            ai.dimDamageMod = 1.0f;
            ai.dimDetectMod = 1.0f;
        }

        // Ice weapon freeze slow: 50% speed reduction while frozen
        if (ai.freezeTimer > 0) {
            ai.dimSpeedMod *= 0.5f;
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
            case EnemyType::Boss: {
                int bt = e->getComponent<AIComponent>().bossType;
                if (bt == 5)
                    updateEntropyIncarnate(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 4)
                    updateVoidSovereign(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 3)
                    updateTemporalWeaver(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 2)
                    updateDimensionalArchitect(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 1)
                    updateVoidWyrm(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else
                    updateBoss(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                break;
            }
        }

        // Drive sprite animation based on AI state
        updateEnemyAnimation(*e);
    }
}

void AISystem::updateWalker(Entity& entity, float dt, Vec2 playerPos) {
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

            if (dist < ai.attackRange) {
                ai.state = AIState::Attack;
                ai.attackTimer = ai.attackWindup; // wind-up before first attack
            }
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;

            // Visual wind-up telegraph: warning particles + color pulse before attack
            if (ai.attackTimer > 0 && ai.attackTimer < ai.attackWindup) {
                attackWindupEffect(entity, ai.attackTimer, ai.attackWindup);
            }

            if (ai.attackTimer <= 0) {
                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                ai.attackTimer = ai.attackCooldown / ai.dimSpeedMod; // Faster attacks in dim B
                if (entity.hasComponent<SpriteComponent>())
                    entity.getComponent<SpriteComponent>().restoreColor();
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
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            Vec2 dir = (target - pos).normalized();
            phys.velocity = dir * ai.patrolSpeed * ai.dimSpeedMod;

            if (distanceTo(pos, target) < 10.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.attackCooldown * 0.5f; // brief hover before first swoop
            }
            break;
        }
        case AIState::Chase: {
            // Hover above player, then swoop
            Vec2 hoverTarget = {playerPos.x, playerPos.y - ai.flyHeight};
            Vec2 dir = (hoverTarget - pos).normalized();
            phys.velocity = dir * effectiveChase;

            ai.attackTimer -= dt;

            // Visual wind-up telegraph before swoop
            if (ai.attackTimer > 0 && ai.attackTimer < ai.attackWindup && dist < effectiveDetect) {
                attackWindupEffect(entity, ai.attackTimer, ai.attackWindup);
                phys.velocity = phys.velocity * 0.5f; // slow down while winding up
            }

            if (ai.attackTimer <= 0 && dist < effectiveDetect) {
                ai.state = AIState::Attack;
                ai.targetPosition = playerPos;
            }
            if (dist > ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            // Swoop down toward player
            Vec2 dir = (ai.targetPosition - pos).normalized();
            phys.velocity = dir * ai.swoopSpeed * ai.dimSpeedMod;

            if (distanceTo(pos, ai.targetPosition) < 20.0f || pos.y > ai.targetPosition.y + 30.0f) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.attackCooldown / ai.dimSpeedMod;
                auto& combat = entity.getComponent<CombatComponent>();
                combat.startAttack(AttackType::Melee, dir);
                if (entity.hasComponent<SpriteComponent>())
                    entity.getComponent<SpriteComponent>().restoreColor();
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

    if (dist < ai.detectRange * ai.dimDetectMod) {
        ai.attackTimer -= dt;
        if (ai.attackTimer <= 0) {
            auto& combat = entity.getComponent<CombatComponent>();
            Vec2 dir = (playerPos - pos).normalized();
            combat.startAttack(AttackType::Ranged, dir);
            ai.attackTimer = ai.attackCooldown / ai.dimSpeedMod;
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
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (dist < effectiveDetect) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.chargeWindup / ai.dimSpeedMod;
            }
            break;
        }
        case AIState::Chase: {
            // Wind up before charging
            phys.velocity.x *= 0.9f;
            ai.attackTimer -= dt;

            // Visual warning: flash
            float windupTime = ai.chargeWindup / ai.dimSpeedMod;
            auto& sprite = entity.getComponent<SpriteComponent>();
            sprite.setColor(255, static_cast<Uint8>(120 + 135 * (ai.attackTimer / windupTime)), 40);

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
            phys.velocity.x = (ai.facingRight ? 1.0f : -1.0f) * ai.chargeSpeed * ai.dimSpeedMod;
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

                // Visual wind-up telegraph
                if (ai.attackTimer > 0 && ai.attackTimer < ai.attackWindup) {
                    attackWindupEffect(entity, ai.attackTimer, ai.attackWindup);
                }

                if (ai.attackTimer <= 0) {
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                    combat.startAttack(AttackType::Melee, dir);
                    ai.attackTimer = ai.attackCooldown;
                    // Phaser has its own flicker colors; don't restore here
                }
            } else {
                // Reset timer for next approach wind-up
                if (ai.attackTimer <= 0) ai.attackTimer = ai.attackWindup;
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

    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

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
            // Rush toward player at high speed
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
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
            auto& hp = target.getComponent<HealthComponent>();
            if (!hp.isInvincible()) {
                float dmg = ai.explodeDamage * falloff;
                hp.takeDamage(dmg);
                if (m_combatSystem) {
                    bool isPlayer = (target.getTag() == "player");
                    m_combatSystem->addDamageEvent(targetPos, dmg, isPlayer);
                }
            }

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

            // Visual wind-up telegraph
            if (ai.attackTimer > 0 && ai.attackTimer < ai.attackWindup) {
                attackWindupEffect(entity, ai.attackTimer, ai.attackWindup);
            }

            if (ai.attackTimer <= 0) {
                // Lower shield briefly to attack
                ai.shieldUp = false;

                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                ai.attackTimer = ai.attackCooldown;
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

void AISystem::updateCrawler(Entity& entity, float dt, Vec2 playerPos) {
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
                    ai.attackTimer = ai.attackCooldown / ai.dimSpeedMod;
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

void AISystem::updateSniper(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
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
                ai.telegraphTimer = ai.telegraphDuration / ai.dimSpeedMod;
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
                                    bool isPlayer = (other->getTag() == "player");
                                    cs->addDamageEvent(other->getComponent<TransformComponent>().getCenter(), damage, isPlayer);
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
        }
    }
}

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

void AISystem::updateDimensionalArchitect(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& hp = entity.getComponent<HealthComponent>();
    Vec2 pos = transform.getCenter();

    float dist = distanceTo(pos, playerPos);
    float hpPct = hp.getPercent();

    // Phase transitions
    int newPhase = (hpPct > 0.66f) ? 1 : (hpPct > 0.33f) ? 2 : 3;
    if (newPhase != ai.bossPhase) {
        ai.bossPhase = newPhase;
        ai.archSwapSize = (newPhase == 1) ? 3 : (newPhase == 2) ? 5 : 7;
        // Reset timers so next attacks use new phase parameters
        ai.archSwapTimer = 1.5f;
        ai.archRiftTimer = 2.0f;
        ai.archConstructTimer = 1.0f;
        ai.archConstructing = false;
        if (m_particles) {
            m_particles->burst(pos, 30, {100, 180, 255, 220}, 200.0f, 6.0f);
        }
        AudioManager::instance().play(SFX::ArchCollapse);
    }

    // Hover animation (floating boss)
    ai.archHoverY += dt * 2.5f;
    if (ai.archHoverY > 6.283185f) ai.archHoverY -= 6.283185f;
    float hoverOffset = std::sin(ai.archHoverY) * 8.0f;
    phys.velocity.y = hoverOffset * 3.0f;
    phys.onGround = false;
    phys.useGravity = false;

    // Facing
    ai.facingRight = playerPos.x > pos.x;

    // Beam angle rotation
    ai.archBeamAngle += dt * 1.5f * ai.bossPhase;
    if (ai.archBeamAngle > 6.283185f) ai.archBeamAngle -= 6.283185f;

    float phaseCooldownMult = 1.0f - (ai.bossPhase - 1) * 0.2f; // 1.0, 0.8, 0.6
    float telegraphTime = 0.6f - (ai.bossPhase - 1) * 0.1f; // 0.6, 0.5, 0.4

    // --- Dimensional Architect Telegraph Processing ---
    if (ai.bossTelegraphTimer > 0) {
        ai.bossTelegraphTimer -= dt;
        phys.velocity.x *= 0.8f;
        phys.velocity.y *= 0.8f;

        float telegraphMax = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
        float flashRate = 12.0f + (1.0f - ai.bossTelegraphTimer / telegraphMax) * 25.0f;
        float flash = std::sin(ai.bossTelegraphTimer * flashRate) * 0.5f + 0.5f;

        if (entity.hasComponent<SpriteComponent>()) {
            auto& sprite = entity.getComponent<SpriteComponent>();
            switch (ai.bossTelegraphAttack) {
                case 1: sprite.setColor(static_cast<Uint8>(80 + 80 * flash), static_cast<Uint8>(160 + 60 * flash), 255); break; // beam: bright blue
                case 2: sprite.setColor(static_cast<Uint8>(200 + 55 * flash), static_cast<Uint8>(60 * flash), static_cast<Uint8>(120 + 60 * flash)); break; // rift: red-magenta
                case 3: sprite.setColor(static_cast<Uint8>(180 + 75 * flash), static_cast<Uint8>(100 * flash), 255); break; // collapse: deep purple
                default: break;
            }
        }

        if (m_particles) {
            SDL_Color warnColor;
            switch (ai.bossTelegraphAttack) {
                case 1: warnColor = {100, 180, 255, 200}; break;
                case 2: warnColor = {255, 80, 140, 200}; break;
                case 3: warnColor = {200, 120, 255, 200}; break;
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
                case 1: {
                    // Construct Beam - execute
                    float damage = entity.getComponent<CombatComponent>().rangedAttack.damage;
                    Vec2 dir = ai.bossTelegraphDir;
                    auto& proj = entities.addEntity("projectile");
                    proj.dimension = entity.dimension;
                    proj.addComponent<TransformComponent>(pos.x - 6, pos.y - 6, 12, 12);
                    auto& ps = proj.addComponent<SpriteComponent>();
                    ps.setColor(80, 160, 255);
                    ps.renderLayer = 5;
                    auto& pp = proj.addComponent<PhysicsBody>();
                    pp.useGravity = false;
                    pp.velocity = dir * 220.0f;
                    auto& pc = proj.addComponent<ColliderComponent>();
                    pc.width = 10; pc.height = 10;
                    pc.layer = LAYER_PROJECTILE;
                    pc.mask = LAYER_TILE | LAYER_PLAYER;
                    pc.type = ColliderType::Trigger;
                    auto* cs = m_combatSystem;
                    pc.onTrigger = [damage, cs](Entity* self, Entity* other) {
                        if (other->hasComponent<HealthComponent>()) {
                            auto& hp = other->getComponent<HealthComponent>();
                            if (!hp.isInvincible()) {
                                hp.takeDamage(damage);
                                if (cs && other->hasComponent<TransformComponent>()) {
                                    bool isPlayer = (other->getTag() == "player");
                                    cs->addDamageEvent(other->getComponent<TransformComponent>().getCenter(), damage, isPlayer);
                                }
                            }
                        }
                        self->destroy();
                    };
                    AudioManager::instance().play(SFX::ArchBeam);
                    ai.archConstructTimer = 3.0f * phaseCooldownMult;

                    // Phase 3: Construct Storm extra projectiles alongside beam
                    if (ai.bossPhase >= 3) {
                        float stormDmg = damage * 0.6f;
                        for (int si = -1; si <= 1; si += 2) {
                            float sAngle = std::atan2(dir.y, dir.x) + si * 0.4f;
                            Vec2 sDir = {std::cos(sAngle), std::sin(sAngle)};
                            auto& sproj = entities.addEntity("projectile");
                            sproj.dimension = entity.dimension;
                            sproj.addComponent<TransformComponent>(pos.x - 5, pos.y - 5, 10, 10);
                            auto& sps = sproj.addComponent<SpriteComponent>();
                            sps.setColor(150, 100, 255);
                            sps.renderLayer = 5;
                            auto& spp = sproj.addComponent<PhysicsBody>();
                            spp.useGravity = false;
                            spp.velocity = sDir * 180.0f;
                            auto& spc = sproj.addComponent<ColliderComponent>();
                            spc.width = 8; spc.height = 8;
                            spc.layer = LAYER_PROJECTILE;
                            spc.mask = LAYER_TILE | LAYER_PLAYER;
                            spc.type = ColliderType::Trigger;
                            spc.onTrigger = [stormDmg, cs](Entity* self, Entity* other) {
                                if (other->hasComponent<HealthComponent>()) {
                                    auto& hp = other->getComponent<HealthComponent>();
                                    if (!hp.isInvincible()) {
                                        hp.takeDamage(stormDmg);
                                        if (cs && other->hasComponent<TransformComponent>()) {
                                            bool isPlayer = (other->getTag() == "player");
                                            cs->addDamageEvent(other->getComponent<TransformComponent>().getCenter(), stormDmg, isPlayer);
                                        }
                                    }
                                }
                                self->destroy();
                            };
                        }
                        AudioManager::instance().play(SFX::ArchConstruct);
                    }
                    break;
                }
                case 2: {
                    // Rift Zones - execute
                    if (m_level) {
                        Vec2 targetPos = ai.bossTelegraphDir;
                        int ts = m_level->getTileSize();
                        int cx = static_cast<int>(targetPos.x) / ts;
                        int cy = static_cast<int>(targetPos.y) / ts + 2;
                        for (int dx = -1; dx <= 1; dx++) {
                            int tx = cx + dx;
                            int ty = cy;
                            if (!m_level->inBounds(tx, ty)) continue;
                            Tile spikeTile;
                            spikeTile.type = TileType::Spike;
                            spikeTile.color = {200, 60, 120, 255};
                            m_level->setTile(tx, ty, 1, spikeTile);
                            m_level->setTile(tx, ty, 2, spikeTile);
                        }
                    }
                    AudioManager::instance().play(SFX::ArchRiftOpen);
                    if (m_particles) {
                        m_particles->burst(ai.bossTelegraphDir + Vec2{0, 64.0f}, 20, {255, 60, 100, 200}, 100.0f, 5.0f);
                    }
                    ai.archRiftTimer = 6.0f * phaseCooldownMult;
                    break;
                }
                case 3: {
                    // Dimension Collapse - execute
                    if (m_level) {
                        int ts = m_level->getTileSize();
                        int bx = static_cast<int>(pos.x) / ts;
                        int by = static_cast<int>(pos.y) / ts;
                        for (int tx = bx - 10; tx <= bx + 10; tx++) {
                            for (int ty = by - 5; ty <= by + 5; ty++) {
                                if (!m_level->inBounds(tx, ty)) continue;
                                if (std::rand() % 3 == 0) continue;
                                Tile tileA = m_level->getTile(tx, ty, 1);
                                Tile tileB = m_level->getTile(tx, ty, 2);
                                m_level->setTile(tx, ty, 1, tileB);
                                m_level->setTile(tx, ty, 2, tileA);
                            }
                        }
                    }
                    AudioManager::instance().play(SFX::ArchCollapse);
                    if (m_particles) {
                        m_particles->burst(pos, 50, {180, 100, 255, 220}, 300.0f, 8.0f);
                    }
                    if (m_camera) {
                        m_camera->shake(12.0f, 0.6f);
                    }
                    ai.archCollapseTimer = 12.0f * phaseCooldownMult;
                    break;
                }
                default: break;
            }
            ai.bossTelegraphAttack = -1;
        }
        return; // Skip normal AI during telegraph
    }

    if (dist < ai.detectRange || hpPct < 1.0f) {
        // --- Tile Swap Attack (instant, no telegraph) ---
        ai.archSwapTimer -= dt;
        if (ai.archSwapTimer <= 0 && m_level) {
            int ts = m_level->getTileSize();
            int centerTX = static_cast<int>(playerPos.x) / ts;
            int centerTY = static_cast<int>(playerPos.y) / ts;
            int halfW = ai.archSwapSize / 2;
            int halfH = (ai.bossPhase >= 2) ? 2 : 1;

            for (int tx = centerTX - halfW; tx <= centerTX + halfW; tx++) {
                for (int ty = centerTY - halfH; ty <= centerTY + halfH; ty++) {
                    if (!m_level->inBounds(tx, ty)) continue;
                    Tile tileA = m_level->getTile(tx, ty, 1);
                    Tile tileB = m_level->getTile(tx, ty, 2);
                    m_level->setTile(tx, ty, 1, tileB);
                    m_level->setTile(tx, ty, 2, tileA);
                }
            }
            AudioManager::instance().play(SFX::ArchTileSwap);
            if (m_particles) {
                m_particles->burst({playerPos.x, playerPos.y - 20.0f}, 15, {100, 150, 255, 200}, 120.0f, 4.0f);
            }
            ai.archSwapTimer = 4.0f * phaseCooldownMult;
        }

        // --- Construct Beam (telegraphed) ---
        ai.archConstructTimer -= dt;
        if (ai.archConstructTimer <= 0 && dist < 350.0f && ai.bossTelegraphAttack < 0) {
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 1;
            ai.bossTelegraphDir = (playerPos - pos).normalized();
            AudioManager::instance().play(SFX::SniperTelegraph);
        }

        // --- Dimensional Rift Zones (Phase 2+, telegraphed) ---
        if (ai.bossPhase >= 2) {
            ai.archRiftTimer -= dt;
            if (ai.archRiftTimer <= 0 && m_level && ai.bossTelegraphAttack < 0) {
                ai.bossTelegraphTimer = telegraphTime;
                ai.bossTelegraphAttack = 2;
                ai.bossTelegraphDir = playerPos;
                AudioManager::instance().play(SFX::CollapseWarning);
            }
        }

        // --- Dimension Collapse (Phase 3, telegraphed) ---
        if (ai.bossPhase >= 3) {
            ai.archCollapseTimer -= dt;
            if (ai.archCollapseTimer <= 0 && m_level && ai.bossTelegraphAttack < 0) {
                ai.bossTelegraphTimer = telegraphTime;
                ai.bossTelegraphAttack = 3;
                AudioManager::instance().play(SFX::CollapseWarning);
                if (m_camera) m_camera->shake(4.0f, 0.2f); // Pre-warning shake
            }
        }

        // Movement: slow hover toward player, maintaining distance
        float idealDist = 180.0f;
        if (dist > idealDist + 40.0f) {
            Vec2 dir = (playerPos - pos).normalized();
            phys.velocity.x = dir.x * ai.chaseSpeed;
        } else if (dist < idealDist - 40.0f) {
            Vec2 dir = (pos - playerPos).normalized();
            phys.velocity.x = dir.x * ai.chaseSpeed;
        } else {
            // Strafe slowly
            float strafeDir = std::sin(SDL_GetTicks() * 0.002f) > 0 ? 1.0f : -1.0f;
            phys.velocity.x = strafeDir * ai.patrolSpeed * 0.5f;
        }
    } else {
        // Patrol: slow orbit
        ai.archBeamAngle += dt * 0.8f;
        phys.velocity.x = std::cos(ai.archBeamAngle) * ai.patrolSpeed;
    }

    // Visual color
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        float pulse = (std::sin(SDL_GetTicks() * 0.005f * ai.bossPhase) + 1.0f) * 0.5f;
        switch (ai.bossPhase) {
            case 1: sprite.setColor(80, static_cast<Uint8>(150 + 40 * pulse), 255); break;
            case 2: sprite.setColor(static_cast<Uint8>(120 + 40 * pulse), 100, 255); break;
            case 3: sprite.setColor(static_cast<Uint8>(180 + 75 * pulse), static_cast<Uint8>(60 * pulse), 255); break;
        }
    }
}

void AISystem::updateTemporalWeaver(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& t = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    auto& hp = entity.getComponent<HealthComponent>();
    Vec2 center = t.getCenter();

    // Phase determination based on HP
    float hpPct = hp.getPercent();
    if (hpPct > 0.66f) ai.bossPhase = 1;
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
        }
    }
}

void AISystem::updateVoidSovereign(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities) {
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
    if (hpPct <= 0.4f && ai.bossPhase < 3) {
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
                        if (target.getTag() != "player") return;
                        if (!target.hasComponent<TransformComponent>()) return;
                        Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                        float d = distanceTo(pos, tPos);
                        if (d < 150.0f) {
                            if (target.hasComponent<HealthComponent>()) {
                                auto& hp = target.getComponent<HealthComponent>();
                                if (!hp.isInvincible()) {
                                    hp.takeDamage(35.0f);
                                    if (m_combatSystem)
                                        m_combatSystem->addDamageEvent(tPos, 35.0f, true);
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
            if (e.getTag() == "enemy_shadow_clone" && e.hasComponent<HealthComponent>()) {
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
                if (target.getTag() != "player") return;
                if (!target.hasComponent<TransformComponent>()) return;
                Vec2 tPos = target.getComponent<TransformComponent>().getCenter();
                // Point-to-segment distance
                float lx = laserEnd.x - pos.x, ly = laserEnd.y - pos.y;
                float tx = tPos.x - pos.x, ty = tPos.y - pos.y;
                float lenSq = lx * lx + ly * ly;
                float proj = (lenSq > 0) ? (tx * lx + ty * ly) / lenSq : 0;
                proj = std::max(0.0f, std::min(1.0f, proj));
                float cx = pos.x + proj * lx, cy = pos.y + proj * ly;
                float d2 = std::sqrt((tPos.x - cx) * (tPos.x - cx) + (tPos.y - cy) * (tPos.y - cy));
                if (d2 < 30.0f && target.hasComponent<HealthComponent>()) {
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
                if (target.getTag() != "player") return;
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
    }
}

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
    if (hpPct <= 0.33f && ai.bossPhase < 3) {
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
    }
}
