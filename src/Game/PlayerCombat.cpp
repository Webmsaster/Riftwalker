// PlayerCombat.cpp -- Split from Player.cpp (combat, weapons, class stats)
#include "Player.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/AbilityComponent.h"
#include "Components/RelicComponent.h"
#include "Core/AudioManager.h"
#include "Systems/CombatSystem.h"
#include "Game/RelicSynergy.h"
#include <cmath>

void Player::handleAttack(const InputManager& input) {
    auto& combat = m_entity->getComponent<CombatComponent>();

    // Directional attack: check vertical input
    float hDir = facingRight ? 1.0f : -1.0f;
    float vAxis = 0;
    if (input.isActionDown(Action::MoveUp)) vAxis = -1.0f;
    else if (input.isActionDown(Action::MoveDown)) vAxis = 1.0f;

    Vec2 dir;
    if (vAxis != 0 && !m_entity->getComponent<PhysicsBody>().onGround && vAxis > 0) {
        // Downward attack (air only)
        dir = {0.0f, 1.0f};
    } else if (vAxis < 0) {
        // Upward attack
        dir = {0.0f, -1.0f};
    } else {
        dir = {hDir, 0.0f};
    }

    // Charged attack: release detection
    if (isChargingAttack && combat.isCharging) {
        if (input.isActionReleased(Action::Attack)) {
            float chargePercent = combat.getChargePercent();
            if (combat.chargeTimer >= combat.minChargeTime) {
                bool isVoidHammer = (combat.currentMelee == WeaponID::VoidHammer);

                // Scale damage with charge
                combat.chargedAttack.damage = 50.0f * (0.5f + 0.5f * chargePercent);
                combat.chargedAttack.knockback = 500.0f * (0.5f + 0.5f * chargePercent);

                // VoidHammer: AoE shockwave with larger range and bonus damage
                if (isVoidHammer) {
                    combat.chargedAttack.range = 120.0f;
                    combat.chargedAttack.damage *= 1.3f;
                    combat.chargedAttack.knockback = 700.0f * (0.5f + 0.5f * chargePercent);
                } else {
                    combat.chargedAttack.range = 72.0f;
                }

                combat.releaseCharged(dir);

                // Charged attack lunge: stronger forward push
                if (std::abs(dir.x) > 0.5f) {
                    attackLungeTimer = 0.12f; // 120ms for charged
                    attackLungeDir = dir.x > 0 ? 1.0f : -1.0f;
                }

                if (isVoidHammer) {
                    AudioManager::instance().play(SFX::GroundSlam);
                } else {
                    AudioManager::instance().play(SFX::ChargedAttackRelease);
                }

                if (particles) {
                    auto& t = m_entity->getComponent<TransformComponent>();
                    Vec2 attackPos = t.getCenter() + dir * 40.0f;
                    int pCount = 15 + static_cast<int>(chargePercent * 20);
                    particles->burst(attackPos, pCount, {255, 200, 50, 255}, 250.0f, 5.0f);
                    particles->burst(attackPos, pCount / 2, {255, 255, 150, 255}, 180.0f, 3.0f);

                    // VoidHammer: shockwave ring expanding outward
                    if (isVoidHammer) {
                        int ringCount = 4 + static_cast<int>(chargePercent * 6);
                        for (int i = 0; i < 8; i++) {
                            float angle = i * 45.0f;
                            particles->directionalBurst(attackPos, ringCount,
                                {180, 120, 255, 255}, angle, 30.0f,
                                200.0f + chargePercent * 150.0f, 4.0f);
                        }
                        // Ground dust ring
                        particles->burst(attackPos, pCount, {160, 140, 120, 200}, 180.0f, 2.5f);
                    }
                }
            } else {
                combat.isCharging = false;
            }
            isChargingAttack = false;
            return;
        }
        // Charge ready cue: play once when charge reaches minimum time
        if (combat.chargeTimer >= combat.minChargeTime && !chargeReadyCued) {
            chargeReadyCued = true;
            AudioManager::instance().play(SFX::ChargeReady);
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                // Bright inward-converging burst to signal "ready"
                particles->burst(t.getCenter(), 14, {255, 230, 100, 255}, 80.0f, 2.0f);
            }
        }
        // Charge particles while holding
        if (particles && combat.chargeTimer >= combat.minChargeTime) {
            auto& t = m_entity->getComponent<TransformComponent>();
            float cp = combat.getChargePercent();
            Uint8 g = static_cast<Uint8>(200 * (1.0f - cp * 0.5f));
            particles->burst(t.getCenter(), 1, {255, g, 50, 200}, 30.0f + cp * 60.0f, 1.5f);
        }
        return;
    }

    if (input.isActionPressed(Action::Attack)) {
        // Parry window: opens on attack press
        combat.startParry();

        if (isDashing) {
            // Dash attack: cancel dash into a powerful hit
            isDashing = false;
            auto& phys = m_entity->getComponent<PhysicsBody>();
            phys.useGravity = true;
            phys.velocity.x *= 0.5f;

            // Brief invincibility during dash attack (don't shorten existing invincibility)
            auto& hp = m_entity->getComponent<HealthComponent>();
            hp.invincibilityTimer = std::max(hp.invincibilityTimer, 0.25f);

            Vec2 dashDir = {facingRight ? 1.0f : -1.0f, -0.3f};
            combat.startAttack(AttackType::Dash, dashDir);
            // Weapon mastery: reduce cooldown
            if (combatSystemRef) {
                MasteryBonus mb = WeaponSystem::getMasteryBonus(
                    combatSystemRef->weaponKills[static_cast<int>(combat.currentMelee)]);
                combat.cooldownTimer *= mb.cooldownMult;
            }
            AudioManager::instance().play(SFX::MeleeSwing);
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                Vec2 attackPos = t.getCenter() + dashDir * 40.0f;
                particles->burst(attackPos, 18, {100, 200, 255, 255}, 180.0f, 4.0f);
            }
        } else {
            AudioManager::instance().play(SFX::MeleeSwing);
            combat.startAttack(AttackType::Melee, dir);
            // Weapon mastery: reduce cooldown
            if (combatSystemRef) {
                MasteryBonus mb = WeaponSystem::getMasteryBonus(
                    combatSystemRef->weaponKills[static_cast<int>(combat.currentMelee)]);
                combat.cooldownTimer *= mb.cooldownMult;
            }
            // Melee attack lunge: brief forward push scaling with combo
            if (std::abs(dir.x) > 0.5f) {
                attackLungeTimer = 0.08f; // 80ms lunge window
                attackLungeDir = dir.x > 0 ? 1.0f : -1.0f;
            }
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                Vec2 attackPos = t.getCenter() + dir * 32.0f;
                particles->burst(attackPos, 8, {255, 255, 200, 255}, 100.0f, 3.0f);
            }
        }
    }

    // Charged attack: start charging if attack held after melee ends
    if (input.isActionDown(Action::Attack) && !combat.isAttacking && !combat.isCharging
        && combat.cooldownTimer <= 0 && !isDashing) {
        combat.startCharging();
        isChargingAttack = true;
        chargeReadyCued = false;
    }

    if (input.isActionPressed(Action::RangedAttack)) {
        // Grappling Hook: special handling — fire hook instead of normal projectile
        if (combat.currentRanged == WeaponID::GrapplingHook) {
            if (hookCooldownTimer <= 0 && !isGrappling && !hookFlying && combat.canAttack()) {
                fireGrapplingHook(dir);
                combat.cooldownTimer = 0.1f; // Brief lockout to prevent spam
            }
        } else {
            combat.startAttack(AttackType::Ranged, dir);
            // Weapon mastery: reduce cooldown
            if (combatSystemRef) {
                MasteryBonus mb = WeaponSystem::getMasteryBonus(
                    combatSystemRef->weaponKills[static_cast<int>(combat.currentRanged)]);
                combat.cooldownTimer *= mb.cooldownMult;
            }
        }
    }
}

void Player::applyClassStats() {
    const auto& data = ClassSystem::getData(playerClass);
    moveSpeed = data.baseSpeed;

    auto& hp = m_entity->getComponent<HealthComponent>();
    hp.maxHP = data.baseHP;
    hp.currentHP = data.baseHP;

    auto& sprite = m_entity->getComponent<SpriteComponent>();
    sprite.setColor(data.color.r, data.color.g, data.color.b);

    // Phantom: longer dash
    if (playerClass == PlayerClass::Phantom) {
        dashDuration = 0.2f * data.dashLengthMult;
    }

    // Berserker: enhanced Ground Slam
    if (playerClass == PlayerClass::Berserker && m_entity->hasComponent<AbilityComponent>()) {
        auto& abil = m_entity->getComponent<AbilityComponent>();
        abil.slamRadius = 120.0f * 1.4f;        // +40% radius
        abil.slamStunDuration = 0.8f + 0.4f;     // +0.4s stun
    }

    // Voidwalker: enhanced Phase Strike
    if (playerClass == PlayerClass::Voidwalker && m_entity->hasComponent<AbilityComponent>()) {
        auto& abil = m_entity->getComponent<AbilityComponent>();
        abil.phaseStrikeRange = 200.0f * 1.5f;   // +50% range
    }

    // Phantom: enhanced Rift Shield
    if (playerClass == PlayerClass::Phantom && m_entity->hasComponent<AbilityComponent>()) {
        auto& abil = m_entity->getComponent<AbilityComponent>();
        abil.shieldMaxHits = 3 + 1;               // +1 max hits
    }

    // Technomancer: reset construct counts on class apply
    if (playerClass == PlayerClass::Technomancer) {
        activeTurrets = 0;
        activeTraps = 0;
        turretCooldownTimer = 0;
        trapCooldownTimer = 0;
    }
}

bool Player::isBloodRageActive() const {
    if (playerClass != PlayerClass::Berserker) return false;
    const auto& data = ClassSystem::getData(PlayerClass::Berserker);
    auto& hp = m_entity->getComponent<HealthComponent>();
    return hp.getPercent() < data.lowHPThreshold;
}

float Player::getClassDamageMultiplier() const {
    float mult = 1.0f;
    if (isBloodRageActive()) {
        mult *= ClassSystem::getData(PlayerClass::Berserker).rageDmgBonus;
    }
    // Voidwalker: Dimensional Affinity - Rift Charge damage buff
    if (isRiftChargeActive()) {
        mult *= riftChargeDamageMult;
    }
    return mult;
}

void Player::activateRiftCharge() {
    if (playerClass != PlayerClass::Voidwalker) return;
    riftChargeTimer = riftChargeDuration;
    if (particles) {
        auto& t = m_entity->getComponent<TransformComponent>();
        // Burst of blue-white particles to signal activation
        particles->burst(t.getCenter(), 12, {100, 180, 255, 220}, 80.0f, 2.5f);
    }
}

float Player::getClassAttackSpeedMultiplier() const {
    float mult = 1.0f;
    if (isBloodRageActive()) {
        mult *= ClassSystem::getData(PlayerClass::Berserker).rageAtkSpeedBonus;
    }
    if (hasMomentum()) {
        mult *= getMomentumAtkSpeedMult();
    }
    mult *= smithAtkSpdMult; // Blacksmith attack speed upgrade
    return mult;
}

void Player::addMomentumStack() {
    if (playerClass != PlayerClass::Berserker) return;
    if (momentumStacks < momentumMaxStacks) {
        momentumStacks++;
    }
    momentumTimer = momentumDuration; // refresh timer on kill
    // Burst particles on stack gain
    if (particles) {
        auto& t = m_entity->getComponent<TransformComponent>();
        int pCount = 6 + momentumStacks * 2;
        particles->burst(t.getCenter(), pCount,
                        {255, 120, 40, 230}, 100.0f + momentumStacks * 20.0f, 2.5f);
    }
    AudioManager::instance().play(SFX::MeleeHit); // satisfying feedback
}

float Player::getMomentumSpeedMult() const {
    if (!hasMomentum()) return 1.0f;
    return 1.0f + momentumStacks * momentumSpeedPerStack;
}

float Player::getMomentumAtkSpeedMult() const {
    if (!hasMomentum()) return 1.0f;
    return 1.0f + momentumStacks * momentumAtkSpdPerStack;
}

void Player::switchMelee() {
    if (weaponSwitchCooldown > 0) return;
    auto& combat = m_entity->getComponent<CombatComponent>();
    combat.currentMelee = WeaponSystem::nextMelee(combat.currentMelee);
    combat.daggerHitCount = 0; // Reset PhaseDaggers hit counter on weapon switch
    applyWeaponStats();
    weaponSwitchCooldown = 0.3f;
    AudioManager::instance().play(SFX::MenuSelect);
}

void Player::switchRanged() {
    if (weaponSwitchCooldown > 0) return;
    auto& combat = m_entity->getComponent<CombatComponent>();
    combat.currentRanged = WeaponSystem::nextRanged(combat.currentRanged);
    applyWeaponStats();
    weaponSwitchCooldown = 0.3f;
    AudioManager::instance().play(SFX::MenuSelect);
}

void Player::applyWeaponStats() {
    auto& combat = m_entity->getComponent<CombatComponent>();

    // Apply melee weapon stats
    const auto& melee = WeaponSystem::getWeaponData(combat.currentMelee);
    combat.meleeAttack.damage = melee.damage;
    combat.meleeAttack.cooldown = melee.cooldown;
    combat.meleeAttack.range = melee.range;
    combat.meleeAttack.knockback = melee.knockback;
    combat.meleeAttack.duration = melee.duration;

    // Apply ranged weapon stats
    const auto& ranged = WeaponSystem::getWeaponData(combat.currentRanged);
    combat.rangedAttack.damage = ranged.damage;
    combat.rangedAttack.cooldown = ranged.cooldown;
    combat.rangedAttack.knockback = ranged.knockback;
    combat.rangedAttack.duration = ranged.duration;

    // Phase Daggers speed bonus (use class base speed, not hardcoded)
    moveSpeed = ClassSystem::getData(playerClass).baseSpeed;
    if (combat.currentMelee == WeaponID::PhaseDaggers) {
        moveSpeed *= (1.0f + melee.speedModifier);
        // PhantomRush: PhaseDaggers + SwiftBoots → +10% extra movespeed
        if (m_entity->hasComponent<RelicComponent>()) {
            moveSpeed *= (1.0f + RelicSynergy::getPhantomRushSpeedBonus(
                m_entity->getComponent<RelicComponent>(), combat.currentMelee));
        }
    }
}
