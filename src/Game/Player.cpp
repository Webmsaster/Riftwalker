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
#include "Game/SpriteConfig.h"
#include "Game/RelicSynergy.h"
#include "Game/DimensionManager.h"
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
    const Uint32 frameTicks = SDL_GetTicks(); // Cache once per frame

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

        // Check if hook stopped (hit terrain) — squared comparison avoids sqrt
        float hookSpeedSq = hookPhys.velocity.x * hookPhys.velocity.x +
                            hookPhys.velocity.y * hookPhys.velocity.y;
        if (hookSpeedSq < 10.0f * 10.0f) {
            // Hook hit terrain — attach and start swinging
            attachHook(hookT.getCenter());
        }

        // Check max range (300px, squared comparison)
        Vec2 diff = hookT.getCenter() - playerT.getCenter();
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq > 300.0f * 300.0f) {
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

    // Combo finisher timers
    if (finisherAvailableTimer > 0) {
        finisherAvailableTimer -= dt;
        if (finisherAvailableTimer <= 0) {
            finisherAvailable = FinisherTier::None;
            finisherAvailableTimer = 0;
        }
    }
    if (finisherCooldown > 0) finisherCooldown -= dt;
    if (finisherExecuting) {
        updateComboFinisher(dt);
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

            // Base trail color from equipped weapon for visual identity
            SDL_Color trailColor;
            WeaponID activeWpn = combat.currentMelee; // melee/charged/dash use melee weapon
            switch (activeWpn) {
                case WeaponID::RiftBlade:     trailColor = {220, 240, 255, 255}; break; // white-cyan
                case WeaponID::VoidHammer:    trailColor = {120,  40, 180, 255}; break; // dark purple
                case WeaponID::PhaseDaggers:  trailColor = { 50, 220, 100, 255}; break; // emerald
                case WeaponID::EntropyScythe: trailColor = {200,  30,  50, 255}; break; // crimson
                case WeaponID::ChainWhip:     trailColor = {240, 170,  40, 255}; break; // amber
                default:                      trailColor = {255, 255, 200, 255}; break;
            }
            // Combo stage boosts intensity: higher combo = brighter trail
            int combo = combat.comboCount % 3;
            float comboBright = 1.0f + combo * 0.15f; // 1.0, 1.15, 1.30
            trailColor.r = static_cast<Uint8>(std::min(255.0f, trailColor.r * comboBright));
            trailColor.g = static_cast<Uint8>(std::min(255.0f, trailColor.g * comboBright));
            trailColor.b = static_cast<Uint8>(std::min(255.0f, trailColor.b * comboBright));
            // Override for special attack types
            if (combat.currentAttack == AttackType::Dash)
                trailColor = {100, 200, 255, 255}; // cyan dash override
            else if (combat.currentAttack == AttackType::Charged)
                trailColor = {255, 200, 50, 255}; // gold charged override

            // Intensity peaks mid-swing
            float intensity = 1.0f - std::abs(progress - 0.5f) * 2.0f;
            intensity = 0.5f + intensity * 0.5f; // range: 0.5 to 1.0

            particles->weaponTrail(center, tipPos, trailColor, intensity);

            // Slash arc: emit particles along a curved arc path at peak swing
            if (progress > 0.3f && progress < 0.7f) {
                float arcAngle = std::atan2(combat.attackDirection.y, combat.attackDirection.x);
                // Arc spans ±45° from attack direction
                for (int seg = 0; seg < 4; seg++) {
                    float segT = (seg - 1.5f) / 3.0f; // -0.5 to 0.5
                    float segAngle = arcAngle + segT * 1.57f; // ±45°
                    float arcR = reach * (0.8f + 0.2f * intensity);
                    Vec2 arcPos = {center.x + std::cos(segAngle) * arcR,
                                   center.y + std::sin(segAngle) * arcR};
                    SDL_Color arcColor = trailColor;
                    arcColor.a = static_cast<Uint8>(180 * intensity);
                    particles->burst(arcPos, 1, arcColor, 15.0f, 2.0f + intensity);
                }
            }
        }
    }

    handleAbilities(dt, input);
    updateAnimation();

    // Weapon switch: Q = melee, R = ranged
    if (weaponSwitchCooldown > 0) weaponSwitchCooldown -= dt;
    if ((input.isKeyPressed(SDL_SCANCODE_Q) || input.isKeyDown(SDL_SCANCODE_Q)) && weaponSwitchCooldown <= 0) switchMelee();
    if ((input.isKeyPressed(SDL_SCANCODE_R) || input.isKeyDown(SDL_SCANCODE_R)) && weaponSwitchCooldown <= 0) switchRanged();

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
            // Extra debris on high falls (>400px drop distance)
            float fallDist = t.position.y - fallStartY;
            if (fallDist > 400.0f && !slamLanding) {
                particles->directionalBurst(feetPos, 4, {140, 130, 110, 180}, 180.0f, 70.0f, 60.0f, 3.0f);
                particles->directionalBurst(feetPos, 4, {140, 130, 110, 180}, 0.0f, 70.0f, 60.0f, 3.0f);
            }
        }
        AudioManager::instance().play(slamLanding ? SFX::MeleeHit : SFX::PlayerLand);
    }
    // Track fall start position
    if (!wasInAir && !phys.onGround) {
        fallStartY = m_entity->getComponent<TransformComponent>().position.y;
    }
    wasInAir = !phys.onGround;

    // Footstep dust: subtle ground particles while walking
    if (phys.onGround && std::abs(phys.velocity.x) > 10.0f && particles) {
        footstepDustTimer -= dt;
        if (footstepDustTimer <= 0) {
            footstepDustTimer = 0.15f;
            auto& t = m_entity->getComponent<TransformComponent>();
            Vec2 feetPos = {t.getCenter().x, t.position.y + static_cast<float>(t.height)};
            float behindDir = facingRight ? 210.0f : 330.0f; // slightly behind player
            particles->directionalBurst(feetPos, 1, {160, 145, 120, 140}, behindDir, 40.0f, 30.0f, 2.5f);
        }
    } else {
        footstepDustTimer = 0;
    }

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
            float angle = static_cast<float>(frameTicks) * 0.005f;
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
        if (momentumStacks > 0 && particles && frameTicks % 3 == 0) {
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

    // Variable jump height: release early = lower jump (0.65 for better game feel)
    if (input.isActionReleased(Action::Jump) && phys.velocity.y < 0) {
        phys.velocity.y *= 0.65f;
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
        // Color is dimension-tinted: blue for Dim A, red for Dim B
        spr.afterimageSpawnTimer -= dt;
        if (spr.afterimageSpawnTimer <= 0) {
            spr.afterimageSpawnTimer = 0.04f;
            SDL_Color ghostColor;
            if (isPhaseThrough()) {
                ghostColor = {60, 220, 200, 255};
            } else if (dimensionManager) {
                int dim = dimensionManager->getCurrentDimension();
                ghostColor = (dim == 1) ? SDL_Color{80, 140, 255, 255}   // blue for Dim A
                                        : SDL_Color{255, 80, 100, 255};  // red for Dim B
            } else {
                ghostColor = {cc.r, cc.g, cc.b, 255};
            }
            spr.addAfterimage(t.position.x, t.position.y,
                              static_cast<float>(t.width), static_cast<float>(t.height),
                              ghostColor, spr.flipX);
        }

        // Particle trail: dimension-tinted + speed line particles
        if (particles) {
            SDL_Color trailColor;
            if (dimensionManager) {
                int dim = dimensionManager->getCurrentDimension();
                trailColor = (dim == 1) ? SDL_Color{80, 140, 255, 100}
                                        : SDL_Color{255, 80, 100, 100};
            } else {
                trailColor = {cc.r, cc.g, cc.b, 100};
            }
            particles->burst(t.getCenter(), 2, trailColor, 20.0f, 3.5f);
            // Speed line particles: fast horizontal streaks in dash direction
            float dashDir = spr.flipX ? 180.0f : 0.0f;
            particles->directionalBurst(t.getCenter(), 1, {200, 220, 255, 60},
                                        dashDir + 180.0f, 15.0f, 50.0f, 1.5f);
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
            // Dash end: deceleration dust puff
            if (particles) {
                auto& t2 = m_entity->getComponent<TransformComponent>();
                particles->burst(t2.getCenter(), 6, {160, 160, 180, 100}, 60.0f, 2.5f);
            }
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

        // Dash start burst: directional explosion + dust cloud
        if (particles) {
            auto& t = m_entity->getComponent<TransformComponent>();
            SDL_Color dashColor = {100, 180, 255, 255};
            // Directional speed burst in dash direction
            float dashDir = facingRight ? 0.0f : 180.0f;
            particles->directionalBurst(t.getCenter(), 12, dashColor, dashDir, 40.0f, 250.0f, 4.5f);
            // Backward dust cloud (opposite direction)
            float backDir = facingRight ? 180.0f : 0.0f;
            SDL_Color dustColor = {180, 180, 200, 120};
            particles->directionalBurst(t.getCenter(), 8, dustColor, backDir, 60.0f, 120.0f, 3.0f);
            // White flash burst at start position
            particles->burst(t.getCenter(), 6, {255, 255, 255, 200}, 80.0f, 3.0f);
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

        // Wall slide friction sparks
        if (particles) {
            wallSlideParticleTimer -= dt;
            if (wallSlideParticleTimer <= 0) {
                wallSlideParticleTimer = 0.08f;
                auto& t = m_entity->getComponent<TransformComponent>();
                float px = phys.onWallLeft ? t.position.x : t.position.x + t.width;
                Vec2 sparkPos{px, t.position.y + t.height * 0.4f};
                // Sparks fly downward and slightly outward from wall
                float outDir = phys.onWallLeft ? 210.0f : 330.0f;
                particles->directionalBurst(sparkPos, 2, {255, 200, 100, 180},
                                            outDir, 40.0f, 50.0f, 2.0f);
            }
        }
    } else {
        isWallSliding = false;
        wallSlideParticleTimer = 0;
    }
}

