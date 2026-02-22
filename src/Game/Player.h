#pragma once
#include "ECS/Entity.h"
#include "Core/InputManager.h"
#include "Core/Camera.h"
#include "Systems/ParticleSystem.h"

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

private:
    void handleMovement(float dt, const InputManager& input);
    void handleJump(const InputManager& input);
    void handleDash(float dt, const InputManager& input);
    void handleWallSlide(float dt);
    void handleAttack(const InputManager& input);
    void updateAnimation();

    Entity* m_entity;
};
