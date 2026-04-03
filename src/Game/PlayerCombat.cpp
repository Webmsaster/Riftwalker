// PlayerCombat.cpp -- Split from Player.cpp (combat, weapons, class stats)
#include "Player.h"
#include "ECS/EntityManager.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/AbilityComponent.h"
#include "Components/RelicComponent.h"
#include "Components/AIComponent.h"
#include "Core/AudioManager.h"
#include "Systems/CombatSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ItemDrop.h"
#include "Game/Bestiary.h"
#include <cmath>
#include <vector>
#include <algorithm>

void Player::handleAttack(const InputManager& input) {
    // Block normal attacks during finisher execution
    if (finisherExecuting) return;

    // Combo finisher activation: Interact key while finisher is available
    if (isFinisherAvailable() && input.isActionPressed(Action::Interact)) {
        executeComboFinisher();
        return;
    }

    auto& combat = m_entity->getComponent<CombatComponent>();

    // Parry counter-attack: pressing attack during counter window triggers weapon-specific counter
    if (combat.counterReady && input.isActionPressed(Action::Attack)) {
        float hDir = facingRight ? 1.0f : -1.0f;
        Vec2 dir = {hDir, 0.0f};
        if (input.isActionDown(Action::MoveUp)) dir = {0.0f, -1.0f};
        else if (input.isActionDown(Action::MoveDown) &&
                 !m_entity->getComponent<PhysicsBody>().onGround) dir = {0.0f, 1.0f};
        executeCounterAttack(combat, dir);
        return;
    }

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
        // Charge-up gathering particles (entire charge duration)
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            float cp = combat.getChargePercent();
            Uint8 brightness = static_cast<Uint8>(100 + 155 * cp);
            Uint8 g = static_cast<Uint8>(150 * (1.0f - cp * 0.6f));
            int count = 1 + static_cast<int>(cp * 2); // 1-3 particles
            float radius = 35.0f - cp * 15.0f;        // tightens as charge grows
            particles->chargeGather(t.getCenter(), count, {brightness, g, 30, 220},
                                    radius, 60.0f + cp * 80.0f, 1.5f + cp * 1.5f);
            // Extra glow particles post-minCharge
            if (combat.chargeTimer >= combat.minChargeTime) {
                particles->burst(t.getCenter(), 1, {255, g, 50, 200}, 30.0f + cp * 60.0f, 1.5f);
            }
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
                int wIdx = static_cast<int>(combat.currentMelee);
                if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT)) {
                    MasteryBonus mb = WeaponSystem::getMasteryBonus(combatSystemRef->weaponKills[wIdx]);
                    combat.cooldownTimer *= mb.cooldownMult;
                }
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
                int wIdx = static_cast<int>(combat.currentMelee);
                if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT)) {
                    MasteryBonus mb = WeaponSystem::getMasteryBonus(combatSystemRef->weaponKills[wIdx]);
                    combat.cooldownTimer *= mb.cooldownMult;
                }
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
                int wIdx = static_cast<int>(combat.currentRanged);
                if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT)) {
                    MasteryBonus mb = WeaponSystem::getMasteryBonus(combatSystemRef->weaponKills[wIdx]);
                    combat.cooldownTimer *= mb.cooldownMult;
                }
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
    weaponSwitchPending = 1; // Signal PlayState for visual feedback
    AudioManager::instance().play(SFX::MenuSelect);
}

void Player::switchRanged() {
    if (weaponSwitchCooldown > 0) return;
    auto& combat = m_entity->getComponent<CombatComponent>();
    combat.currentRanged = WeaponSystem::nextRanged(combat.currentRanged);
    applyWeaponStats();
    weaponSwitchCooldown = 0.3f;
    weaponSwitchPending = 2; // Signal PlayState for visual feedback
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

void Player::executeCounterAttack(CombatComponent& combat, Vec2 dir) {
    // Determine weapon for counter: use melee weapon for melee weapons, ranged for ranged
    bool isMeleeWeapon = WeaponSystem::isMelee(combat.currentMelee);
    int weaponId = isMeleeWeapon ? static_cast<int>(combat.currentMelee)
                                 : static_cast<int>(combat.currentRanged);

    float duration = getDurationForCounter(weaponId);
    combat.startCounterAttack(duration);
    combat.attackDirection = dir;

    // Brief invincibility during counter-attack
    auto& hp = m_entity->getComponent<HealthComponent>();
    hp.invincibilityTimer = std::max(hp.invincibilityTimer, duration + 0.1f);

    AudioManager::instance().play(SFX::ParryCounter);

    // Emit weapon-colored particles
    auto& t = m_entity->getComponent<TransformComponent>();
    emitCounterParticles(weaponId, t.getCenter(), dir);
}

void Player::emitCounterParticles(int weaponId, Vec2 pos, Vec2 dir) {
    if (!particles) return;

    SDL_Color color;
    int count = 20;
    float speed = 200.0f;

    switch (static_cast<WeaponID>(weaponId)) {
        case WeaponID::RiftBlade:
            color = {100, 200, 255, 255}; // Cyan-blue rift energy
            count = 25;
            speed = 250.0f;
            break;
        case WeaponID::PhaseDaggers:
            color = {180, 100, 255, 255}; // Purple shadow
            count = 18;
            speed = 300.0f;
            break;
        case WeaponID::VoidHammer:
            color = {160, 80, 255, 255};  // Deep purple void
            count = 30;
            speed = 200.0f;
            break;
        case WeaponID::ShardPistol:
            color = {255, 230, 100, 255}; // Golden shards
            count = 20;
            speed = 350.0f;
            break;
        case WeaponID::RiftShotgun:
            color = {255, 160, 50, 255};  // Orange blast
            count = 22;
            speed = 180.0f;
            break;
        case WeaponID::VoidBeam:
            color = {200, 120, 255, 255}; // Bright purple beam
            count = 24;
            speed = 400.0f;
            break;
        case WeaponID::EntropyScythe:
            color = {200, 40, 255, 255};  // Purple-pink entropy
            count = 28;
            speed = 220.0f;
            break;
        case WeaponID::ChainWhip:
            color = {255, 200, 100, 255}; // Warm chain sparks
            count = 22;
            speed = 320.0f;
            break;
        case WeaponID::GravityGauntlet:
            color = {120, 60, 200, 255};  // Dark gravity purple
            count = 26;
            speed = 150.0f;
            break;
        case WeaponID::GrapplingHook:
            color = {200, 200, 220, 255}; // Steel grey-white
            count = 20;
            speed = 380.0f;
            break;
        case WeaponID::DimLauncher:
            color = {100, 180, 255, 255}; // Dimensional blue
            count = 30;
            speed = 260.0f;
            break;
        case WeaponID::RiftCrossbow:
            color = {160, 255, 200, 255}; // Teal-green bolt
            count = 22;
            speed = 350.0f;
            break;
        default:
            color = {255, 215, 0, 255};   // Gold fallback
            break;
    }

    // Directional burst in attack direction + radial burst
    float dirDeg = std::atan2(dir.y, dir.x) * 180.0f / 3.14159f;
    particles->directionalBurst(pos, count, color, dirDeg, 45.0f, speed, 4.0f);
    particles->burst(pos, count / 2, {255, 255, 255, 255}, speed * 0.6f, 2.5f);
    // Golden flash ring for parry counter feel
    particles->burst(pos, 10, {255, 215, 0, 255}, 160.0f, 3.0f);
}

float Player::getDurationForCounter(int weaponId) const {
    switch (static_cast<WeaponID>(weaponId)) {
        case WeaponID::RiftBlade:    return 0.25f; // Wide arc slash
        case WeaponID::PhaseDaggers: return 0.30f; // 3 rapid hits + teleport
        case WeaponID::VoidHammer:   return 0.35f; // Heavy shockwave
        case WeaponID::ShardPistol:  return 0.20f; // Quick burst fire
        case WeaponID::RiftShotgun:  return 0.20f; // Point blank blast
        case WeaponID::VoidBeam:     return 0.25f; // Overcharge beam
        case WeaponID::EntropyScythe: return 0.30f; // Wide entropy reap
        case WeaponID::ChainWhip:    return 0.20f; // Rapid chain lash
        case WeaponID::GravityGauntlet: return 0.35f; // Gravity slam pull
        case WeaponID::GrapplingHook: return 0.20f; // Quick hook strike
        case WeaponID::DimLauncher:  return 0.30f; // Dual-dimension blast
        case WeaponID::RiftCrossbow: return 0.25f; // Piercing bolt barrage
        default:                     return 0.25f;
    }
}
// ---------- Combo Finisher System ----------

void Player::executeComboFinisher() {
    if (!isFinisherAvailable() || !entityManager || !combatSystemRef) return;

    auto& combat = m_entity->getComponent<CombatComponent>();
    auto& t = m_entity->getComponent<TransformComponent>();
    auto& hp = m_entity->getComponent<HealthComponent>();
    Vec2 playerPos = t.getCenter();

    // Keep combo alive during finisher
    combat.comboTimer = combat.comboWindow;

    // Common: clear availability, set cooldown
    finisherAvailable = FinisherTier::None;
    finisherAvailableTimer = 0;
    finisherCooldown = 3.0f;

    switch (playerClass) {
    case PlayerClass::Voidwalker: {
        // Rift Pulse: AoE dimension wave
        float radius = 160.0f;
        float damage = 40.0f;
        float knockback = 500.0f;
        float stunDuration = 0.5f;

        AudioManager::instance().play(SFX::GroundSlam);
        combatSystemRef->addHitFreeze(0.1f);

        // Purple ring particles
        if (particles) {
            for (int i = 0; i < 16; i++) {
                float angle = i * 6.283185f / 16.0f;
                Vec2 ringPos = {playerPos.x + std::cos(angle) * radius * 0.5f,
                                playerPos.y + std::sin(angle) * radius * 0.5f};
                particles->directionalBurst(ringPos, 4, {180, 80, 255, 255},
                    angle * 57.2958f, 25.0f, 200.0f, 4.0f);
            }
            particles->burst(playerPos, 25, {200, 120, 255, 220}, 180.0f, 5.0f);
        }

        // Damage all enemies in radius
        entityManager->forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.isAlive() || !e.hasComponent<TransformComponent>()) return;
            if (!e.hasComponent<HealthComponent>()) return;
            auto& eHP = e.getComponent<HealthComponent>();
            if (eHP.currentHP <= 0) return;

            auto& et = e.getComponent<TransformComponent>();
            Vec2 ePos = et.getCenter();
            float dx = ePos.x - playerPos.x;
            float dy = ePos.y - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius) return;

            // Apply damage
            eHP.takeDamage(damage);
            combatSystemRef->addDamageEvent(ePos, damage, false, false);

            // Knockback outward
            if (e.hasComponent<PhysicsBody>()) {
                float nx = dist > 1.0f ? dx / dist : 0;
                float ny = dist > 1.0f ? dy / dist : -1.0f;
                e.getComponent<PhysicsBody>().velocity += Vec2{nx, ny} * knockback;
            }

            // Stun
            if (e.hasComponent<AIComponent>()) {
                e.getComponent<AIComponent>().stun(stunDuration);
            }

            // Kill handling (follows Phase Strike pattern)
            if (eHP.currentHP <= 0) {
                combatSystemRef->killCount++;
                KillEvent ke;
                ke.wasComboFinisher = true;
                auto& phys2 = m_entity->getComponent<PhysicsBody>();
                ke.wasAerial = !phys2.onGround;
                if (e.hasComponent<AIComponent>()) {
                    auto& ai = e.getComponent<AIComponent>();
                    ke.enemyType = static_cast<int>(ai.enemyType);
                    ke.wasElite = ai.isElite;
                    ke.wasMiniBoss = ai.isMiniBoss;
                    ke.wasBoss = (ai.enemyType == EnemyType::Boss);
                    if (ai.isMiniBoss) combatSystemRef->killedMiniBoss = true;
                    if (ai.element != EnemyElement::None) combatSystemRef->killedElemental = true;
                    Bestiary::onEnemyKill(ai.enemyType);
                }
                combatSystemRef->killEvents.push_back(ke);

                int wIdx = static_cast<int>(combat.currentMelee);
                if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT))
                    combatSystemRef->weaponKills[wIdx]++;

                addMomentumStack();
                if (combatSystemRef->getDashRefreshOnKill()) dashCooldownTimer = 0;

                Vec2 deathPos = ePos;
                int dropCount = 1;
                if (e.hasComponent<AIComponent>()) {
                    auto& ai = e.getComponent<AIComponent>();
                    if (ai.isMiniBoss) dropCount = 3;
                    else if (ai.isElite) dropCount = 2;
                }
                ItemDrop::spawnRandomDrop(*entityManager, deathPos, e.dimension, dropCount, this);
                AudioManager::instance().play(SFX::EnemyDeath);
                if (particles) {
                    particles->burst(deathPos, 25, {180, 80, 255, 255}, 200.0f, 5.0f);
                }
                e.destroy();
            }
        });
        break;
    }

    case PlayerClass::Berserker: {
        // Blood Cleave: wide melee arc, 2x damage, lifesteal
        float range = 96.0f;
        float damage = combat.meleeAttack.damage * 2.0f;
        float knockback = 400.0f;
        float totalDamageDealt = 0;

        AudioManager::instance().play(SFX::MeleeSwing);
        combatSystemRef->addHitFreeze(0.1f);

        float hDir = facingRight ? 1.0f : -1.0f;
        Vec2 attackCenter = playerPos + Vec2{hDir * 40.0f, 0};

        // Red slash particles
        if (particles) {
            for (int i = 0; i < 10; i++) {
                float angle = (facingRight ? -60.0f : 120.0f) + i * 12.0f;
                particles->directionalBurst(attackCenter, 3, {255, 80, 80, 255},
                    angle, 20.0f, 180.0f, 4.0f);
            }
            particles->burst(attackCenter, 20, {255, 60, 40, 220}, 150.0f, 4.5f);
        }

        // Damage enemies in arc
        entityManager->forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.isAlive() || !e.hasComponent<TransformComponent>()) return;
            if (!e.hasComponent<HealthComponent>()) return;
            auto& eHP = e.getComponent<HealthComponent>();
            if (eHP.currentHP <= 0) return;

            auto& et = e.getComponent<TransformComponent>();
            Vec2 ePos = et.getCenter();
            float dx = ePos.x - playerPos.x;
            float dy = ePos.y - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > range) return;

            // Check arc direction (wide ~120 degree cone in facing direction)
            float dotProd = (dx * hDir) / std::max(1.0f, dist);
            if (dotProd < -0.2f) return; // behind player

            eHP.takeDamage(damage);
            totalDamageDealt += damage;
            combatSystemRef->addDamageEvent(ePos, damage, false, false);

            // Knockback
            if (e.hasComponent<PhysicsBody>()) {
                float nx = dist > 1.0f ? dx / dist : hDir;
                float ny = dist > 1.0f ? dy / dist : -0.3f;
                e.getComponent<PhysicsBody>().velocity += Vec2{nx, ny} * knockback;
            }

            // Kill handling
            if (eHP.currentHP <= 0) {
                combatSystemRef->killCount++;
                KillEvent ke;
                ke.wasComboFinisher = true;
                auto& phys2 = m_entity->getComponent<PhysicsBody>();
                ke.wasAerial = !phys2.onGround;
                if (e.hasComponent<AIComponent>()) {
                    auto& ai = e.getComponent<AIComponent>();
                    ke.enemyType = static_cast<int>(ai.enemyType);
                    ke.wasElite = ai.isElite;
                    ke.wasMiniBoss = ai.isMiniBoss;
                    ke.wasBoss = (ai.enemyType == EnemyType::Boss);
                    if (ai.isMiniBoss) combatSystemRef->killedMiniBoss = true;
                    if (ai.element != EnemyElement::None) combatSystemRef->killedElemental = true;
                    Bestiary::onEnemyKill(ai.enemyType);
                }
                combatSystemRef->killEvents.push_back(ke);

                int wIdx = static_cast<int>(combat.currentMelee);
                if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT))
                    combatSystemRef->weaponKills[wIdx]++;

                addMomentumStack();
                if (combatSystemRef->getDashRefreshOnKill()) dashCooldownTimer = 0;

                Vec2 deathPos = ePos;
                int dropCount = 1;
                if (e.hasComponent<AIComponent>()) {
                    auto& ai = e.getComponent<AIComponent>();
                    if (ai.isMiniBoss) dropCount = 3;
                    else if (ai.isElite) dropCount = 2;
                }
                ItemDrop::spawnRandomDrop(*entityManager, deathPos, e.dimension, dropCount, this);
                AudioManager::instance().play(SFX::EnemyDeath);
                if (particles) {
                    particles->burst(deathPos, 25, {255, 80, 80, 255}, 200.0f, 5.0f);
                }
                e.destroy();
            }
        });

        // Lifesteal: heal 10% of total damage dealt
        float healAmount = totalDamageDealt * 0.1f;
        if (healAmount > 0) {
            hp.currentHP = std::min(hp.currentHP + healAmount, hp.maxHP);
            if (hp.onHeal) hp.onHeal(healAmount);
        }
        break;
    }

    case PlayerClass::Phantom: {
        // Phase Burst: teleport to up to 4 nearest enemies, multi-frame sequence
        float searchRadius = 200.0f;
        float damage = 25.0f;
        (void)damage; // used in updateComboFinisher

        // Cache nearest enemies
        struct EnemyDist {
            Entity* entity;
            float dist;
        };
        std::vector<EnemyDist> candidates;
        entityManager->forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.isAlive() || !e.hasComponent<TransformComponent>()) return;
            if (!e.hasComponent<HealthComponent>()) return;
            auto& eHP = e.getComponent<HealthComponent>();
            if (eHP.currentHP <= 0) return;

            auto& et = e.getComponent<TransformComponent>();
            Vec2 ePos = et.getCenter();
            float dx = ePos.x - playerPos.x;
            float dy = ePos.y - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= searchRadius) {
                candidates.push_back({&e, dist});
            }
        });

        // Sort by distance, take up to 4
        std::sort(candidates.begin(), candidates.end(),
            [](const EnemyDist& a, const EnemyDist& b) { return a.dist < b.dist; });

        finisherTargetCount = 0;
        for (int i = 0; i < std::min(static_cast<int>(candidates.size()), MAX_FINISHER_TARGETS); i++) {
            finisherTargets[i].entity = candidates[i].entity;
            finisherTargets[i].position = candidates[i].entity->getComponent<TransformComponent>().getCenter();
            finisherTargetCount++;
        }

        if (finisherTargetCount > 0) {
            finisherExecuting = true;
            finisherTargetIndex = 0;
            finisherStepTimer = 0;
            finisherExecuteTimer = 0;

            // Make player invulnerable during sequence
            hp.invulnerable = true;

            AudioManager::instance().play(SFX::PhaseStrikeTeleport);

            // Vanish particles at start position
            if (particles) {
                particles->burst(playerPos, 15, {80, 220, 255, 255}, 150.0f, 3.0f);
            }
        }
        break;
    }

    default:
        // Technomancer or future classes: basic AoE burst fallback
        if (particles) {
            particles->burst(playerPos, 15, {230, 180, 50, 255}, 150.0f, 4.0f);
        }
        break;
    }
}

void Player::updateComboFinisher(float dt) {
    if (!finisherExecuting) return;
    if (playerClass != PlayerClass::Phantom) {
        finisherExecuting = false;
        return;
    }

    finisherExecuteTimer += dt;
    finisherStepTimer += dt;

    float stepInterval = 0.08f; // 80ms per target

    auto& t = m_entity->getComponent<TransformComponent>();
    auto& hp = m_entity->getComponent<HealthComponent>();
    auto& combat = m_entity->getComponent<CombatComponent>();

    // Keep combo alive during multi-hit sequence
    combat.comboTimer = combat.comboWindow;
    // Keep invulnerable
    hp.invulnerable = true;

    while (finisherStepTimer >= stepInterval && finisherTargetIndex < finisherTargetCount) {
        finisherStepTimer -= stepInterval;

        auto& target = finisherTargets[finisherTargetIndex];
        Entity* enemy = target.entity;

        if (enemy && enemy->isAlive() && enemy->hasComponent<HealthComponent>()) {
            auto& eHP = enemy->getComponent<HealthComponent>();
            if (eHP.currentHP > 0) {
                auto& et = enemy->getComponent<TransformComponent>();
                Vec2 ePos = et.getCenter();

                // Teleport to enemy
                float dx = ePos.x - t.getCenter().x;
                float behindDir = (dx > 0) ? 1.0f : -1.0f;
                t.position.x = ePos.x + behindDir * 30.0f - t.width * 0.5f;
                t.position.y = ePos.y - (t.height - et.height) * 0.5f;
                t.prevPosition = t.position;
                facingRight = (dx > 0);
                m_entity->getComponent<SpriteComponent>().flipX = !facingRight;

                // Deal damage
                float damage = 25.0f;
                eHP.takeDamage(damage);

                // Cyan teleport particles
                if (particles) {
                    particles->burst(ePos, 12, {80, 220, 255, 255}, 120.0f, 3.0f);
                    particles->burst(t.getCenter(), 8, {80, 220, 255, 180}, 80.0f, 2.5f);
                }

                AudioManager::instance().play(SFX::PhaseStrikeHit);

                if (combatSystemRef) {
                    combatSystemRef->addDamageEvent(ePos, damage, false, false);
                    combatSystemRef->addHitFreeze(0.04f);

                    // Stun
                    if (enemy->hasComponent<AIComponent>()) {
                        enemy->getComponent<AIComponent>().stun(0.3f);
                    }

                    // Kill handling
                    if (eHP.currentHP <= 0) {
                        combatSystemRef->killCount++;
                        KillEvent ke;
                        ke.wasComboFinisher = true;
                        auto& phys = m_entity->getComponent<PhysicsBody>();
                        ke.wasAerial = !phys.onGround;
                        if (enemy->hasComponent<AIComponent>()) {
                            auto& ai = enemy->getComponent<AIComponent>();
                            ke.enemyType = static_cast<int>(ai.enemyType);
                            ke.wasElite = ai.isElite;
                            ke.wasMiniBoss = ai.isMiniBoss;
                            ke.wasBoss = (ai.enemyType == EnemyType::Boss);
                            if (ai.isMiniBoss) combatSystemRef->killedMiniBoss = true;
                            if (ai.element != EnemyElement::None) combatSystemRef->killedElemental = true;
                            Bestiary::onEnemyKill(ai.enemyType);
                        }
                        combatSystemRef->killEvents.push_back(ke);

                        int wIdx = static_cast<int>(combat.currentMelee);
                        if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT))
                            combatSystemRef->weaponKills[wIdx]++;

                        addMomentumStack();
                        if (combatSystemRef->getDashRefreshOnKill()) dashCooldownTimer = 0;

                        Vec2 deathPos = ePos;
                        int dropCount = 1;
                        if (enemy->hasComponent<AIComponent>()) {
                            auto& ai = enemy->getComponent<AIComponent>();
                            if (ai.isMiniBoss) dropCount = 3;
                            else if (ai.isElite) dropCount = 2;
                        }
                        ItemDrop::spawnRandomDrop(*entityManager, deathPos, enemy->dimension, dropCount, this);
                        AudioManager::instance().play(SFX::EnemyDeath);
                        if (particles) {
                            particles->burst(deathPos, 25, {80, 220, 255, 255}, 200.0f, 5.0f);
                        }
                        enemy->destroy();
                    }
                }
            }
        }

        finisherTargetIndex++;
    }

    // Sequence complete
    if (finisherTargetIndex >= finisherTargetCount) {
        finisherExecuting = false;
        hp.invulnerable = false;
        // Appear burst at final position
        if (particles) {
            particles->burst(t.getCenter(), 20, {80, 220, 255, 255}, 150.0f, 4.0f);
        }
        AudioManager::instance().play(SFX::PhaseStrikeTeleport);
    }

    // Safety timeout: 2 seconds max
    if (finisherExecuteTimer > 2.0f) {
        finisherExecuting = false;
        hp.invulnerable = false;
    }
}
