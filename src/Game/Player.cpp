#include "Player.h"
#include "ECS/EntityManager.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/AIComponent.h"
#include "Core/AudioManager.h"
#include "Systems/CombatSystem.h"
#include "Game/SpriteConfig.h"
#include "Game/RelicSynergy.h"
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
    phys.apexThreshold = 80.0f;         // apex hang when |vel.y| < 80 px/s
    phys.apexGravityMultiplier = 0.4f;  // 60% gravity reduction at jump peak

    auto& col = m_entity->addComponent<ColliderComponent>();
    col.width = 24;
    col.height = 44;
    col.offset = {2, 4};
    col.layer = LAYER_PLAYER;
    col.mask = LAYER_TILE | LAYER_ENEMY | LAYER_PICKUP | LAYER_TRIGGER;

    auto& hp = m_entity->addComponent<HealthComponent>();
    // BALANCE: Player HP 100 -> 120 (more breathing room in early game)
    hp.maxHP = 120.0f;
    hp.currentHP = 120.0f;
    // BALANCE: Invincibility 1.0s -> 1.2s (slightly more recovery time after hit)
    hp.invincibilityTime = 1.2f;

    auto& combat = m_entity->addComponent<CombatComponent>();
    // BALANCE: Melee damage 20 -> 25 (first boss was 55 hits, now ~30 hits)
    combat.meleeAttack.damage = 25.0f;
    combat.meleeAttack.range = 48.0f;
    combat.meleeAttack.knockback = 250.0f;
    combat.meleeAttack.cooldown = 0.35f;
    combat.meleeAttack.duration = 0.15f;

    // BALANCE: Ranged damage 12 -> 15 (proportional to melee buff)
    combat.rangedAttack.damage = 15.0f;
    combat.rangedAttack.range = 300.0f;
    combat.rangedAttack.knockback = 100.0f;
    combat.rangedAttack.cooldown = 0.6f;
    combat.rangedAttack.duration = 0.1f;
    combat.rangedAttack.type = AttackType::Ranged;

    combat.dashAttack.damage = 30.0f;
    combat.dashAttack.range = 64.0f;
    combat.dashAttack.knockback = 400.0f;
    combat.dashAttack.cooldown = 0.0f; // uses dash cooldown instead
    combat.dashAttack.duration = 0.2f;
    combat.dashAttack.type = AttackType::Dash;

    combat.chargedAttack.damage = 50.0f;
    combat.chargedAttack.range = 72.0f;
    combat.chargedAttack.knockback = 500.0f;
    combat.chargedAttack.cooldown = 0.6f;
    combat.chargedAttack.duration = 0.25f;
    combat.chargedAttack.type = AttackType::Charged;

    auto& anim = m_entity->addComponent<AnimationComponent>();
    m_entity->addComponent<AbilityComponent>();
    m_entity->addComponent<RelicComponent>();
    m_entity->dimension = 0; // Player exists in both dimensions

    // Try to load sprite texture and register animations
    SpriteConfig::setupPlayer(anim, sprite);
}

void Player::update(float dt, const InputManager& input) {
    if (!m_entity || !m_entity->isAlive()) return;

    auto& phys = m_entity->getComponent<PhysicsBody>();

    // Tick input buffer timers
    if (jumpBufferTimer > 0) jumpBufferTimer -= dt;
    if (dashBufferTimer > 0) dashBufferTimer -= dt;
    if (dashMomentumTimer > 0) dashMomentumTimer -= dt;

    handleDash(dt, input);

    if (!isDashing) {
        handleMovement(dt, input);
        handleJump(input);
        handleWallSlide(dt);
    }

    handleAttack(input);

    // Weapon trail: emit particles each frame during melee/dash attacks
    {
        auto& combat = m_entity->getComponent<CombatComponent>();
        if (combat.isAttacking && particles &&
            (combat.currentAttack == AttackType::Melee ||
             combat.currentAttack == AttackType::Dash ||
             combat.currentAttack == AttackType::Charged)) {
            auto& t = m_entity->getComponent<TransformComponent>();
            Vec2 center = t.getCenter();
            auto& atk = combat.getCurrentAttackData();

            // Weapon tip position based on attack direction and range
            float reach = atk.range + 8.0f;
            Vec2 tipPos = center + combat.attackDirection * reach;

            // Attack progress (0=start, 1=end)
            float progress = atk.duration > 0.0f ? 1.0f - (combat.attackTimer / atk.duration) : 1.0f;

            // Color varies by attack type
            SDL_Color trailColor;
            if (combat.currentAttack == AttackType::Dash) {
                trailColor = {100, 200, 255, 255}; // cyan for dash
            } else if (combat.currentAttack == AttackType::Charged) {
                trailColor = {255, 200, 50, 255}; // gold for charged
            } else {
                // Melee: escalate color with combo
                switch (combat.comboCount % 3) {
                    case 0: trailColor = {255, 255, 200, 255}; break; // white-warm
                    case 1: trailColor = {255, 200, 80, 255};  break; // orange
                    case 2: trailColor = {255, 100, 50, 255};  break; // red-orange
                    default: trailColor = {255, 255, 200, 255}; break;
                }
            }

            // Intensity peaks mid-swing
            float intensity = 1.0f - std::abs(progress - 0.5f) * 2.0f;
            intensity = 0.5f + intensity * 0.5f; // range: 0.5 to 1.0

            particles->weaponTrail(center, tipPos, trailColor, intensity);
        }
    }

    handleAbilities(dt, input);
    updateAnimation();

    // Weapon switch: Q = melee, E = ranged
    if (weaponSwitchCooldown > 0) weaponSwitchCooldown -= dt;
    if (input.isKeyPressed(SDL_SCANCODE_Q)) switchMelee();
    if (input.isKeyPressed(SDL_SCANCODE_E)) switchRanged();

    // Advance animation timer
    auto& sprite = m_entity->getComponent<SpriteComponent>();
    sprite.animTimer += dt;

    // Landing effect: was airborne, now on ground
    if (phys.onGround && wasInAir) {
        auto& combat = m_entity->getComponent<CombatComponent>();
        bool slamLanding = combat.isAttacking && combat.attackDirection.y > 0.5f;

        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            Vec2 feetPos = {t.getCenter().x, t.position.y + t.height};
            if (slamLanding) {
                // Groundpound shockwave
                particles->burst(feetPos, 20, {255, 200, 80, 255}, 180.0f, 4.0f);
                particles->burst(feetPos, 12, {255, 150, 50, 255}, 100.0f, 3.0f);
            } else {
                particles->burst(feetPos, 6, {180, 180, 200, 200}, 60.0f, 2.0f);
            }
        }
        AudioManager::instance().play(slamLanding ? SFX::MeleeHit : SFX::PlayerLand);
    }
    wasInAir = !phys.onGround;

    // Reset jumps on ground
    if (phys.onGround) {
        jumpsRemaining = maxJumps;
        isWallSliding = false;
    }

    // Update ability cooldowns
    if (m_entity->hasComponent<AbilityComponent>()) {
        auto& abil = m_entity->getComponent<AbilityComponent>();
        abil.update(dt);

        // Ground Slam: fast fall in progress
        if (abil.slamFalling) {
            auto& phys = m_entity->getComponent<PhysicsBody>();
            phys.velocity.y = 800.0f; // Fast slam fall
            phys.velocity.x *= 0.3f;  // Reduce horizontal movement

            if (phys.onGround) {
                // Slam landed!
                abil.slamFalling = false;
                auto& t = m_entity->getComponent<TransformComponent>();
                float fallDist = t.position.y - slamFallStartY;
                float fallBonus = std::min(fallDist / 200.0f, 1.5f); // Up to 1.5x bonus

                AudioManager::instance().play(SFX::GroundSlam);

                if (particles) {
                    Vec2 feetPos = {t.getCenter().x, t.position.y + t.height};
                    int pCount = 25 + static_cast<int>(fallBonus * 15);
                    particles->burst(feetPos, pCount, {255, 180, 60, 255}, 250.0f, 5.0f);
                    particles->burst(feetPos, pCount / 2, {255, 120, 30, 255}, 180.0f, 3.0f);
                    // Ground shockwave ring
                    for (int i = 0; i < 16; i++) {
                        float angle = i * 6.283185f / 16.0f;
                        Vec2 ringDir = {std::cos(angle) * 120.0f, std::sin(angle) * 40.0f - 30.0f};
                        particles->burst({feetPos.x + ringDir.x * 0.3f, feetPos.y + ringDir.y * 0.3f},
                                        2, {255, 200, 80, 200}, 100.0f, 2.0f);
                    }
                }

                // AoE damage is handled by CombatSystem check in PlayState
                // Store slam data for combat system to process
                abil.abilities[0].active = true;
                abil.abilities[0].duration = 0.05f; // brief window for combat to detect
                abil.abilities[0].maxDuration = 0.05f;
            }
        }

        // Rift Shield duration
        if (abil.abilities[1].active && abil.shieldHitsRemaining <= 0) {
            abil.abilities[1].active = false;
            abil.abilities[1].duration = 0;
        }
    }

    // Phase tint decay
    if (phaseTintTimer > 0) {
        phaseTintTimer -= dt;
        if (phaseTintTimer <= 0) isPhaseStriking = false;
    }

    // Update buff timers
    if (speedBoostTimer > 0) {
        speedBoostTimer -= dt;
        if (speedBoostTimer <= 0) speedBoostTimer = 0;
    }
    if (damageBoostTimer > 0) {
        damageBoostTimer -= dt;
        if (damageBoostTimer <= 0) damageBoostTimer = 0;
    }

    // Status effect DOT damage
    if (burnTimer > 0) {
        burnTimer -= dt;
        burnDmgTimer -= dt;
        if (burnDmgTimer <= 0) {
            burnDmgTimer = 0.3f;
            auto& hp = m_entity->getComponent<HealthComponent>();
            hp.takeDamage(5.0f);
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                particles->burst(t.getCenter(), 4, {255, 120, 30, 255}, 60.0f, 1.5f);
            }
        }
    }
    if (freezeTimer > 0) freezeTimer -= dt;
    if (poisonTimer > 0) {
        poisonTimer -= dt;
        poisonDmgTimer -= dt;
        if (poisonDmgTimer <= 0) {
            poisonDmgTimer = 0.5f;
            auto& hp = m_entity->getComponent<HealthComponent>();
            hp.takeDamage(3.0f);
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                particles->burst(t.getCenter(), 3, {80, 220, 60, 255}, 40.0f, 1.5f);
            }
        }
    }
    // Phantom: post-dash invisibility timer
    if (postDashInvisTimer > 0) {
        postDashInvisTimer -= dt;
    }

    if (hasShield) {
        shieldTimer -= dt;
        if (shieldTimer <= 0) {
            hasShield = false;
            shieldTimer = 0;
        }
    }

    // Resolve invulnerability: shield takes priority over dash invis
    auto& hp = m_entity->getComponent<HealthComponent>();
    hp.invulnerable = hasShield || (postDashInvisTimer > 0);
}

void Player::handleMovement(float dt, const InputManager& input) {
    auto& phys = m_entity->getComponent<PhysicsBody>();

    float axis = input.getAxis(Action::MoveLeft, Action::MoveRight);

    if (std::abs(axis) > 0.1f) {
        float speed = moveSpeed * (speedBoostTimer > 0 ? speedBoostMultiplier : 1.0f);
        if (freezeTimer > 0) speed *= 0.4f; // Freeze slows movement
        float targetVelX = axis * speed;

        // Post-dash momentum: keep higher velocity if moving in same direction
        if (dashMomentumTimer > 0 &&
            std::abs(phys.velocity.x) > speed &&
            (phys.velocity.x > 0) == (axis > 0)) {
            // Don't override — let friction naturally decelerate
        } else {
            phys.velocity.x = targetVelX;
        }
        facingRight = axis > 0;
        m_entity->getComponent<SpriteComponent>().flipX = !facingRight;
    }
}

void Player::handleJump(const InputManager& input) {
    auto& phys = m_entity->getComponent<PhysicsBody>();

    // Input buffering: remember jump press for a short window
    if (input.isActionPressed(Action::Jump)) {
        jumpBufferTimer = jumpBufferTime;
    }

    bool wantsJump = jumpBufferTimer > 0;

    if (wantsJump) {
        if (isWallSliding) {
            // Wall jump
            float dir = facingRight ? -1.0f : 1.0f;
            phys.velocity.x = dir * wallJumpForceX;
            phys.velocity.y = wallJumpForceY;
            facingRight = !facingRight;
            isWallSliding = false;
            jumpBufferTimer = 0;
            AudioManager::instance().play(SFX::PlayerJump);
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                particles->burst(t.getCenter(), 8, {200, 200, 255, 255}, 100.0f, 3.0f);
            }
        } else if (phys.onGround || phys.canCoyoteJump() || jumpsRemaining > 0) {
            AudioManager::instance().play(SFX::PlayerJump);
            phys.velocity.y = jumpForce;
            phys.coyoteTimer = 0;
            jumpBufferTimer = 0;
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

    // Input buffering: remember dash press while on cooldown
    if (input.isActionPressed(Action::Dash)) {
        dashBufferTimer = dashBufferTime;
    }

    if (isDashing) {
        dashTimer -= dt;
        // Afterimage trail during dash (class-colored)
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            const auto& cc = ClassSystem::getData(playerClass).color;
            SDL_Color ghostColor = {cc.r, cc.g, cc.b, 100};
            particles->burst(t.getCenter(), 2, ghostColor, 20.0f, 6.0f);
        }
        if (dashTimer <= 0) {
            isDashing = false;
            auto& phys = m_entity->getComponent<PhysicsBody>();
            phys.useGravity = true;
            phys.velocity.x *= 0.7f;
            dashMomentumTimer = dashMomentumTime;
            // Phantom: post-dash invisibility
            if (playerClass == PlayerClass::Phantom) {
                const auto& classData = ClassSystem::getData(PlayerClass::Phantom);
                postDashInvisTimer = classData.postDashInvisTime;
            }
        }
        return;
    }

    if (dashBufferTimer > 0 && dashCooldownTimer <= 0) {
        dashBufferTimer = 0;
        isDashing = true;
        dashTimer = dashDuration;
        dashCooldownTimer = dashCooldown;
        AudioManager::instance().play(SFX::PlayerDash);

        auto& phys = m_entity->getComponent<PhysicsBody>();
        phys.useGravity = false;
        phys.velocity.y = 0;
        float finalDashSpeed = dashSpeed;
        if (m_entity->hasComponent<RelicComponent>()) {
            finalDashSpeed *= (1.0f + RelicSynergy::getDashSpeedBonus(
                m_entity->getComponent<RelicComponent>()));
        }
        phys.velocity.x = (facingRight ? 1.0f : -1.0f) * finalDashSpeed;

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
                // Scale damage with charge
                combat.chargedAttack.damage = 50.0f * (0.5f + 0.5f * chargePercent);
                combat.chargedAttack.knockback = 500.0f * (0.5f + 0.5f * chargePercent);
                combat.releaseCharged(dir);
                AudioManager::instance().play(SFX::ChargedAttackRelease);
                if (particles) {
                    auto& t = m_entity->getComponent<TransformComponent>();
                    Vec2 attackPos = t.getCenter() + dir * 40.0f;
                    int pCount = 15 + static_cast<int>(chargePercent * 20);
                    particles->burst(attackPos, pCount, {255, 200, 50, 255}, 250.0f, 5.0f);
                    particles->burst(attackPos, pCount / 2, {255, 255, 150, 255}, 180.0f, 3.0f);
                }
            } else {
                combat.isCharging = false;
            }
            isChargingAttack = false;
            return;
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

            // Brief invincibility during dash attack
            auto& hp = m_entity->getComponent<HealthComponent>();
            hp.invincibilityTimer = 0.25f;

            Vec2 dashDir = {facingRight ? 1.0f : -1.0f, -0.3f};
            combat.startAttack(AttackType::Dash, dashDir);
            AudioManager::instance().play(SFX::MeleeSwing);
            if (particles) {
                auto& t = m_entity->getComponent<TransformComponent>();
                Vec2 attackPos = t.getCenter() + dashDir * 40.0f;
                particles->burst(attackPos, 18, {100, 200, 255, 255}, 180.0f, 4.0f);
            }
        } else {
            AudioManager::instance().play(SFX::MeleeSwing);
            combat.startAttack(AttackType::Melee, dir);
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

        // Drive spritesheet animation if texture is loaded
        if (sprite.texture) {
            auto& anim = m_entity->getComponent<AnimationComponent>();
            anim.play(SpriteConfig::animStateToName(newState));
        }
    }

    // Class color
    const auto& classColor = ClassSystem::getData(playerClass).color;

    // Color based on state
    switch (sprite.animState) {
        case AnimState::Hurt: {
            Uint32 t = SDL_GetTicks();
            if ((t / 80) % 2 == 0) sprite.setColor(255, 255, 255);
            else sprite.setColor(classColor.r, classColor.g, classColor.b);
            break;
        }
        case AnimState::Attack:
            sprite.setColor(255, 220, 100);
            break;
        case AnimState::Dash:
            sprite.setColor(classColor.r, classColor.g, classColor.b, 180);
            break;
        default:
            sprite.setColor(classColor.r, classColor.g, classColor.b);
            break;
    }

    // Phantom: ghostly during post-dash invis
    if (postDashInvisTimer > 0) {
        float alpha = 80.0f + 60.0f * std::sin(SDL_GetTicks() * 0.02f);
        sprite.color.a = static_cast<Uint8>(alpha);
    }

    // Berserker: Blood Rage glow
    if (isBloodRageActive()) {
        float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.012f);
        sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 80 * pulse));
        sprite.color.g = static_cast<Uint8>(sprite.color.g * (0.5f + 0.2f * pulse));
    }

    // Status effect tinting (overrides above for active effects)
    if (burnTimer > 0 && sprite.animState != AnimState::Hurt) {
        sprite.color.r = std::min(255, sprite.color.r + 100);
        sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.6f);
    }
    if (freezeTimer > 0) {
        sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.5f);
        sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.7f);
        sprite.color.b = std::min(255, sprite.color.b + 80);
    }
    if (poisonTimer > 0) {
        sprite.color.g = std::min(255, sprite.color.g + 60);
        sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.7f);
    }

    // Ability tinting
    if (m_entity->hasComponent<AbilityComponent>()) {
        auto& abil = m_entity->getComponent<AbilityComponent>();
        // Ground Slam falling: orange glow
        if (abil.slamFalling) {
            float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.015f);
            sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 120 * pulse));
            sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g * 0.7f + 80 * pulse));
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.4f);
        }
        // Rift Shield active: cyan shimmer
        if (abil.abilities[1].active) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.01f);
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.5f);
            sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g * 0.8f + 100 * pulse));
            sprite.color.b = static_cast<Uint8>(std::min(255.0f, sprite.color.b + 120 * pulse));
        }
        // Phase Strike tint: purple flash
        if (phaseTintTimer > 0) {
            float t = phaseTintTimer / 0.3f;
            sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 100 * t));
            sprite.color.g = static_cast<Uint8>(sprite.color.g * (1.0f - 0.5f * t));
            sprite.color.b = static_cast<Uint8>(std::min(255.0f, sprite.color.b + 120 * t));
        }
    }
}

void Player::handleAbilities(float dt, const InputManager& input) {
    if (!m_entity->hasComponent<AbilityComponent>()) return;
    auto& abil = m_entity->getComponent<AbilityComponent>();
    auto& phys = m_entity->getComponent<PhysicsBody>();
    auto& t = m_entity->getComponent<TransformComponent>();

    // Ability 1: Ground Slam (must be in air)
    if (input.isActionPressed(Action::Ability1) && abil.abilities[0].isReady()) {
        if (!phys.onGround) {
            abil.slamFalling = true;
            slamFallStartY = t.position.y;
            abil.slamFallStart = t.position.y; // Sync to component for CombatSystem
            abil.activate(0);
            // Brief upward pause before slam
            phys.velocity.y = -50.0f;
            phys.velocity.x *= 0.2f;

            if (particles) {
                particles->burst(t.getCenter(), 10, {255, 180, 60, 255}, 80.0f, 2.0f);
            }
        }
    }

    // Ability 2: Rift Shield
    if (input.isActionPressed(Action::Ability2) && abil.abilities[1].isReady()) {
        abil.activate(1);
        abil.shieldHitsRemaining = abil.shieldMaxHits;
        abil.shieldAbsorbedDamage = 0;
        abil.shieldBurst = false;
        AudioManager::instance().play(SFX::RiftShieldActivate);

        if (particles) {
            // Shield activation burst
            for (int i = 0; i < 12; i++) {
                float angle = i * 6.283185f / 12.0f;
                Vec2 pos = {t.getCenter().x + std::cos(angle) * 30.0f,
                           t.getCenter().y + std::sin(angle) * 30.0f};
                particles->burst(pos, 2, {80, 220, 255, 200}, 40.0f, 2.0f);
            }
        }
    }

    // Ability 3: Phase Strike (teleport behind nearest enemy)
    if (input.isActionPressed(Action::Ability3) && abil.abilities[2].isReady() && entityManager) {
        // Find nearest enemy within range
        Entity* nearestEnemy = nullptr;
        float nearestDist = abil.phaseStrikeRange;
        Vec2 playerPos = t.getCenter();

        entityManager->forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.isAlive() || !e.hasComponent<TransformComponent>()) return;
            if (!e.hasComponent<HealthComponent>()) return;
            auto& hp = e.getComponent<HealthComponent>();
            if (hp.currentHP <= 0) return;

            auto& et = e.getComponent<TransformComponent>();
            float dx = et.getCenter().x - playerPos.x;
            float dy = et.getCenter().y - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < nearestDist) {
                nearestDist = dist;
                nearestEnemy = &e;
            }
        });

        if (nearestEnemy) {
            abil.activate(2);
            auto& et = nearestEnemy->getComponent<TransformComponent>();
            Vec2 enemyCenter = et.getCenter();

            // Teleport behind enemy (opposite of player's approach direction)
            float dx = enemyCenter.x - playerPos.x;
            float behindDir = (dx > 0) ? 1.0f : -1.0f; // go past them
            float teleportX = enemyCenter.x + behindDir * 40.0f;
            float teleportY = enemyCenter.y - (t.height - et.height) * 0.5f;

            // Vanish particles at old position
            AudioManager::instance().play(SFX::PhaseStrikeTeleport);
            if (particles) {
                particles->burst(playerPos, 15, {180, 80, 255, 255}, 150.0f, 3.0f);
            }

            // Teleport
            t.position.x = teleportX - t.width * 0.5f;
            t.position.y = teleportY;
            phys.velocity = {0, 0};
            facingRight = (dx > 0) ? false : true; // face toward enemy (behind them)
            m_entity->getComponent<SpriteComponent>().flipX = !facingRight;

            // Appear particles
            if (particles) {
                particles->burst(t.getCenter(), 15, {180, 80, 255, 255}, 150.0f, 3.0f);
            }

            // Guaranteed backstab crit hit
            auto& combat = m_entity->getComponent<CombatComponent>();
            float hDir = facingRight ? 1.0f : -1.0f;
            Vec2 strikeDir = {hDir, 0.0f};

            // Apply damage directly
            auto& enemyHP = nearestEnemy->getComponent<HealthComponent>();
            float damage = combat.meleeAttack.damage * abil.phaseStrikeDamageMult;
            enemyHP.takeDamage(damage);
            AudioManager::instance().play(SFX::PhaseStrikeHit);

            // Knockback
            if (nearestEnemy->hasComponent<PhysicsBody>()) {
                auto& ePhys = nearestEnemy->getComponent<PhysicsBody>();
                ePhys.velocity += strikeDir * 400.0f + Vec2{0, -200.0f};
            }

            // Stun
            if (nearestEnemy->hasComponent<AIComponent>()) {
                nearestEnemy->getComponent<AIComponent>().stun(0.8f);
            }

            // Crit particles
            if (particles) {
                Vec2 hitPos = et.getCenter();
                particles->burst(hitPos, 20, {255, 80, 255, 255}, 200.0f, 4.0f);
                particles->burst(hitPos, 10, {255, 255, 200, 255}, 150.0f, 3.0f);
            }

            phaseTintTimer = 0.3f;
            isPhaseStriking = true;

            // Kill check for CD refund
            if (enemyHP.currentHP <= 0) {
                abil.reduceCooldown(2, abil.phaseStrikeCDRefund);
                if (particles) {
                    particles->burst(et.getCenter(), 30, {200, 100, 255, 255}, 250.0f, 5.0f);
                }
            }

            // Report damage event to combat system
            if (combatSystemRef) {
                combatSystemRef->addHitFreeze(0.08f);
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
}

bool Player::isBloodRageActive() const {
    if (playerClass != PlayerClass::Berserker) return false;
    const auto& data = ClassSystem::getData(PlayerClass::Berserker);
    auto& hp = m_entity->getComponent<HealthComponent>();
    return hp.getPercent() < data.lowHPThreshold;
}

float Player::getClassDamageMultiplier() const {
    if (isBloodRageActive()) {
        return ClassSystem::getData(PlayerClass::Berserker).rageDmgBonus;
    }
    return 1.0f;
}

float Player::getClassAttackSpeedMultiplier() const {
    if (isBloodRageActive()) {
        return ClassSystem::getData(PlayerClass::Berserker).rageAtkSpeedBonus;
    }
    return 1.0f;
}

void Player::switchMelee() {
    if (weaponSwitchCooldown > 0) return;
    auto& combat = m_entity->getComponent<CombatComponent>();
    combat.currentMelee = WeaponSystem::nextMelee(combat.currentMelee);
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
    }
}
