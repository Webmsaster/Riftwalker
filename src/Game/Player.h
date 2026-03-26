#pragma once
#include "ECS/Entity.h"
#include "Core/InputManager.h"
#include "Core/Camera.h"
#include "Systems/ParticleSystem.h"
#include "Components/AbilityComponent.h"
#include "Components/RelicComponent.h"
#include "Game/ClassSystem.h"

struct CombatComponent;

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

    // XP / Level system
    int level = 1;
    int xp = 0;
    int xpToNextLevel = 100;      // XP required for next level-up
    bool levelUpPending = false;   // Consumed by PlayState for celebration effects
    int getXPForLevel(int lvl) const { return 80 + lvl * 20; }
    void addXP(int amount) {
        xp += amount;
        while (xp >= xpToNextLevel) {
            xp -= xpToNextLevel;
            level++;
            xpToNextLevel = getXPForLevel(level);
            levelUpPending = true;
        }
    }
    float getXPPercent() const {
        return xpToNextLevel > 0 ? static_cast<float>(xp) / xpToNextLevel : 0.0f;
    }

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
    float footstepDustTimer = 0;  // Spawn walking dust every 0.15s

    // Temporary buffs
    float speedBoostTimer = 0;
    float damageBoostTimer = 0;
    float shieldTimer = 0;
    bool hasShield = false;
    float speedBoostMultiplier = 1.5f;
    float damageBoostMultiplier = 1.5f;

    // Blacksmith weapon upgrades (per-run, permanent within run)
    float smithMeleeDmgMult = 1.0f;   // Melee damage multiplier from Blacksmith
    float smithRangedDmgMult = 1.0f;  // Ranged damage multiplier from Blacksmith
    float smithAtkSpdMult = 1.0f;     // Attack speed multiplier from Blacksmith

    // Pickup effect flags (consumed by PlayState for particles)
    bool pickupShieldPending = false;
    bool pickupSpeedPending = false;
    bool pickupDamagePending = false;
    int weaponPickupPending = -1; // WeaponID cast to int, -1 = none

    // Combo Finisher system
    enum class FinisherTier { None = 0, Minor = 1, Major = 2, Ultimate = 3 };
    FinisherTier finisherAvailable = FinisherTier::None;
    float finisherAvailableTimer = 0;
    float finisherAvailableMaxTime = 2.0f;
    bool finisherExecuting = false;
    float finisherExecuteTimer = 0;
    float finisherCooldown = 0;
    int finisherTargetIndex = 0;
    float finisherStepTimer = 0;
    struct FinisherTarget {
        Entity* entity = nullptr;
        Vec2 position{0, 0};
    };
    static constexpr int MAX_FINISHER_TARGETS = 4;
    FinisherTarget finisherTargets[MAX_FINISHER_TARGETS] = {};
    int finisherTargetCount = 0;
    bool isFinisherAvailable() const { return finisherAvailable != FinisherTier::None && finisherAvailableTimer > 0; }
    void executeComboFinisher();
    void updateComboFinisher(float dt);

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
    int weaponSwitchPending = 0; // 0=none, 1=melee switched, 2=ranged switched (consumed by PlayState)

    // Grappling Hook state
    bool isGrappling = false;         // Currently swinging on rope
    bool hookFlying = false;          // Hook projectile in flight
    Vec2 hookAttachPoint{0, 0};       // World position where hook attached
    float hookRopeLength = 0;         // Current rope length (distance at attach)
    float hookCooldownTimer = 0;      // Cooldown after detach (1.5s)
    Entity* hookProjectile = nullptr; // Flying hook entity (tracked for rope drawing)
    Entity* hookPulledEnemy = nullptr; // Enemy being pulled toward player
    float hookPullTimer = 0;          // Duration of enemy pull
    void fireGrapplingHook(const Vec2& dir);
    void attachHook(const Vec2& point);
    void detachHook();
    void updateGrappleSwing(float dt);
    void updateHookPull(float dt);

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

    // Parry counter-attack
    void executeCounterAttack(CombatComponent& combat, Vec2 dir);
    void emitCounterParticles(int weaponId, Vec2 pos, Vec2 dir);
    float getDurationForCounter(int weaponId) const;

    Entity* m_entity;
};
