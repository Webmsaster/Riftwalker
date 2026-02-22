#include "Player.h"
#include "ECS/EntityManager.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/AnimationComponent.h"
#include "Core/AudioManager.h"
#include <cmath>

Player::Player(EntityManager& entities) {
    m_entity = &entities.addEntity("player");

    auto& t = m_entity->addComponent<TransformComponent>(100.0f, 100.0f, 28, 48);
    auto& sprite = m_entity->addComponent<SpriteComponent>();
    sprite.setColor(60, 120, 220); // Blue player
    sprite.renderLayer = 2;

    auto& phys = m_entity->addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 1200.0f;
    phys.airResistance = 200.0f;

    auto& col = m_entity->addComponent<ColliderComponent>();
    col.width = 24;
    col.height = 44;
    col.offset = {2, 4};
    col.layer = LAYER_PLAYER;
    col.mask = LAYER_TILE | LAYER_ENEMY | LAYER_PICKUP | LAYER_TRIGGER;

    auto& hp = m_entity->addComponent<HealthComponent>();
    hp.maxHP = 100.0f;
    hp.currentHP = 100.0f;
    hp.invincibilityTime = 1.0f;

    auto& combat = m_entity->addComponent<CombatComponent>();
    combat.meleeAttack.damage = 20.0f;
    combat.meleeAttack.range = 48.0f;
    combat.meleeAttack.knockback = 250.0f;
    combat.meleeAttack.cooldown = 0.35f;
    combat.meleeAttack.duration = 0.15f;

    combat.rangedAttack.damage = 12.0f;
    combat.rangedAttack.range = 300.0f;
    combat.rangedAttack.knockback = 100.0f;
    combat.rangedAttack.cooldown = 0.6f;
    combat.rangedAttack.duration = 0.1f;
    combat.rangedAttack.type = AttackType::Ranged;

    m_entity->addComponent<AnimationComponent>();
    m_entity->dimension = 0; // Player exists in both dimensions
}

void Player::update(float dt, const InputManager& input) {
    if (!m_entity || !m_entity->isAlive()) return;

    auto& phys = m_entity->getComponent<PhysicsBody>();

    handleDash(dt, input);

    if (!isDashing) {
        handleMovement(dt, input);
        handleJump(input);
        handleWallSlide(dt);
    }

    handleAttack(input);
    updateAnimation();

    // Advance animation timer
    auto& sprite = m_entity->getComponent<SpriteComponent>();
    sprite.animTimer += dt;

    // Landing effect: was airborne, now on ground
    if (phys.onGround && wasInAir) {
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            Vec2 feetPos = {t.getCenter().x, t.position.y + t.height};
            particles->burst(feetPos, 6, {180, 180, 200, 200}, 60.0f, 2.0f);
        }
        AudioManager::instance().play(SFX::PlayerLand);
    }
    wasInAir = !phys.onGround;

    // Reset jumps on ground
    if (phys.onGround) {
        jumpsRemaining = maxJumps;
        isWallSliding = false;
    }
}

void Player::handleMovement(float dt, const InputManager& input) {
    auto& phys = m_entity->getComponent<PhysicsBody>();

    float axis = input.getAxis(Action::MoveLeft, Action::MoveRight);

    if (std::abs(axis) > 0.1f) {
        phys.velocity.x = axis * moveSpeed;
        facingRight = axis > 0;
        m_entity->getComponent<SpriteComponent>().flipX = !facingRight;
    }
}

void Player::handleJump(const InputManager& input) {
    auto& phys = m_entity->getComponent<PhysicsBody>();

    if (input.isActionPressed(Action::Jump)) {
        if (isWallSliding) {
            // Wall jump
            float dir = facingRight ? -1.0f : 1.0f;
            phys.velocity.x = dir * wallJumpForceX;
            phys.velocity.y = wallJumpForceY;
            facingRight = !facingRight;
            isWallSliding = false;
            AudioManager::instance().play(SFX::PlayerJump);
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                particles->burst(t.getCenter(), 8, {200, 200, 255, 255}, 100.0f, 3.0f);
            }
        } else if (phys.onGround || phys.canCoyoteJump() || jumpsRemaining > 0) {
            AudioManager::instance().play(SFX::PlayerJump);
            phys.velocity.y = jumpForce;
            phys.coyoteTimer = 0;
            if (!phys.onGround && !phys.canCoyoteJump()) {
                jumpsRemaining--;
                // Double jump particle effect
                if (particles) {
                    auto& t = m_entity->getComponent<TransformComponent>();
                    particles->burst(
                        {t.getCenter().x, t.position.y + t.height},
                        12, {150, 180, 255, 255}, 80.0f, 3.0f
                    );
                }
            } else {
                jumpsRemaining = maxJumps - 1;
            }
        }
    }

    // Variable jump height: release early = lower jump
    if (input.isActionReleased(Action::Jump) && phys.velocity.y < 0) {
        phys.velocity.y *= 0.5f;
    }
}

void Player::handleDash(float dt, const InputManager& input) {
    if (dashCooldownTimer > 0) dashCooldownTimer -= dt;

    if (isDashing) {
        dashTimer -= dt;
        // Afterimage trail during dash
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            SDL_Color ghostColor = {60, 120, 220, 100};
            particles->burst(t.getCenter(), 2, ghostColor, 20.0f, 6.0f);
        }
        if (dashTimer <= 0) {
            isDashing = false;
            auto& phys = m_entity->getComponent<PhysicsBody>();
            phys.useGravity = true;
            phys.velocity.x *= 0.3f;
        }
        return;
    }

    if (input.isActionPressed(Action::Dash) && dashCooldownTimer <= 0) {
        isDashing = true;
        dashTimer = dashDuration;
        dashCooldownTimer = dashCooldown;
        AudioManager::instance().play(SFX::PlayerDash);

        auto& phys = m_entity->getComponent<PhysicsBody>();
        phys.useGravity = false;
        phys.velocity.y = 0;
        phys.velocity.x = (facingRight ? 1.0f : -1.0f) * dashSpeed;

        // Dash particles
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            SDL_Color dashColor = {100, 180, 255, 255};
            particles->burst(t.getCenter(), 15, dashColor, 150.0f, 4.0f);
        }
    }
}

void Player::handleWallSlide(float dt) {
    auto& phys = m_entity->getComponent<PhysicsBody>();

    bool onWall = phys.onWallLeft || phys.onWallRight;
    if (onWall && !phys.onGround && phys.velocity.y > 0) {
        isWallSliding = true;
        phys.velocity.y = wallSlideSpeed;
        jumpsRemaining = 1; // Allow wall jump

        // Wall slide particles
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            float px = phys.onWallLeft ? t.position.x : t.position.x + t.width;
            if (SDL_GetTicks() % 5 == 0) {
                particles->burst({px, t.position.y + t.height * 0.5f}, 2,
                                {200, 200, 200, 150}, 30.0f, 2.0f);
            }
        }
    } else {
        isWallSliding = false;
    }
}

void Player::handleAttack(const InputManager& input) {
    auto& combat = m_entity->getComponent<CombatComponent>();

    Vec2 dir = {facingRight ? 1.0f : -1.0f, 0.0f};

    if (input.isActionPressed(Action::Attack)) {
        AudioManager::instance().play(SFX::MeleeSwing);
        combat.startAttack(AttackType::Melee, dir);
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            Vec2 attackPos = t.getCenter() + dir * 32.0f;
            particles->burst(attackPos, 8, {255, 255, 200, 255}, 100.0f, 3.0f);
        }
    }

    if (input.isActionPressed(Action::RangedAttack)) {
        combat.startAttack(AttackType::Ranged, dir);
    }
}

void Player::updateAnimation() {
    auto& phys = m_entity->getComponent<PhysicsBody>();
    auto& combat = m_entity->getComponent<CombatComponent>();
    auto& hp = m_entity->getComponent<HealthComponent>();
    auto& sprite = m_entity->getComponent<SpriteComponent>();

    // Determine animation state from physics/combat
    AnimState newState = AnimState::Idle;

    if (hp.currentHP <= 0) {
        newState = AnimState::Dead;
    } else if (hp.isInvincible() && !hp.invulnerable) {
        newState = AnimState::Hurt;
    } else if (isDashing) {
        newState = AnimState::Dash;
    } else if (combat.isAttacking) {
        newState = AnimState::Attack;
    } else if (isWallSliding) {
        newState = AnimState::WallSlide;
    } else if (!phys.onGround) {
        newState = phys.velocity.y < 0 ? AnimState::Jump : AnimState::Fall;
    } else if (std::abs(phys.velocity.x) > 20.0f) {
        newState = AnimState::Run;
    }

    // Reset timer on state change
    if (newState != sprite.animState) {
        sprite.animState = newState;
        sprite.animTimer = 0;
    }

    // Color based on state
    switch (sprite.animState) {
        case AnimState::Hurt: {
            Uint32 t = SDL_GetTicks();
            if ((t / 80) % 2 == 0) sprite.setColor(255, 255, 255);
            else sprite.setColor(60, 120, 220);
            break;
        }
        case AnimState::Attack:
            sprite.setColor(255, 220, 100);
            break;
        case AnimState::Dash:
            sprite.setColor(100, 200, 255, 180);
            break;
        default:
            sprite.setColor(60, 120, 220);
            break;
    }
}
