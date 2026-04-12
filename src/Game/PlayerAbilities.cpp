// PlayerAbilities.cpp -- Split from Player.cpp (abilities, animation)
#include "Player.h"
#include "ECS/EntityManager.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/RelicComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/AIComponent.h"
#include "Core/AudioManager.h"
#include "Systems/CombatSystem.h"
#include "Game/DimensionManager.h"
#include "Game/SpriteConfig.h"
#include "Game/ItemDrop.h"
#include "Game/Bestiary.h"
#include "Game/RelicSystem.h"
#include <cmath>

// TimeTax relic: pay HP cost per ability activation.
// Called at each ability activate() site.
static void payTimeTaxCost(Entity* entity) {
    if (!entity || !entity->hasComponent<RelicComponent>()) return;
    auto& relics = entity->getComponent<RelicComponent>();
    float cost = RelicSystem::getAbilityHPCost(relics);
    if (cost > 0.0f && entity->hasComponent<HealthComponent>()) {
        auto& hp = entity->getComponent<HealthComponent>();
        // Direct deduction (bypass i-frames and armor — this is a self-imposed cost,
        // not incoming damage). Don't let it kill the player outright.
        hp.currentHP = std::max(1.0f, hp.currentHP - cost);
    }
}

void Player::updateAnimation() {
    auto& phys = m_entity->getComponent<PhysicsBody>();
    auto& combat = m_entity->getComponent<CombatComponent>();
    auto& hp = m_entity->getComponent<HealthComponent>();
    auto& sprite = m_entity->getComponent<SpriteComponent>();
    const Uint32 ticks = SDL_GetTicks(); // Cache once per frame

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
            if ((ticks / 80) % 2 == 0) sprite.setColor(255, 255, 255);
            else sprite.setColor(classColor.r, classColor.g, classColor.b);
            break;
        }
        case AnimState::Attack:
            sprite.setColor(255, 220, 100);
            break;
        case AnimState::Dash:
            if (isPhaseThrough()) {
                // Phantom Phase Through: cyan tint, very transparent
                float flicker = 100.0f + 40.0f * std::sin(ticks * 0.03f);
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
        float alpha = 80.0f + 60.0f * std::sin(ticks * 0.02f);
        sprite.color.a = static_cast<Uint8>(alpha);
    }

    // Berserker: Blood Rage glow
    if (isBloodRageActive()) {
        float pulse = 0.6f + 0.4f * std::sin(ticks * 0.012f);
        sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 80 * pulse));
        sprite.color.g = static_cast<Uint8>(sprite.color.g * (0.5f + 0.2f * pulse));
    }

    // Berserker: Momentum glow (orange-red tint scaling with stacks)
    if (hasMomentum()) {
        float intensity = static_cast<float>(momentumStacks) / momentumMaxStacks;
        float pulse = 0.7f + 0.3f * std::sin(ticks * 0.01f * (1.0f + intensity));
        sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 50 * intensity * pulse));
        sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g * (1.0f - 0.15f * intensity)));
    }

    // Voidwalker: Rift Charge shimmer (blue-white pulsing)
    if (isRiftChargeActive()) {
        float pulse = 0.5f + 0.5f * std::sin(ticks * 0.015f);
        sprite.color.b = static_cast<Uint8>(std::min(255.0f, sprite.color.b + 80 * pulse));
        sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g + 30 * pulse));
    }

    // Technomancer: subtle yellow-orange tech glow when constructs are active
    if (isTechnomancer() && (activeTurrets > 0 || activeTraps > 0)) {
        float intensity = static_cast<float>(activeTurrets + activeTraps) / (maxTurrets + maxTraps);
        float pulse = 0.6f + 0.4f * std::sin(ticks * 0.008f);
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
            float pulse = 0.6f + 0.4f * std::sin(ticks * 0.015f);
            sprite.color.r = static_cast<Uint8>(std::min(255.0f, sprite.color.r + 120 * pulse));
            sprite.color.g = static_cast<Uint8>(std::min(255.0f, sprite.color.g * 0.7f + 80 * pulse));
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.4f);
        }
        // Rift Shield active: cyan shimmer
        if (abil.abilities[1].active) {
            float pulse = 0.5f + 0.5f * std::sin(ticks * 0.01f);
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
            // Apply player multiplier chain (relic + class + smith + resonance + boost) at spawn time
            {
                int trapDim = dimensionManager ? dimensionManager->getCurrentDimension() : 0;
                trapDmg *= CombatSystem::computePlayerDamageMult(*m_entity, this, dimensionManager, trapDim, /*isMelee=*/false);
            }
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
                if (!other->isEnemy) return;
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
            payTimeTaxCost(m_entity);
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
        payTimeTaxCost(m_entity);
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
        // Find nearest enemy within range (squared distance — avoids sqrt per enemy)
        Entity* nearestEnemy = nullptr;
        float nearestDistSq = abil.phaseStrikeRange * abil.phaseStrikeRange;
        Vec2 playerPos = t.getCenter();

        entityManager->forEach([&](Entity& e) {
            if (!e.isEnemy) return;
            if (!e.isAlive() || !e.hasComponent<TransformComponent>()) return;
            if (!e.hasComponent<HealthComponent>()) return;
            auto& hp = e.getComponent<HealthComponent>();
            if (hp.currentHP <= 0) return;

            auto& et = e.getComponent<TransformComponent>();
            float dx = et.getCenter().x - playerPos.x;
            float dy = et.getCenter().y - playerPos.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearestEnemy = &e;
            }
        });

        if (nearestEnemy) {
            payTimeTaxCost(m_entity);
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

            // Apply damage with full player multiplier chain
            auto& enemyHP = nearestEnemy->getComponent<HealthComponent>();
            float damage = combat.meleeAttack.damage * abil.phaseStrikeDamageMult;
            {
                int psDim = dimensionManager ? dimensionManager->getCurrentDimension() : 0;
                damage *= CombatSystem::computePlayerDamageMult(*m_entity, this, dimensionManager, psDim, /*isMelee=*/true);
            }
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
