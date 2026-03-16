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
#include "Game/ClassSystem.h"
#include "Game/Enemy.h"
#include "Game/Bestiary.h"
#include "Game/ClassSystem.h"
#include "Game/DimensionManager.h"
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
        if (!e.hasComponent<TransformComponent>()) return;
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

                if (dist < radius && radius > 0.0f) {
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
                        // Kill event for combat challenges
                        {
                            KillEvent ke;
                            ke.wasSlam = true;
                            if (m_player && m_player->getEntity()->hasComponent<PhysicsBody>())
                                ke.wasAerial = !m_player->getEntity()->getComponent<PhysicsBody>().onGround;
                            if (target.hasComponent<AIComponent>()) {
                                auto& ai = target.getComponent<AIComponent>();
                                ke.enemyType = static_cast<int>(ai.enemyType);
                                ke.wasElite = ai.isElite;
                                ke.wasMiniBoss = ai.isMiniBoss;
                                ke.wasBoss = (ai.enemyType == EnemyType::Boss);
                            }
                            killEvents.push_back(ke);
                        }
                        // Weapon mastery: slam counts as melee weapon kill
                        if (e.hasComponent<CombatComponent>()) {
                            int wIdx = static_cast<int>(e.getComponent<CombatComponent>().currentMelee);
                            if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT))
                                weaponKills[wIdx]++;
                        }
                        if (target.hasComponent<AIComponent>()) {
                            auto& tAI = target.getComponent<AIComponent>();
                            if (tAI.isMiniBoss) killedMiniBoss = true;
                            if (tAI.element != EnemyElement::None) killedElemental = true;
                            Bestiary::onEnemyKill(tAI.enemyType);
                        }
                        // Berserker: Momentum stack on slam kill
                        if (m_player) m_player->addMomentumStack();
                        // Run buff: DashRefresh on kill
                        if (m_dashRefreshOnKill && m_player) {
                            m_player->dashCooldownTimer = 0;
                        }
                        // Drop items (mini-bosses 3x, elites 2x)
                        int slamDropCount = 1;
                        if (target.hasComponent<AIComponent>()) {
                            auto& tAI2 = target.getComponent<AIComponent>();
                            if (tAI2.isMiniBoss) slamDropCount = 3;
                            else if (tAI2.isElite) slamDropCount = 2;
                        }
                        ItemDrop::spawnRandomDrop(entities, tt.getCenter(), target.dimension, slamDropCount, m_player);
                        int slamElem = 0;
                        if (target.hasComponent<AIComponent>())
                            slamElem = static_cast<int>(target.getComponent<AIComponent>().element);
                        AudioManager::instance().play(SFX::EnemyDeath);
                        target.destroy();
                        if (m_particles) {
                            m_particles->burst(tt.getCenter(), 25, {255, 180, 60, 255}, 200.0f, 4.0f);
                            // Slam death launch: burst left+right from impact
                            m_particles->directionalBurst(tt.getCenter(), 8, {255, 200, 80, 255},
                                                           0.0f, 60.0f, 280.0f, 3.0f);
                            m_particles->directionalBurst(tt.getCenter(), 8, {255, 200, 80, 255},
                                                           180.0f, 60.0f, 280.0f, 3.0f);
                        }
                        emitElementDeathFX(tt.getCenter(), slamElem);
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
                    // Kill event for combat challenges (burn DOT kill)
                    {
                        KillEvent ke{};
                        ke.enemyType = static_cast<int>(ai.enemyType);
                        ke.wasElite = ai.isElite;
                        ke.wasMiniBoss = ai.isMiniBoss;
                        ke.wasBoss = (ai.enemyType == EnemyType::Boss);
                        killEvents.push_back(ke);
                    }
                    // Weapon mastery: burn kills attributed to current melee
                    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
                        int wIdx = static_cast<int>(m_player->getEntity()->getComponent<CombatComponent>().currentMelee);
                        if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT))
                            weaponKills[wIdx]++;
                    }
                    if (ai.isMiniBoss) killedMiniBoss = true;
                    if (ai.element != EnemyElement::None) killedElemental = true;
                    Bestiary::onEnemyKill(ai.enemyType);
                    // Berserker: Momentum stack on burn kill
                    if (m_player) m_player->addMomentumStack();
                    // Run buff: DashRefresh on kill
                    if (m_dashRefreshOnKill && m_player) {
                        m_player->dashCooldownTimer = 0;
                    }
                    Vec2 deathPos = e.hasComponent<TransformComponent>() ?
                        e.getComponent<TransformComponent>().getCenter() : Vec2{0,0};
                    // Drop items (mini-bosses 3x, elites 2x)
                    int burnDropCount = 1;
                    if (ai.isMiniBoss) burnDropCount = 3;
                    else if (ai.isElite) burnDropCount = 2;
                    ItemDrop::spawnRandomDrop(entities, deathPos, e.dimension, burnDropCount, m_player);
                    AudioManager::instance().play(SFX::EnemyDeath);
                    e.destroy();
                    if (m_particles) {
                        m_particles->burst(deathPos, 15, {255, 100, 30, 255}, 180.0f, 3.0f);
                    }
                }
            }
        }
    });

    // Process freeze slow decay on enemies
    entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") == std::string::npos) return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& ai = e.getComponent<AIComponent>();
        if (ai.freezeTimer > 0) {
            ai.freezeTimer -= dt;
            // Periodic ice crystal particles while frozen
            if (m_particles && e.hasComponent<TransformComponent>() && SDL_GetTicks() % 8 == 0) {
                auto& t = e.getComponent<TransformComponent>();
                Vec2 center = t.getCenter();
                float ox = (static_cast<float>(std::rand() % 20) - 10.0f);
                m_particles->burst({center.x + ox, center.y - 4.0f}, 1,
                    {150, 210, 255, 160}, 20.0f, 1.0f);
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

    // Sweep: catch enemies killed by secondary damage (chain lightning, electric chain,
    // explosive AoE, shield burst, etc.) that bypassed the normal death handler.
    // Without this, enemies at HP <= 0 remain alive as "zombies".
    entities.forEach([&](Entity& e) {
        if (!e.isAlive()) return;
        if (e.getTag().find("enemy") == std::string::npos) return;
        if (!e.hasComponent<HealthComponent>()) return;
        auto& hp = e.getComponent<HealthComponent>();
        if (hp.currentHP > 0) return;

        // This enemy was killed by secondary/chain damage
        killCount++;
        {
            KillEvent ke{};
            if (e.hasComponent<AIComponent>()) {
                auto& ai2 = e.getComponent<AIComponent>();
                ke.enemyType = static_cast<int>(ai2.enemyType);
                ke.wasElite = ai2.isElite;
                ke.wasMiniBoss = ai2.isMiniBoss;
                ke.wasBoss = (ai2.enemyType == EnemyType::Boss);
            }
            killEvents.push_back(ke);
        }

        if (e.hasComponent<AIComponent>()) {
            auto& ai = e.getComponent<AIComponent>();
            if (ai.isMiniBoss) killedMiniBoss = true;
            if (ai.element != EnemyElement::None) killedElemental = true;
            Bestiary::onEnemyKill(ai.enemyType);
        }
        if (m_player) m_player->addMomentumStack();

        // Run buff: DashRefresh on kill
        if (m_dashRefreshOnKill && m_player) {
            m_player->dashCooldownTimer = 0;
        }

        Vec2 deathPos = e.hasComponent<TransformComponent>() ?
            e.getComponent<TransformComponent>().getCenter() : Vec2{0,0};

        // Drop items (mini-bosses 3x, elites 2x)
        int dropCount = 1;
        if (e.hasComponent<AIComponent>()) {
            auto& ai = e.getComponent<AIComponent>();
            if (ai.isMiniBoss) dropCount = 3;
            else if (ai.isElite) dropCount = 2;
        }
        ItemDrop::spawnRandomDrop(entities, deathPos, e.dimension, dropCount, m_player);

        // Weapon drop chance for zombie-sweep kills
        if (e.hasComponent<AIComponent>()) {
            auto& ai2 = e.getComponent<AIComponent>();
            int weaponRoll = std::rand() % 100;
            bool dropWeapon = false;
            if (ai2.enemyType == EnemyType::Boss) dropWeapon = true;
            else if (ai2.isMiniBoss && weaponRoll < 40) dropWeapon = true;
            else if (ai2.isElite && weaponRoll < 20) dropWeapon = true;

            if (dropWeapon && m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
                auto& pc = m_player->getEntity()->getComponent<CombatComponent>();
                std::vector<WeaponID> candidates;
                for (int w = 0; w < static_cast<int>(WeaponID::COUNT); w++) {
                    auto wid = static_cast<WeaponID>(w);
                    if (wid != pc.currentMelee && wid != pc.currentRanged)
                        candidates.push_back(wid);
                }
                if (!candidates.empty()) {
                    WeaponID drop = candidates[std::rand() % candidates.size()];
                    ItemDrop::spawnWeaponDrop(entities, deathPos, e.dimension, drop, m_player);
                }
            }
        }

        // Read element + type before destroy for themed death FX
        int elemType = 0;
        int eType = -1;
        if (e.hasComponent<AIComponent>()) {
            elemType = static_cast<int>(e.getComponent<AIComponent>().element);
            eType = static_cast<int>(e.getComponent<AIComponent>().enemyType);
        }

        SDL_Color deathColor = {255, 255, 255, 255};
        if (e.hasComponent<SpriteComponent>()) {
            deathColor = e.getComponent<SpriteComponent>().color;
        }

        AudioManager::instance().play(SFX::EnemyDeath);
        if (m_particles) {
            m_particles->burst(deathPos, 25, deathColor, 200.0f, 4.0f);
            // Death launch: directional burst away from player
            if (m_player && m_player->getEntity()->hasComponent<TransformComponent>()) {
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                float dx = deathPos.x - pPos.x;
                float launchDir = dx >= 0 ? 0.0f : 180.0f;
                m_particles->directionalBurst(deathPos, 10, deathColor,
                                               launchDir, 45.0f, 250.0f, 3.5f);
            }
        }
        emitElementDeathFX(deathPos, elemType);
        if (eType >= 0) emitEnemyTypeDeathFX(deathPos, eType, deathColor);
        if (m_camera) m_camera->shake(8.0f, 0.2f);
        e.destroy();
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

            // CursedBlade: ranged damage modifier
            float projDamage = atkData.damage;
            if (isPlayer && attacker.hasComponent<RelicComponent>()) {
                projDamage *= RelicSystem::getCursedRangedMult(attacker.getComponent<RelicComponent>());
            }

            // Technomancer: +10% ranged damage (Construct Mastery passive)
            if (isPlayer && m_player && m_player->playerClass == PlayerClass::Technomancer) {
                projDamage *= ClassSystem::getData(PlayerClass::Technomancer).rangedDmgBonus;
            }

            // Weapon-specific ranged behavior (player only)
            if (isPlayer && combat.currentRanged == WeaponID::RiftShotgun) {
                // 5 pellets in a fan pattern
                float baseAngle = std::atan2(combat.attackDirection.y, combat.attackDirection.x);
                float spread = 0.4f; // ~23 degrees total
                for (int i = 0; i < 5; i++) {
                    float angle = baseAngle + (i - 2) * (spread / 4.0f);
                    Vec2 dir = {std::cos(angle), std::sin(angle)};
                    createProjectile(entities, pos, dir, projDamage, 350.0f, attacker.dimension, false, isPlayer);
                }
                AudioManager::instance().play(SFX::RangedShot);
            } else if (isPlayer && combat.currentRanged == WeaponID::VoidBeam) {
                // Continuous beam: piercing projectile passes through enemies
                createProjectile(entities, pos, combat.attackDirection,
                                projDamage, 500.0f, attacker.dimension, true, isPlayer);
                // No extra SFX each tick (too fast)
            } else {
                // RapidShards: ShardPistol + QuickHands → +30% proj speed, 25% double-shot
                float projSpeed = 400.0f;
                bool doubleShot = false;
                if (isPlayer && combat.currentRanged == WeaponID::ShardPistol &&
                    attacker.hasComponent<RelicComponent>()) {
                    auto& rel = attacker.getComponent<RelicComponent>();
                    projSpeed *= RelicSynergy::getRapidShardsSpeedMult(rel, combat.currentRanged);
                    doubleShot = RelicSynergy::rollRapidShardsDoubleShot(rel, combat.currentRanged);
                }
                createProjectile(entities, pos, combat.attackDirection,
                                projDamage, projSpeed, attacker.dimension, false, isPlayer);
                if (doubleShot) {
                    // Slight offset for second projectile
                    Vec2 offset = {combat.attackDirection.y * 6.0f, -combat.attackDirection.x * 6.0f};
                    createProjectile(entities, {pos.x + offset.x, pos.y + offset.y},
                                    combat.attackDirection, projDamage, projSpeed, attacker.dimension, false, isPlayer);
                }
                AudioManager::instance().play(SFX::RangedShot);
            }
        }
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
        bool targetIsEnemy = target.getTag().find("enemy") != std::string::npos;
        if (isPlayer && !targetIsEnemy) return;
        if (!isPlayer && target.getTag() != "player") return;

        auto& targetTransform = target.getComponent<TransformComponent>();
        Vec2 targetCenter = targetTransform.getCenter();
        float dx = targetCenter.x - attackCenter.x;
        float dy = targetCenter.y - attackCenter.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < attackRadius) {
            // Phantom Phase Through: negate enemy hits during dash
            if (!isPlayer && target.getTag() == "player" && m_player && m_player->isPhaseThrough()) {
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
            if (!isPlayer && target.getTag() == "player" && target.hasComponent<AbilityComponent>()) {
                auto& targetAbil = target.getComponent<AbilityComponent>();
                if (targetAbil.abilities[1].active && targetAbil.shieldHitsRemaining > 0) {
                    targetAbil.shieldHitsRemaining = std::max(0, targetAbil.shieldHitsRemaining - 1);
                    targetAbil.shieldAbsorbedDamage += atkData.damage;

                    // Heal player on absorb
                    if (target.hasComponent<HealthComponent>()) {
                        auto& hp = target.getComponent<HealthComponent>();
                        hp.heal(targetAbil.shieldHealPerAbsorb);
                    }

                    // Reflect projectiles
                    if (combat.currentAttack == AttackType::Ranged) {
                        AudioManager::instance().play(SFX::RiftShieldReflect);
                        // Create reflected projectile back at attacker
                        Vec2 reflectDir = combat.attackDirection * -1.0f;
                        auto& targetT = target.getComponent<TransformComponent>();
                        createProjectile(entities, targetT.getCenter(), reflectDir,
                                        atkData.damage * 1.5f, 500.0f, target.dimension, false, true);
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

            // Dimension behavior damage modifier (enemy attacks only)
            if (!isPlayer && attacker.hasComponent<AIComponent>()) {
                damage *= attacker.getComponent<AIComponent>().dimDamageMod;
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

            // Weapon mastery damage bonus (player only)
            if (isPlayer) {
                WeaponID masteryWeapon = (combat.currentAttack == AttackType::Ranged)
                    ? combat.currentRanged : combat.currentMelee;
                int wIdx = static_cast<int>(masteryWeapon);
                if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT)) {
                    MasteryBonus mb = WeaponSystem::getMasteryBonus(weaponKills[wIdx]);
                    damage *= mb.damageMult;
                }
            }

            // Blacksmith weapon upgrade bonus (player only, per-run)
            if (isPlayer && m_player) {
                if (combat.currentAttack == AttackType::Ranged)
                    damage *= m_player->smithRangedDmgMult;
                else
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
            if (!isPlayer && target.getTag() == "player" && target.hasComponent<RelicComponent>()) {
                damage *= RelicSystem::getDamageTakenMult(
                    target.getComponent<RelicComponent>(), currentDim);
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
                // Skip if player has active i-frames (no phantom feedback)
                bool targetIsPlayer = (!isPlayer && target.getTag() == "player");
                if (targetIsPlayer && hp.isInvincible()) return;
                hp.takeDamage(damage);
                m_damageEvents.push_back({targetCenter, damage, !isPlayer, isCrit});

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
                    float freezeBase = isCrit ? 0.1f : 0.05f;
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
                            Vec2 ePos = e.getComponent<TransformComponent>().getCenter();
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
                    attackerHP.heal(healAmount);
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
                    targetAI.freezeTimer = std::max(targetAI.freezeTimer, 1.5f);
                    // Brief stun on first application (when not already frozen)
                    if (targetAI.freezeTimer <= 0.01f) {
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
                AudioManager::instance().play(isPlayer ? SFX::PlayerDeath : SFX::EnemyDeath);

                // Track kills for achievements + bestiary
                if (isPlayer && target.hasComponent<AIComponent>()) {
                    killCount++;
                    // Kill event for combat challenges
                    {
                        KillEvent ke;
                        ke.wasDash = isDashAttack;
                        ke.wasCharged = isChargedAttack;
                        ke.wasRanged = (combat.currentAttack == AttackType::Ranged);
                        int ks = (combat.comboCount > 0) ? ((combat.comboCount - 1) % 3) : 0;
                        ke.wasComboFinisher = (!isDashAttack && !isChargedAttack &&
                            combat.currentAttack == AttackType::Melee && ks == 2);
                        if (m_player && m_player->getEntity()->hasComponent<PhysicsBody>())
                            ke.wasAerial = !m_player->getEntity()->getComponent<PhysicsBody>().onGround;
                        auto& tAI2 = target.getComponent<AIComponent>();
                        ke.enemyType = static_cast<int>(tAI2.enemyType);
                        ke.wasElite = tAI2.isElite;
                        ke.wasMiniBoss = tAI2.isMiniBoss;
                        ke.wasBoss = (tAI2.enemyType == EnemyType::Boss);
                        killEvents.push_back(ke);
                    }
                    // Weapon mastery: attribute kill to attack weapon
                    {
                        WeaponID killWeapon = (combat.currentAttack == AttackType::Ranged)
                            ? combat.currentRanged : combat.currentMelee;
                        int wIdx = static_cast<int>(killWeapon);
                        if (wIdx >= 0 && wIdx < static_cast<int>(WeaponID::COUNT))
                            weaponKills[wIdx]++;
                    }
                    auto& tAI = target.getComponent<AIComponent>();
                    if (tAI.isMiniBoss) killedMiniBoss = true;
                    if (tAI.element != EnemyElement::None) killedElemental = true;
                    Bestiary::onEnemyKill(tAI.enemyType);

                    // Run buff: DashRefresh - kill resets dash cooldown
                    if (m_dashRefreshOnKill && m_player) {
                        m_player->dashCooldownTimer = 0;
                    }

                    // Berserker: Momentum stack on kill
                    if (m_player) {
                        m_player->addMomentumStack();
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

                    // Weapon drop chance: elite 20%, mini-boss 40%, boss 100%
                    if (target.hasComponent<AIComponent>()) {
                        auto& tAI2 = target.getComponent<AIComponent>();
                        int weaponRoll = std::rand() % 100;
                        bool dropWeapon = false;
                        if (tAI2.enemyType == EnemyType::Boss) dropWeapon = true;
                        else if (tAI2.isMiniBoss && weaponRoll < 40) dropWeapon = true;
                        else if (tAI2.isElite && weaponRoll < 20) dropWeapon = true;

                        if (dropWeapon && m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
                            auto& pc = m_player->getEntity()->getComponent<CombatComponent>();
                            // Pick a random weapon the player doesn't currently have equipped
                            std::vector<WeaponID> candidates;
                            for (int w = 0; w < static_cast<int>(WeaponID::COUNT); w++) {
                                auto wid = static_cast<WeaponID>(w);
                                if (wid != pc.currentMelee && wid != pc.currentRanged)
                                    candidates.push_back(wid);
                            }
                            if (!candidates.empty()) {
                                WeaponID drop = candidates[std::rand() % candidates.size()];
                                ItemDrop::spawnWeaponDrop(entities, targetCenter, target.dimension, drop, m_player);
                            }
                        }
                    }
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
                                    float dmg = explodeDmg * falloff;
                                    // Defensive relic multiplier for player
                                    if (nearby.getTag() == "player" && nearby.hasComponent<RelicComponent>()) {
                                        dmg *= RelicSystem::getDamageTakenMult(
                                            nearby.getComponent<RelicComponent>(), currentDim);
                                    }
                                    nearby.getComponent<HealthComponent>().takeDamage(dmg);
                                    m_damageEvents.push_back({nt.getCenter(), dmg,
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
                SDL_Color deathColor = {255, 255, 255, 255};
                if (target.hasComponent<SpriteComponent>()) {
                    deathColor = target.getComponent<SpriteComponent>().color;
                }
                // Read AI data before destroy
                bool wasMB = false;
                bool wasElite = false;
                int targetElem = 0;
                int targetType = -1;
                if (target.hasComponent<AIComponent>()) {
                    auto& tAI = target.getComponent<AIComponent>();
                    wasMB = tAI.isMiniBoss;
                    wasElite = tAI.isElite;
                    targetElem = static_cast<int>(tAI.element);
                    targetType = static_cast<int>(tAI.enemyType);
                }
                target.destroy();
                if (m_particles) {
                    m_particles->burst(targetCenter, 25, deathColor, 200.0f, 4.0f);
                    // Death launch: directional burst in kill direction
                    if (isPlayer) {
                        float launchDir = combat.attackDirection.x >= 0 ? 0.0f : 180.0f;
                        // Bias upward for charged/dash kills
                        if (isChargedAttack) launchDir += (combat.attackDirection.x >= 0 ? -30.0f : 30.0f);
                        else if (isDashAttack) launchDir += (combat.attackDirection.x >= 0 ? -15.0f : 15.0f);
                        int launchCount = wasMB ? 20 : (wasElite ? 15 : 10);
                        float launchSpeed = wasMB ? 350.0f : (wasElite ? 300.0f : 250.0f);
                        m_particles->directionalBurst(targetCenter, launchCount, deathColor,
                                                       launchDir, 45.0f, launchSpeed, 3.5f);
                    }
                }
                emitElementDeathFX(targetCenter, targetElem);
                if (targetType >= 0) emitEnemyTypeDeathFX(targetCenter, targetType, deathColor);
                // Bigger shake on kill (extra for mini-bosses and elites)
                if (m_camera && isPlayer) {
                    m_camera->shake(wasMB ? 12.0f : (wasElite ? 10.0f : 8.0f),
                                    wasMB ? 0.35f : (wasElite ? 0.3f : 0.2f));
                }
            }
        }
    });
}

void CombatSystem::createProjectile(EntityManager& entities, Vec2 pos, Vec2 dir,
                                      float damage, float speed, int dimension,
                                      bool piercing, bool isPlayerOwned) {
    auto& proj = entities.addEntity("projectile");
    proj.dimension = dimension;

    auto& t = proj.addComponent<TransformComponent>(pos.x - 4, pos.y - 4, 8, 8);
    auto& sprite = proj.addComponent<SpriteComponent>();
    if (piercing) {
        sprite.setColor(160, 80, 255); // Purple beam color for piercing
    } else {
        sprite.setColor(255, 230, 100);
    }
    sprite.renderLayer = 3;

    auto& phys = proj.addComponent<PhysicsBody>();
    phys.useGravity = false;
    phys.velocity = dir * speed;

    auto& col = proj.addComponent<ColliderComponent>();
    col.width = 8;
    col.height = 8;
    col.layer = LAYER_PROJECTILE;
    // Ownership-based collision mask: player projectiles hit enemies only, enemy projectiles hit player only
    col.mask = LAYER_TILE | (isPlayerOwned ? LAYER_ENEMY : LAYER_PLAYER);
    col.type = ColliderType::Trigger;
    auto* cs = this;
    col.onTrigger = [damage, dimension, piercing, isPlayerOwned, cs](Entity* self, Entity* other) {
        if (other->hasComponent<HealthComponent>()) {
            auto& hp = other->getComponent<HealthComponent>();
            // Only apply damage and track event if target has no active i-frames
            if (!hp.isInvincible()) {
                float finalDmg = damage;
                bool isPlayerDamage = (other->getTag() == "player");
                // Defensive relic multiplier when projectile hits the player
                if (isPlayerDamage && other->hasComponent<RelicComponent>()) {
                    finalDmg *= RelicSystem::getDamageTakenMult(
                        other->getComponent<RelicComponent>(), dimension);
                }
                hp.takeDamage(finalDmg);
                // Track damage event for floating numbers + achievement tracking
                if (other->hasComponent<TransformComponent>()) {
                    cs->m_damageEvents.push_back({
                        other->getComponent<TransformComponent>().getCenter(),
                        finalDmg, isPlayerDamage, false
                    });
                }
                // Impact particles at hit location
                if (cs->m_particles && self->hasComponent<TransformComponent>()) {
                    Vec2 hitPos = self->getComponent<TransformComponent>().getCenter();
                    SDL_Color col = {255, 230, 100, 255};
                    if (self->hasComponent<SpriteComponent>())
                        col = self->getComponent<SpriteComponent>().color;
                    cs->m_particles->burst(hitPos, 6, col, 120.0f, 2.5f);
                }
            }
        }
        // Piercing projectiles pass through enemies (still destroyed by tiles)
        if (piercing && other->getTag() != "player") {
            // Don't destroy on enemy hit — keep going
            return;
        }
        self->destroy();
    };

    auto& hp = proj.addComponent<HealthComponent>();
    hp.maxHP = piercing ? 999 : 1; // Piercing projectiles survive hits
    hp.currentHP = piercing ? 999.0f : 1.0f;
    hp.invincibilityTime = 0;
    if (!piercing) {
        Entity* projPtr = &proj;
        hp.onDamage = [projPtr](float) { projPtr->destroy(); };
    }
}

void CombatSystem::emitEnemyTypeDeathFX(Vec2 pos, int enemyType, SDL_Color color) {
    if (!m_particles) return;
    switch (static_cast<EnemyType>(enemyType)) {
        case EnemyType::Walker: // Crumble: pieces falling down
            for (int i = 0; i < 6; i++) {
                float offX = (std::rand() % 30) - 15.0f;
                m_particles->burst({pos.x + offX, pos.y}, 2, color, 60.0f, 3.0f);
            }
            break;
        case EnemyType::Flyer: // Spiral fall trajectory
            m_particles->directionalBurst(pos, 10, color, 90.0f, 60.0f, 200.0f, 3.0f);
            m_particles->burst(pos, 4, {200, 200, 200, 150}, 80.0f, 2.0f); // feather-like
            break;
        case EnemyType::Charger: // Skid marks + dust cloud
            m_particles->directionalBurst(pos, 8, {180, 160, 140, 200}, 0.0f, 30.0f, 150.0f, 2.5f);
            m_particles->directionalBurst(pos, 8, {180, 160, 140, 200}, 180.0f, 30.0f, 150.0f, 2.5f);
            break;
        case EnemyType::Exploder: // Chain explosion ring
            m_particles->burst(pos, 20, {255, 160, 40, 255}, 300.0f, 2.5f);
            m_particles->burst(pos, 10, {255, 80, 20, 200}, 200.0f, 3.5f);
            break;
        case EnemyType::Turret: // Sparks and metal debris
            m_particles->burst(pos, 12, {200, 200, 180, 220}, 180.0f, 3.0f);
            m_particles->burst(pos, 6, {255, 255, 100, 255}, 120.0f, 1.5f); // sparks
            break;
        case EnemyType::Shielder: // Shield shatter
            m_particles->burst(pos, 15, {80, 160, 255, 220}, 250.0f, 3.5f);
            m_particles->burst(pos, 5, {180, 220, 255, 180}, 180.0f, 4.0f);
            break;
        case EnemyType::Crawler: // Goo splatter
            m_particles->burst(pos, 10, {120, 200, 80, 220}, 150.0f, 3.5f);
            m_particles->directionalBurst(pos, 6, {80, 160, 50, 200}, 90.0f, 50.0f, 100.0f, 2.5f);
            break;
        case EnemyType::Summoner: // Magical dissipation
            m_particles->burst(pos, 8, {180, 80, 220, 200}, 200.0f, 4.0f);
            m_particles->burst(pos, 12, {220, 140, 255, 150}, 120.0f, 3.0f);
            break;
        case EnemyType::Sniper: // Lens flare + precision sparks
            m_particles->burst(pos, 8, {255, 60, 60, 220}, 180.0f, 2.0f);
            m_particles->directionalBurst(pos, 4, {255, 255, 200, 255}, 0.0f, 180.0f, 300.0f, 1.5f);
            break;
        default: break;
    }
}

void CombatSystem::emitElementDeathFX(Vec2 pos, int element) {
    if (!m_particles || element == 0) return;
    switch (element) {
        case 1: // Fire: rising flame particles
            m_particles->burst(pos, 12, {255, 120, 20, 255}, 180.0f, 3.5f);
            m_particles->directionalBurst(pos, 8, {255, 200, 50, 200}, 270.0f, 40.0f, 120.0f, 2.5f);
            break;
        case 2: // Ice: shattering ice shards spreading outward
            m_particles->burst(pos, 14, {140, 200, 255, 255}, 220.0f, 3.0f);
            m_particles->burst(pos, 6, {220, 240, 255, 200}, 160.0f, 4.0f);
            break;
        case 3: // Electric: sparking bolts in random directions
            m_particles->burst(pos, 10, {255, 255, 80, 255}, 250.0f, 2.5f);
            m_particles->burst(pos, 6, {200, 220, 255, 200}, 300.0f, 2.0f);
            break;
    }
}
