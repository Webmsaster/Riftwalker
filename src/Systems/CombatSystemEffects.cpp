// CombatSystemEffects.cpp -- Split from CombatSystem.cpp (projectiles, death FX, ranged attacks, enemy death handling)
#include "CombatSystem.h"
#include "Components/TransformComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/AIComponent.h"
#include "Components/RelicComponent.h"
#include "Core/AudioManager.h"
#include "Game/ItemDrop.h"
#include "Game/Player.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/Enemy.h"
#include "Game/Bestiary.h"
#include "Game/WeaponSystem.h"
#include "Game/DimensionManager.h"
#include <cmath>
#include <algorithm>

void CombatSystem::processRangedAttack(Entity& attacker, EntityManager& entities,
                                         CombatComponent& combat, bool isPlayer) {
    auto& transform = attacker.getComponent<TransformComponent>();
    auto& atkData = combat.getCurrentAttackData();

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
        } else if (isPlayer && combat.currentRanged == WeaponID::DimLauncher) {
            // Dimensional Launcher: projectile exists in both dimensions (dimension 0)
            // DimensionalBarrage synergy (DimLauncher + DimensionalEcho): +50% damage
            float launcherDmg = projDamage;
            if (attacker.hasComponent<RelicComponent>()) {
                launcherDmg *= RelicSynergy::getDimensionalBarrageDamageMult(
                    attacker.getComponent<RelicComponent>(), combat.currentRanged);
            }
            createProjectile(entities, pos, combat.attackDirection,
                            launcherDmg, 300.0f, 0, false, isPlayer);
            AudioManager::instance().play(SFX::RangedShot);
            // Dimensional rift particles at launch point
            if (m_particles) {
                m_particles->burst(pos, 10, {180, 120, 255, 220}, 100.0f, 2.5f);
                m_particles->burst(pos, 6, {100, 200, 255, 180}, 60.0f, 2.0f);
            }
        } else if (isPlayer && combat.currentRanged == WeaponID::RiftCrossbow) {
            // Rift Crossbow: piercing bolt that passes through enemies
            createProjectile(entities, pos, combat.attackDirection,
                            projDamage, 450.0f, attacker.dimension, true, isPlayer);
            AudioManager::instance().play(SFX::RangedShot);
            // Cyan piercing trail particles
            if (m_particles) {
                m_particles->burst(pos, 6, {80, 220, 255, 200}, 80.0f, 2.0f);
            }
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
}

void CombatSystem::handleEnemyDeath(Entity& attacker, Entity& target, EntityManager& entities,
                                      int currentDim, bool isPlayer, bool isDashAttack,
                                      bool isChargedAttack, CombatComponent& combat,
                                      const Vec2& targetCenter, float damage) {
    auto& hp = target.getComponent<HealthComponent>();
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
                // Pick a random unlocked weapon the player doesn't currently have equipped
                std::vector<WeaponID> candidates;
                for (int w = 0; w < static_cast<int>(WeaponID::COUNT); w++) {
                    auto wid = static_cast<WeaponID>(w);
                    if (WeaponSystem::isUnlocked(wid) &&
                        wid != pc.currentMelee && wid != pc.currentRanged)
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
                        st.width = static_cast<int>(st.width * 0.7f);
                        st.height = static_cast<int>(st.height * 0.7f);
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

    // Summoner chain death: destroy all alive minions in same dimension
    if (isPlayer && target.hasComponent<AIComponent>()) {
        auto& targetAI2 = target.getComponent<AIComponent>();
        if (targetAI2.enemyType == EnemyType::Summoner) {
            int targetDim = target.dimension;
            entities.forEach([&](Entity& minion) {
                if (!minion.isAlive()) return;
                if (minion.getTag() != "enemy_minion") return;
                if (minion.dimension != 0 && minion.dimension != targetDim) return;
                // Purple particles on each dying minion
                if (m_particles && minion.hasComponent<TransformComponent>()) {
                    Vec2 mPos = minion.getComponent<TransformComponent>().getCenter();
                    m_particles->burst(mPos, 10, {180, 80, 220, 255}, 150.0f, 3.0f);
                    m_particles->burst(mPos, 5, {220, 140, 255, 200}, 100.0f, 2.0f);
                }
                minion.destroy();
            });
        }
    }

    // Leech death effect: if player within 80px, apply poison
    if (isPlayer && target.hasComponent<AIComponent>()) {
        auto& targetAI3 = target.getComponent<AIComponent>();
        // Use integer comparison for forward-compat (Leech = 12 in new enum)
        if (static_cast<int>(targetAI3.enemyType) == 12) {
            if (m_player && m_player->getEntity() && m_player->getEntity()->hasComponent<TransformComponent>()) {
                Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                float pdx = playerPos.x - targetCenter.x;
                float pdy = playerPos.y - targetCenter.y;
                float playerDist = std::sqrt(pdx * pdx + pdy * pdy);
                if (playerDist < 80.0f) {
                    // Check i-frames before applying poison
                    if (m_player->getEntity()->hasComponent<HealthComponent>() &&
                        !m_player->getEntity()->getComponent<HealthComponent>().isInvincible()) {
                        m_player->applyPoison(3.0f);
                        // Green poison cloud particles
                        if (m_particles) {
                            m_particles->burst(targetCenter, 15, {60, 200, 40, 220}, 180.0f, 3.5f);
                            m_particles->burst(playerPos, 8, {80, 220, 60, 200}, 80.0f, 2.0f);
                        }
                    }
                }
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
    bool wasBoss = false;
    int targetElem = 0;
    int targetType = -1;
    if (target.hasComponent<AIComponent>()) {
        auto& tAI = target.getComponent<AIComponent>();
        wasMB = tAI.isMiniBoss;
        wasElite = tAI.isElite;
        wasBoss = (tAI.enemyType == EnemyType::Boss);
        targetElem = static_cast<int>(tAI.element);
        targetType = static_cast<int>(tAI.enemyType);
    }

    // Capture death ghost effect BEFORE destroying entity
    {
        DeathEffect de{};
        de.position = targetCenter;
        if (target.hasComponent<TransformComponent>()) {
            auto& tf = target.getComponent<TransformComponent>();
            de.width = static_cast<float>(tf.width);
            de.height = static_cast<float>(tf.height);
        } else {
            de.width = 32.0f;
            de.height = 32.0f;
        }
        de.color = deathColor;
        de.texture = nullptr;
        de.srcRect = {0, 0, 32, 32};
        de.flipX = false;
        if (target.hasComponent<SpriteComponent>()) {
            auto& spr = target.getComponent<SpriteComponent>();
            de.texture = spr.texture;
            de.srcRect = spr.srcRect;
            de.flipX = spr.flipX;
            if (target.hasComponent<AnimationComponent>()) {
                de.srcRect = target.getComponent<AnimationComponent>().getCurrentSrcRect();
            }
        }
        de.timer = 0;
        de.maxLife = wasBoss ? 0.6f : (wasMB ? 0.4f : 0.3f);
        de.isBoss = wasBoss;
        de.isElite = wasElite || wasMB;
        de.flashPhase = 0;
        m_deathEffects.push_back(de);
    }

    target.destroy();

    if (wasBoss && isPlayer) {
        // === Epic boss death effects ===
        // Massive particle burst (50 particles)
        if (m_particles) {
            m_particles->burst(targetCenter, 50, deathColor, 350.0f, 6.0f);
            // Bright white core explosion
            m_particles->burst(targetCenter, 30, {255, 255, 255, 255}, 280.0f, 5.0f);
            // Expanding ring of particles (8 directions)
            for (int i = 0; i < 8; i++) {
                float angle = i * 45.0f;
                m_particles->directionalBurst(targetCenter, 8, {255, 240, 200, 255},
                                               angle, 20.0f, 320.0f, 5.0f);
            }
            // Kill-direction launch burst
            float launchDir = combat.attackDirection.x >= 0 ? 0.0f : 180.0f;
            if (isChargedAttack) launchDir += (combat.attackDirection.x >= 0 ? -30.0f : 30.0f);
            m_particles->directionalBurst(targetCenter, 25, deathColor,
                                           launchDir, 50.0f, 400.0f, 5.0f);
        }
        // Big screen shake (20px, 0.5s)
        if (m_camera) {
            m_camera->shake(20.0f, 0.5f);
            // White screen flash
            m_camera->flash(0.4f, 255, 255, 255);
        }
        // Long hitstop (0.25s)
        m_pendingHitFreeze += 0.25f;
        AudioManager::instance().play(SFX::BossShieldBurst); // Biggest boom SFX available
    } else {
        // === Normal / elite / mini-boss death effects ===
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
        // Bigger shake on kill (extra for mini-bosses and elites)
        if (m_camera && isPlayer) {
            m_camera->shake(wasMB ? 12.0f : (wasElite ? 10.0f : 8.0f),
                            wasMB ? 0.35f : (wasElite ? 0.3f : 0.2f));
        }
        // Graduated hitstop: normal 2f, elite 4f, mini-boss 6f (at 60fps)
        if (isPlayer) {
            m_pendingHitFreeze += wasMB ? 0.10f : (wasElite ? 0.06f : 0.04f);
        }
    }

    emitElementDeathFX(targetCenter, targetElem);
    if (targetType >= 0) emitEnemyTypeDeathFX(targetCenter, targetType, deathColor);
}

void CombatSystem::createProjectile(EntityManager& entities, const Vec2& pos, const Vec2& dir,
                                      float damage, float speed, int dimension,
                                      bool piercing, bool isPlayerOwned) {
    auto& proj = entities.addEntity("projectile");
    proj.dimension = dimension;

    auto& t = proj.addComponent<TransformComponent>(pos.x - 4, pos.y - 4, 8, 8);
    (void)t;
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
                    Vec2 srcPos = self->hasComponent<TransformComponent>()
                        ? self->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
                    cs->m_damageEvents.push_back({
                        other->getComponent<TransformComponent>().getCenter(),
                        finalDmg, isPlayerDamage, false, false, srcPos
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

void CombatSystem::emitEnemyTypeDeathFX(const Vec2& pos, int enemyType, SDL_Color color) {
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
        case EnemyType::Charger: // Enhanced: more sparks + long dust trail
            m_particles->directionalBurst(pos, 12, {180, 160, 140, 200}, 0.0f, 30.0f, 200.0f, 3.0f);
            m_particles->directionalBurst(pos, 12, {180, 160, 140, 200}, 180.0f, 30.0f, 200.0f, 3.0f);
            // Extra spark shower
            m_particles->burst(pos, 15, {255, 220, 100, 255}, 250.0f, 2.5f);
            // Long dust trail behind
            m_particles->directionalBurst(pos, 8, {160, 140, 120, 150}, 0.0f, 15.0f, 120.0f, 4.0f);
            m_particles->directionalBurst(pos, 8, {160, 140, 120, 150}, 180.0f, 15.0f, 120.0f, 4.0f);
            break;
        case EnemyType::Exploder: // Enhanced: large white flash particles
            m_particles->burst(pos, 25, {255, 160, 40, 255}, 350.0f, 3.0f);
            m_particles->burst(pos, 15, {255, 80, 20, 200}, 250.0f, 4.0f);
            // Bright white flash burst
            m_particles->burst(pos, 20, {255, 255, 240, 255}, 300.0f, 2.0f);
            m_particles->burst(pos, 10, {255, 255, 255, 255}, 180.0f, 1.5f);
            break;
        case EnemyType::Turret: // Sparks and metal debris
            m_particles->burst(pos, 12, {200, 200, 180, 220}, 180.0f, 3.0f);
            m_particles->burst(pos, 6, {255, 255, 100, 255}, 120.0f, 1.5f); // sparks
            break;
        case EnemyType::Shielder: // Enhanced: blue falling shield fragments
            m_particles->burst(pos, 15, {80, 160, 255, 220}, 250.0f, 3.5f);
            m_particles->burst(pos, 5, {180, 220, 255, 180}, 180.0f, 4.0f);
            // Shield fragments flying upward then falling
            m_particles->directionalBurst(pos, 10, {120, 200, 255, 240}, 270.0f, 60.0f, 220.0f, 4.5f);
            m_particles->directionalBurst(pos, 6, {200, 230, 255, 200}, 270.0f, 40.0f, 150.0f, 3.5f);
            break;
        case EnemyType::Crawler: // Goo splatter
            m_particles->burst(pos, 10, {120, 200, 80, 220}, 150.0f, 3.5f);
            m_particles->directionalBurst(pos, 6, {80, 160, 50, 200}, 90.0f, 50.0f, 100.0f, 2.5f);
            break;
        case EnemyType::Summoner: // Magical dissipation
            m_particles->burst(pos, 8, {180, 80, 220, 200}, 200.0f, 4.0f);
            m_particles->burst(pos, 12, {220, 140, 255, 150}, 120.0f, 3.0f);
            break;
        case EnemyType::Sniper: // Enhanced: fading red scope beam particles
            m_particles->burst(pos, 8, {255, 60, 60, 220}, 180.0f, 2.0f);
            m_particles->directionalBurst(pos, 4, {255, 255, 200, 255}, 0.0f, 180.0f, 300.0f, 1.5f);
            // Fading red scope beam trail
            m_particles->directionalBurst(pos, 8, {255, 40, 40, 180}, 0.0f, 10.0f, 200.0f, 3.0f);
            m_particles->directionalBurst(pos, 8, {255, 40, 40, 180}, 180.0f, 10.0f, 200.0f, 3.0f);
            break;
        default: break;
    }
}

void CombatSystem::emitElementDeathFX(const Vec2& pos, int element) {
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

void CombatSystem::processCounterAttack(Entity& player, EntityManager& entities, int currentDim) {
    if (!player.hasComponent<CombatComponent>() || !player.hasComponent<TransformComponent>()) return;
    auto& combat = player.getComponent<CombatComponent>();
    auto& transform = player.getComponent<TransformComponent>();
    Vec2 playerCenter = transform.getCenter();
    Vec2 dir = combat.attackDirection;

    // Determine which weapon drives the counter-attack
    // Melee weapons use melee counter, ranged weapons use ranged counter
    WeaponID weapon = combat.currentMelee;
    bool isMelee = WeaponSystem::isMelee(weapon);

    // Base damage values from weapon data
    float meleeBaseDmg = WeaponSystem::getWeaponData(combat.currentMelee).damage;
    float rangedBaseDmg = WeaponSystem::getWeaponData(combat.currentRanged).damage;

    // Apply class + relic + resonance damage multipliers
    float dmgMult = 1.0f;
    if (m_player) dmgMult *= m_player->getClassDamageMultiplier();
    if (m_player && m_player->damageBoostTimer > 0) dmgMult *= m_player->damageBoostMultiplier;
    if (m_dimMgr) dmgMult *= m_dimMgr->getResonanceDamageMult();
    if (player.hasComponent<RelicComponent>()) {
        auto& relics = player.getComponent<RelicComponent>();
        float hpPct = player.hasComponent<HealthComponent>() ? player.getComponent<HealthComponent>().getPercent() : 1.0f;
        dmgMult *= RelicSystem::getDamageMultiplier(relics, hpPct, currentDim);
    }

    // Helper: find nearest alive enemy within radius
    auto findNearest = [&](float radius) -> Entity* {
        Entity* nearest = nullptr;
        float bestDist = radius * radius;
        entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos || !e.isAlive()) return;
            if (!e.hasComponent<TransformComponent>() || !e.hasComponent<HealthComponent>()) return;
            if (e.dimension != 0 && e.dimension != currentDim) return;
            auto& et = e.getComponent<TransformComponent>();
            float dx = et.getCenter().x - playerCenter.x;
            float dy = et.getCenter().y - playerCenter.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestDist) { bestDist = d2; nearest = &e; }
        });
        return nearest;
    };

    // Helper: apply damage + knockback + stun to a single target, respecting i-frames
    auto hitTarget = [&](Entity& target, float damage, float knockback, float stunDur,
                         Vec2 knockDir, bool isCrit) {
        if (!target.hasComponent<HealthComponent>()) return;
        auto& hp = target.getComponent<HealthComponent>();
        if (hp.isInvincible()) return;

        hp.takeDamage(damage);
        Vec2 tc = target.getComponent<TransformComponent>().getCenter();
        addDamageEvent(tc, damage, false, isCrit);

        // Impact sparks (bright directional burst at hit point)
        if (m_particles) {
            SDL_Color sparkColor = isCrit
                ? SDL_Color{255, 255, 200, 255}   // Bright gold for crits
                : SDL_Color{255, 220, 160, 255};   // Warm white for normal hits
            float impactDir = std::atan2(knockDir.y, knockDir.x) * 180.0f / 3.14159f;
            m_particles->directionalBurst(tc, isCrit ? 12 : 6, sparkColor,
                                          impactDir, 60.0f, 280.0f, 2.5f);
        }

        // Knockback
        if (knockback > 0 && target.hasComponent<PhysicsBody>()) {
            auto& phys = target.getComponent<PhysicsBody>();
            float len = std::sqrt(std::max(0.01f, knockDir.x * knockDir.x + knockDir.y * knockDir.y));
            phys.velocity += knockDir * (knockback / len);
        }

        // Stun
        if (stunDur > 0 && target.hasComponent<AIComponent>()) {
            target.getComponent<AIComponent>().stun(stunDur);
            AudioManager::instance().play(SFX::EnemyStun);
        }

        // Check death — let zombie sweep handle it
        if (hp.currentHP <= 0) {
            handleEnemyDeath(player, target, entities, currentDim, true, false, false,
                            combat, tc, damage);
        }
    };

    switch (combat.currentMelee) {
        case WeaponID::RiftBlade: {
            // "Rift Slash": Wide 120px AoE arc, 3x melee damage, 800 knockback, hits all enemies
            float counterDmg = meleeBaseDmg * 3.0f * dmgMult;
            float counterKB = 800.0f;
            float counterRange = 120.0f;

            entities.forEach([&](Entity& target) {
                if (target.getTag().find("enemy") == std::string::npos || !target.isAlive()) return;
                if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                if (target.dimension != 0 && target.dimension != currentDim) return;
                auto& tt = target.getComponent<TransformComponent>();
                Vec2 tc = tt.getCenter();
                float dx = tc.x - playerCenter.x;
                float dy = tc.y - playerCenter.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < counterRange) {
                    Vec2 kd = {dx, dy - 80.0f};
                    hitTarget(target, counterDmg, counterKB, 0.3f, kd, true);
                }
            });

            if (m_camera) m_camera->shake(14.0f, 0.35f);
            m_pendingHitFreeze += 0.15f;
            if (m_particles) {
                // Cyan rift arc sweep
                float dirDeg = std::atan2(dir.y, dir.x) * 180.0f / 3.14159f;
                m_particles->directionalBurst(playerCenter, 20, {100, 200, 255, 255},
                    dirDeg, 90.0f, 300.0f, 4.0f);
                m_particles->burst(playerCenter, 15, {180, 240, 255, 255}, 200.0f, 3.0f);
            }
            AudioManager::instance().play(SFX::MeleeHit);
            break;
        }

        case WeaponID::PhaseDaggers: {
            // "Shadow Flurry": 3 rapid hits on nearest enemy, 1.5x damage each, teleport behind
            Entity* target = findNearest(150.0f);
            if (target && target->hasComponent<TransformComponent>()) {
                float counterDmg = meleeBaseDmg * 1.5f * dmgMult;
                Vec2 tc = target->getComponent<TransformComponent>().getCenter();

                // Teleport player behind target
                float behindX = (playerCenter.x < tc.x) ? tc.x + 30.0f : tc.x - 30.0f;
                transform.position.x = behindX - transform.width * 0.5f;

                // 3 rapid hits
                for (int i = 0; i < 3; i++) {
                    Vec2 kd = {(playerCenter.x < tc.x) ? 1.0f : -1.0f, -0.5f};
                    hitTarget(*target, counterDmg, 200.0f, (i == 2) ? 0.3f : 0, kd, true);
                }

                if (m_camera) m_camera->shake(10.0f, 0.25f);
                m_pendingHitFreeze += 0.12f;
                if (m_particles) {
                    // Purple shadow trail from old position to new
                    m_particles->burst(playerCenter, 12, {180, 100, 255, 200}, 150.0f, 3.0f);
                    m_particles->burst(tc, 15, {200, 130, 255, 255}, 180.0f, 3.5f);
                }
            }
            AudioManager::instance().play(SFX::PhaseStrikeHit);
            break;
        }

        case WeaponID::VoidHammer: {
            // "Void Crush": 160px AoE shockwave, 2x charged damage, 2s stun on all hit
            float counterDmg = WeaponSystem::getWeaponData(WeaponID::VoidHammer).damage * 2.0f * dmgMult;
            // Use charged attack base as reference (VoidHammer charged = 50*chargePercent normally)
            counterDmg = std::max(counterDmg, 50.0f * dmgMult);
            float counterRange = 160.0f;

            entities.forEach([&](Entity& target) {
                if (target.getTag().find("enemy") == std::string::npos || !target.isAlive()) return;
                if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                if (target.dimension != 0 && target.dimension != currentDim) return;
                auto& tt = target.getComponent<TransformComponent>();
                Vec2 tc = tt.getCenter();
                float dx = tc.x - playerCenter.x;
                float dy = tc.y - playerCenter.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < counterRange) {
                    Vec2 kd = {dx, dy - 100.0f};
                    hitTarget(target, counterDmg, 600.0f, 2.0f, kd, true);
                }
            });

            if (m_camera) m_camera->shake(20.0f, 0.45f);
            m_pendingHitFreeze += 0.2f;
            if (m_particles) {
                // Purple shockwave ring expanding outward
                for (int i = 0; i < 8; i++) {
                    float angle = i * 45.0f;
                    m_particles->directionalBurst(playerCenter, 6,
                        {160, 80, 255, 255}, angle, 25.0f, 280.0f, 5.0f);
                }
                m_particles->burst(playerCenter, 30, {200, 150, 255, 255}, 250.0f, 4.5f);
                // Ground dust
                m_particles->directionalBurst(playerCenter, 10,
                    {160, 140, 120, 200}, 0.0f, 180.0f, 150.0f, 3.0f);
            }
            AudioManager::instance().play(SFX::GroundSlam);
            break;
        }

        case WeaponID::GravityGauntlet: {
            // "Gravity Vortex": Pull ALL enemies within 150px toward player, deal 2x damage
            float counterDmg = meleeBaseDmg * 2.0f * dmgMult;
            float vortexRange = 150.0f;

            entities.forEach([&](Entity& target) {
                if (target.getTag().find("enemy") == std::string::npos || !target.isAlive()) return;
                if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                if (target.dimension != 0 && target.dimension != currentDim) return;
                auto& tt = target.getComponent<TransformComponent>();
                Vec2 tc = tt.getCenter();
                float dx = tc.x - playerCenter.x;
                float dy = tc.y - playerCenter.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > vortexRange) return;

                target.getComponent<HealthComponent>().takeDamage(counterDmg);
                m_damageEvents.push_back({tc, counterDmg, false, false});

                // Pull toward player
                if (target.hasComponent<PhysicsBody>()) {
                    auto& phys = target.getComponent<PhysicsBody>();
                    float pullStr = 400.0f * (1.0f - dist / vortexRange);
                    phys.velocity.x += (-dx / std::max(dist, 1.0f)) * pullStr;
                    phys.velocity.y += (-dy / std::max(dist, 1.0f)) * pullStr;
                }
                if (target.hasComponent<AIComponent>()) {
                    target.getComponent<AIComponent>().stun(0.4f);
                }
            });

            if (m_camera) m_camera->shake(14.0f, 0.3f);
            m_pendingHitFreeze += 0.12f;
            if (m_particles) {
                // Purple gravity vortex ring
                for (int i = 0; i < 12; i++) {
                    float angle = i * 30.0f;
                    m_particles->directionalBurst(playerCenter, 4,
                        {140, 80, 220, 255}, angle, 30.0f, 180.0f, 3.0f);
                }
                m_particles->burst(playerCenter, 20, {180, 100, 255, 220}, 200.0f, 4.0f);
            }
            AudioManager::instance().play(SFX::GroundSlam);
            break;
        }

        default:
            // Ranged weapons: switch on ranged weapon type
            break;
    }

    // Ranged weapon counters (if melee weapon wasn't one of the special ones above,
    // OR if player has a ranged weapon equipped — we always use melee weapon for the switch.
    // Ranged counters are handled separately when the equipped melee is RiftBlade/PhaseDaggers/VoidHammer
    // but the currentRanged determines the ranged counter)
    // Actually: the counter always uses the melee weapon. For ranged weapons, we need a different approach.
    // Let's check: the spec says "unique counter-attacks per weapon". Since melee and ranged are separate,
    // and the parry is melee-centric, we use currentMelee for melee weapons and currentRanged for ranged.
    // The simplest approach: if the player's MELEE weapon matched above, we're done.
    // If not (shouldn't happen since all 3 melee weapons are covered), fall through.
    // For ranged weapon counters, we handle them based on currentRanged only if the melee
    // weapon was already handled above — meaning ranged counters are ADDITIONAL.
    // Actually re-reading the spec: it lists ALL 6 weapons. The counter depends on which weapon the player
    // is using. Since there are 3 melee + 3 ranged, and we do melee counters above, let's also
    // fire the ranged counter:

    switch (combat.currentRanged) {
        case WeaponID::ShardPistol: {
            // "Shard Burst": 3 projectiles aimed at nearest enemies, 2.5x ranged damage
            float projDmg = rangedBaseDmg * 2.5f * dmgMult;

            // Find up to 3 nearest enemies
            struct TargetInfo { Entity* e; float dist; };
            std::vector<TargetInfo> targets;
            entities.forEach([&](Entity& e) {
                if (e.getTag().find("enemy") == std::string::npos || !e.isAlive()) return;
                if (!e.hasComponent<TransformComponent>() || !e.hasComponent<HealthComponent>()) return;
                if (e.dimension != 0 && e.dimension != currentDim) return;
                auto& et = e.getComponent<TransformComponent>();
                float dx = et.getCenter().x - playerCenter.x;
                float dy = et.getCenter().y - playerCenter.y;
                targets.push_back({&e, dx * dx + dy * dy});
            });
            // Sort by distance
            std::sort(targets.begin(), targets.end(),
                [](const TargetInfo& a, const TargetInfo& b) { return a.dist < b.dist; });

            int shotsToFire = std::min(3, static_cast<int>(targets.size()));
            for (int i = 0; i < shotsToFire; i++) {
                auto& et = targets[i].e->getComponent<TransformComponent>();
                Vec2 tc = et.getCenter();
                Vec2 projDir = {tc.x - playerCenter.x, tc.y - playerCenter.y};
                float len = std::sqrt(std::max(0.01f, projDir.x * projDir.x + projDir.y * projDir.y));
                projDir = projDir * (1.0f / len);
                createProjectile(entities, playerCenter, projDir, projDmg, 500.0f,
                    player.dimension, false, true);
            }
            // If no enemies, fire 3 in attack direction
            if (shotsToFire == 0) {
                for (int i = 0; i < 3; i++) {
                    float spread = (i - 1) * 0.15f;
                    float baseAng = std::atan2(dir.y, dir.x) + spread;
                    Vec2 projDir = {std::cos(baseAng), std::sin(baseAng)};
                    createProjectile(entities, playerCenter, projDir, projDmg, 500.0f,
                        player.dimension, false, true);
                }
            }
            if (m_particles) {
                m_particles->burst(playerCenter, 12, {255, 230, 100, 255}, 250.0f, 3.0f);
            }
            AudioManager::instance().play(SFX::RangedShot);
            break;
        }

        case WeaponID::RiftShotgun: {
            // "Point Blank": 80px close range, 5x ranged damage, 1200 knockback
            float counterDmg = rangedBaseDmg * 5.0f * dmgMult;
            float counterRange = 80.0f;
            float counterKB = 1200.0f;

            entities.forEach([&](Entity& target) {
                if (target.getTag().find("enemy") == std::string::npos || !target.isAlive()) return;
                if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                if (target.dimension != 0 && target.dimension != currentDim) return;
                auto& tt = target.getComponent<TransformComponent>();
                Vec2 tc = tt.getCenter();
                float dx = tc.x - playerCenter.x;
                float dy = tc.y - playerCenter.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < counterRange) {
                    Vec2 kd = {dx, dy - 60.0f};
                    hitTarget(target, counterDmg, counterKB, 0.5f, kd, true);
                }
            });

            if (m_camera) m_camera->shake(16.0f, 0.35f);
            m_pendingHitFreeze += 0.15f;
            if (m_particles) {
                float dirDeg = std::atan2(dir.y, dir.x) * 180.0f / 3.14159f;
                m_particles->directionalBurst(playerCenter, 25, {255, 160, 50, 255},
                    dirDeg, 60.0f, 350.0f, 3.5f);
                m_particles->burst(playerCenter, 15, {255, 200, 100, 255}, 200.0f, 3.0f);
            }
            AudioManager::instance().play(SFX::RangedShot);
            break;
        }

        case WeaponID::VoidBeam: {
            // "Overcharge": Thick piercing beam, 4x ranged damage
            float projDmg = rangedBaseDmg * 4.0f * dmgMult;
            // Create 3 parallel piercing projectiles for "thick beam" effect
            for (int i = -1; i <= 1; i++) {
                Vec2 offset = {dir.y * i * 6.0f, -dir.x * i * 6.0f};
                Vec2 projPos = {playerCenter.x + offset.x, playerCenter.y + offset.y};
                createProjectile(entities, projPos, dir, projDmg, 600.0f,
                    player.dimension, true, true);
            }

            if (m_camera) m_camera->shake(12.0f, 0.3f);
            m_pendingHitFreeze += 0.1f;
            if (m_particles) {
                float dirDeg = std::atan2(dir.y, dir.x) * 180.0f / 3.14159f;
                m_particles->directionalBurst(playerCenter, 20, {200, 120, 255, 255},
                    dirDeg, 15.0f, 500.0f, 4.0f);
                m_particles->burst(playerCenter, 10, {160, 80, 255, 200}, 150.0f, 3.0f);
            }
            AudioManager::instance().play(SFX::RangedShot);
            break;
        }

        case WeaponID::RiftCrossbow: {
            // "Rift Barrage": Rapid 5 piercing bolts in tight spread, 2x damage each
            float projDmg = rangedBaseDmg * 2.0f * dmgMult;
            Vec2 dir = combat.attackDirection;
            float baseAngle = std::atan2(dir.y, dir.x);

            for (int i = 0; i < 5; i++) {
                float offset = (i - 2) * 0.08f; // Tight spread ±0.16 radians
                Vec2 boltDir = {std::cos(baseAngle + offset), std::sin(baseAngle + offset)};
                createProjectile(entities, playerCenter, boltDir,
                                projDmg, 550.0f, currentDim, true, true);
            }

            if (m_camera) m_camera->shake(10.0f, 0.25f);
            m_pendingHitFreeze += 0.08f;
            if (m_particles) {
                float dirDeg = baseAngle * 180.0f / 3.14159f;
                m_particles->directionalBurst(playerCenter, 25, {80, 200, 255, 255},
                    dirDeg, 20.0f, 400.0f, 3.5f);
                m_particles->burst(playerCenter, 8, {120, 220, 255, 200}, 100.0f, 2.0f);
            }
            AudioManager::instance().play(SFX::RangedShot);
            break;
        }

        default:
            // GrapplingHook or unknown — no ranged counter
            break;
    }
}
