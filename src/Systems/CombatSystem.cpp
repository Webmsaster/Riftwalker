#include "CombatSystem.h"
#include "Components/TransformComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AIComponent.h"
#include "Components/AbilityComponent.h"
#include "Components/RelicComponent.h"
#include "Core/AudioManager.h"
#include "Game/ItemDrop.h"
#include "Game/Player.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/Enemy.h"
#include "Game/Bestiary.h"
#include "Game/ClassSystem.h"
#include <cmath>

float CombatSystem::consumeHitFreeze() {
    float t = m_pendingHitFreeze;
    m_pendingHitFreeze = 0;
    return t;
}

void CombatSystem::update(EntityManager& entities, float dt, int currentDimension) {
    auto combatEnts = entities.getEntitiesWithComponent<CombatComponent>();

    for (auto* e : combatEnts) {
        auto& combat = e->getComponent<CombatComponent>();
        if (!combat.isAttacking) continue;
        if (!e->hasComponent<TransformComponent>()) continue;

        // Check dimension
        if (e->dimension != 0 && e->dimension != currentDimension) continue;

        processAttack(*e, entities, currentDimension);
    }

    // Ground Slam AoE: check if player just landed a slam
    entities.forEach([&](Entity& e) {
        if (e.getTag() != "player") return;
        if (!e.hasComponent<AbilityComponent>()) return;
        auto& abil = e.getComponent<AbilityComponent>();

        // Ground Slam AoE damage
        if (abil.abilities[0].active && abil.abilities[0].duration > 0) {
            auto& pt = e.getComponent<TransformComponent>();
            Vec2 slamCenter = {pt.getCenter().x, pt.position.y + pt.height};
            float radius = abil.slamRadius;
            float damage = abil.slamDamage;
            // Fall height bonus
            float fallDist = pt.position.y - abil.slamFallStart;
            float fallBonus = std::max(0.0f, std::min(fallDist / 200.0f, 1.5f));
            damage *= (1.0f + fallBonus);

            entities.forEach([&](Entity& target) {
                if (target.getTag().find("enemy") == std::string::npos) return;
                if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                if (target.dimension != 0 && target.dimension != currentDimension) return;

                auto& tt = target.getComponent<TransformComponent>();
                float dx = tt.getCenter().x - slamCenter.x;
                float dy = tt.getCenter().y - slamCenter.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < radius) {
                    float dmgScale = 1.0f - (dist / radius) * 0.4f; // 60-100% based on distance
                    float finalDmg = damage * dmgScale;
                    target.getComponent<HealthComponent>().takeDamage(finalDmg);
                    m_damageEvents.push_back({tt.getCenter(), finalDmg, false, true});

                    // Stun
                    if (target.hasComponent<AIComponent>()) {
                        target.getComponent<AIComponent>().stun(abil.slamStunDuration);
                    }
                    // Knockback away from slam center
                    if (target.hasComponent<PhysicsBody>()) {
                        Vec2 kb = {dx, dy - 100.0f};
                        float len = std::sqrt(kb.x * kb.x + kb.y * kb.y);
                        if (len > 0) kb = kb * (400.0f / len);
                        target.getComponent<PhysicsBody>().velocity += kb;
                    }
                    if (m_particles) {
                        m_particles->burst(tt.getCenter(), 10, {255, 200, 80, 255}, 150.0f, 3.0f);
                    }

                    // Kill tracking
                    if (target.getComponent<HealthComponent>().currentHP <= 0) {
                        killCount++;
                        if (target.hasComponent<AIComponent>()) {
                            auto& tAI = target.getComponent<AIComponent>();
                            if (tAI.isMiniBoss) killedMiniBoss = true;
                            if (tAI.element != EnemyElement::None) killedElemental = true;
                        }
                        ItemDrop::spawnRandomDrop(entities, tt.getCenter(), target.dimension, 1, m_player);
                        target.destroy();
                        if (m_particles) {
                            m_particles->burst(tt.getCenter(), 25, {255, 180, 60, 255}, 200.0f, 4.0f);
                        }
                    }
                }
            });

            if (m_camera) m_camera->shake(12.0f, 0.3f);
            m_pendingHitFreeze += 0.12f;

            // Mark slam as processed
            abil.abilities[0].duration = 0;
            abil.abilities[0].active = false;
        }
    });

    // Rift Shield: absorb incoming damage to player
    // (handled in processAttack below via parry-like check)

    // Process element burn DoT on enemies
    entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") == std::string::npos) return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& ai = e.getComponent<AIComponent>();
        if (ai.burnTimer > 0) {
            ai.burnTimer -= dt;
            ai.burnDmgTick -= dt;
            if (ai.burnDmgTick <= 0 && e.hasComponent<HealthComponent>()) {
                ai.burnDmgTick = 0.3f;
                float burnDmg = 5.0f;
                e.getComponent<HealthComponent>().takeDamage(burnDmg);
                m_damageEvents.push_back({e.hasComponent<TransformComponent>() ?
                    e.getComponent<TransformComponent>().getCenter() : Vec2{0,0},
                    burnDmg, false, false});
                if (m_particles && e.hasComponent<TransformComponent>()) {
                    m_particles->burst(e.getComponent<TransformComponent>().getCenter(),
                        3, {255, 120, 30, 255}, 60.0f, 1.5f);
                }
                // Burn kill
                if (e.getComponent<HealthComponent>().currentHP <= 0) {
                    killCount++;
                    if (ai.isMiniBoss) killedMiniBoss = true;
                    if (ai.element != EnemyElement::None) killedElemental = true;
                    ItemDrop::spawnRandomDrop(entities, e.getComponent<TransformComponent>().getCenter(),
                        e.dimension, 1, m_player);
                    e.destroy();
                    if (m_particles) {
                        m_particles->burst(e.getComponent<TransformComponent>().getCenter(),
                            15, {255, 100, 30, 255}, 180.0f, 3.0f);
                    }
                }
            }
        }
    });

    // Projectile lifetime: destroy stale projectiles
    entities.forEach([&](Entity& proj) {
        if (proj.getTag() != "projectile") return;
        if (!proj.hasComponent<SpriteComponent>()) return;
        auto& sprite = proj.getComponent<SpriteComponent>();
        sprite.animTimer += dt;
        if (sprite.animTimer > 3.0f) {
            proj.destroy();
        }
        // Fade out in last second
        if (sprite.animTimer > 2.0f) {
            sprite.color.a = static_cast<Uint8>(255 * (3.0f - sprite.animTimer));
        }
    });
}

void CombatSystem::processAttack(Entity& attacker, EntityManager& entities, int currentDim) {
    auto& combat = attacker.getComponent<CombatComponent>();
    auto& transform = attacker.getComponent<TransformComponent>();

    bool isPlayer = (attacker.getTag() == "player");
    auto& atkData = combat.getCurrentAttackData();

    if (combat.currentAttack == AttackType::Ranged) {
        // Create projectile (only once at start of attack)
        if (combat.attackTimer >= atkData.duration - 0.02f) {
            Vec2 pos = transform.getCenter();

            // Weapon-specific ranged behavior (player only)
            if (isPlayer && combat.currentRanged == WeaponID::RiftShotgun) {
                // 5 pellets in a fan pattern
                float baseAngle = std::atan2(combat.attackDirection.y, combat.attackDirection.x);
                float spread = 0.4f; // ~23 degrees total
                for (int i = 0; i < 5; i++) {
                    float angle = baseAngle + (i - 2) * (spread / 4.0f);
                    Vec2 dir = {std::cos(angle), std::sin(angle)};
                    createProjectile(entities, pos, dir, atkData.damage, 350.0f, attacker.dimension);
                }
                AudioManager::instance().play(SFX::RangedShot);
            } else if (isPlayer && combat.currentRanged == WeaponID::VoidBeam) {
                // Continuous beam: create piercing projectile
                createProjectile(entities, pos, combat.attackDirection,
                                atkData.damage, 500.0f, attacker.dimension);
                // No extra SFX each tick (too fast)
            } else {
                createProjectile(entities, pos, combat.attackDirection,
                                atkData.damage, 400.0f, attacker.dimension);
                AudioManager::instance().play(SFX::RangedShot);
            }
        }
        return;
    }

    bool isDashAttack = (combat.currentAttack == AttackType::Dash);

    // Melee / Dash: check all potential targets
    Vec2 attackCenter = transform.getCenter() + combat.attackDirection * atkData.range * 0.5f;
    float attackRadius = atkData.range;

    entities.forEach([&](Entity& target) {
        if (&target == &attacker) return;
        if (!target.hasComponent<HealthComponent>() || !target.hasComponent<TransformComponent>()) return;

        // Check dimension
        if (target.dimension != 0 && target.dimension != currentDim) return;

        // Player attacks enemies, enemies attack player
        bool targetIsEnemy = target.getTag().find("enemy") != std::string::npos;
        if (isPlayer && !targetIsEnemy) return;
        if (!isPlayer && target.getTag() != "player") return;

        auto& targetTransform = target.getComponent<TransformComponent>();
        Vec2 targetCenter = targetTransform.getCenter();
        float dx = targetCenter.x - attackCenter.x;
        float dy = targetCenter.y - attackCenter.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < attackRadius) {
            // Rift Shield check: absorb hits while shield is active
            if (!isPlayer && target.getTag() == "player" && target.hasComponent<AbilityComponent>()) {
                auto& targetAbil = target.getComponent<AbilityComponent>();
                if (targetAbil.abilities[1].active && targetAbil.shieldHitsRemaining > 0) {
                    targetAbil.shieldHitsRemaining = std::max(0, targetAbil.shieldHitsRemaining - 1);
                    targetAbil.shieldAbsorbedDamage += atkData.damage;

                    // Heal player on absorb
                    if (target.hasComponent<HealthComponent>()) {
                        auto& hp = target.getComponent<HealthComponent>();
                        hp.currentHP = std::min(hp.maxHP, hp.currentHP + targetAbil.shieldHealPerAbsorb);
                    }

                    // Reflect projectiles
                    if (combat.currentAttack == AttackType::Ranged) {
                        AudioManager::instance().play(SFX::RiftShieldReflect);
                        // Create reflected projectile back at attacker
                        Vec2 reflectDir = combat.attackDirection * -1.0f;
                        auto& targetT = target.getComponent<TransformComponent>();
                        createProjectile(entities, targetT.getCenter(), reflectDir,
                                        atkData.damage * 1.5f, 500.0f, target.dimension);
                    } else {
                        AudioManager::instance().play(SFX::RiftShieldAbsorb);
                    }

                    // Phantom class: speed boost on shield absorb
                    if (m_player && m_player->playerClass == PlayerClass::Phantom) {
                        m_player->speedBoostTimer = 1.5f;
                        m_player->speedBoostMultiplier = 1.3f;
                    }

                    if (m_particles) {
                        auto& targetT = target.getComponent<TransformComponent>();
                        Vec2 shieldPos = targetT.getCenter();
                        m_particles->burst(shieldPos, 12, {80, 220, 255, 255}, 120.0f, 2.0f);
                    }

                    if (m_camera) m_camera->shake(4.0f, 0.1f);
                    m_pendingHitFreeze += 0.06f;

                    // Shield burst at 0 hits remaining
                    if (targetAbil.shieldHitsRemaining <= 0) {
                        targetAbil.shieldBurst = true;
                        targetAbil.abilities[1].active = false;
                        AudioManager::instance().play(SFX::RiftShieldBurst);

                        auto& targetT = target.getComponent<TransformComponent>();
                        Vec2 burstCenter = targetT.getCenter();

                        // AoE burst damage
                        entities.forEach([&](Entity& nearby) {
                            if (nearby.getTag().find("enemy") == std::string::npos) return;
                            if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                            auto& nt = nearby.getComponent<TransformComponent>();
                            float dx = nt.getCenter().x - burstCenter.x;
                            float dy = nt.getCenter().y - burstCenter.y;
                            if (dx * dx + dy * dy < targetAbil.shieldBurstRadius * targetAbil.shieldBurstRadius) {
                                nearby.getComponent<HealthComponent>().takeDamage(targetAbil.shieldBurstDamage);
                                m_damageEvents.push_back({nt.getCenter(), targetAbil.shieldBurstDamage, false, false});
                                if (nearby.hasComponent<AIComponent>()) {
                                    nearby.getComponent<AIComponent>().stun(0.5f);
                                }
                                if (nearby.hasComponent<PhysicsBody>()) {
                                    Vec2 kb = {dx, dy - 50.0f};
                                    float len = std::sqrt(kb.x * kb.x + kb.y * kb.y);
                                    if (len > 0) kb = kb * (300.0f / len);
                                    nearby.getComponent<PhysicsBody>().velocity += kb;
                                }
                            }
                        });

                        if (m_particles) {
                            m_particles->burst(burstCenter, 30, {80, 200, 255, 255}, 250.0f, 4.0f);
                            m_particles->burst(burstCenter, 15, {255, 255, 255, 255}, 180.0f, 3.0f);
                        }
                        if (m_camera) m_camera->shake(10.0f, 0.25f);
                        m_pendingHitFreeze += 0.1f;
                    }

                    return; // Absorbed - negate hit
                }
            }

            // Parry check: if enemy hits player during parry window, negate damage
            if (!isPlayer && target.getTag() == "player" && target.hasComponent<CombatComponent>()) {
                auto& playerCombat = target.getComponent<CombatComponent>();
                if (playerCombat.isParrying) {
                    playerCombat.isParrying = false;
                    playerCombat.parryTimer = 0;
                    playerCombat.onParrySuccess();
                    AudioManager::instance().play(SFX::Parry);

                    // Stun attacker for 1.5s
                    if (attacker.hasComponent<AIComponent>()) {
                        attacker.getComponent<AIComponent>().stun(1.5f);
                    }

                    // Parry feedback
                    if (m_camera) m_camera->shake(6.0f, 0.15f);
                    m_pendingHitFreeze += 0.12f;
                    if (m_particles) {
                        m_particles->burst(targetCenter, 20, {255, 215, 0, 255}, 200.0f, 3.0f);
                        m_particles->burst(targetCenter, 10, {255, 255, 255, 255}, 150.0f, 2.0f);
                    }
                    return; // Negate the hit entirely
                }
            }

            auto& hp = target.getComponent<HealthComponent>();
            float comboMult = combat.comboCount * (0.15f + m_comboBonus);
            float damage = atkData.damage * (1.0f + comboMult);

            // Relic damage multiplier (player attacks only)
            if (isPlayer && attacker.hasComponent<RelicComponent>()) {
                auto& relics = attacker.getComponent<RelicComponent>();
                float hpPct = attacker.getComponent<HealthComponent>().getPercent();
                damage *= RelicSystem::getDamageMultiplier(relics, hpPct);
                // PhaseHunter synergy: consume buff after attack
                if (relics.phaseHunterBuffActive) {
                    relics.phaseHunterBuffActive = false;
                }
            }

            // Class damage multiplier (Berserker Blood Rage)
            if (isPlayer && m_player) {
                damage *= m_player->getClassDamageMultiplier();
            }

            // Pogo bounce: downward attack bounces player up
            bool isDownwardAttack = isPlayer && combat.attackDirection.y > 0.5f;
            if (isDownwardAttack && attacker.hasComponent<PhysicsBody>()) {
                auto& attackerPhys = attacker.getComponent<PhysicsBody>();
                attackerPhys.velocity.y = -350.0f;
                if (m_player) m_player->jumpsRemaining = m_player->maxJumps;
            }

            // Critical hit check (player only)
            bool isCrit = false;
            if (isPlayer) {
                // Phase Daggers: every 5th melee hit is a guaranteed crit
                if (combat.currentMelee == WeaponID::PhaseDaggers &&
                    combat.currentAttack == AttackType::Melee) {
                    combat.daggerHitCount++;
                    if (combat.daggerHitCount >= 5) {
                        combat.daggerHitCount = 0;
                        damage *= 2.0f;
                        isCrit = true;
                        AudioManager::instance().play(SFX::CriticalHit);
                        if (m_particles) {
                            m_particles->burst(targetCenter, 12, {180, 100, 255, 255}, 180.0f, 3.0f);
                        }
                    }
                }
                // Parry success guarantees crit
                bool guaranteedCrit = (combat.parrySuccessTimer > 0);
                float roll = static_cast<float>(std::rand()) / RAND_MAX;
                if (guaranteedCrit || (m_critChance > 0 && roll < m_critChance)) {
                    damage *= 2.0f;
                    isCrit = true;
                    if (guaranteedCrit) {
                        combat.parrySuccessTimer = 0;
                        AudioManager::instance().play(SFX::ParryCounter);
                    } else {
                        AudioManager::instance().play(SFX::CriticalHit);
                    }
                    if (m_particles) {
                        m_particles->burst(targetCenter, 16, {255, 255, 255, 255}, 200.0f, 3.0f);
                        m_particles->burst(targetCenter, 8, {255, 240, 180, 255}, 140.0f, 2.0f);
                    }
                    if (m_camera) m_camera->shake(8.0f, 0.2f);
                    m_pendingHitFreeze += 0.06f;
                }
            }

            // Charged attack: scale damage with charge percent
            bool isChargedAttack = (combat.currentAttack == AttackType::Charged);
            if (isChargedAttack) {
                // Charged attacks already have scaled base damage from Player
                if (m_camera) m_camera->shake(12.0f, 0.3f);
                m_pendingHitFreeze += 0.12f;
                if (m_particles) {
                    m_particles->burst(targetCenter, 25, {255, 200, 50, 255}, 250.0f, 5.0f);
                }
            }

            // Damage boost from pickup
            if (isPlayer && m_player && m_player->damageBoostTimer > 0) {
                damage *= m_player->damageBoostMultiplier;
            }

            // Check for Shielder shield blocking
            bool shieldBlocked = false;
            if (isPlayer && target.hasComponent<AIComponent>()) {
                auto& targetAI = target.getComponent<AIComponent>();
                if (targetAI.enemyType == EnemyType::Shielder && targetAI.shieldUp) {
                    bool attackFromRight = attackCenter.x > targetCenter.x;
                    if (attackFromRight == targetAI.facingRight) {
                        shieldBlocked = true;
                    }
                }
            }

            // Elite Shielded: shield absorbs damage before HP
            if (isPlayer && target.hasComponent<AIComponent>()) {
                auto& tAI = target.getComponent<AIComponent>();
                if (tAI.isElite && tAI.eliteMod == EliteModifier::Shielded && tAI.eliteShieldHP > 0) {
                    float absorbed = std::min(damage, tAI.eliteShieldHP);
                    tAI.eliteShieldHP -= absorbed;
                    damage -= absorbed;
                    if (m_particles) {
                        m_particles->burst(targetCenter, 6, {80, 150, 255, 255}, 80.0f, 1.5f);
                    }
                    if (damage <= 0) {
                        m_damageEvents.push_back({targetCenter, absorbed, false, false});
                        return; // Fully absorbed by shield
                    }
                }
            }

            if (shieldBlocked) {
                float actualDmg = damage * 0.1f;
                hp.takeDamage(actualDmg);
                m_damageEvents.push_back({targetCenter, actualDmg, !isPlayer, false});
                if (m_particles) {
                    Vec2 shieldPos = {targetCenter.x + (target.getComponent<AIComponent>().facingRight ? 16.0f : -16.0f),
                                      targetCenter.y};
                    m_particles->burst(shieldPos, 8, {100, 200, 255, 255}, 120.0f, 2.0f);
                }
                AudioManager::instance().play(SFX::RiftFail);
                if (m_camera) m_camera->shake(2.0f, 0.08f);
            } else {
                hp.takeDamage(damage);
                m_damageEvents.push_back({targetCenter, damage, !isPlayer, isCrit});

                // Combo-stage knockback variation (player only)
                int comboStage = isPlayer ? ((combat.comboCount - 1) % 3) : 0;
                if (target.hasComponent<PhysicsBody>()) {
                    auto& phys = target.getComponent<PhysicsBody>();
                    Vec2 knockDir = combat.attackDirection;
                    float knockMult = 1.0f;

                    if (isChargedAttack) {
                        // Charged attack: massive knockback
                        knockDir.y = -0.6f;
                        knockMult = 2.0f;
                    } else if (isDashAttack) {
                        // Dash attack: massive horizontal knockback
                        knockDir.y = -0.5f;
                        knockMult = 1.8f;
                    } else if (isPlayer) {
                        switch (comboStage) {
                            case 0: // Normal horizontal hit
                                knockDir.y = -0.3f;
                                break;
                            case 1: // Diagonal sweep
                                knockDir.y = -0.6f;
                                knockMult = 1.1f;
                                break;
                            case 2: // Finisher uppercut
                                knockDir.y = -1.2f;
                                knockMult = 1.4f;
                                break;
                        }
                    } else {
                        knockDir.y = -0.3f;
                    }

                    phys.velocity += knockDir * atkData.knockback * knockMult;
                }

                // Stun: dash attack stuns 1s, combo finisher (stage 2) stuns 0.5s
                if (isPlayer && target.hasComponent<AIComponent>()) {
                    auto& targetAI = target.getComponent<AIComponent>();
                    if (isDashAttack) {
                        targetAI.stun(1.0f);
                        AudioManager::instance().play(SFX::EnemyStun);
                    } else if (comboStage == 2) {
                        targetAI.stun(0.5f);
                        AudioManager::instance().play(SFX::EnemyStun);
                    }
                }

                // Air Juggle: upward attack on grounded enemy = launch
                bool isUpwardAttack = isPlayer && combat.attackDirection.y < -0.5f;
                if (isPlayer && target.hasComponent<AIComponent>() && target.hasComponent<PhysicsBody>()) {
                    auto& targetAI = target.getComponent<AIComponent>();
                    auto& targetPhys = target.getComponent<PhysicsBody>();

                    if (targetAI.state == AIState::Juggled) {
                        // Follow-up hit on juggled enemy: reset upward velocity, bonus damage
                        targetAI.juggleHitCount++;
                        targetPhys.velocity.y = -300.0f;
                        targetAI.juggleTimer = std::max(targetAI.juggleTimer, 1.0f);
                        float juggleBonus = targetAI.juggleHitCount * targetAI.juggleDamageBonus;
                        // Apply bonus (damage already calculated, add retroactively)
                        float bonusDmg = atkData.damage * juggleBonus;
                        hp.takeDamage(bonusDmg);
                        m_damageEvents.push_back({targetCenter, bonusDmg, false, false});
                        if (m_particles) {
                            m_particles->burst(targetCenter, 6, {200, 150, 255, 255}, 100.0f, 2.0f);
                        }
                    } else if (isUpwardAttack && targetPhys.onGround) {
                        // Launch enemy into the air
                        targetAI.launch();
                        targetPhys.velocity.y = -500.0f;
                        AudioManager::instance().play(SFX::AirJuggleLaunch);
                        if (m_particles) {
                            m_particles->burst(targetCenter, 12, {180, 140, 255, 255}, 150.0f, 3.0f);
                        }
                    }
                }

                // SFX
                AudioManager::instance().play(isPlayer ? SFX::MeleeHit : SFX::EnemyHit);

                // Screen shake scales with combo stage (dash attack gets extra shake)
                if (m_camera) {
                    float shakeIntensity, shakeDuration;
                    if (isDashAttack) {
                        shakeIntensity = 10.0f;
                        shakeDuration = 0.25f;
                    } else if (isPlayer) {
                        shakeIntensity = 5.0f + comboStage * 3.0f;
                        shakeDuration = 0.12f + comboStage * 0.04f;
                    } else {
                        shakeIntensity = 3.0f;
                        shakeDuration = 0.12f;
                    }
                    m_camera->shake(shakeIntensity, shakeDuration);
                }

                // Hit-freeze scales with combo stage
                if (isDashAttack) {
                    m_pendingHitFreeze += 0.1f;
                } else if (isPlayer) {
                    float freezeBase = isCrit ? 0.08f : 0.05f;
                    m_pendingHitFreeze += freezeBase + comboStage * 0.03f;
                } else {
                    m_pendingHitFreeze += 0.06f;
                }

                // Particles scale with combo stage
                if (m_particles) {
                    SDL_Color hitColor;
                    int particleCount;
                    if (isDashAttack) {
                        hitColor = {100, 200, 255, 255};
                        particleCount = 25;
                    } else if (isPlayer) {
                        switch (comboStage) {
                            case 0: hitColor = {255, 80, 80, 255}; particleCount = 8; break;
                            case 1: hitColor = {255, 160, 50, 255}; particleCount = 12; break;
                            case 2: hitColor = {255, 255, 100, 255}; particleCount = 20; break;
                            default: hitColor = {255, 80, 80, 255}; particleCount = 8; break;
                        }
                    } else {
                        hitColor = {100, 150, 255, 255};
                        particleCount = 8;
                    }
                    m_particles->damageEffect(targetCenter, hitColor);

                    Vec2 hitPos = {(attackCenter.x + targetCenter.x) * 0.5f,
                                   (attackCenter.y + targetCenter.y) * 0.5f};
                    float burstSpeed = isDashAttack ? 250.0f : (120.0f + comboStage * 60.0f);
                    float burstLife = isDashAttack ? 5.0f : (2.0f + comboStage);
                    m_particles->burst(hitPos, particleCount, {255, 220, 100, 255}, burstSpeed, burstLife);
                }
            }

            // Relic: ThornMail - reflect damage when player is hit
            if (!isPlayer && target.getTag() == "player" && target.hasComponent<RelicComponent>()) {
                auto& relics = target.getComponent<RelicComponent>();
                float thornDmg = RelicSystem::getThornDamage(relics);
                if (thornDmg > 0 && attacker.hasComponent<HealthComponent>()) {
                    attacker.getComponent<HealthComponent>().takeDamage(thornDmg);
                    m_damageEvents.push_back({transform.getCenter(), thornDmg, false, false});
                    if (m_particles) {
                        m_particles->burst(transform.getCenter(), 6, {200, 100, 50, 255}, 100.0f, 2.0f);
                    }

                    // ChainThorns synergy: chain thorn damage to 2 nearby enemies
                    float chainDmg = RelicSynergy::getThornChainDamage(relics);
                    if (chainDmg > 0) {
                        Vec2 attackerPos = attacker.getComponent<TransformComponent>().getCenter();
                        int chainsLeft = 2;
                        entities.forEach([&](Entity& e) {
                            if (chainsLeft <= 0) return;
                            if (!e.isAlive() || &e == &attacker) return;
                            if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>()) return;
                            auto& ePos = e.getComponent<TransformComponent>().getCenter();
                            float dist = std::sqrt((ePos.x - attackerPos.x) * (ePos.x - attackerPos.x) +
                                                   (ePos.y - attackerPos.y) * (ePos.y - attackerPos.y));
                            if (dist < 150.0f) {
                                e.getComponent<HealthComponent>().takeDamage(chainDmg);
                                m_damageEvents.push_back({ePos, chainDmg, false, false});
                                if (m_particles) {
                                    m_particles->burst(ePos, 4, {255, 255, 80, 255}, 80.0f, 2.0f);
                                }
                                chainsLeft--;
                            }
                        });
                    }
                }
            }

            // Relic: EchoStrike - 20% chance double hit (player attacks only)
            if (isPlayer && attacker.hasComponent<RelicComponent>()) {
                auto& relics = attacker.getComponent<RelicComponent>();
                if (RelicSystem::rollEchoStrike(relics) && !shieldBlocked) {
                    float echoDmg = damage * 0.5f;
                    hp.takeDamage(echoDmg);
                    m_damageEvents.push_back({targetCenter, echoDmg, false, false});
                    if (m_particles) {
                        m_particles->burst(targetCenter, 8, {120, 200, 255, 255}, 120.0f, 2.0f);
                    }
                }
            }

            // Relic: ChainLightning on kill
            if (isPlayer && hp.currentHP <= 0 && attacker.hasComponent<RelicComponent>()) {
                auto& relics = attacker.getComponent<RelicComponent>();
                float chainDmg = RelicSystem::getChainLightningDamage(relics);
                if (chainDmg > 0) {
                    // Find nearest enemy and deal chain damage
                    float nearestDist = 150.0f;
                    Entity* nearestTarget = nullptr;
                    entities.forEach([&](Entity& nearby) {
                        if (&nearby == &target || !nearby.isAlive()) return;
                        if (nearby.getTag().find("enemy") == std::string::npos) return;
                        if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                        if (nearby.dimension != 0 && nearby.dimension != currentDim) return;
                        auto& nt = nearby.getComponent<TransformComponent>();
                        float dx2 = nt.getCenter().x - targetCenter.x;
                        float dy2 = nt.getCenter().y - targetCenter.y;
                        float dist2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
                        if (dist2 < nearestDist) {
                            nearestDist = dist2;
                            nearestTarget = &nearby;
                        }
                    });
                    if (nearestTarget) {
                        nearestTarget->getComponent<HealthComponent>().takeDamage(chainDmg);
                        auto& nt = nearestTarget->getComponent<TransformComponent>();
                        m_damageEvents.push_back({nt.getCenter(), chainDmg, false, false});
                        if (m_particles) {
                            // Lightning bolt visual
                            m_particles->burst(nt.getCenter(), 8, {255, 255, 80, 255}, 150.0f, 2.0f);
                            m_particles->burst(targetCenter, 4, {255, 255, 120, 255}, 100.0f, 1.5f);
                        }
                    }
                }
            }

            // Element effects on hit (enemy hitting player)
            if (!isPlayer && target.getTag() == "player" && attacker.hasComponent<AIComponent>()) {
                auto& ai = attacker.getComponent<AIComponent>();
                if (ai.element == EnemyElement::Ice) {
                    // Apply freeze: slow player for 1.5 seconds
                    if (m_player) m_player->applyFreeze(1.5f);
                    if (target.hasComponent<PhysicsBody>()) {
                        target.getComponent<PhysicsBody>().velocity.x *= 0.3f;
                    }
                    AudioManager::instance().play(SFX::IceFreeze);
                    if (m_particles) {
                        m_particles->burst(targetCenter, 10, {100, 180, 255, 255}, 80.0f, 2.0f);
                    }
                } else if (ai.element == EnemyElement::Fire) {
                    // Apply burn: damage over time for 2 seconds
                    if (m_player) m_player->applyBurn(2.0f);
                    if (m_particles) {
                        m_particles->burst(targetCenter, 8, {255, 150, 30, 255}, 100.0f, 2.5f);
                    }
                }
            }

            // Run buff: Lifesteal (player heals on dealing damage)
            if (isPlayer && m_lifesteal > 0 && m_player && !shieldBlocked) {
                float healAmount = damage * m_lifesteal;
                if (attacker.hasComponent<HealthComponent>()) {
                    auto& attackerHP = attacker.getComponent<HealthComponent>();
                    attackerHP.currentHP = std::min(attackerHP.maxHP, attackerHP.currentHP + healAmount);
                }
            }

            // Run buff: Element weapon effects (player hitting enemy)
            if (isPlayer && m_elementWeapon > 0 && targetIsEnemy && !shieldBlocked) {
                if (m_elementWeapon == 1 && target.hasComponent<AIComponent>()) {
                    // Fire: apply burn DoT for 2s
                    auto& targetAI = target.getComponent<AIComponent>();
                    targetAI.burnTimer = 2.0f;
                    targetAI.burnDmgTick = 0.3f; // Reset tick on re-apply
                    if (m_particles) {
                        m_particles->burst(targetCenter, 5, {255, 120, 30, 255}, 80.0f, 2.0f);
                    }
                } else if (m_elementWeapon == 2 && target.hasComponent<AIComponent>()) {
                    // Ice: slow (stun 0.3s)
                    target.getComponent<AIComponent>().stun(0.3f);
                    if (m_particles) {
                        m_particles->burst(targetCenter, 5, {100, 180, 255, 255}, 60.0f, 1.5f);
                    }
                } else if (m_elementWeapon == 3) {
                    // Electric: chain 30% damage to nearby enemies
                    float chainDmg = damage * 0.3f;
                    entities.forEach([&](Entity& nearby) {
                        if (&nearby == &target || !nearby.isAlive()) return;
                        if (nearby.getTag().find("enemy") == std::string::npos) return;
                        if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                        if (nearby.dimension != 0 && nearby.dimension != currentDim) return;
                        auto& nt = nearby.getComponent<TransformComponent>();
                        float edx = nt.getCenter().x - targetCenter.x;
                        float edy = nt.getCenter().y - targetCenter.y;
                        if (edx * edx + edy * edy < 80.0f * 80.0f) {
                            nearby.getComponent<HealthComponent>().takeDamage(chainDmg);
                            m_damageEvents.push_back({nt.getCenter(), chainDmg, false, false});
                            if (m_particles) {
                                m_particles->burst(nt.getCenter(), 4, {255, 255, 80, 255}, 100.0f, 1.5f);
                            }
                        }
                    });
                }
            }

            // Elite Vampiric: heal on dealing damage
            if (!isPlayer && target.hasComponent<AIComponent>()) {
                auto& tAI = target.getComponent<AIComponent>();
                if (tAI.isElite && tAI.eliteMod == EliteModifier::Vampiric) {
                    float healAmt = damage * 0.2f;
                    if (attacker.hasComponent<HealthComponent>()) {
                        auto& aHP = attacker.getComponent<HealthComponent>();
                        aHP.currentHP = std::min(aHP.maxHP, aHP.currentHP + healAmt);
                        if (m_particles) {
                            m_particles->burst(transform.getCenter(), 4, {180, 20, 40, 255}, 60.0f, 1.5f);
                        }
                    }
                }
            }

            // Death (shared for both shield-blocked and normal hits)
            if (hp.currentHP <= 0) {
                AudioManager::instance().play(isPlayer ? SFX::PlayerDeath : SFX::EnemyDeath);

                // Track kills for achievements + bestiary
                if (isPlayer && target.hasComponent<AIComponent>()) {
                    killCount++;
                    auto& tAI = target.getComponent<AIComponent>();
                    if (tAI.isMiniBoss) killedMiniBoss = true;
                    if (tAI.element != EnemyElement::None) killedElemental = true;
                    Bestiary::onEnemyKill(tAI.enemyType);

                    // Run buff: DashRefresh - kill resets dash cooldown
                    if (m_dashRefreshOnKill && m_player) {
                        m_player->dashCooldownTimer = 0;
                    }
                }

                // Drop items from enemies (mini-bosses drop 3x, elites drop 2x loot)
                if (isPlayer && target.getTag().find("enemy") != std::string::npos) {
                    int dropCount = 1;
                    if (target.hasComponent<AIComponent>()) {
                        auto& tAI = target.getComponent<AIComponent>();
                        if (tAI.isMiniBoss) dropCount = 3;
                        else if (tAI.isElite) dropCount = 2;
                    }
                    ItemDrop::spawnRandomDrop(entities, targetCenter, target.dimension, dropCount, m_player);
                }

                // Elite on-death effects
                if (isPlayer && target.hasComponent<AIComponent>()) {
                    auto& tAI = target.getComponent<AIComponent>();
                    if (tAI.isElite) {
                        // Splitter: spawn 2 smaller copies
                        if (tAI.eliteMod == EliteModifier::Splitter) {
                            for (int split = 0; split < 2; split++) {
                                float offX = (split == 0) ? -20.0f : 20.0f;
                                Vec2 splitPos = {targetCenter.x + offX, targetCenter.y - 10.0f};
                                auto& splitE = Enemy::createByType(entities, static_cast<int>(tAI.enemyType), splitPos, target.dimension);
                                // Smaller + weaker split copy
                                if (splitE.hasComponent<TransformComponent>()) {
                                    auto& st = splitE.getComponent<TransformComponent>();
                                    st.width *= 0.7f;
                                    st.height *= 0.7f;
                                }
                                if (splitE.hasComponent<HealthComponent>()) {
                                    auto& sh = splitE.getComponent<HealthComponent>();
                                    sh.maxHP *= 0.4f;
                                    sh.currentHP = sh.maxHP;
                                }
                                if (splitE.hasComponent<CombatComponent>()) {
                                    splitE.getComponent<CombatComponent>().meleeAttack.damage *= 0.5f;
                                }
                                if (splitE.hasComponent<PhysicsBody>()) {
                                    splitE.getComponent<PhysicsBody>().velocity.y = -150.0f;
                                    splitE.getComponent<PhysicsBody>().velocity.x = offX * 5.0f;
                                }
                                if (splitE.hasComponent<SpriteComponent>()) {
                                    splitE.getComponent<SpriteComponent>().setColor(60, 220, 80);
                                }
                            }
                            if (m_particles) {
                                m_particles->burst(targetCenter, 15, {60, 220, 80, 255}, 180.0f, 3.0f);
                            }
                        }
                        // Explosive: AoE damage on death
                        else if (tAI.eliteMod == EliteModifier::Explosive) {
                            float explodeDmg = 40.0f;
                            float explodeRadius = 80.0f;
                            entities.forEach([&](Entity& nearby) {
                                if (&nearby == &target || !nearby.isAlive()) return;
                                if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                                if (nearby.dimension != 0 && nearby.dimension != currentDim) return;
                                auto& nt = nearby.getComponent<TransformComponent>();
                                float edx = nt.getCenter().x - targetCenter.x;
                                float edy = nt.getCenter().y - targetCenter.y;
                                float edist = std::sqrt(edx * edx + edy * edy);
                                if (edist < explodeRadius) {
                                    float falloff = 1.0f - (edist / explodeRadius) * 0.5f;
                                    nearby.getComponent<HealthComponent>().takeDamage(explodeDmg * falloff);
                                    m_damageEvents.push_back({nt.getCenter(), explodeDmg * falloff,
                                        nearby.getTag() == "player", false});
                                    if (nearby.hasComponent<PhysicsBody>()) {
                                        Vec2 kb = {edx, edy - 80.0f};
                                        float len = std::sqrt(kb.x * kb.x + kb.y * kb.y);
                                        if (len > 0) kb = kb * (300.0f / len);
                                        nearby.getComponent<PhysicsBody>().velocity += kb;
                                    }
                                }
                            });
                            if (m_particles) {
                                m_particles->burst(targetCenter, 30, {255, 160, 30, 255}, 300.0f, 5.0f);
                                m_particles->burst(targetCenter, 20, {255, 80, 20, 255}, 200.0f, 4.0f);
                            }
                            if (m_camera) m_camera->shake(14.0f, 0.4f);
                            AudioManager::instance().play(SFX::BossShieldBurst);
                        }
                    }
                }

                // Electric chain damage on enemy death
                if (isPlayer && target.hasComponent<AIComponent>()) {
                    auto& targetAI = target.getComponent<AIComponent>();
                    if (targetAI.element == EnemyElement::Electric) {
                        AudioManager::instance().play(SFX::ElectricChain);
                        float chainDmg = damage * 0.5f;
                        float chainRadius = 100.0f;
                        entities.forEach([&](Entity& nearby) {
                            if (&nearby == &target || !nearby.isAlive()) return;
                            if (nearby.getTag().find("enemy") == std::string::npos) return;
                            if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                            auto& nt = nearby.getComponent<TransformComponent>();
                            float cdx = nt.getCenter().x - targetCenter.x;
                            float cdy = nt.getCenter().y - targetCenter.y;
                            if (cdx * cdx + cdy * cdy < chainRadius * chainRadius) {
                                nearby.getComponent<HealthComponent>().takeDamage(chainDmg);
                                if (m_particles) {
                                    m_particles->burst(nt.getCenter(), 6, {255, 255, 80, 255}, 150.0f, 1.5f);
                                }
                            }
                        });
                        if (m_particles) {
                            m_particles->burst(targetCenter, 15, {255, 255, 100, 255}, 200.0f, 3.0f);
                        }
                    }
                }

                // Exploder death explosion is handled by AISystem
                target.destroy();
                if (m_particles) {
                    auto& sprite = target.getComponent<SpriteComponent>();
                    m_particles->burst(targetCenter, 25, sprite.color, 200.0f, 4.0f);
                }
                // Bigger shake on kill (extra for mini-bosses and elites)
                if (m_camera && isPlayer) {
                    bool wasMB = target.hasComponent<AIComponent>() && target.getComponent<AIComponent>().isMiniBoss;
                    bool wasElite = target.hasComponent<AIComponent>() && target.getComponent<AIComponent>().isElite;
                    m_camera->shake(wasMB ? 12.0f : (wasElite ? 10.0f : 8.0f),
                                    wasMB ? 0.35f : (wasElite ? 0.3f : 0.2f));
                }
            }
        }
    });
}

void CombatSystem::createProjectile(EntityManager& entities, Vec2 pos, Vec2 dir,
                                      float damage, float speed, int dimension) {
    auto& proj = entities.addEntity("projectile");
    proj.dimension = dimension;

    auto& t = proj.addComponent<TransformComponent>(pos.x - 4, pos.y - 4, 8, 8);
    auto& sprite = proj.addComponent<SpriteComponent>();
    sprite.setColor(255, 230, 100);
    sprite.renderLayer = 3;

    auto& phys = proj.addComponent<PhysicsBody>();
    phys.useGravity = false;
    phys.velocity = dir * speed;

    auto& col = proj.addComponent<ColliderComponent>();
    col.width = 8;
    col.height = 8;
    col.layer = LAYER_PROJECTILE;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_ENEMY;
    col.type = ColliderType::Trigger;
    col.onTrigger = [damage](Entity* self, Entity* other) {
        if (other->hasComponent<HealthComponent>()) {
            other->getComponent<HealthComponent>().takeDamage(damage);
        }
        self->destroy();
    };

    auto& hp = proj.addComponent<HealthComponent>();
    hp.maxHP = 1;
    hp.currentHP = 1;
    hp.invincibilityTime = 0;
    hp.onDamage = [&proj](float) { proj.destroy(); };
}
