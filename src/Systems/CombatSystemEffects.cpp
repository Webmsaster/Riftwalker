// CombatSystemEffects.cpp -- Split from CombatSystem.cpp (projectiles, death FX, ranged attacks, enemy death handling)
#include "CombatSystem.h"
#include "Components/TransformComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/SpriteComponent.h"
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
#include <cmath>

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
                                      Vec2 targetCenter, float damage) {
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

void CombatSystem::createProjectile(EntityManager& entities, Vec2 pos, Vec2 dir,
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
