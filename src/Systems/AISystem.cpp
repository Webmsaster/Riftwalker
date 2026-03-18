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
#include "Game/Player.h"
#include <algorithm>
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
        // Skip player-owned constructs (Technomancer turrets) — handled by PlayState
        if (e->getTag() == "player_turret") continue;
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
                    t.prevPosition = t.position; // Sync for interpolation (no smear on teleport)
                    if (m_particles) {
                        m_particles->burst(t.getCenter(), 12, {150, 80, 220, 255}, 120.0f, 2.0f);
                    }
                }
            }

            // Vampiric: heal percentage of damage dealt is handled in CombatSystem

            // Fire Aura: burn player if within 100px
            if (ai.eliteMod == EliteModifier::FireAura && e->hasComponent<TransformComponent>()) {
                auto& t = e->getComponent<TransformComponent>();
                float dist = std::abs(t.getCenter().x - playerPos.x) + std::abs(t.getCenter().y - playerPos.y);
                if (dist < 100.0f && m_player && m_player->getEntity()->hasComponent<HealthComponent>()) {
                    auto& php = m_player->getEntity()->getComponent<HealthComponent>();
                    if (!php.isInvincible()) {
                        m_player->burnTimer = std::max(m_player->burnTimer, 1.0f);
                    }
                }
                // Visual: fire particles around elite
                if (m_particles) {
                    float auraPhase = std::fmod(ai.eliteGlowTimer * 8.0f, 6.28f);
                    float ox = std::cos(auraPhase) * 20.0f;
                    float oy = std::sin(auraPhase) * 12.0f;
                    m_particles->burst({t.getCenter().x + ox, t.getCenter().y + oy}, 1,
                                        {255, 100, 30, 180}, 40.0f, 1.5f);
                }
            }

            // Heal Aura: heal nearby enemies 3 HP/s
            if (ai.eliteMod == EliteModifier::HealAura && e->hasComponent<TransformComponent>()) {
                auto& t = e->getComponent<TransformComponent>();
                Vec2 center = t.getCenter();
                entities.forEach([&](Entity& other) {
                    if (&other == e || !other.isAlive()) return;
                    if (other.getTag().find("enemy") == std::string::npos) return;
                    if (!other.hasComponent<TransformComponent>() || !other.hasComponent<HealthComponent>()) return;
                    auto& ot = other.getComponent<TransformComponent>();
                    float dist = std::abs(ot.getCenter().x - center.x) + std::abs(ot.getCenter().y - center.y);
                    if (dist < 120.0f) {
                        other.getComponent<HealthComponent>().heal(3.0f * dt);
                    }
                });
                // Visual: green heal particles
                if (m_particles) {
                    float auraPhase = std::fmod(ai.eliteGlowTimer * 5.0f, 6.28f);
                    float ox = std::cos(auraPhase) * 22.0f;
                    float oy = std::sin(auraPhase) * 14.0f;
                    m_particles->burst({center.x + ox, center.y + oy}, 1,
                                        {60, 230, 80, 160}, 30.0f, 1.5f);
                }
            }

            // Shield Aura: mark handled in CombatSystem (damage reduction for nearby enemies)
            // Visual particles only here
            if (ai.eliteMod == EliteModifier::ShieldAura && e->hasComponent<TransformComponent>() && m_particles) {
                auto& t = e->getComponent<TransformComponent>();
                float auraPhase = std::fmod(ai.eliteGlowTimer * 6.0f, 6.28f);
                float ox = std::cos(auraPhase) * 18.0f;
                float oy = std::sin(auraPhase) * 10.0f;
                m_particles->burst({t.getCenter().x + ox, t.getCenter().y + oy}, 1,
                                    {60, 200, 255, 140}, 35.0f, 1.5f);
            }
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
                ai.attackTimer = ai.attackCooldown / std::max(0.1f, ai.dimSpeedMod); // Faster attacks in dim B
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
                ai.attackTimer = ai.attackCooldown / std::max(0.1f, ai.dimSpeedMod);
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
            ai.attackTimer = ai.attackCooldown / std::max(0.1f, ai.dimSpeedMod);
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
                ai.attackTimer = ai.chargeWindup / std::max(0.1f, ai.dimSpeedMod);
            }
            break;
        }
        case AIState::Chase: {
            // Wind up before charging
            phys.velocity.x *= 0.9f;
            ai.attackTimer -= dt;

            // Visual warning: flash
            float windupTime = ai.chargeWindup / std::max(0.1f, ai.dimSpeedMod);
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
            float falloff = 1.0f - (dist / std::max(1.0f, ai.explodeRadius));
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

// Boss update implementations have been moved to separate files:
// BossRiftGuardian.cpp, BossVoidWyrm.cpp, BossDimensionalArchitect.cpp,
// BossTemporalWeaver.cpp, BossVoidSovereign.cpp, BossEntropyIncarnate.cpp
