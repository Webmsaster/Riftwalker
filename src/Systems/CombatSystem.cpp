#include "CombatSystem.h"
#include <tracy/Tracy.hpp>
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
#include "Game/ClassSystem.h"
#include "Game/Enemy.h"
#include "Game/Bestiary.h"
#include "Game/DimensionManager.h"
#include "Game/WeaponSystem.h"
#include "Game/SuitEntropy.h"
#include <cmath>

float CombatSystem::consumeHitFreeze() {
    float t = m_pendingHitFreeze;
    m_pendingHitFreeze = 0;
    return t;
}

void CombatSystem::update(EntityManager& entities, float dt, int currentDimension) {
    ZoneScopedN("CombatUpdate");
    m_currentEntities = &entities;
    auto combatEnts = entities.getEntitiesWithComponent<CombatComponent>();

    for (auto* e : combatEnts) {
        auto& combat = e->getComponent<CombatComponent>();
        if (!combat.isAttacking) continue;
        if (!e->hasComponent<TransformComponent>()) continue;

        // Check dimension
        if (e->dimension != 0 && e->dimension != currentDimension) continue;

        processAttack(*e, entities, currentDimension);
    }

    // Parry counter-attack: process weapon-specific counter once when it starts
    if (m_player && m_player->getEntity() && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& playerCombat = m_player->getEntity()->getComponent<CombatComponent>();
        if (playerCombat.isCounterAttacking && !playerCombat.counterProcessed) {
            playerCombat.counterProcessed = true;
            processCounterAttack(*m_player->getEntity(), entities, currentDimension);
        }
    }

    processGroundSlam(entities, currentDimension);

    // Rift Shield: absorb incoming damage to player
    // (handled in processAttack below via parry-like check)

    processBurnDoT(entities, dt);
    processFreezeDecay(entities, dt);
    processProjectileLifetime(entities, dt);
    processZombieSweep(entities, currentDimension);
}

void CombatSystem::processAttack(Entity& attacker, EntityManager& entities, int currentDim) {
    auto& combat = attacker.getComponent<CombatComponent>();
    auto& transform = attacker.getComponent<TransformComponent>();

    bool isPlayer = (attacker.isPlayer);
    auto& atkData = combat.getCurrentAttackData();

    if (combat.currentAttack == AttackType::Ranged) {
        processRangedAttack(attacker, entities, combat, isPlayer);
        return;
    }

    bool isDashAttack = (combat.currentAttack == AttackType::Dash);

    // Melee / Dash: check all potential targets
    Vec2 attackCenter = transform.getCenter() + combat.attackDirection * atkData.range * 0.5f;
    float attackRadius = atkData.range;

    // BerserkerSmash: VoidHammer + BerserkerCore → +50% charged AoE radius
    bool isChargedVoidHammer = isPlayer && combat.currentMelee == WeaponID::VoidHammer
        && combat.currentAttack == AttackType::Charged;
    float berserkerSmashStunBonus = 0;
    if (isChargedVoidHammer && attacker.hasComponent<RelicComponent>()) {
        auto& rel = attacker.getComponent<RelicComponent>();
        attackRadius *= RelicSynergy::getBerserkerSmashRadiusMult(rel, combat.currentMelee);
        berserkerSmashStunBonus = RelicSynergy::getBerserkerSmashStunBonus(rel, combat.currentMelee);
    }

    entities.forEach([&](Entity& target) {
        if (&target == &attacker) return;
        if (!target.hasComponent<HealthComponent>() || !target.hasComponent<TransformComponent>()) return;

        // Check dimension (VoidResonance + DimensionalEcho bypass)
        bool isCrossDimHit = false;
        if (target.dimension != 0 && target.dimension != currentDim) {
            if (isPlayer && attacker.hasComponent<RelicComponent>()) {
                auto& rel = attacker.getComponent<RelicComponent>();
                if (rel.hasRelic(RelicID::VoidResonance) || rel.hasRelic(RelicID::DimensionalEcho)) {
                    isCrossDimHit = true; // Allow hit, mark for bonus
                } else {
                    return;
                }
            } else {
                return;
            }
        }

        // Player attacks enemies, enemies attack player
        bool targetIsEnemy = target.isEnemy;
        if (isPlayer && !targetIsEnemy) return;
        if (!isPlayer && !target.isPlayer) return;

        auto& targetTransform = target.getComponent<TransformComponent>();
        Vec2 targetCenter = targetTransform.getCenter();
        float dx = targetCenter.x - attackCenter.x;
        float dy = targetCenter.y - attackCenter.y;
        float distSq = dx * dx + dy * dy;

        if (distSq < attackRadius * attackRadius) {
            // Phantom Phase Through: negate enemy hits during dash
            if (!isPlayer && target.isPlayer && m_player && m_player->isPhaseThrough()) {
                // Deal phase damage back to the attacker
                if (attacker.hasComponent<HealthComponent>()) {
                    attacker.getComponent<HealthComponent>().takeDamage(m_player->phaseDamage);
                    m_damageEvents.push_back({transform.getCenter(), m_player->phaseDamage, false, false});
                }
                // Brief slow on phased enemy
                if (attacker.hasComponent<AIComponent>()) {
                    attacker.getComponent<AIComponent>().stun(m_player->phaseSlowDuration);
                }
                // Cyan phase particles
                if (m_particles) {
                    m_particles->burst(targetCenter, 12, {60, 220, 200, 200}, 120.0f, 3.0f);
                }
                return; // Negate the hit
            }

            // Rift Shield check: absorb hits while shield is active
            if (!isPlayer && target.isPlayer && target.hasComponent<AbilityComponent>()) {
                auto& targetAbil = target.getComponent<AbilityComponent>();
                if (targetAbil.abilities[1].active && targetAbil.shieldHitsRemaining > 0) {
                    targetAbil.shieldHitsRemaining = std::max(0, targetAbil.shieldHitsRemaining - 1);
                    targetAbil.shieldAbsorbedDamage += atkData.damage;

                    // Heal player on absorb
                    if (target.hasComponent<HealthComponent>()) {
                        auto& hp = target.getComponent<HealthComponent>();
                        hp.heal(targetAbil.shieldHealPerAbsorb);
                    }

                    // Melee absorb SFX (ranged reflect handled in projectile
                    // onTrigger at CombatSystemEffects.cpp)
                    AudioManager::instance().play(SFX::RiftShieldAbsorb);

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
                            if (!nearby.isEnemy) return;
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
            if (!isPlayer && target.isPlayer && target.hasComponent<CombatComponent>()) {
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

                    // Parry feedback — the MOST satisfying moment in the game
                    if (m_camera) {
                        m_camera->shake(10.0f, 0.2f);
                        m_camera->flash(0.15f, 255, 235, 200); // warm white-gold flash
                    }
                    m_pendingHitFreeze += 0.12f;
                    if (m_particles) {
                        // Expanding ring: fast outward, large, white-gold
                        m_particles->burst(targetCenter, 24, {255, 255, 255, 255}, 350.0f, 4.0f);
                        m_particles->burst(targetCenter, 16, {255, 215, 0, 255}, 250.0f, 3.5f);
                        // Inner gold sparkle
                        m_particles->burst(targetCenter, 10, {255, 200, 60, 255}, 100.0f, 2.0f);
                    }
                    // Queue "PARRY!" floating text for PlayState
                    parryEvents.push_back({targetCenter});
                    return; // Negate the hit entirely
                }
            }

            auto& hp = target.getComponent<HealthComponent>();
            float comboMult = combat.comboCount * (0.15f + m_comboBonus);
            float damage = atkData.damage * (1.0f + comboMult);

            // Dimension behavior damage modifier (enemy attacks only)
            if (!isPlayer && attacker.hasComponent<AIComponent>()) {
                auto& attackAI = attacker.getComponent<AIComponent>();
                damage *= attackAI.dimDamageMod;
                if (attackAI.synergySummonerBuff) {
                    damage *= attackAI.summonerBuffDamageMult;
                }
            }

            // Relic damage multiplier (player attacks only)
            if (isPlayer && attacker.hasComponent<RelicComponent>()) {
                auto& relics = attacker.getComponent<RelicComponent>();
                float hpPct = attacker.getComponent<HealthComponent>().getPercent();
                damage *= RelicSystem::getDamageMultiplier(relics, hpPct, currentDim);
                // CursedBlade: melee damage modifier
                damage *= RelicSystem::getCursedMeleeMult(relics);
                // VoidResonance: 2x damage on cross-dimension hits (with ICD)
                if (isCrossDimHit && relics.hasRelic(RelicID::VoidResonance)
                    && relics.voidResonanceProcCD <= 0) {
                    damage *= 2.0f;
                    relics.voidResonanceProcCD = RelicSystem::getVoidResonanceICD();
                    voidResonanceProcs++;
                }
                // PhaseHunter synergy: consume buff after attack
                if (relics.phaseHunterBuffActive) {
                    relics.phaseHunterBuffActive = false;
                }
            }

            // Class damage multiplier (Berserker Blood Rage)
            if (isPlayer && m_player) {
                damage *= m_player->getClassDamageMultiplier();
            }

            // Weapon mastery damage bonus (melee only — ranged mastery applied
            // in processRangedAttack at projectile spawn time)
            if (isPlayer) {
                int wIdx = static_cast<int>(combat.currentMelee);
                if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT)) {
                    MasteryBonus mb = WeaponSystem::getMasteryBonus(weaponKills[wIdx]);
                    damage *= mb.damageMult;
                }
            }

            // Blacksmith melee damage upgrade (ranged applied in processRangedAttack)
            if (isPlayer && m_player) {
                damage *= m_player->smithMeleeDmgMult;
            }

            // Resonance damage bonus from rapid dimension switching
            if (isPlayer && m_dimMgr) {
                damage *= m_dimMgr->getResonanceDamageMult();
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
                    int critThreshold = 5;
                    if (attacker.hasComponent<RelicComponent>()) {
                        critThreshold = RelicSynergy::getPhantomRushCritThreshold(
                            attacker.getComponent<RelicComponent>(), combat.currentMelee);
                    }
                    if (combat.daggerHitCount >= critThreshold) {
                        combat.daggerHitCount = 0;
                        damage *= 2.0f;
                        isCrit = true;
                        AudioManager::instance().play(SFX::CriticalHit);
                        if (m_particles) {
                            m_particles->burst(targetCenter, 12, {180, 100, 255, 255}, 180.0f, 3.0f);
                        }
                    }
                }
                // Parry success guarantees crit (skip if already crit from Phase Daggers)
                if (!isCrit) {
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
                } // end if (!isCrit)
            }

            // Charged attack: scale damage with charge percent
            bool isChargedAttack = (combat.currentAttack == AttackType::Charged);
            if (isChargedAttack) {
                bool isVoidHammer = isPlayer && combat.currentMelee == WeaponID::VoidHammer;

                if (isVoidHammer) {
                    // VoidHammer shockwave: heavier impact
                    if (m_camera) m_camera->shake(18.0f, 0.4f);
                    m_pendingHitFreeze += 0.18f;
                    if (m_particles) {
                        // Purple shockwave ring at target
                        for (int i = 0; i < 8; i++) {
                            float angle = i * 45.0f;
                            m_particles->directionalBurst(targetCenter, 3,
                                {180, 100, 255, 255}, angle, 25.0f, 220.0f, 4.0f);
                        }
                        m_particles->burst(targetCenter, 30, {200, 160, 255, 255}, 280.0f, 5.0f);
                        // Ground crack particles
                        m_particles->directionalBurst(targetCenter, 8,
                            {160, 140, 120, 200}, 0.0f, 180.0f, 120.0f, 2.5f);
                    }
                } else {
                    // Normal charged attack
                    if (m_camera) m_camera->shake(12.0f, 0.3f);
                    m_pendingHitFreeze += 0.12f;
                    if (m_particles) {
                        m_particles->burst(targetCenter, 25, {255, 200, 50, 255}, 250.0f, 5.0f);
                    }
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
                // Reflector mirror-shield block: blocks frontal attacks, reflects ranged back
                if (targetAI.enemyType == EnemyType::Reflector && targetAI.reflectorShieldUp) {
                    bool attackFromFront = (attackCenter.x > targetCenter.x) == targetAI.facingRight;
                    if (attackFromFront) {
                        // Melee block (ranged reflect is handled in projectile
                        // onTrigger lambda at CombatSystemEffects.cpp)
                        AudioManager::instance().play(SFX::RiftFail);
                        if (m_particles) {
                            m_particles->burst(targetCenter, 8, {180, 200, 255, 255}, 100.0f, 2.0f);
                        }
                        if (m_camera) m_camera->shake(3.0f, 0.1f);
                        shieldBlocked = true;
                    }
                }
            }

            // Track hits for enrage system
            if (isPlayer && target.hasComponent<AIComponent>()) {
                auto& targetAI = target.getComponent<AIComponent>();
                targetAI.timesHit++;
                if (targetAI.timesHit >= 3 && !targetAI.isEnraged) {
                    targetAI.isEnraged = true;
                    if (target.hasComponent<SpriteComponent>()) {
                        target.getComponent<SpriteComponent>().setColor(255, 60, 40);
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

            // Shield Aura: nearby ShieldAura elite reduces damage by 30%
            if (isPlayer && target.hasComponent<AIComponent>() && target.hasComponent<TransformComponent>()) {
                bool hasShieldAuraNearby = false;
                entities.forEach([&](Entity& other) {
                    if (hasShieldAuraNearby || &other == &target || !other.isAlive()) return;
                    if (!other.hasComponent<AIComponent>() || !other.hasComponent<TransformComponent>()) return;
                    auto& oAI = other.getComponent<AIComponent>();
                    if (oAI.isElite && oAI.eliteMod == EliteModifier::ShieldAura) {
                        auto& oT = other.getComponent<TransformComponent>();
                        auto& tT = target.getComponent<TransformComponent>();
                        float dist = std::abs(oT.getCenter().x - tT.getCenter().x)
                                   + std::abs(oT.getCenter().y - tT.getCenter().y);
                        if (dist < 100.0f) hasShieldAuraNearby = true;
                    }
                });
                if (hasShieldAuraNearby) {
                    damage *= 0.7f; // 30% damage reduction
                    if (m_particles) {
                        m_particles->burst(targetCenter, 4, {60, 200, 255, 180}, 50.0f, 1.0f);
                    }
                }
            }

            // Defensive relic multiplier (GlassHeart, DualityGem armor, FortifiedSoul)
            if (!isPlayer && target.isPlayer && target.hasComponent<RelicComponent>()) {
                damage *= RelicSystem::getDamageTakenMult(
                    target.getComponent<RelicComponent>(), currentDim);
            }

            if (shieldBlocked) {
                float actualDmg = damage * 0.1f;
                hp.takeDamage(actualDmg);
                m_damageEvents.push_back({targetCenter, actualDmg, !isPlayer, false, false, transform.getCenter()});
                if (m_particles) {
                    Vec2 shieldPos = {targetCenter.x + (target.getComponent<AIComponent>().facingRight ? 16.0f : -16.0f),
                                      targetCenter.y};
                    m_particles->burst(shieldPos, 8, {100, 200, 255, 255}, 120.0f, 2.0f);
                }
                AudioManager::instance().play(SFX::RiftFail);
                if (m_camera) m_camera->shake(2.0f, 0.08f);
            } else {
                // Skip if player has active i-frames (no phantom feedback)
                bool targetIsPlayer = (!isPlayer && target.isPlayer);
                if (targetIsPlayer && hp.isInvincible()) return;
                hp.takeDamage(damage);
                m_damageEvents.push_back({targetCenter, damage, !isPlayer, isCrit, false, transform.getCenter()});

                // Combo-stage knockback variation (player only)
                int comboStage = (isPlayer && combat.comboCount > 0) ? ((combat.comboCount - 1) % 3) : 0;
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

                // Stun: dash attack stuns 1s, combo finisher (stage 2) stuns 0.5s,
                // all other player hits apply brief hitstun for game-feel
                if (isPlayer && target.hasComponent<AIComponent>()) {
                    auto& targetAI = target.getComponent<AIComponent>();
                    if (isDashAttack) {
                        targetAI.stun(1.0f);
                        AudioManager::instance().play(SFX::EnemyStun);
                    } else if (isChargedAttack) {
                        // Charged attacks stun 0.3s base + BerserkerSmash bonus
                        targetAI.stun(0.3f + berserkerSmashStunBonus);
                        AudioManager::instance().play(SFX::EnemyStun);
                    } else if (comboStage == 2) {
                        targetAI.stun(0.5f);
                        AudioManager::instance().play(SFX::EnemyStun);
                    } else if (targetAI.state != AIState::Juggled &&
                               targetAI.state != AIState::Stunned) {
                        // Brief hitstun on normal hits — makes every hit feel punchy
                        targetAI.stun(0.1f);
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

                // SFX: skip MeleeHit on crits (CriticalHit/ParryCounter already played)
                if (isCrit && isPlayer) {
                    // Crit SFX already played above — don't double up
                } else {
                    AudioManager::instance().play(isPlayer ? SFX::MeleeHit : SFX::EnemyHit);
                }

                // Screen shake scales with combo stage; crits get extra punch
                if (m_camera) {
                    float shakeIntensity, shakeDuration;
                    if (isDashAttack) {
                        shakeIntensity = 10.0f;
                        shakeDuration = 0.25f;
                    } else if (isCrit && isPlayer) {
                        shakeIntensity = 12.0f;
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

                // Hit-freeze scales with combo stage; crits freeze longer
                if (isDashAttack) {
                    m_pendingHitFreeze += 0.1f;
                } else if (isPlayer) {
                    // Subtle hitstop on every melee hit (0.03s base),
                    // crits get more punch (0.1s), scales up with combo stage
                    float freezeBase = isCrit ? 0.1f : 0.03f;
                    m_pendingHitFreeze += freezeBase + comboStage * 0.03f;
                } else {
                    m_pendingHitFreeze += 0.06f;
                }

                // Particles scale with combo stage; crits get bright white burst
                if (m_particles) {
                    SDL_Color hitColor;
                    int particleCount;
                    if (isDashAttack) {
                        hitColor = {100, 200, 255, 255};
                        particleCount = 25;
                    } else if (isCrit && isPlayer) {
                        hitColor = {255, 255, 220, 255}; // bright white-gold
                        particleCount = 22;
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
                    float burstSpeed = isDashAttack ? 250.0f :
                                       (isCrit && isPlayer) ? 220.0f : (120.0f + comboStage * 60.0f);
                    float burstLife = isDashAttack ? 5.0f :
                                     (isCrit && isPlayer) ? 4.0f : (2.0f + comboStage);
                    m_particles->burst(hitPos, particleCount / 2, {255, 220, 100, 255}, burstSpeed, burstLife);

                    // Directional impact particles: spray in hit direction
                    float hitDirDeg = std::atan2(targetCenter.y - attackCenter.y,
                                                 targetCenter.x - attackCenter.x) * 180.0f / 3.14159f;
                    float dirSpread = isDashAttack ? 60.0f : (isCrit && isPlayer) ? 70.0f : 90.0f;
                    float dirSpeed = isDashAttack ? 280.0f :
                                     (isCrit && isPlayer) ? 260.0f : (150.0f + comboStage * 50.0f);
                    int dirCount = isDashAttack ? 15 : (isCrit && isPlayer) ? 14 : (5 + comboStage * 3);
                    m_particles->directionalBurst(hitPos, dirCount, hitColor, hitDirDeg, dirSpread, dirSpeed, burstLife);

                    // Small white sparks at impact point
                    int sparkCount = (isCrit && isPlayer) ? 8 : (3 + comboStage);
                    m_particles->burst(hitPos, sparkCount, {255, 255, 255, 255}, 80.0f, 1.5f);

                    // Crit finisher: gold flash + extra radial gold burst
                    if (isCrit && isPlayer) {
                        m_camera->flash(0.07f, 255, 220, 100); // brief gold flash
                        m_particles->burst(hitPos, 12, {255, 200, 60, 255}, 300.0f, 3.5f);
                    }

                    // Combo finisher (3rd hit): extra visual punch + audio
                    if (isPlayer && comboStage == 2 && !isDashAttack && !isCrit) {
                        m_camera->flash(0.1f);  // brief white screen flash
                        m_particles->burst(hitPos, 15, {255, 240, 150, 255}, 250.0f, 4.0f); // gold burst
                        m_particles->directionalBurst(hitPos, 8, {255, 255, 220, 255},
                            hitDirDeg, 15.0f, 350.0f, 2.5f); // tight slash line
                        AudioManager::instance().play(SFX::ComboMilestone);
                    }
                }
            }

            // Relic: ThornMail - reflect damage when player is hit
            if (!isPlayer && target.isPlayer && target.hasComponent<RelicComponent>()) {
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
                        constexpr float chainRadiusSq = 150.0f * 150.0f;
                        entities.forEach([&](Entity& e) {
                            if (chainsLeft <= 0) return;
                            if (!e.isAlive() || &e == &attacker) return;
                            if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>()
                                || !e.hasComponent<TransformComponent>()) return;
                            Vec2 ePos = e.getComponent<TransformComponent>().getCenter();
                            float dx = ePos.x - attackerPos.x;
                            float dy = ePos.y - attackerPos.y;
                            if (dx * dx + dy * dy < chainRadiusSq) {
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
                    // Find nearest enemy and deal chain damage (squared distance avoids sqrt)
                    float nearestDistSq = 150.0f * 150.0f;
                    Entity* nearestTarget = nullptr;
                    entities.forEach([&](Entity& nearby) {
                        if (&nearby == &target || !nearby.isAlive()) return;
                        if (!nearby.isEnemy) return;
                        if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                        if (nearby.dimension != 0 && nearby.dimension != currentDim) return;
                        auto& nt = nearby.getComponent<TransformComponent>();
                        float dx2 = nt.getCenter().x - targetCenter.x;
                        float dy2 = nt.getCenter().y - targetCenter.y;
                        float distSq = dx2 * dx2 + dy2 * dy2;
                        if (distSq < nearestDistSq) {
                            nearestDistSq = distSq;
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
            if (!isPlayer && target.isPlayer && attacker.hasComponent<AIComponent>()) {
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
                    attackerHP.heal(healAmount);
                }
            }

            // Entropy Scythe: reduce entropy on successful melee hit
            // EntropyDrain synergy (EntropyScythe + EntropySponge): 5.0 instead of 2.0
            if (isPlayer && !shieldBlocked && m_suitEntropy &&
                combat.currentMelee == WeaponID::EntropyScythe &&
                (combat.currentAttack == AttackType::Melee ||
                 combat.currentAttack == AttackType::Charged ||
                 combat.currentAttack == AttackType::Dash)) {
                float entropyReduction = 2.0f;
                if (attacker.hasComponent<RelicComponent>()) {
                    entropyReduction = RelicSynergy::getEntropyDrainReduction(
                        attacker.getComponent<RelicComponent>(), combat.currentMelee);
                }
                m_suitEntropy->reduceEntropy(entropyReduction);
                if (m_particles) {
                    // Green entropy-drain particles flowing toward player
                    Vec2 playerPos = transform.getCenter();
                    m_particles->burst(targetCenter, 6, {80, 255, 120, 200}, 100.0f, 2.0f);
                    m_particles->burst(playerPos, 4, {60, 200, 100, 180}, 40.0f, 1.5f);
                }
            }

            // Chain Whip: visual piercing chain effect on hit
            if (isPlayer && !shieldBlocked &&
                combat.currentMelee == WeaponID::ChainWhip &&
                combat.currentAttack == AttackType::Melee && m_particles) {
                // Orange chain-link sparks along attack direction
                Vec2 hitDir = combat.attackDirection;
                for (int i = 0; i < 3; i++) {
                    float t = (i + 1) * 0.3f;
                    Vec2 sparkPos = {targetCenter.x + hitDir.x * 20.0f * t,
                                     targetCenter.y + hitDir.y * 20.0f * t};
                    m_particles->burst(sparkPos, 3, {255, 180, 60, 220}, 60.0f, 1.5f);
                }

                // ChainReaction synergy (ChainWhip + ChainLightning): spawn chain lightning bolt at hit position
                if (attacker.hasComponent<RelicComponent>()) {
                    auto& relics = attacker.getComponent<RelicComponent>();
                    if (RelicSynergy::isChainReactionActive(relics, combat.currentMelee)) {
                        float chainDmg = RelicSystem::getChainLightningDamage(relics);
                        if (chainDmg <= 0) chainDmg = 15.0f; // Fallback if relic not owned standalone
                        float nearestDistSq = 150.0f * 150.0f;
                        Entity* nearestTarget = nullptr;
                        entities.forEach([&](Entity& nearby) {
                            if (&nearby == &target || !nearby.isAlive()) return;
                            if (!nearby.isEnemy) return;
                            if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                            if (nearby.dimension != 0 && nearby.dimension != currentDim) return;
                            auto& nt = nearby.getComponent<TransformComponent>();
                            float dx2 = nt.getCenter().x - targetCenter.x;
                            float dy2 = nt.getCenter().y - targetCenter.y;
                            float distSq = dx2 * dx2 + dy2 * dy2;
                            if (distSq < nearestDistSq) {
                                nearestDistSq = distSq;
                                nearestTarget = &nearby;
                            }
                        });
                        if (nearestTarget) {
                            nearestTarget->getComponent<HealthComponent>().takeDamage(chainDmg);
                            auto& nt = nearestTarget->getComponent<TransformComponent>();
                            m_damageEvents.push_back({nt.getCenter(), chainDmg, false, false});
                            if (m_particles) {
                                m_particles->burst(nt.getCenter(), 8, {255, 255, 80, 255}, 150.0f, 2.0f);
                                m_particles->burst(targetCenter, 4, {255, 255, 120, 255}, 100.0f, 1.5f);
                            }
                        }
                    }
                }
            }

            // Gravity Gauntlet: pull effect particles + camera feel + synergy impact
            if (isPlayer && !shieldBlocked &&
                combat.currentMelee == WeaponID::GravityGauntlet &&
                (combat.currentAttack == AttackType::Melee ||
                 combat.currentAttack == AttackType::Charged)) {
                // Purple gravity-pull particles from target toward player
                if (m_particles) {
                    Vec2 playerPos = transform.getCenter();
                    m_particles->burst(targetCenter, 8, {140, 80, 220, 200}, 80.0f, 1.5f);
                    m_particles->burst(playerPos, 4, {180, 120, 255, 180}, 40.0f, 1.0f);
                }
                if (m_camera) m_camera->shake(5.0f, 0.12f);
                // GravityThorns synergy: pulled enemies take extra impact damage
                float thornImpact = attacker.hasComponent<RelicComponent>()
                    ? RelicSynergy::getGravityThornsImpactDmg(attacker.getComponent<RelicComponent>(), combat.currentMelee)
                    : 0;
                if (thornImpact > 0 && target.hasComponent<HealthComponent>()) {
                    target.getComponent<HealthComponent>().takeDamage(thornImpact);
                    m_damageEvents.push_back({targetCenter, thornImpact, false, false});
                    if (m_particles) {
                        m_particles->burst(targetCenter, 6, {200, 100, 255, 255}, 60.0f, 1.0f);
                    }
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
                    // Ice: apply freeze slow (50% speed reduction for 1.5s)
                    auto& targetAI = target.getComponent<AIComponent>();
                    // Brief stun on first application (before the freeze timer is refreshed,
                    // otherwise the <=0.01f check below could never trigger because line 922
                    // always clamps it to at least 1.5f).
                    bool alreadyFrozen = (targetAI.freezeTimer > 0.01f);
                    targetAI.freezeTimer = std::max(targetAI.freezeTimer, 1.5f);
                    if (!alreadyFrozen) {
                        targetAI.stun(0.15f);
                    }
                    if (m_particles) {
                        m_particles->burst(targetCenter, 6, {100, 180, 255, 255}, 70.0f, 2.0f);
                        // Snowflake-like particles spreading outward
                        m_particles->burst(targetCenter, 3, {180, 220, 255, 200}, 40.0f, 1.0f);
                    }
                } else if (m_elementWeapon == 3) {
                    // Electric: chain 30% damage to nearby enemies
                    float chainDmg = damage * 0.3f;
                    entities.forEach([&](Entity& nearby) {
                        if (&nearby == &target || !nearby.isAlive()) return;
                        if (!nearby.isEnemy) return;
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

            // Elite Vampiric: heal on dealing damage (check attacker, not target)
            if (attacker.hasComponent<AIComponent>()) {
                auto& aAI = attacker.getComponent<AIComponent>();
                if (aAI.isElite && aAI.eliteMod == EliteModifier::Vampiric) {
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
                handleEnemyDeath(attacker, target, entities, currentDim,
                                 isPlayer, isDashAttack, isChargedAttack,
                                 combat, targetCenter, damage);
            }
        }
    });
}
