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
#include "Game/DimensionManager.h"
#include "Game/ItemDrop.h"
#include "Game/Bestiary.h"
#include <cmath>

Player::Player(EntityManager& entities) {
    m_entity = &entities.addEntity("player");

    auto& t = m_entity->addComponent<TransformComponent>(100.0f, 100.0f, 28, 48);
    (void)t;
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
    if (attackLungeTimer > 0) attackLungeTimer -= dt;
    if (hookCooldownTimer > 0) hookCooldownTimer -= dt;

    // Grappling hook: update flying hook, swing physics, enemy pull
    updateGrappleSwing(dt);
    updateHookPull(dt);

    // Grappling hook: check if flying hook hit terrain (velocity zeroed by PhysicsSystem)
    if (hookFlying && hookProjectile && hookProjectile->isAlive()) {
        auto& hookT = hookProjectile->getComponent<TransformComponent>();
        auto& hookPhys = hookProjectile->getComponent<PhysicsBody>();
        auto& playerT = m_entity->getComponent<TransformComponent>();

        // Check if hook stopped (hit terrain)
        float hookSpeed = std::sqrt(hookPhys.velocity.x * hookPhys.velocity.x +
                                     hookPhys.velocity.y * hookPhys.velocity.y);
        if (hookSpeed < 10.0f) {
            // Hook hit terrain — attach and start swinging
            attachHook(hookT.getCenter());
        }

        // Check max range (300px)
        Vec2 diff = hookT.getCenter() - playerT.getCenter();
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (dist > 300.0f) {
            // Out of range — retract
            hookProjectile->destroy();
            hookProjectile = nullptr;
            hookFlying = false;
        }
    } else if (hookFlying) {
        // Hook entity was destroyed externally
        hookFlying = false;
        hookProjectile = nullptr;
    }

    // Grappling hook: release on jump while swinging
    if (isGrappling && input.isActionPressed(Action::Jump)) {
        // Release with a boost in current velocity direction
        detachHook();
        // Give a jump-like upward boost
        phys.velocity.y = std::min(phys.velocity.y, jumpForce * 0.8f);
        jumpsRemaining = std::max(jumpsRemaining - 1, 0);
        AudioManager::instance().play(SFX::PlayerJump);
    }

    // Grappling hook: auto-detach if player touches ground while swinging
    if (isGrappling && phys.onGround) {
        detachHook();
    }

    // Decay landing squash visual effect
    auto& spr = m_entity->getComponent<SpriteComponent>();
    if (spr.landingSquashTimer > 0) spr.landingSquashTimer -= dt;
    spr.updateAfterimages(dt);

    handleDash(dt, input);

    // Reset jumps on ground BEFORE handleJump to avoid overwriting jump consumption
    if (phys.onGround) {
        jumpsRemaining = maxJumps;
    }

    if (!isDashing) {
        handleMovement(dt, input);
        handleJump(input);
        // Falling off a ledge without jumping consumes one jump slot
        // (prevents getting 2 air jumps instead of 1 when walking off edges)
        if (!phys.onGround && !phys.canCoyoteJump() && jumpsRemaining >= maxJumps) {
            jumpsRemaining = maxJumps - 1;
        }
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

    // Weapon switch: Q = melee, R = ranged
    if (weaponSwitchCooldown > 0) weaponSwitchCooldown -= dt;
    if (input.isKeyPressed(SDL_SCANCODE_Q) || input.isKeyDown(SDL_SCANCODE_Q) && weaponSwitchCooldown <= 0) switchMelee();
    if (input.isKeyPressed(SDL_SCANCODE_R) || input.isKeyDown(SDL_SCANCODE_R) && weaponSwitchCooldown <= 0) switchRanged();

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

    // Reset wall slide on ground
    if (phys.onGround) {
        isWallSliding = false;
    }

    // Update ability cooldowns
    if (m_entity->hasComponent<AbilityComponent>()) {
        auto& abil = m_entity->getComponent<AbilityComponent>();
        abil.update(dt);

        // Ground Slam: fast fall in progress
        if (abil.slamFalling) {
            auto& slamPhys = m_entity->getComponent<PhysicsBody>();
            slamPhys.velocity.y = 800.0f; // Fast slam fall
            slamPhys.velocity.x *= 0.3f;  // Reduce horizontal movement

            if (slamPhys.onGround) {
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

    // Status effect duration decay (DOT damage handled in PlayState with relic reduction)
    if (burnTimer > 0) burnTimer -= dt;
    if (freezeTimer > 0) freezeTimer -= dt;
    if (poisonTimer > 0) poisonTimer -= dt;
    // Phantom: post-dash invisibility timer
    if (postDashInvisTimer > 0) {
        postDashInvisTimer -= dt;
    }

    // Technomancer: construct cooldowns
    if (turretCooldownTimer > 0) turretCooldownTimer -= dt;
    if (trapCooldownTimer > 0) trapCooldownTimer -= dt;

    // Voidwalker: Rift Charge timer + shimmer particles
    if (riftChargeTimer > 0) {
        riftChargeTimer -= dt;
        if (riftChargeTimer > 0 && particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            Vec2 center = t.getCenter();
            // Subtle blue shimmer particles around player
            float angle = static_cast<float>(SDL_GetTicks()) * 0.005f;
            float ox = std::cos(angle) * 14.0f;
            float oy = std::sin(angle) * 10.0f;
            particles->burst({center.x + ox, center.y + oy}, 1, {80, 160, 255, 150}, 30.0f, 0.8f);
        }
    }

    // Berserker: Momentum decay
    if (momentumStacks > 0) {
        momentumTimer -= dt;
        if (momentumTimer <= 0) {
            momentumStacks--;
            if (momentumStacks > 0) {
                momentumTimer = momentumDuration;
            } else {
                momentumTimer = 0;
            }
        }
        // Red speed-line particles (intensity scales with stacks)
        if (momentumStacks > 0 && particles && SDL_GetTicks() % 3 == 0) {
            auto& t = m_entity->getComponent<TransformComponent>();
            Vec2 center = t.getCenter();
            float intensity = static_cast<float>(momentumStacks) / momentumMaxStacks;
            Uint8 alpha = static_cast<Uint8>(120 + 135 * intensity);
            // Trail behind player based on movement direction
            float trailX = facingRight ? -12.0f : 12.0f;
            Uint8 green = static_cast<Uint8>(80 + 60 * intensity);
            particles->burst({center.x + trailX, center.y}, 1 + momentumStacks / 2,
                            {255, green, 40, alpha},
                            40.0f + 30.0f * intensity, 1.0f + 0.5f * intensity);
        }
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
    hp.invulnerable = hasShield || (postDashInvisTimer > 0) || isPhaseThrough();
}

void Player::handleMovement(float dt, const InputManager& input) {
    auto& phys = m_entity->getComponent<PhysicsBody>();

    // Skip normal movement while grappling (pendulum physics handles it)
    if (isGrappling) return;

    float axis = input.getAxis(Action::MoveLeft, Action::MoveRight);

    if (std::abs(axis) > 0.1f) {
        float speed = moveSpeed * (speedBoostTimer > 0 ? speedBoostMultiplier : 1.0f);
        if (dimensionManager) speed *= dimensionManager->getResonanceSpeedMult();
        if (hasMomentum()) speed *= getMomentumSpeedMult(); // Berserker momentum
        if (freezeTimer > 0) speed *= 0.4f; // Freeze slows movement
        float targetVelX = axis * speed;

        // Post-dash momentum: keep higher velocity if moving in same direction
        if (dashMomentumTimer > 0 &&
            std::abs(phys.velocity.x) > speed &&
            (phys.velocity.x > 0) == (axis > 0)) {
            // Don't override — let friction naturally decelerate
        } else if (phys.onIce) {
            // Ice: lerp toward target for slippery sliding feel
            float lerpRate = 3.0f * dt; // ~0.05 per frame at 60fps — sluggish control
            phys.velocity.x += (targetVelX - phys.velocity.x) * lerpRate;
        } else {
            phys.velocity.x = targetVelX;
        }
        facingRight = axis > 0;
        m_entity->getComponent<SpriteComponent>().flipX = !facingRight;
    }

    // Melee attack lunge: additive forward boost during brief window after melee start
    if (attackLungeTimer > 0) {
        phys.velocity.x += attackLungeDir * 130.0f;
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
            // Wall jump — direction based on actual wall contact, not facingRight
            // (facingRight can be wrong if player pressed away from wall before jumping)
            float dir = phys.onWallRight ? -1.0f : 1.0f;
            phys.velocity.x = dir * wallJumpForceX;
            phys.velocity.y = wallJumpForceY;
            facingRight = (dir > 0);
            m_entity->getComponent<SpriteComponent>().flipX = !facingRight;
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
                // Ground jump dust puff at feet — spreads sideways
                if (particles) {
                    auto& t = m_entity->getComponent<TransformComponent>();
                    Vec2 feetPos = {t.getCenter().x, t.position.y + t.height};
                    SDL_Color dustColor = {180, 160, 130, 180};
                    particles->directionalBurst(feetPos, 3, dustColor, 180.0f, 50.0f, 50.0f, 2.5f);
                    particles->directionalBurst(feetPos, 3, dustColor, 0.0f, 50.0f, 50.0f, 2.5f);
                }
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
        auto& t = m_entity->getComponent<TransformComponent>();
        auto& spr = m_entity->getComponent<SpriteComponent>();
        const auto& cc = ClassSystem::getData(playerClass).color;

        // Spawn afterimage ghost copies every ~0.04s during dash
        spr.afterimageSpawnTimer -= dt;
        if (spr.afterimageSpawnTimer <= 0) {
            spr.afterimageSpawnTimer = 0.04f;
            SDL_Color ghostColor = {cc.r, cc.g, cc.b, 255};
            if (isPhaseThrough()) ghostColor = {60, 220, 200, 255};
            spr.addAfterimage(t.position.x, t.position.y,
                              static_cast<float>(t.width), static_cast<float>(t.height),
                              ghostColor, spr.flipX);
        }

        // Particle trail (smaller now that afterimages handle the main visual)
        if (particles) {
            SDL_Color ghostColor = {cc.r, cc.g, cc.b, 80};
            particles->burst(t.getCenter(), 1, ghostColor, 15.0f, 4.0f);
            // Phantom Phase Through: extra cyan phase particles
            if (isPhaseThrough()) {
                particles->burst(t.getCenter(), 3, {60, 220, 200, 120}, 35.0f, 5.0f);
            }
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
            if (isPhaseThrough()) {
                // Phantom Phase Through: cyan tint, very transparent
                float flicker = 100.0f + 40.0f * std::sin(SDL_GetTicks() * 0.03f);
                sprite.setColor(60, 220, 200, static_cast<Uint8>(flicker));
            } else {
                sprite.setColor(classColor.r, classColor.g, classColor.b, 180);
            }
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

    // Berserker: Momentum glow (orange-red tint scaling with stacks)
    if (hasMomentum()) {
        float intensity = static_cast<float>(momentumStacks) / momentumMaxStacks;
        float pulse = 0.7f + 0.3f * std::sin(SDL_GetTicks() * 0.01f * (1.0f + intensity));
        sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 50 * intensity * pulse));
        sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g * (1.0f - 0.15f * intensity)));
    }

    // Voidwalker: Rift Charge shimmer (blue-white pulsing)
    if (isRiftChargeActive()) {
        float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.015f);
        sprite.color.b = static_cast<Uint8>(std::min(255.0f, sprite.color.b + 80 * pulse));
        sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g + 30 * pulse));
    }

    // Technomancer: subtle yellow-orange tech glow when constructs are active
    if (isTechnomancer() && (activeTurrets > 0 || activeTraps > 0)) {
        float intensity = static_cast<float>(activeTurrets + activeTraps) / (maxTurrets + maxTraps);
        float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.008f);
        sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 25 * intensity * pulse));
        sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g + 15 * intensity * pulse));
    }

    // Status effect tinting (overrides above for active effects)
    if (burnTimer > 0 && sprite.animState != AnimState::Hurt) {
        sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 100));
        sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.6f);
    }
    if (freezeTimer > 0) {
        sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.5f);
        sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.7f);
        sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b + 80));
    }
    if (poisonTimer > 0) {
        sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 60));
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

    // Technomancer: Deploy Turret (Ability1 / Q) and Shock Trap (Ability2 / E)
    if (playerClass == PlayerClass::Technomancer) {
        // Ability 1: Deploy Turret
        if (input.isActionPressed(Action::Ability1) && turretCooldownTimer <= 0 && activeTurrets < maxTurrets && entityManager) {
            const auto& classData = ClassSystem::getData(PlayerClass::Technomancer);
            float turretDmg = 8.0f * classData.turretDamageMult;   // Construct Mastery: +20%
            float turretLife = 12.0f * classData.turretDurationMult; // Construct Mastery: +50% longer

            // Spawn turret entity at player's feet
            Vec2 spawnPos = {t.getCenter().x + (facingRight ? 30.0f : -30.0f),
                             t.position.y + t.height - 20.0f};

            auto& turret = entityManager->addEntity("player_turret");
            turret.dimension = 0; // exists in both dimensions
            auto& tt = turret.addComponent<TransformComponent>(spawnPos.x - 10, spawnPos.y - 10, 20, 20);
            (void)tt;
            auto& tSprite = turret.addComponent<SpriteComponent>();
            tSprite.setColor(230, 180, 50);
            tSprite.renderLayer = 2;

            auto& tPhys = turret.addComponent<PhysicsBody>();
            tPhys.useGravity = true;
            tPhys.gravity = 980.0f;

            auto& tCol = turret.addComponent<ColliderComponent>();
            tCol.width = 20;
            tCol.height = 20;
            tCol.layer = LAYER_TRIGGER;
            tCol.mask = LAYER_TILE;
            tCol.type = ColliderType::Dynamic;

            auto& tHP = turret.addComponent<HealthComponent>();
            tHP.maxHP = turretLife; // lifetime timer stored as HP, decayed externally
            tHP.currentHP = turretLife;
            tHP.invulnerable = true;

            auto& tCombat = turret.addComponent<CombatComponent>();
            tCombat.rangedAttack.damage = turretDmg;
            tCombat.rangedAttack.range = 160.0f;    // 60px detect range but 160px projectile range
            tCombat.rangedAttack.cooldown = 1.5f;
            tCombat.rangedAttack.type = AttackType::Ranged;

            auto& tAI = turret.addComponent<AIComponent>();
            tAI.enemyType = EnemyType::Turret; // reuse turret AI type
            tAI.detectRange = 160.0f;
            tAI.attackCooldown = 1.5f;
            tAI.attackTimer = 0.5f; // brief delay before first shot

            activeTurrets++;
            turretCooldownTimer = turretCooldown;
            AudioManager::instance().play(SFX::RiftRepair); // deploy sound

            if (particles) {
                particles->burst(spawnPos, 12, {230, 180, 50, 200}, 80.0f, 2.5f);
            }
        }

        // Ability 2: Shock Trap
        if (input.isActionPressed(Action::Ability2) && trapCooldownTimer <= 0 && activeTraps < maxTraps && entityManager) {
            const auto& classData = ClassSystem::getData(PlayerClass::Technomancer);
            float trapDmg = 25.0f * classData.turretDamageMult;    // Construct Mastery: +20%
            float trapLife = 15.0f * classData.turretDurationMult;  // Construct Mastery: +50% longer

            // Spawn trap at player's feet
            Vec2 spawnPos = {t.getCenter().x, t.position.y + t.height - 6.0f};

            auto& trap = entityManager->addEntity("player_trap");
            trap.dimension = 0;
            auto& trapT = trap.addComponent<TransformComponent>(spawnPos.x - 12, spawnPos.y - 6, 24, 12);
            (void)trapT;
            auto& trapSprite = trap.addComponent<SpriteComponent>();
            trapSprite.setColor(255, 200, 50);
            trapSprite.renderLayer = 1; // below entities

            auto& trapPhys = trap.addComponent<PhysicsBody>();
            trapPhys.useGravity = false; // sits on floor

            auto& trapCol = trap.addComponent<ColliderComponent>();
            trapCol.width = 24;
            trapCol.height = 12;
            trapCol.layer = LAYER_TRIGGER;
            trapCol.mask = LAYER_ENEMY;
            trapCol.type = ColliderType::Trigger;

            auto& trapHP = trap.addComponent<HealthComponent>();
            trapHP.maxHP = trapLife;
            trapHP.currentHP = trapLife;
            trapHP.invulnerable = true;

            // Store damage in combat component for trigger callback
            auto& trapCombat = trap.addComponent<CombatComponent>();
            trapCombat.meleeAttack.damage = trapDmg;

            // Trigger: on enemy contact, deal damage + stun + electric particles, then destroy
            auto* playerPtr = this;
            auto* csRef = combatSystemRef;
            trapCol.onTrigger = [trapDmg, playerPtr, csRef](Entity* self, Entity* other) {
                if (!other || !other->isAlive()) return;
                if (other->getTag().find("enemy") == std::string::npos) return;
                if (!other->hasComponent<HealthComponent>()) return;

                auto& hp = other->getComponent<HealthComponent>();
                hp.takeDamage(trapDmg);

                // Stun for 1 second
                if (other->hasComponent<AIComponent>()) {
                    other->getComponent<AIComponent>().stun(1.0f);
                }

                // Knockback upward
                if (other->hasComponent<PhysicsBody>()) {
                    other->getComponent<PhysicsBody>().velocity.y -= 200.0f;
                }

                // Report damage event for floating numbers
                if (csRef && other->hasComponent<TransformComponent>()) {
                    Vec2 hitPos = other->getComponent<TransformComponent>().getCenter();
                    csRef->addDamageEvent(hitPos, trapDmg, false, false, false);
                }

                // Electric particles at trap location
                if (playerPtr->particles && self->hasComponent<TransformComponent>()) {
                    Vec2 trapPos = self->getComponent<TransformComponent>().getCenter();
                    playerPtr->particles->burst(trapPos, 20, {255, 255, 100, 255}, 120.0f, 3.0f);
                    playerPtr->particles->burst(trapPos, 10, {100, 200, 255, 200}, 80.0f, 2.0f);
                }

                AudioManager::instance().play(SFX::ElectricChain);

                // Destroy trap after triggering
                if (playerPtr) playerPtr->activeTraps = std::max(0, playerPtr->activeTraps - 1);
                self->destroy();
            };

            activeTraps++;
            trapCooldownTimer = trapCooldown;
            AudioManager::instance().play(SFX::ShockTrap); // placement buzz

            if (particles) {
                particles->burst(spawnPos, 8, {255, 200, 50, 200}, 60.0f, 2.0f);
            }
        }

        // Phase Strike still available for Technomancer (Ability3)
        // Fall through to Ability3 handling below
    }

    // Ability 1: Ground Slam (must be in air) — not for Technomancer (uses turret instead)
    if (playerClass != PlayerClass::Technomancer && input.isActionPressed(Action::Ability1) && abil.abilities[0].isReady()) {
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

    // Ability 2: Rift Shield — not for Technomancer (uses shock trap instead)
    if (playerClass != PlayerClass::Technomancer && input.isActionPressed(Action::Ability2) && abil.abilities[1].isReady()) {
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
            t.prevPosition = t.position; // Sync for interpolation (no smear on teleport)
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

            // Report damage + hit freeze to combat system
            if (combatSystemRef) {
                Vec2 hitPos = et.getCenter();
                combatSystemRef->addDamageEvent(hitPos, damage, false, true); // always crit
                combatSystemRef->addHitFreeze(0.08f);

                // Kill check for CD refund + proper kill tracking
                if (enemyHP.currentHP <= 0) {
                    abil.reduceCooldown(2, abil.phaseStrikeCDRefund);

                    // Track kill with context for combat challenges
                    combatSystemRef->killCount++;
                    auto& phys2 = m_entity->getComponent<PhysicsBody>();
                    KillEvent ke;
                    ke.wasAerial = !phys2.onGround;
                    if (nearestEnemy->hasComponent<AIComponent>()) {
                        auto& keAI = nearestEnemy->getComponent<AIComponent>();
                        ke.enemyType = static_cast<int>(keAI.enemyType);
                        ke.wasElite = keAI.isElite;
                        ke.wasMiniBoss = keAI.isMiniBoss;
                        ke.wasBoss = (keAI.enemyType == EnemyType::Boss);
                    }
                    combatSystemRef->killEvents.push_back(ke);

                    // Weapon mastery: Phase Strike kills count for current melee weapon
                    int wIdx = static_cast<int>(combat.currentMelee);
                    if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT))
                        combatSystemRef->weaponKills[wIdx]++;

                    // Mini-boss / elemental tracking + bestiary
                    if (nearestEnemy->hasComponent<AIComponent>()) {
                        auto& tAI = nearestEnemy->getComponent<AIComponent>();
                        if (tAI.isMiniBoss) combatSystemRef->killedMiniBoss = true;
                        if (tAI.element != EnemyElement::None) combatSystemRef->killedElemental = true;
                        Bestiary::onEnemyKill(tAI.enemyType);
                    }

                    // Berserker momentum
                    addMomentumStack();
                    // Run buff: DashRefresh on kill
                    if (combatSystemRef->getDashRefreshOnKill()) {
                        dashCooldownTimer = 0;
                    }

                    // Item drops (mini-bosses 3x, elites 2x)
                    Vec2 deathPos = et.getCenter();
                    int dropCount = 1;
                    if (nearestEnemy->hasComponent<AIComponent>()) {
                        auto& tAI = nearestEnemy->getComponent<AIComponent>();
                        if (tAI.isMiniBoss) dropCount = 3;
                        else if (tAI.isElite) dropCount = 2;
                    }
                    ItemDrop::spawnRandomDrop(*entityManager, deathPos,
                        nearestEnemy->dimension, dropCount, this);

                    AudioManager::instance().play(SFX::EnemyDeath);
                    if (particles) {
                        particles->burst(deathPos, 30, {200, 100, 255, 255}, 250.0f, 5.0f);
                    }
                    nearestEnemy->destroy();
                }
            } else if (enemyHP.currentHP <= 0) {
                // Fallback if no combat system ref
                abil.reduceCooldown(2, abil.phaseStrikeCDRefund);
                if (particles) {
                    particles->burst(et.getCenter(), 30, {200, 100, 255, 255}, 250.0f, 5.0f);
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
        if (other->getTag() == "player") return;

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

    if (newDist > hookRopeLength) {
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

    if (dist < 32.0f) {
        // Close enough, stop pulling
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
