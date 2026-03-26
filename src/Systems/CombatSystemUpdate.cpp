// CombatSystemUpdate.cpp -- Split from CombatSystem.cpp (update sub-steps: slam, DoT, sweep)
#include "CombatSystem.h"
#include "Components/TransformComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/SpriteComponent.h"
#include "Components/AIComponent.h"
#include "Components/AbilityComponent.h"
#include "Components/RelicComponent.h"
#include "Core/AudioManager.h"
#include "Game/ItemDrop.h"
#include "Game/Player.h"
#include "Game/RelicSystem.h"
#include "Game/Bestiary.h"
#include "Game/WeaponSystem.h"
#include <cmath>

void CombatSystem::processGroundSlam(EntityManager& entities, int currentDim) {
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

            // Early-out: check if any enemy could possibly be in slam radius
            // using a fast AABB pre-check before the full entity scan
            float radiusSq = radius * radius;
            bool anyInRange = false;
            entities.forEach([&](Entity& target) {
                if (anyInRange) return; // already found one, skip rest of pre-check
                if (target.getTag().find("enemy") == std::string::npos) return;
                if (!target.hasComponent<TransformComponent>()) return;
                if (target.dimension != 0 && target.dimension != currentDim) return;
                auto& tt = target.getComponent<TransformComponent>();
                // Cheap Manhattan distance pre-check (Manhattan > sqrt(2)*radius means definitely out)
                float adx = std::abs(tt.getCenter().x - slamCenter.x);
                float ady = std::abs(tt.getCenter().y - slamCenter.y);
                if (adx < radius && ady < radius) anyInRange = true;
            });

            if (anyInRange) {
            entities.forEach([&](Entity& target) {
                if (target.getTag().find("enemy") == std::string::npos) return;
                if (!target.hasComponent<TransformComponent>() || !target.hasComponent<HealthComponent>()) return;
                if (target.dimension != 0 && target.dimension != currentDim) return;

                auto& tt = target.getComponent<TransformComponent>();
                float dx = tt.getCenter().x - slamCenter.x;
                float dy = tt.getCenter().y - slamCenter.y;
                // Skip if clearly out of range (cheap squared-distance check)
                if (dx * dx + dy * dy >= radiusSq) return;
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
                        m_particles->burst(tt.getCenter(), 4, {255, 200, 80, 255}, 150.0f, 3.0f); // reduced from 10
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
                            m_particles->burst(tt.getCenter(), 12, {255, 180, 60, 255}, 200.0f, 4.0f); // reduced from 25
                            // Slam death launch: burst left+right from impact
                            m_particles->directionalBurst(tt.getCenter(), 4, {255, 200, 80, 255},  // reduced from 8
                                                           0.0f, 60.0f, 280.0f, 3.0f);
                            m_particles->directionalBurst(tt.getCenter(), 4, {255, 200, 80, 255},  // reduced from 8
                                                           180.0f, 60.0f, 280.0f, 3.0f);
                        }
                        emitElementDeathFX(tt.getCenter(), slamElem);
                    }
                }
            });
            } // anyInRange

            if (m_camera) m_camera->shake(12.0f, 0.3f);
            m_pendingHitFreeze += 0.12f;

            // Mark slam as processed
            abil.abilities[0].duration = 0;
            abil.abilities[0].active = false;
        }
    });
}

void CombatSystem::processBurnDoT(EntityManager& entities, float dt) {
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
}

void CombatSystem::processFreezeDecay(EntityManager& entities, float dt) {
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
}

void CombatSystem::processProjectileLifetime(EntityManager& entities, float dt) {
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

void CombatSystem::processZombieSweep(EntityManager& entities, int currentDim) {
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
                    // Only drop unlocked weapons the player doesn't already have
                    if (WeaponSystem::isUnlocked(wid) &&
                        wid != pc.currentMelee && wid != pc.currentRanged)
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
