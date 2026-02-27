#include "CombatSystem.h"
#include "Components/TransformComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AIComponent.h"
#include "Core/AudioManager.h"
#include "Game/ItemDrop.h"
#include "Game/Player.h"
#include <cmath>

float CombatSystem::consumeHitFreeze() {
    float t = m_pendingHitFreeze;
    m_pendingHitFreeze = 0;
    return t;
}

void CombatSystem::update(EntityManager& entities, float dt, int currentDimension) {
    auto combatEnts = entities.getEntitiesWithComponent<CombatComponent>();

    for (auto* e : combatEnts) {
        auto& combat = e->getComponent<CombatComponent>();
        if (!combat.isAttacking) continue;
        if (!e->hasComponent<TransformComponent>()) continue;

        // Check dimension
        if (e->dimension != 0 && e->dimension != currentDimension) continue;

        processAttack(*e, entities, currentDimension);
    }
}

void CombatSystem::processAttack(Entity& attacker, EntityManager& entities, int currentDim) {
    auto& combat = attacker.getComponent<CombatComponent>();
    auto& transform = attacker.getComponent<TransformComponent>();

    bool isPlayer = (attacker.getTag() == "player");
    auto& atkData = combat.getCurrentAttackData();

    if (combat.currentAttack == AttackType::Ranged) {
        // Create projectile (only once at start of attack)
        if (combat.attackTimer >= atkData.duration - 0.02f) {
            Vec2 pos = transform.getCenter();
            createProjectile(entities, pos, combat.attackDirection,
                            atkData.damage, 400.0f, attacker.dimension);
            AudioManager::instance().play(SFX::RangedShot);
        }
        return;
    }

    // Melee: check all potential targets
    Vec2 attackCenter = transform.getCenter() + combat.attackDirection * atkData.range * 0.5f;
    float attackRadius = atkData.range;

    entities.forEach([&](Entity& target) {
        if (&target == &attacker) return;
        if (!target.hasComponent<HealthComponent>() || !target.hasComponent<TransformComponent>()) return;

        // Check dimension
        if (target.dimension != 0 && target.dimension != currentDim) return;

        // Player attacks enemies, enemies attack player
        bool targetIsEnemy = target.getTag().find("enemy") != std::string::npos;
        if (isPlayer && !targetIsEnemy) return;
        if (!isPlayer && target.getTag() != "player") return;

        auto& targetTransform = target.getComponent<TransformComponent>();
        Vec2 targetCenter = targetTransform.getCenter();
        float dx = targetCenter.x - attackCenter.x;
        float dy = targetCenter.y - attackCenter.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < attackRadius) {
            auto& hp = target.getComponent<HealthComponent>();
            float comboMult = combat.comboCount * (0.15f + m_comboBonus);
            float damage = atkData.damage * (1.0f + comboMult);

            // Critical hit check (player only)
            bool isCrit = false;
            if (isPlayer && m_critChance > 0) {
                float roll = static_cast<float>(std::rand()) / RAND_MAX;
                if (roll < m_critChance) {
                    damage *= 2.0f;
                    isCrit = true;
                }
            }

            // Damage boost from pickup
            if (isPlayer && m_player && m_player->damageBoostTimer > 0) {
                damage *= m_player->damageBoostMultiplier;
            }

            // Check for Shielder shield blocking
            bool shieldBlocked = false;
            if (isPlayer && target.hasComponent<AIComponent>()) {
                auto& targetAI = target.getComponent<AIComponent>();
                if (targetAI.enemyType == EnemyType::Shielder && targetAI.shieldUp) {
                    bool attackFromRight = attackCenter.x > targetCenter.x;
                    if (attackFromRight == targetAI.facingRight) {
                        shieldBlocked = true;
                    }
                }
            }

            if (shieldBlocked) {
                float actualDmg = damage * 0.1f;
                hp.takeDamage(actualDmg);
                m_damageEvents.push_back({targetCenter, actualDmg, !isPlayer, false});
                if (m_particles) {
                    Vec2 shieldPos = {targetCenter.x + (target.getComponent<AIComponent>().facingRight ? 16.0f : -16.0f),
                                      targetCenter.y};
                    m_particles->burst(shieldPos, 8, {100, 200, 255, 255}, 120.0f, 2.0f);
                }
                AudioManager::instance().play(SFX::RiftFail);
                if (m_camera) m_camera->shake(2.0f, 0.08f);
            } else {
                hp.takeDamage(damage);
                m_damageEvents.push_back({targetCenter, damage, !isPlayer, isCrit});

                // Combo-stage knockback variation (player only)
                int comboStage = isPlayer ? ((combat.comboCount - 1) % 3) : 0;
                if (target.hasComponent<PhysicsBody>()) {
                    auto& phys = target.getComponent<PhysicsBody>();
                    Vec2 knockDir = combat.attackDirection;
                    float knockMult = 1.0f;

                    if (isPlayer) {
                        switch (comboStage) {
                            case 0: // Normal horizontal hit
                                knockDir.y = -0.3f;
                                break;
                            case 1: // Diagonal sweep
                                knockDir.y = -0.6f;
                                knockMult = 1.1f;
                                break;
                            case 2: // Finisher uppercut
                                knockDir.y = -1.2f;
                                knockMult = 1.4f;
                                break;
                        }
                    } else {
                        knockDir.y = -0.3f;
                    }

                    phys.velocity += knockDir * atkData.knockback * knockMult;
                }

                // SFX
                AudioManager::instance().play(isPlayer ? SFX::MeleeHit : SFX::EnemyHit);

                // Screen shake scales with combo stage
                if (m_camera) {
                    float shakeIntensity = isPlayer ? (5.0f + comboStage * 3.0f) : 3.0f;
                    float shakeDuration = 0.12f + comboStage * 0.04f;
                    m_camera->shake(shakeIntensity, shakeDuration);
                }

                // Hit-freeze scales with combo stage
                if (isPlayer) {
                    float freezeBase = isCrit ? 0.08f : 0.05f;
                    m_pendingHitFreeze += freezeBase + comboStage * 0.03f;
                } else {
                    m_pendingHitFreeze += 0.06f;
                }

                // Particles scale with combo stage
                if (m_particles) {
                    SDL_Color hitColor;
                    int particleCount;
                    if (isPlayer) {
                        switch (comboStage) {
                            case 0: hitColor = {255, 80, 80, 255}; particleCount = 8; break;
                            case 1: hitColor = {255, 160, 50, 255}; particleCount = 12; break;
                            case 2: hitColor = {255, 255, 100, 255}; particleCount = 20; break;
                            default: hitColor = {255, 80, 80, 255}; particleCount = 8; break;
                        }
                    } else {
                        hitColor = {100, 150, 255, 255};
                        particleCount = 8;
                    }
                    m_particles->damageEffect(targetCenter, hitColor);

                    Vec2 hitPos = {(attackCenter.x + targetCenter.x) * 0.5f,
                                   (attackCenter.y + targetCenter.y) * 0.5f};
                    float burstSpeed = 120.0f + comboStage * 60.0f;
                    m_particles->burst(hitPos, particleCount, {255, 220, 100, 255}, burstSpeed, 2.0f + comboStage);
                }
            }

            // Element effects on hit (enemy hitting player)
            if (!isPlayer && target.getTag() == "player" && attacker.hasComponent<AIComponent>()) {
                auto& ai = attacker.getComponent<AIComponent>();
                if (ai.element == EnemyElement::Ice && target.hasComponent<PhysicsBody>()) {
                    // Slow player for 1.5 seconds (reduce speed via friction boost)
                    auto& phys = target.getComponent<PhysicsBody>();
                    phys.velocity.x *= 0.3f;
                    if (m_particles) {
                        m_particles->burst(targetCenter, 10, {100, 180, 255, 255}, 80.0f, 2.0f);
                    }
                } else if (ai.element == EnemyElement::Fire) {
                    // Extra burn damage
                    hp.takeDamage(damage * 0.3f);
                    if (m_particles) {
                        m_particles->burst(targetCenter, 8, {255, 150, 30, 255}, 100.0f, 2.5f);
                    }
                }
            }

            // Death (shared for both shield-blocked and normal hits)
            if (hp.currentHP <= 0) {
                AudioManager::instance().play(isPlayer ? SFX::PlayerDeath : SFX::EnemyDeath);
                // Drop items from enemies
                if (isPlayer && target.getTag().find("enemy") != std::string::npos) {
                    ItemDrop::spawnRandomDrop(entities, targetCenter, target.dimension, 1, m_player);
                }

                // Electric chain damage on enemy death
                if (isPlayer && target.hasComponent<AIComponent>()) {
                    auto& targetAI = target.getComponent<AIComponent>();
                    if (targetAI.element == EnemyElement::Electric) {
                        float chainDmg = damage * 0.5f;
                        float chainRadius = 100.0f;
                        entities.forEach([&](Entity& nearby) {
                            if (&nearby == &target || !nearby.isAlive()) return;
                            if (nearby.getTag().find("enemy") == std::string::npos) return;
                            if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                            auto& nt = nearby.getComponent<TransformComponent>();
                            float cdx = nt.getCenter().x - targetCenter.x;
                            float cdy = nt.getCenter().y - targetCenter.y;
                            if (cdx * cdx + cdy * cdy < chainRadius * chainRadius) {
                                nearby.getComponent<HealthComponent>().takeDamage(chainDmg);
                                if (m_particles) {
                                    m_particles->burst(nt.getCenter(), 6, {255, 255, 80, 255}, 150.0f, 1.5f);
                                }
                            }
                        });
                        if (m_particles) {
                            m_particles->burst(targetCenter, 15, {255, 255, 100, 255}, 200.0f, 3.0f);
                        }
                    }
                }

                // Exploder death explosion is handled by AISystem
                target.destroy();
                if (m_particles) {
                    auto& sprite = target.getComponent<SpriteComponent>();
                    m_particles->burst(targetCenter, 25, sprite.color, 200.0f, 4.0f);
                }
                // Bigger shake on kill
                if (m_camera && isPlayer) {
                    m_camera->shake(8.0f, 0.2f);
                }
            }
        }
    });
}

void CombatSystem::createProjectile(EntityManager& entities, Vec2 pos, Vec2 dir,
                                      float damage, float speed, int dimension) {
    auto& proj = entities.addEntity("projectile");
    proj.dimension = dimension;

    auto& t = proj.addComponent<TransformComponent>(pos.x - 4, pos.y - 4, 8, 8);
    auto& sprite = proj.addComponent<SpriteComponent>();
    sprite.setColor(255, 230, 100);
    sprite.renderLayer = 3;

    auto& phys = proj.addComponent<PhysicsBody>();
    phys.useGravity = false;
    phys.velocity = dir * speed;

    auto& col = proj.addComponent<ColliderComponent>();
    col.width = 8;
    col.height = 8;
    col.layer = LAYER_PROJECTILE;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_ENEMY;
    col.type = ColliderType::Trigger;
    col.onTrigger = [damage](Entity* self, Entity* other) {
        if (other->hasComponent<HealthComponent>()) {
            other->getComponent<HealthComponent>().takeDamage(damage);
        }
        self->destroy();
    };

    auto& hp = proj.addComponent<HealthComponent>();
    hp.maxHP = 1;
    hp.currentHP = 1;
    hp.onDamage = [&proj](float) { proj.destroy(); };
}
