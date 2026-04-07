// PlayerGrapple.cpp -- Split from Player.cpp (grappling hook system)
#include "Player.h"
#include "ECS/EntityManager.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/AIComponent.h"
#include "Core/AudioManager.h"
#include "Systems/CombatSystem.h"
#include <cmath>

void Player::fireGrapplingHook(const Vec2& dir) {
    if (!entityManager || hookCooldownTimer > 0 || isGrappling || hookFlying) return;

    // Create hook projectile entity
    auto& hook = entityManager->addEntity("grapple_hook");
    hook.dimension = m_entity->dimension;

    auto& t = m_entity->getComponent<TransformComponent>();
    Vec2 spawnPos = t.getCenter();

    auto& hookT = hook.addComponent<TransformComponent>(spawnPos.x - 4, spawnPos.y - 4, 8, 8);
    (void)hookT;
    auto& hookSprite = hook.addComponent<SpriteComponent>();
    hookSprite.setColor(200, 160, 80); // Brownish-gold hook
    hookSprite.renderLayer = 3;

    auto& hookPhys = hook.addComponent<PhysicsBody>();
    hookPhys.useGravity = false;
    hookPhys.velocity = dir * 800.0f; // Hook speed: 800px/s

    auto& hookCol = hook.addComponent<ColliderComponent>();
    hookCol.width = 8;
    hookCol.height = 8;
    hookCol.layer = LAYER_PROJECTILE;
    hookCol.mask = LAYER_TILE | LAYER_ENEMY;
    hookCol.type = ColliderType::Trigger;

    // onTrigger: if hook hits an enemy, pull them toward player
    auto* playerPtr = this;
    auto* cs = combatSystemRef;
    float hookDamage = WeaponSystem::getWeaponData(WeaponID::GrapplingHook).damage;

    hookCol.onTrigger = [playerPtr, cs, hookDamage](Entity* self, Entity* other) {
        if (!other || !other->isAlive()) return;
        if (other->isPlayer) return;

        // Hit an enemy: deal damage and start pulling
        if (other->hasComponent<HealthComponent>() && other->hasComponent<AIComponent>()) {
            auto& hp = other->getComponent<HealthComponent>();
            if (!hp.isInvincible()) {
                // Apply damage (can crit via mastery system)
                float finalDmg = hookDamage;
                bool isCrit = false;

                // Weapon mastery bonus
                if (cs) {
                    MasteryBonus mb = WeaponSystem::getMasteryBonus(
                        cs->weaponKills[static_cast<int>(WeaponID::GrapplingHook)]);
                    finalDmg *= mb.damageMult;
                }

                hp.takeDamage(finalDmg);

                // Push damage event for floating numbers
                if (cs && other->hasComponent<TransformComponent>()) {
                    cs->addDamageEvent(
                        other->getComponent<TransformComponent>().getCenter(),
                        finalDmg, false, isCrit);
                }

                // Start pulling enemy toward player
                playerPtr->hookPulledEnemy = other;
                playerPtr->hookPullTimer = 0.6f; // Pull duration
            }

            // Destroy hook and stop flying
            self->destroy();
            playerPtr->hookFlying = false;
            playerPtr->hookProjectile = nullptr;
        }
    };

    auto& hookHP = hook.addComponent<HealthComponent>();
    hookHP.maxHP = 1;
    hookHP.currentHP = 1;
    hookHP.invincibilityTime = 0;

    hookProjectile = &hook;
    hookFlying = true;

    AudioManager::instance().play(SFX::RangedShot);
}

void Player::attachHook(const Vec2& point) {
    if (!m_entity || !m_entity->isAlive()) return;

    hookAttachPoint = point;
    isGrappling = true;
    hookFlying = false;

    // Destroy hook projectile entity (no longer needed, we have the attach point)
    if (hookProjectile && hookProjectile->isAlive()) {
        hookProjectile->destroy();
    }
    hookProjectile = nullptr;

    // Calculate rope length from player to attach point
    auto& t = m_entity->getComponent<TransformComponent>();
    Vec2 diff = t.getCenter() - hookAttachPoint;
    hookRopeLength = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    // Disable normal gravity during swing (pendulum handles it)
    auto& phys = m_entity->getComponent<PhysicsBody>();
    phys.useGravity = false;

    AudioManager::instance().play(SFX::Pickup);
}

void Player::detachHook() {
    isGrappling = false;
    hookFlying = false;
    hookCooldownTimer = 1.5f; // Cooldown after detach

    // Restore normal gravity
    if (m_entity && m_entity->isAlive()) {
        auto& phys = m_entity->getComponent<PhysicsBody>();
        phys.useGravity = true;
    }

    // Clean up hook projectile if still alive
    if (hookProjectile && hookProjectile->isAlive()) {
        hookProjectile->destroy();
    }
    hookProjectile = nullptr;
}

void Player::updateGrappleSwing(float dt) {
    if (!isGrappling || !m_entity || !m_entity->isAlive()) return;

    auto& t = m_entity->getComponent<TransformComponent>();
    auto& phys = m_entity->getComponent<PhysicsBody>();

    Vec2 playerCenter = t.getCenter();
    Vec2 toAttach = hookAttachPoint - playerCenter;
    float dist = std::sqrt(toAttach.x * toAttach.x + toAttach.y * toAttach.y);

    if (dist < 1.0f) return; // Prevent division by zero

    // Normalize direction to attach point
    Vec2 ropeDir = {toAttach.x / dist, toAttach.y / dist};

    // Pendulum physics: apply gravity, then constrain to rope arc
    // 1. Apply gravity as a force
    float gravityForce = 980.0f;
    phys.velocity.y += gravityForce * dt;

    // 2. Calculate tangential component of velocity (perpendicular to rope)
    float radialVel = phys.velocity.x * ropeDir.x + phys.velocity.y * ropeDir.y;
    Vec2 tangentVel = {phys.velocity.x - radialVel * ropeDir.x,
                        phys.velocity.y - radialVel * ropeDir.y};

    // 3. Set velocity to purely tangential (no stretching)
    phys.velocity = tangentVel;

    // 4. Apply slight damping to prevent infinite swinging
    phys.velocity.x *= 0.998f;
    phys.velocity.y *= 0.998f;

    // 5. Constrain position to rope length (snap back if too far)
    Vec2 newCenter = playerCenter + phys.velocity * dt;
    Vec2 newToAttach = hookAttachPoint - newCenter;
    float newDist = std::sqrt(newToAttach.x * newToAttach.x + newToAttach.y * newToAttach.y);

    if (newDist > hookRopeLength && newDist > 1.0f) {
        // Pull back to rope length
        Vec2 constrainedDir = {newToAttach.x / newDist, newToAttach.y / newDist};
        Vec2 constrainedPos = hookAttachPoint - constrainedDir * hookRopeLength;
        t.position.x = constrainedPos.x - t.width * 0.5f;
        t.position.y = constrainedPos.y - t.height * 0.5f;
    }

    // 6. Emit rope particles while swinging
    if (particles) {
        float speed = std::sqrt(phys.velocity.x * phys.velocity.x + phys.velocity.y * phys.velocity.y);
        if (speed > 100.0f) {
            particles->burst(playerCenter, 1, {200, 160, 80, 150}, 40.0f, 1.5f);
        }
    }
}

void Player::updateHookPull(float dt) {
    if (!hookPulledEnemy || hookPullTimer <= 0) {
        hookPulledEnemy = nullptr;
        hookPullTimer = 0;
        return;
    }

    if (!hookPulledEnemy->isAlive() || !m_entity || !m_entity->isAlive()) {
        hookPulledEnemy = nullptr;
        hookPullTimer = 0;
        return;
    }

    hookPullTimer -= dt;

    auto& playerT = m_entity->getComponent<TransformComponent>();
    Vec2 playerCenter = playerT.getCenter();

    if (!hookPulledEnemy->hasComponent<TransformComponent>() ||
        !hookPulledEnemy->hasComponent<PhysicsBody>()) {
        hookPulledEnemy = nullptr;
        hookPullTimer = 0;
        return;
    }

    auto& enemyT = hookPulledEnemy->getComponent<TransformComponent>();
    auto& enemyPhys = hookPulledEnemy->getComponent<PhysicsBody>();

    Vec2 enemyCenter = enemyT.getCenter();
    Vec2 toPlayer = playerCenter - enemyCenter;
    float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

    if (dist < 32.0f || dist < 1.0f) {
        // Close enough or degenerate, stop pulling
        hookPulledEnemy = nullptr;
        hookPullTimer = 0;
        return;
    }

    // Pull enemy toward player at 500px/s
    Vec2 pullDir = {toPlayer.x / dist, toPlayer.y / dist};
    enemyPhys.velocity.x = pullDir.x * 500.0f;
    enemyPhys.velocity.y = pullDir.y * 500.0f;

    // Pull particles
    if (particles) {
        particles->burst(enemyCenter, 1, {200, 160, 80, 180}, 30.0f, 1.0f);
    }
}
