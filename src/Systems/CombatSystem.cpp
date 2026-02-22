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
            float damage = atkData.damage * (1.0f + combat.comboCount * 0.15f);

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

                // Knockback
                if (target.hasComponent<PhysicsBody>()) {
                    auto& phys = target.getComponent<PhysicsBody>();
                    Vec2 knockDir = combat.attackDirection;
                    knockDir.y = -0.3f;
                    phys.velocity += knockDir * atkData.knockback;
                }

                // SFX
                AudioManager::instance().play(isPlayer ? SFX::MeleeHit : SFX::EnemyHit);

                // Screen shake on melee hit
                if (m_camera) {
                    float shakeIntensity = isPlayer ? 5.0f : 3.0f;
                    m_camera->shake(shakeIntensity, 0.12f);
                }

                // Hit-freeze effect (brief pause for impact feel)
                if (isPlayer) {
                    m_pendingHitFreeze += isCrit ? 0.08f : 0.05f;
                } else {
                    // Enemy hitting player also gets brief freeze
                    m_pendingHitFreeze += 0.06f;
                }

                // Particles
                if (m_particles) {
                    SDL_Color hitColor = isPlayer ? SDL_Color{255, 80, 80, 255} : SDL_Color{100, 150, 255, 255};
                    m_particles->damageEffect(targetCenter, hitColor);

                    if (isPlayer) {
                        Vec2 hitPos = {(attackCenter.x + targetCenter.x) * 0.5f,
                                       (attackCenter.y + targetCenter.y) * 0.5f};
                        m_particles->burst(hitPos, 8, {255, 220, 100, 255}, 120.0f, 2.0f);
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
