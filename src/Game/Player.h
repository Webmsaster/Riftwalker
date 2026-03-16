#pragma once
#include "ECS/Entity.h"
#include "Core/InputManager.h"
#include "Core/Camera.h"
#include "Systems/ParticleSystem.h"
#include "Components/AbilityComponent.h"
#include "Components/RelicComponent.h"
#include "Game/ClassSystem.h"

class Player {
public:
    Player(EntityManager& entities);

    void update(float dt, const InputManager& input);
    Entity* getEntity() const { return m_entity; }

    // Class system
    PlayerClass playerClass = PlayerClass::Voidwalker;
    void applyClassStats();
    bool isBloodRageActive() const;
    float getClassDamageMultiplier() const;
    float getClassAttackSpeedMultiplier() const;

    // Phantom: post-dash invisibility
    float postDashInvisTimer = 0;

    // Phantom: Phase Through — invulnerable during dash, damages enemies phased through
    bool isPhaseThrough() const { return isDashing && playerClass == PlayerClass::Phantom; }
    float phaseDamage = 15.0f;       // damage dealt to enemies phased through
    float phaseSlowDuration = 0.4f;  // slow duration on phased enemies

    // Voidwalker: Dimensional Affinity - Rift Charge buff after dim-switch
    float riftChargeTimer = 0;
    float riftChargeDuration = 2.5f;
    float riftChargeDamageMult = 1.3f; // +30% damage
    bool isRiftChargeActive() const { return riftChargeTimer > 0 && playerClass == PlayerClass::Voidwalker; }
    void activateRiftCharge();

    // Technomancer: Construct Mastery — turrets and traps
    int activeTurrets = 0;
    int maxTurrets = 2;
    int activeTraps = 0;
    int maxTraps = 3;
    float turretCooldownTimer = 0;
    float turretCooldown = 10.0f;
    float trapCooldownTimer = 0;
    float trapCooldown = 6.0f;
    bool isTechnomancer() const { return playerClass == PlayerClass::Technomancer; }

    // Berserker: Momentum - kill streaks build stacks for speed/attack speed
    int momentumStacks = 0;
    int momentumMaxStacks = 5;
    float momentumTimer = 0;
    float momentumDuration = 5.0f;     // seconds per stack refresh
    float momentumSpeedPerStack = 0.06f;  // +6% move speed per stack
    float momentumAtkSpdPerStack = 0.05f; // +5% attack speed per stack
    void addMomentumStack();
    bool hasMomentum() const { return momentumStacks > 0 && playerClass == PlayerClass::Berserker; }
    float getMomentumSpeedMult() const;
    float getMomentumAtkSpeedMult() const;

    // Stats
    float moveSpeed = 250.0f;
    float jumpForce = -420.0f;
    float dashSpeed = 500.0f;
    float dashDuration = 0.2f;
    float wallSlideSpeed = 60.0f;
    float wallJumpForceX = 300.0f;
    float wallJumpForceY = -380.0f;
    int maxJumps = 2; // double jump

    // State
    bool isChargingAttack = false;
    bool isDashing = false;
    bool isWallSliding = false;
    bool facingRight = true;
    int jumpsRemaining = 2;
    int riftShardsCollected = 0;

    // Dash
    float dashTimer = 0;
    float dashCooldown = 0.5f;
    float dashCooldownTimer = 0;

    // Input buffering (jump/dash register even if pressed slightly too early)
    float jumpBufferTimer = 0;
    float jumpBufferTime = 0.1f;   // 100ms buffer window
    float dashBufferTimer = 0;
    float dashBufferTime = 0.08f;  // 80ms buffer window

    // Post-dash momentum preservation
    float dashMomentumTimer = 0;
    float dashMomentumTime = 0.15f; // 150ms momentum window after dash

    // Melee attack lunge (brief forward push on melee start)
    float attackLungeTimer = 0;
    float attackLungeDir = 0; // +1.0 or -1.0

    // Charge attack ready cue (played once when charge reaches min time)
    bool chargeReadyCued = false;

    ParticleSystem* particles = nullptr;
    bool wasInAir = false;

    // Temporary buffs
    float speedBoostTimer = 0;
    float damageBoostTimer = 0;
    float shieldTimer = 0;
    bool hasShield = false;
    float speedBoostMultiplier = 1.5f;
    float damageBoostMultiplier = 1.5f;

    // Pickup effect flags (consumed by PlayState for particles)
    bool pickupShieldPending = false;
    bool pickupSpeedPending = false;
    bool pickupDamagePending = false;

    // Ability state
    float slamFallStartY = 0;    // Y when slam initiated
    float phaseTintTimer = 0;    // visual tint after phase strike
    bool isPhaseStriking = false;

    // Status effects
    float burnTimer = 0;
    float burnDmgTimer = 0;      // ticks damage every 0.3s
    float freezeTimer = 0;       // slows movement
    float poisonTimer = 0;
    float poisonDmgTimer = 0;    // ticks damage every 0.5s

    float dotDurationMult = 1.0f; // Achievement bonus: < 1.0 = shorter DOTs
    void applyBurn(float duration) { burnTimer = std::max(burnTimer, duration * dotDurationMult); }
    void applyFreeze(float duration) { freezeTimer = std::max(freezeTimer, duration * dotDurationMult); }
    void applyPoison(float duration) { poisonTimer = std::max(poisonTimer, duration * dotDurationMult); }
    bool isBurning() const { return burnTimer > 0; }
    bool isFrozen() const { return freezeTimer > 0; }
    bool isPoisoned() const { return poisonTimer > 0; }

    // Weapon switching
    void switchMelee();   // Cycle melee weapon (Q)
    void switchRanged();  // Cycle ranged weapon (E)
    void applyWeaponStats();
    float weaponSwitchCooldown = 0;

    // Entity manager reference for Phase Strike targeting
    class EntityManager* entityManager = nullptr;
    class CombatSystem* combatSystemRef = nullptr;
    class DimensionManager* dimensionManager = nullptr;

private:
    void handleMovement(float dt, const InputManager& input);
    void handleJump(const InputManager& input);
    void handleDash(float dt, const InputManager& input);
    void handleWallSlide(float dt);
    void handleAttack(const InputManager& input);
    void handleAbilities(float dt, const InputManager& input);
    void updateAnimation();

    Entity* m_entity;
};
