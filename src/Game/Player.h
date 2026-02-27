#pragma once
#include "ECS/Entity.h"
#include "Core/InputManager.h"
#include "Core/Camera.h"
#include "Systems/ParticleSystem.h"
#include "Components/AbilityComponent.h"

class Player {
public:
    Player(EntityManager& entities);

    void update(float dt, const InputManager& input);
    Entity* getEntity() const { return m_entity; }

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

    ParticleSystem* particles = nullptr;
    bool wasInAir = false;

    // Temporary buffs
    float speedBoostTimer = 0;
    float damageBoostTimer = 0;
    float shieldTimer = 0;
    bool hasShield = false;
    float speedBoostMultiplier = 1.5f;
    float damageBoostMultiplier = 1.5f;

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

    void applyBurn(float duration) { burnTimer = std::max(burnTimer, duration); }
    void applyFreeze(float duration) { freezeTimer = std::max(freezeTimer, duration); }
    void applyPoison(float duration) { poisonTimer = std::max(poisonTimer, duration); }
    bool isBurning() const { return burnTimer > 0; }
    bool isFrozen() const { return freezeTimer > 0; }
    bool isPoisoned() const { return poisonTimer > 0; }

    // Entity manager reference for Phase Strike targeting
    class EntityManager* entityManager = nullptr;
    class CombatSystem* combatSystemRef = nullptr;

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
