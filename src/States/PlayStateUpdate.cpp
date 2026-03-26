// PlayStateUpdate.cpp -- Split from PlayState.cpp (update sub-systems extracted from update())
#include "PlayState.h"
#include "Core/Game.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AbilityComponent.h"
#include "Core/AudioManager.h"
#include "Game/AchievementSystem.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/LoreSystem.h"
#include "Game/DailyRun.h"
#include "Game/Bestiary.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

// Smoke test logging (defined in PlayStateSmokeBot.cpp)
extern void smokeLog(const char* fmt, ...);

void PlayState::updateDimensionSwitch() {
    auto& input = game->getInput();

    // Dimension switch
    if (input.isActionPressed(Action::DimensionSwitch)) {
        if (m_smokeTest && m_player) {
            Vec2 dimPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            smokeLog("[SMOKE] DIM_REQUEST floor=%d seed=%d pos=(%.0f,%.0f) current=%d switching=%d cooldown=%.2f",
                     m_currentDifficulty,
                     m_level ? m_level->getGenerationSeed() : -1,
                     dimPos.x,
                     dimPos.y,
                     m_dimManager.getCurrentDimension(),
                     m_dimManager.isSwitching() ? 1 : 0,
                     m_dimManager.getCooldownTimer());
        }
        // Check if dimension is locked by Void Sovereign or Entropy Incarnate
        bool dimLocked = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_boss" && e.hasComponent<AIComponent>()) {
                auto& bossAi = e.getComponent<AIComponent>();
                if (bossAi.bossType == 4 && bossAi.vsDimLockActive > 0) dimLocked = true;
                if (bossAi.bossType == 5 && bossAi.eiDimLockActive > 0) dimLocked = true;
            }
        });
        if (dimLocked) {
            m_camera.shake(3.0f, 0.1f); // Feedback: can't switch
        } else if (m_dimManager.switchDimension()) {
            m_dimensionSwitches++; // Track total dimension switches for run summary
            // Dimension Dancer: 50 dimension switches in one run
            if (m_dimensionSwitches >= 50) {
                game->getAchievements().unlock("dimension_dancer");
            }
            if (m_smokeTest) {
                smokeLog("[SMOKE] DIM_STARTED floor=%d seed=%d current=%d switching=%d cooldown=%.2f",
                         m_currentDifficulty,
                         m_level ? m_level->getGenerationSeed() : -1,
                         m_dimManager.getCurrentDimension(),
                         m_dimManager.isSwitching() ? 1 : 0,
                         m_dimManager.getCooldownTimer());
            }
            m_entropy.onDimensionSwitch();
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
            game->getInputMutable().rumble(0.3f, 100);
            m_screenEffects.triggerDimensionRipple();

            // Relic dimension-switch effects
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                auto& relicComp = m_player->getEntity()->getComponent<RelicComponent>();
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                int prevDim = relicComp.lastSwitchDimension;
                relicComp.lastSwitchDimension = m_dimManager.getCurrentDimension();
                RelicSystem::onDimensionSwitch(relicComp, &playerHP);

                // DimResidue: leave damage zone (with spawn ICD + zone cap)
                if (relicComp.hasRelic(RelicID::DimResidue) && prevDim > 0
                    && relicComp.dimResidueSpawnCD <= 0) {
                    // Count existing zones
                    int zoneCount = 0;
                    m_entities.forEach([&](Entity& e) {
                        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
                    });
                    if (zoneCount < RelicSystem::getMaxResidueZones()) {
                        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                        float residueDur = RelicSynergy::getResidueDuration(relicComp);
                        auto& zone = m_entities.addEntity("dim_residue");
                        zone.dimension = prevDim;
                        auto& zt = zone.addComponent<TransformComponent>();
                        zt.position = {pPos.x - 24, pPos.y - 24};
                        zt.width = 48;
                        zt.height = 48;
                        auto& zh = zone.addComponent<HealthComponent>();
                        zh.maxHP = residueDur;
                        zh.currentHP = residueDur;
                        m_particles.burst(pPos, 12, {200, 80, 150, 255}, 80.0f, 2.0f);
                        relicComp.dimResidueSpawnCD = RelicSystem::getDimResidueSpawnICD();
                    }
                }
            }

            // Voidwalker passive: Rift Affinity - heal on dim-switch + Dimensional Affinity
            if (m_player && m_player->playerClass == PlayerClass::Voidwalker) {
                const auto& voidData = ClassSystem::getData(PlayerClass::Voidwalker);
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.heal(voidData.switchHeal);
                if (m_player->particles) {
                    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                    m_player->particles->burst(pPos, 8, {60, 200, 255, 200}, 60.0f, 2.0f);
                }
                // Dimensional Affinity: activate Rift Charge damage buff
                m_player->activateRiftCharge();
            }

            // Lore: Echoes of Origin - first dimension switch
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::DimensionOrigin)) {
                    lore->discover(LoreID::DimensionOrigin);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }

            // Run buff: PhantomStep - invincible after dimension switch
            float phantomDur = game->getRunBuffSystem().getPhantomStepDuration();
            if (phantomDur > 0 && m_player) {
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.invincibilityTimer = std::max(hp.invincibilityTimer, phantomDur);
                if (m_player->particles) {
                    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                    m_player->particles->burst(pPos, 15, {180, 150, 255, 200}, 120.0f, 2.0f);
                }
            }
        } // end else if (switchDimension succeeded)
    }

    // Forced dimension switch from entropy (bypasses cooldown)
    if (m_entropy.shouldForceSwitch()) {
        m_dimManager.switchDimension(true);
        m_entropy.clearForceSwitch();
        m_camera.shake(8.0f, 0.3f);
        AudioManager::instance().play(SFX::DimensionSwitch);
        AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
        m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
        m_screenEffects.triggerDimensionRipple();
    }
}

void PlayState::updateDimensionEffects(float dt) {
    // Resonance particle aura around player
    int resTier = m_dimManager.getResonanceTier();
    if (resTier > 0) {
        Vec2 pCenter = m_dimManager.playerPos;
        float angle = static_cast<float>(std::rand() % 628) / 100.0f;
        float dist = 12.0f + static_cast<float>(std::rand() % 10);
        Vec2 sparkPos = {pCenter.x + std::cos(angle) * dist, pCenter.y + std::sin(angle) * dist};
        SDL_Color auraColor;
        if (resTier >= 3) auraColor = {255, 220, 80, 200};
        else if (resTier >= 2) auraColor = {180, 100, 255, 180};
        else auraColor = {80, 200, 220, 140};
        m_particles.burst(sparkPos, resTier, auraColor, 30.0f, 2.0f);
    }

    // Update input distortion from entropy
    game->getInputMutable().setInputDistortion(m_entropy.getInputDistortion());

    // Trail system
    m_trails.update(dt);

    // Move trail (subtle)
    m_moveTrailTimer -= dt;
    if (m_moveTrailTimer <= 0 && m_player) {
        auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
        auto& classData = ClassSystem::getData(m_player->playerClass);
        m_trails.emit(pt.getCenter().x, pt.getCenter().y + pt.height * 0.3f,
                      2.0f, classData.color.r, classData.color.g, classData.color.b, 50, 0.2f);
        m_moveTrailTimer = 0.05f;
    }

    // Screen effects
    if (m_player && m_player->getEntity()->hasComponent<HealthComponent>()) {
        float hp = m_player->getEntity()->getComponent<HealthComponent>().getPercent();
        m_screenEffects.setHP(hp);

        // Low HP heartbeat SFX
        if (hp > 0 && hp < 0.25f) {
            m_heartbeatTimer -= dt;
            if (m_heartbeatTimer <= 0) {
                AudioManager::instance().play(SFX::Heartbeat);
                // Beat faster as HP gets lower (1.2s at 25% -> 0.6s near 0%)
                float urgency = 1.0f - (hp / 0.25f);
                m_heartbeatTimer = 1.2f - urgency * 0.6f;
            }
        } else {
            m_heartbeatTimer = 0; // Reset so it plays immediately when entering low HP
        }
    }
    m_screenEffects.setEntropy(m_entropy.getEntropy());
    // Check if Void Storm or Entropy Storm is active
    bool stormActive = false;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "enemy_boss" && e.hasComponent<AIComponent>()) {
            auto& bossAi = e.getComponent<AIComponent>();
            if (bossAi.bossType == 4 && bossAi.vsStormActive > 0) stormActive = true;
            if (bossAi.bossType == 5 && bossAi.eiStormActive > 0) stormActive = true;
        }
    });
    m_screenEffects.setVoidStorm(stormActive);
    m_screenEffects.update(dt);

    // Music system
    {
        int nearEnemies = 0;
        bool bossActive = false;
        int bossPhase = 1;
        Vec2 pPos = m_player ? m_player->getEntity()->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().substr(0, 5) == "enemy") {
                if (e.getTag() == "enemy_boss") {
                    bossActive = true;
                    if (e.hasComponent<AIComponent>()) {
                        bossPhase = e.getComponent<AIComponent>().bossPhase;
                    }
                }
                if (!e.hasComponent<TransformComponent>()) return;
                auto& et = e.getComponent<TransformComponent>();
                float dx = et.getCenter().x - pPos.x;
                float dy = et.getCenter().y - pPos.y;
                if (dx * dx + dy * dy < 400.0f * 400.0f) nearEnemies++;
            }
        });
        float hp = m_player ? m_player->getEntity()->getComponent<HealthComponent>().getPercent() : 1.0f;
        m_musicSystem.update(dt, nearEnemies, bossActive, hp, m_entropy.getEntropy());
        AudioManager::instance().updateMusicLayers(m_musicSystem);

        // Procedural music: boss track switching and phase changes
        m_musicSystem.setBossActive(bossActive, bossPhase);
    }
}

void PlayState::updatePlayerPhysicsEffects(float dt) {
    // Landing effects: screen shake + dust particles proportional to fall speed
    {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        if (phys.onGround && !phys.wasOnGround && phys.landingImpactSpeed > 250.0f) {
            float t = std::min((phys.landingImpactSpeed - 250.0f) / 550.0f, 1.0f);
            m_camera.shake(2.0f + t * 6.0f, 0.08f + t * 0.12f);
            game->getInputMutable().rumble(0.2f + t * 0.4f, 80 + static_cast<int>(t * 120));

            // Landing squash: sprite goes wide+short on impact
            auto& spr = m_player->getEntity()->getComponent<SpriteComponent>();
            spr.landingSquashTimer = 0.15f;
            spr.landingSquashIntensity = 0.1f + t * 0.15f; // 0.1-0.25 based on fall speed

            // Landing dust cloud at feet -- bigger for harder landings
            auto& tr = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 feetPos = {tr.getCenter().x, tr.position.y + tr.height};
            int dustCount = 6 + static_cast<int>(t * 10);
            float dustSpeed = 40.0f + t * 80.0f;
            float dustSize = 2.0f + t * 3.0f;
            SDL_Color dustColor = {180, 160, 130, static_cast<Uint8>(150 + t * 80)};
            // Spread left and right from feet
            m_particles.directionalBurst(feetPos, dustCount / 2, dustColor, 180.0f, 60.0f, dustSpeed, dustSize);
            m_particles.directionalBurst(feetPos, dustCount / 2, dustColor, 0.0f, 60.0f, dustSpeed, dustSize);
        }
        if (phys.onGround) phys.landingImpactSpeed = 0;
    }

    // Wall slide dust: small particles at wall contact while sliding
    if (m_player->isWallSliding) {
        m_wallSlideDustTimer -= dt;
        if (m_wallSlideDustTimer <= 0) {
            m_wallSlideDustTimer = 0.12f; // emit every 120ms
            auto& tr = m_player->getEntity()->getComponent<TransformComponent>();
            auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
            // Particles at the wall-contact side, near player's mid-height
            bool onRight = phys.onWallRight;
            float wx = onRight ? (tr.position.x + tr.width) : tr.position.x;
            float wy = tr.position.y + tr.height * 0.4f;
            Vec2 contactPoint = {wx, wy};
            // Dust drifts away from wall (left if wall is right, right if wall is left)
            float dir = onRight ? 180.0f : 0.0f;
            SDL_Color dustColor = {190, 170, 140, 140};
            m_particles.directionalBurst(contactPoint, 2, dustColor, dir, 50.0f, 25.0f, 2.0f);
        }
    } else {
        m_wallSlideDustTimer = 0;
    }

    // Projectile terrain impact: particles where projectiles hit walls
    for (auto& impact : m_physics.getProjectileImpacts()) {
        // Directional burst away from wall (opposite of projectile velocity)
        float speed = Vec2(impact.velocity.x, impact.velocity.y).length();
        if (speed > 0.01f) {
            float dirRad = std::atan2(-impact.velocity.y, -impact.velocity.x);
            float dirDeg = dirRad * 180.0f / 3.14159f;
            m_particles.directionalBurst(impact.position, 8, impact.color, dirDeg, 90.0f, 100.0f, 2.5f);
        } else {
            m_particles.burst(impact.position, 8, impact.color, 100.0f, 2.5f);
        }
    }

    // Enemy wall impact: bounce particles + bonus damage + SFX
    m_entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") == std::string::npos) return;
        if (!e.hasComponent<PhysicsBody>()) return;
        auto& phys = e.getComponent<PhysicsBody>();
        if (phys.wallImpactSpeed <= 0) return;

        float impact = phys.wallImpactSpeed;
        phys.wallImpactSpeed = 0;

        if (!e.hasComponent<TransformComponent>()) return;
        auto& t = e.getComponent<TransformComponent>();
        Vec2 center = t.getCenter();

        // Wall side: left wall = particles go right, right wall = particles go left
        float particleDir = phys.onWallLeft ? 0.0f : 180.0f;
        Vec2 wallPoint = phys.onWallLeft ?
            Vec2{t.position.x, center.y} :
            Vec2{t.position.x + t.width, center.y};

        // Impact intensity scales with speed (150-500+ range)
        float intensity = std::min((impact - 150.0f) / 350.0f, 1.0f);

        // Dust/spark particles at wall contact point
        int particleCount = 4 + static_cast<int>(intensity * 8);
        SDL_Color dustColor = {200, 180, 150, static_cast<Uint8>(180 + intensity * 75)};
        m_particles.directionalBurst(wallPoint, particleCount, dustColor,
                                      particleDir, 50.0f, 60.0f + intensity * 100.0f, 2.0f + intensity * 2.0f);
        // White sparks for harder impacts
        if (intensity > 0.4f) {
            m_particles.burst(wallPoint, 3 + static_cast<int>(intensity * 5),
                              {255, 255, 240, 220}, 80.0f + intensity * 80.0f, 1.5f);
        }

        // Bonus wall slam damage (5-15 based on impact speed)
        if (e.hasComponent<HealthComponent>()) {
            float wallDmg = 5.0f + intensity * 10.0f;
            auto& hp = e.getComponent<HealthComponent>();
            hp.takeDamage(wallDmg);
            m_combatSystem.addDamageEvent(center, wallDmg, false, false);
        }

        // Camera shake + SFX
        m_camera.shake(2.0f + intensity * 4.0f, 0.08f + intensity * 0.1f);
        AudioManager::instance().play(SFX::GroundSlam);
    });
}

void PlayState::updateTechnomancerEntities(float dt) {
    // Technomancer: update player-owned turrets (lifetime + auto-fire)
    if (!m_player->isTechnomancer()) return;

    int turretCount = 0;
    int curDim = m_dimManager.getCurrentDimension();
    m_entities.forEach([&](Entity& turret) {
        if (turret.getTag() != "player_turret" || !turret.isAlive()) return;
        if (!turret.hasComponent<HealthComponent>()) return;

        auto& hp = turret.getComponent<HealthComponent>();
        hp.currentHP -= dt; // decay lifetime
        if (hp.currentHP <= 0) {
            // Turret expired
            auto& tt = turret.getComponent<TransformComponent>();
            m_particles.burst(tt.getCenter(), 8, {230, 180, 50, 150}, 60.0f, 2.0f);
            turret.destroy();
            return;
        }
        turretCount++;

        if (!turret.hasComponent<AIComponent>() || !turret.hasComponent<CombatComponent>()) return;
        auto& ai = turret.getComponent<AIComponent>();
        auto& tt = turret.getComponent<TransformComponent>();
        Vec2 turretPos = tt.getCenter();

        // Find nearest enemy in range
        Entity* nearestEnemy = nullptr;
        float nearestDist = ai.detectRange;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.isAlive() || !e.hasComponent<TransformComponent>()) return;
            if (!e.hasComponent<HealthComponent>()) return;
            if (e.dimension != 0 && e.dimension != curDim) return;
            auto& eHP = e.getComponent<HealthComponent>();
            if (eHP.currentHP <= 0) return;
            auto& et = e.getComponent<TransformComponent>();
            float dx = et.getCenter().x - turretPos.x;
            float dy = et.getCenter().y - turretPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestEnemy = &e;
            }
        });

        // Auto-fire at nearest enemy
        ai.attackTimer -= dt;
        if (nearestEnemy && ai.attackTimer <= 0) {
            auto& combat = turret.getComponent<CombatComponent>();
            auto& et = nearestEnemy->getComponent<TransformComponent>();
            Vec2 dir = (et.getCenter() - turretPos).normalized();
            ai.attackTimer = ai.attackCooldown;

            // Fire player-owned projectile
            m_combatSystem.createProjectile(m_entities, turretPos, dir,
                combat.rangedAttack.damage, 350.0f, 0, false, true);
            AudioManager::instance().play(SFX::RangedShot);

            // Muzzle flash particles
            m_particles.burst({turretPos.x + dir.x * 12, turretPos.y + dir.y * 12},
                4, {255, 220, 80, 220}, 40.0f, 1.5f);

            // Face toward target
            if (turret.hasComponent<SpriteComponent>()) {
                turret.getComponent<SpriteComponent>().flipX = (dir.x < 0);
            }
        }
    });
    m_player->activeTurrets = turretCount;

    // Count active traps (lifetime decay)
    int trapCount = 0;
    m_entities.forEach([&](Entity& trap) {
        if (trap.getTag() != "player_trap" || !trap.isAlive()) return;
        if (!trap.hasComponent<HealthComponent>()) return;

        auto& hp = trap.getComponent<HealthComponent>();
        hp.currentHP -= dt; // decay lifetime
        if (hp.currentHP <= 0) {
            auto& trapT = trap.getComponent<TransformComponent>();
            m_particles.burst(trapT.getCenter(), 6, {255, 200, 50, 120}, 40.0f, 1.5f);
            trap.destroy();
            return;
        }
        trapCount++;
    });
    m_player->activeTraps = trapCount;
}

void PlayState::updateBossEffects(float dt) {
    // Void Sovereign Phase 3: auto dimension switch
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() != "enemy_boss") return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& bossAi = e.getComponent<AIComponent>();
        if (bossAi.bossType == 4 && bossAi.vsForceDimSwitch) {
            bossAi.vsForceDimSwitch = false;
            m_dimManager.switchDimension(true);
            m_camera.shake(6.0f, 0.2f);
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
            m_screenEffects.triggerDimensionRipple();
        }
    });

    // Entropy Incarnate boss effects
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() != "enemy_boss") return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& bossAi = e.getComponent<AIComponent>();
        if (bossAi.bossType != 5) return;

        // Entropy Pulse: add entropy when pulse just fired
        if (bossAi.eiPulseFired) {
            bossAi.eiPulseFired = false;
            float entropyAdd = (bossAi.bossPhase >= 3) ? 20.0f : 15.0f;
            if (m_player && e.hasComponent<TransformComponent>()) {
                auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
                Vec2 bossPos = e.getComponent<TransformComponent>().getCenter();
                Vec2 pPos = playerT.getCenter();
                float d = std::sqrt((pPos.x - bossPos.x) * (pPos.x - bossPos.x) +
                                     (pPos.y - bossPos.y) * (pPos.y - bossPos.y));
                if (d < 200.0f) {
                    m_entropy.addEntropy(entropyAdd);
                }
            }
        }

        // Entropy Storm: continuous entropy drain while player in range
        if (bossAi.eiStormActive > 0 && m_player) {
            auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 bossPos = e.getComponent<TransformComponent>().getCenter();
            Vec2 pPos = playerT.getCenter();
            float d = std::sqrt((pPos.x - bossPos.x) * (pPos.x - bossPos.x) +
                                 (pPos.y - bossPos.y) * (pPos.y - bossPos.y));
            if (d < 150.0f) {
                m_entropy.addEntropy(3.0f * dt); // 3 entropy/s while in range
            }
        }

        // Dimension Shatter: force random dimension switch
        if (bossAi.eiForceDimSwitch) {
            bossAi.eiForceDimSwitch = false;
            m_dimManager.switchDimension(true);
            m_camera.shake(8.0f, 0.3f);
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
            m_screenEffects.triggerDimensionRipple();
        }

        // Entropy Overload: if player entropy > 70, boss heals 5% max HP/s
        if (bossAi.bossPhase >= 3 && m_entropy.getEntropy() > 70.0f) {
            if (bossAi.eiOverloadHealAccum >= 0.1f) {
                float healPerTick = e.getComponent<HealthComponent>().maxHP * 0.005f; // 0.5% per 0.1s = 5%/s
                auto& bossHP = e.getComponent<HealthComponent>();
                bossHP.heal(healPerTick);
                bossAi.eiOverloadHealAccum -= 0.1f;
                // Visual feedback: green heal particles
                if (bossAi.eiOverloadHealAccum < 0.2f) {
                    Vec2 bPos = e.getComponent<TransformComponent>().getCenter();
                    m_particles.burst(bPos, 3, {50, 200, 80, 200}, 40.0f, 1.5f);
                }
            }
        }

        // Final Stand: massive +30 entropy applied once on trigger
        if (bossAi.eiFinalStandTriggered && !bossAi.eiFinalStandEntropyApplied) {
            bossAi.eiFinalStandEntropyApplied = true;
            m_entropy.addEntropy(30.0f);
        }
    });

    // Entropy minion death: add +10 entropy to player on death
    // (Handled by zombie sweep in CombatSystem -- detect dying entropy minions here)
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() != "enemy_entropy_minion") return;
        if (!e.hasComponent<HealthComponent>()) return;
        auto& mhp = e.getComponent<HealthComponent>();
        if (mhp.currentHP <= 0 && e.isAlive()) {
            m_entropy.addEntropy(10.0f);
            if (e.hasComponent<TransformComponent>()) {
                Vec2 mPos = e.getComponent<TransformComponent>().getCenter();
                m_particles.burst(mPos, 20, {140, 0, 200, 255}, 150.0f, 4.0f);
            }
        }
    });
}

void PlayState::updateEnemyHazardDamage(float dt) {
    // Enemy hazard damage: enemies on spikes/fire/laser take damage
    int curDim = m_dimManager.getCurrentDimension();
    int ts = m_level->getTileSize();
    m_entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") == std::string::npos) return;
        if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>()) return;
        if (!e.hasComponent<TransformComponent>()) return;
        auto& ai = e.getComponent<AIComponent>();
        if (ai.enemyType == EnemyType::Boss) return; // bosses immune to hazards
        if (ai.spawnTimer > 0) return; // spawning enemies immune
        if (ai.hazardDmgCooldown > 0) { ai.hazardDmgCooldown -= dt; return; }

        auto& t = e.getComponent<TransformComponent>();
        auto& hp = e.getComponent<HealthComponent>();
        Vec2 center = t.getCenter();
        int footX = static_cast<int>(center.x) / ts;
        int footY = static_cast<int>(t.position.y + t.height - 2) / ts;
        if (!m_level->inBounds(footX, footY)) return;

        const auto& tile = m_level->getTile(footX, footY, (e.dimension == 0) ? curDim : e.dimension);

        if (tile.type == TileType::Spike) {
            hp.takeDamage(15.0f);
            m_combatSystem.addDamageEvent(center, 15.0f, false, false, true);
            ai.hazardDmgCooldown = 0.5f;
            m_particles.burst(center, 8, {255, 80, 40, 255}, 100.0f, 2.0f);
            if (e.hasComponent<PhysicsBody>()) {
                e.getComponent<PhysicsBody>().velocity.y = -200.0f;
            }
        } else if (tile.type == TileType::Fire) {
            hp.takeDamage(12.0f);
            m_combatSystem.addDamageEvent(center, 12.0f, false, false, true);
            ai.hazardDmgCooldown = 0.5f;
            ai.burnTimer = std::max(ai.burnTimer, 1.5f);
            m_particles.burst(center, 6, {255, 150, 30, 255}, 80.0f, 2.0f);
        } else if (m_level->isInLaserBeam(center.x, center.y, (e.dimension == 0) ? curDim : e.dimension)) {
            hp.takeDamage(25.0f);
            m_combatSystem.addDamageEvent(center, 25.0f, false, false, true);
            ai.hazardDmgCooldown = 0.3f;
            m_particles.burst(center, 10, {255, 50, 50, 255}, 120.0f, 2.5f);
        }
    });
}

void PlayState::updatePlayerHazardDamage(float dt) {
    // Player hazard damage (spikes, fire, laser, conveyor)
    m_spikeDmgCooldown -= dt;
    {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        int tileSize = m_level->getTileSize();
        int footX = static_cast<int>(playerT.getCenter().x) / tileSize;
        int footY = static_cast<int>(playerT.position.y + playerT.height - 1) / tileSize;
        int dim = m_dimManager.getCurrentDimension();

        // Helper: apply defensive relic multiplier to hazard damage
        auto hazardDmg = [&](float base) -> float {
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                return base * RelicSystem::getDamageTakenMult(
                    m_player->getEntity()->getComponent<RelicComponent>(), dim);
            }
            return base;
        };

        // Spike & fire damage (cooldown-based, skip during i-frames)
        if (m_spikeDmgCooldown <= 0 && m_level->inBounds(footX, footY)) {
            const auto& tile = m_level->getTile(footX, footY, dim);
            auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
            if (tile.type == TileType::Spike && !playerHP.isInvincible()) {
                // BALANCE R2: Spike DMG 5 -> 3, cooldown 0.8 -> 1.0 (still 36-43% of all damage)
                float spikeDmg = hazardDmg(3.0f);
                playerHP.takeDamage(spikeDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), spikeDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(2.0f);
                m_spikeDmgCooldown = 1.0f;
                m_camera.shake(6.0f, 0.2f);
                AudioManager::instance().play(SFX::SpikeDamage);
                if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
                    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                    phys.velocity.y = -250.0f;
                }
                m_particles.burst(playerT.getCenter(), 15, {255, 80, 40, 255}, 150.0f, 3.0f);
                m_hud.triggerDamageFlash();
            } else if (tile.type == TileType::Fire && !playerHP.isInvincible()) {
                // BALANCE R2: Fire DMG 5 -> 3, cooldown 0.7 -> 1.0
                float fireDmg = hazardDmg(3.0f);
                playerHP.takeDamage(fireDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), fireDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(2.0f);
                m_spikeDmgCooldown = 1.0f;
                m_camera.shake(4.0f, 0.15f);
                AudioManager::instance().play(SFX::FireBurn);
                m_player->applyBurn(1.5f); // Fire tiles also apply burn
                if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
                    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                    phys.velocity.y = -200.0f;
                }
                m_particles.burst(playerT.getCenter(), 12, {255, 150, 30, 255}, 120.0f, 2.5f);
                m_hud.triggerDamageFlash();
            }
        }

        // Laser beam damage (separate cooldown via same timer, skip during i-frames)
        if (m_spikeDmgCooldown <= 0) {
            auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
            if (!playerHP.isInvincible() &&
                m_level->isInLaserBeam(playerT.getCenter().x, playerT.getCenter().y, dim)) {
                // BALANCE R2: Laser DMG 10 -> 6, cooldown 0.6 -> 1.0
                float laserDmg = hazardDmg(6.0f);
                playerHP.takeDamage(laserDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), laserDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(4.0f);
                m_spikeDmgCooldown = 1.0f;
                m_camera.shake(8.0f, 0.25f);
                AudioManager::instance().play(SFX::LaserHit);
                m_particles.burst(playerT.getCenter(), 20, {255, 50, 50, 255}, 200.0f, 3.0f);
                m_hud.triggerDamageFlash();
            }
        }

        // Conveyor belt push (always active, no damage)
        // Check tile at feet AND tile below feet (conveyor is the surface tile)
        if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
            int convDir = 0;
            bool onConveyor = (m_level->inBounds(footX, footY) && m_level->isOnConveyor(footX, footY, dim, convDir))
                || (m_level->inBounds(footX, footY + 1) && m_level->isOnConveyor(footX, footY + 1, dim, convDir));
            if (onConveyor) {
                auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                phys.velocity.x += convDir * 350.0f * dt; // Stronger push (was 120)
                // Subtle wind particles in conveyor direction
                m_conveyorParticleTimer -= dt;
                if (m_conveyorParticleTimer <= 0) {
                    Vec2 feet = {playerT.getCenter().x, playerT.position.y + playerT.height};
                    m_particles.burst(feet, 2, {180, 200, 220, 120}, 50.0f, 1.0f);
                    m_conveyorParticleTimer = 0.25f;
                }
            } else {
                m_conveyorParticleTimer = 0;
            }
        }

        // Teleporter: teleport player to paired tile
        // Cooldown ticks down regardless of position to prevent ping-pong
        if (m_teleportCooldown > 0) m_teleportCooldown -= dt;
        {
            int centerTX = static_cast<int>(playerT.getCenter().x) / m_level->getTileSize();
            int centerTY = static_cast<int>(playerT.getCenter().y) / m_level->getTileSize();
            if (m_level->inBounds(centerTX, centerTY)) {
                const auto& tile = m_level->getTile(centerTX, centerTY, dim);
                if (tile.type == TileType::Teleporter && m_teleportCooldown <= 0) {
                    Vec2 dest = m_level->getTeleporterDestination(centerTX, centerTY, dim);
                    if (dest.x != 0 || dest.y != 0) {
                        playerT.position = dest;
                        m_teleportCooldown = 1.0f; // Prevent instant re-teleport
                        m_particles.burst(playerT.getCenter(), 15, {50, 220, 100, 255}, 200.0f, 3.0f);
                        AudioManager::instance().play(SFX::BossTeleport);
                    }
                }
            }
        }
    }

    // Dimension switch plates: activate when player steps on them
    if (m_player) {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        int dim = m_dimManager.getCurrentDimension();
        int footX = static_cast<int>(playerT.getCenter().x) / m_level->getTileSize();
        int footY = static_cast<int>(playerT.position.y + playerT.height + 1) / m_level->getTileSize();
        if (m_level->isDimSwitchAt(footX, footY, dim)) {
            int pairId = m_level->getTile(footX, footY, dim).variant;
            if (m_level->activateDimSwitch(pairId, dim)) {
                m_camera.shake(6.0f, 0.3f);
                m_particles.burst(playerT.getCenter(), 20, {100, 255, 100, 255}, 150.0f, 3.0f);
                AudioManager::instance().play(SFX::RiftRepair);
            }
        }
    }
}

void PlayState::updateStatusEffects(float dt) {
    // Status effect DoT damage
    if (m_player) {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
        int dotDim = m_dimManager.getCurrentDimension();

        // Helper: apply defensive relic multiplier to DoT damage
        auto dotDmg = [&](float base) -> float {
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                return base * RelicSystem::getDamageTakenMult(
                    m_player->getEntity()->getComponent<RelicComponent>(), dotDim);
            }
            return base;
        };

        if (m_player->isBurning()) {
            m_player->burnDmgTimer -= dt;
            if (m_player->burnDmgTimer <= 0) {
                float burnDmg = dotDmg(5.0f);
                playerHP.takeDamage(burnDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), burnDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_player->burnDmgTimer = 0.3f;
                m_particles.burst(playerT.getCenter(), 4, {255, 120, 30, 200}, 60.0f, 1.5f);
            }
        }
        if (m_player->isPoisoned()) {
            m_player->poisonDmgTimer -= dt;
            if (m_player->poisonDmgTimer <= 0) {
                float poisonDmg = dotDmg(3.0f);
                playerHP.takeDamage(poisonDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), poisonDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_player->poisonDmgTimer = 0.5f;
                m_particles.burst(playerT.getCenter(), 3, {80, 200, 40, 200}, 40.0f, 1.5f);
            }
        }
    }

    // HUD flash update
    m_hud.updateFlash(dt);

    // Notification timers (achievement + lore)
    game->getAchievements().update(dt);
    if (auto* lore = game->getLoreSystem()) {
        lore->updateNotification(dt);
    }

    // Tutorial timer + action tracking
    m_tutorialTimer += dt;
    if (m_player && m_player->getEntity()->hasComponent<PhysicsBody>()) {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        if (std::abs(phys.velocity.x) > 10.0f) m_hasMovedThisRun = true;
        if (!phys.onGround) m_hasJumpedThisRun = true;
    }
    if (m_player && m_player->isDashing) m_hasDashedThisRun = true;
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        if (combat.isAttacking) {
            m_hasAttackedThisRun = true;
            // Track non-Scythe melee for entropy_master achievement
            if (combat.currentAttack == AttackType::Melee ||
                combat.currentAttack == AttackType::Charged) {
                if (combat.currentMelee != WeaponID::EntropyScythe) {
                    m_usedNonScytheMelee = true;
                }
            }
        }
    }
    if (game->getInput().isActionPressed(Action::RangedAttack)) m_hasRangedThisRun = true;
    if (m_player && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        for (int i = 0; i < 3; i++) {
            if (abil.abilities[i].active) m_hasUsedAbilityThisRun = true;
        }
    }

    // Entropy
    m_entropy.update(dt);
    if (m_dimManager.getCurrentDimension() == 2) {
        const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
        m_entropy.addEntropy(shiftBalance.dimBEntropyPerSecond * dt);
    }
    if (m_entropy.isCritical() && !m_playerDying) {
        // Suit crash - run over with death sequence
        m_playerDying = true;
        m_deathCause = 2; // Entropy
        m_deathSequenceTimer = m_deathSequenceDuration;
        AudioManager::instance().play(SFX::SuitEntropyCritical);
        m_camera.shake(15.0f, 0.6f);

        // Entropy overload particles (purple/magenta)
        if (m_player) {
            Vec2 deathPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(deathPos, 35, {200, 50, 180, 255}, 280.0f, 5.0f);
            m_particles.burst(deathPos, 20, {255, 80, 255, 255}, 200.0f, 4.0f);
            m_particles.burst(deathPos, 15, {120, 40, 140, 200}, 350.0f, 6.0f);
        }
        m_hud.triggerDamageFlash();
        return; // Caller checks m_playerDying
    }

    // Relic timed effects update
    // Reset per-frame entropy modifiers before applying relic effects
    m_entropy.passiveDecayModifier = 1.0f;
    if (m_player->getEntity()->hasComponent<RelicComponent>()) {
        auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
        RelicSystem::updateTimedEffects(relics, dt);

        // ChaosRift buff effects (type 0=speed, 3=regen)
        if (relics.chaosRiftBuffTimer > 0) {
            if (relics.chaosRiftBuffType == 0) {
                m_player->speedBoostTimer = std::max(m_player->speedBoostTimer, dt + 0.01f);
                m_player->speedBoostMultiplier = 1.3f;
            } else if (relics.chaosRiftBuffType == 3) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                playerHP.heal(5.0f * dt); // 5 HP/sec regen
            }
        }

        // Phase Cloak: invincibility during cloak
        if (relics.phaseCloakTimer > 0) {
            auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
            playerHP.invincibilityTimer = std::max(playerHP.invincibilityTimer, dt + 0.01f);
        }

        // Entropy Anchor: reduce entropy gain in boss fights
        if (m_isBossLevel && !m_bossDefeated) {
            float entropyMult = RelicSystem::getEntropyMultiplier(relics, true);
            if (entropyMult < 1.0f) {
                // Apply reduction by adjusting passive gain
                m_entropy.passiveGainModifier = entropyMult;
            }
        } else {
            m_entropy.passiveGainModifier = 1.0f;
        }
        // EntropySponge: no passive entropy (kills add entropy instead)
        if (RelicSystem::hasNoPassiveEntropy(relics)) {
            m_entropy.passiveGainModifier = 0.0f;
        }

        // EntropySiphon: +50% entropy passive gain (multiplicative with other modifiers)
        float siphonMult = RelicSystem::getEntropySiphonGainMult(relics);
        if (siphonMult > 1.0f) {
            m_entropy.passiveGainModifier *= siphonMult;
        }

        // TimeDistortion: entropy passive decay is 50% slower
        // passiveDecayModifier is reset to 1.0 each frame before relic effects are applied
        m_entropy.passiveDecayModifier = RelicSystem::getTimeDistortionEntropyDecayMult(relics);

        // ChaosCore: force dimension switch every 20s
        if (relics.chaosCoreTimer <= 0) {
            relics.chaosCoreTimer = 20.0f;
            // Force switch even if on cooldown (force=true bypasses cooldown check)
            if (m_dimManager.switchDimension(true)) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                RelicSystem::onDimensionSwitch(relics, &playerHP);
                relics.lastSwitchDimension = m_dimManager.getCurrentDimension();
            }
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(pPos, 20, {220, 80, 220, 200}, 200.0f, 3.0f);
            AudioManager::instance().play(SFX::DimensionSwitch);
        }

        // VampiricEdge: block natural heal-over-time and pickup-orb healing
        // (healing from kills is still applied in kill-event block below)
        relics.vampiricEdgeActive = relics.hasRelic(RelicID::VampiricEdge);

        // BerserkersCurse: strip shield whenever it would be active
        if (RelicSystem::hasBerserkersCurse(relics) && m_player->hasShield) {
            m_player->hasShield = false;
            m_player->shieldTimer = 0;
        }
    }
}

bool PlayState::updatePlayerDeath() {
    // Player death
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    if (hp.currentHP <= 0) {
        // Check for Phoenix Feather relic first
        if (m_player->getEntity()->hasComponent<RelicComponent>()) {
            auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
            if (RelicSystem::hasPhoenixFeather(relics)) {
                RelicSystem::consumePhoenixFeather(relics);
                hp.currentHP = hp.maxHP;
                hp.invincibilityTimer = 3.0f;
                AudioManager::instance().play(SFX::RiftShieldActivate);
                m_camera.shake(12.0f, 0.5f);
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(pPos, 50, {255, 180, 50, 255}, 350.0f, 6.0f);
                m_particles.burst(pPos, 25, {255, 120, 30, 255}, 250.0f, 5.0f);
                // LuckyPhoenix synergy: retain 50% of shards as bonus
                float shardRetain = RelicSynergy::getPhoenixShardRetain(relics);
                if (shardRetain > 0) {
                    int bonusShards = static_cast<int>(shardsCollected * shardRetain);
                    if (bonusShards > 0) {
                        shardsCollected += bonusShards;
                        game->getUpgradeSystem().addRiftShards(bonusShards);
                        m_particles.burst(pPos, 30, {255, 215, 0, 255}, 200.0f, 4.0f);
                    }
                }
                return true; // survived
            }
        }
        // Check for Extra Life run buff
        auto& buffs = game->getRunBuffSystem();
        if (buffs.hasExtraLife()) {
            buffs.consumeExtraLife();
            hp.currentHP = hp.maxHP * 0.5f;
            hp.invincibilityTimer = 2.0f;
            AudioManager::instance().play(SFX::RiftShieldActivate);
            m_camera.shake(10.0f, 0.4f);
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(pPos, 40, {255, 255, 200, 255}, 300.0f, 5.0f);
            m_particles.burst(pPos, 20, {200, 150, 255, 255}, 200.0f, 4.0f);
        } else {
            // Start death sequence: dramatic pause before ending run
            if (!m_playerDying) {
                m_playerDying = true;
                m_deathCause = 1; // HP depleted
                m_deathSequenceTimer = m_deathSequenceDuration;
                AudioManager::instance().play(SFX::PlayerDeath);
                m_camera.shake(20.0f, 0.8f);

                // Death explosion particles
                Vec2 deathPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                const auto& classColor = ClassSystem::getData(m_player->playerClass).color;
                m_particles.burst(deathPos, 40, {classColor.r, classColor.g, classColor.b, 255}, 300.0f, 6.0f);
                m_particles.burst(deathPos, 25, {255, 60, 40, 255}, 200.0f, 5.0f);
                m_particles.burst(deathPos, 15, {255, 255, 200, 255}, 250.0f, 4.0f);
                // Expanding ring
                for (int i = 0; i < 16; i++) {
                    float angle = i * 6.283185f / 16.0f;
                    Vec2 ringPos = {deathPos.x + std::cos(angle) * 8.0f, deathPos.y + std::sin(angle) * 8.0f};
                    m_particles.burst(ringPos, 2, {255, 100, 60, 200}, 180.0f, 3.0f);
                }

                m_hud.triggerDamageFlash();
            }
            return false; // dying
        }
    }
    return true; // alive
}

void PlayState::updateKillEffects() {
    // Consume kill tracking from combat system
    if (m_combatSystem.killCount <= 0) return;

    enemiesKilled += m_combatSystem.killCount;
    game->getUpgradeSystem().totalEnemiesKilled += m_combatSystem.killCount;
    m_screenEffects.triggerKillFlash();

    // Kill streak tracking: chain kills within 3 seconds
    for (int k = 0; k < m_combatSystem.killCount; k++) {
        if (m_killStreakTimer > 0) {
            m_killStreakCount++;
        } else {
            m_killStreakCount = 1;
        }
        m_killStreakTimer = 3.0f;

        // Escalating streak thresholds
        if (m_killStreakCount >= 12) {
            m_killStreakText = "GODLIKE!";
            m_killStreakColor = {255, 40, 40, 255};
            m_killStreakDisplayTimer = 2.0f;
        } else if (m_killStreakCount >= 8) {
            m_killStreakText = "UNSTOPPABLE!";
            m_killStreakColor = {255, 160, 30, 255};
            m_killStreakDisplayTimer = 1.5f;
        } else if (m_killStreakCount >= 5) {
            m_killStreakText = "RAMPAGE!";
            m_killStreakColor = {255, 220, 50, 255};
            m_killStreakDisplayTimer = 1.5f;
        } else if (m_killStreakCount >= 3) {
            m_killStreakText = "TRIPLE KILL!";
            m_killStreakColor = {255, 255, 255, 255};
            m_killStreakDisplayTimer = 1.5f;
        }
    }

    // Relic on-kill effects
    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
        auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
        for (int k = 0; k < m_combatSystem.killCount; k++) {
            // SoulSiphon: heal on kill (VampiricFrenzy synergy: 8 HP under 30%)
            // Blocked by VampiricEdge (VampiricEdge is its own kill-heal source)
            float killHeal = RelicSystem::getKillHeal(relics);
            if (killHeal > 0 && !RelicSystem::hasVampiricEdge(relics)) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                float synergyHeal = RelicSynergy::getKillHealBonus(relics, playerHP.getPercent());
                playerHP.heal(synergyHeal > 0 ? synergyHeal : killHeal);
            }
            // VoidHunger: +1% DMG per kill
            RelicSystem::onEnemyKill(relics);
            // EntropyVortex synergy: kills reduce entropy
            float entropyReduce = RelicSynergy::getKillEntropyReduction(relics);
            if (entropyReduce > 0) {
                m_entropy.reduceEntropy(m_entropy.getMaxEntropy() * entropyReduce);
            }
            // VoidPact: heal 5 HP on kill (capped at 60% max HP)
            // Blocked by VampiricEdge
            float vpHeal = RelicSystem::getVoidPactHeal(relics);
            if (vpHeal > 0 && !RelicSystem::hasVampiricEdge(relics)) {
                auto& pHP = m_player->getEntity()->getComponent<HealthComponent>();
                float hpCap = pHP.maxHP * RelicSystem::getVoidPactMaxHPPercent(relics);
                if (pHP.currentHP < hpCap) {
                    pHP.heal(std::min(vpHeal, hpCap - pHP.currentHP));
                }
            }
            // EntropySponge: kills add entropy
            float killEntropy = RelicSystem::getKillEntropyGain(relics);
            if (killEntropy > 0) {
                m_entropy.addEntropy(killEntropy);
            }

            // BloodPact: -1 HP per kill (can't kill, floor at 1)
            float bloodPactCost = RelicSystem::getBloodPactKillHPCost(relics);
            if (bloodPactCost > 0) {
                auto& bpHP = m_player->getEntity()->getComponent<HealthComponent>();
                if (bpHP.currentHP > bloodPactCost) {
                    bpHP.currentHP -= bloodPactCost;
                }
            }

            // EntropySiphon: kills reduce entropy by 8
            float siphonReduce = RelicSystem::getEntropySiphonKillReduction(relics);
            if (siphonReduce > 0) {
                m_entropy.reduceEntropy(siphonReduce);
            }

            // VampiricEdge: heal 3 HP per kill (this is kill-heal, allowed even with VampiricEdge)
            float vampHeal = RelicSystem::getVampiricEdgeKillHeal(relics);
            if (vampHeal > 0) {
                auto& veHP = m_player->getEntity()->getComponent<HealthComponent>();
                veHP.heal(vampHeal);
            }

            // Weapon-Relic Synergies on kill
            auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
            auto& pHP = m_player->getEntity()->getComponent<HealthComponent>();

            // BloodRift: RiftBlade + BloodFrenzy -- melee kills under 50% HP heal 5
            float bloodRiftHeal = RelicSynergy::getBloodRiftHeal(
                relics, combat.currentMelee, pHP.getPercent());
            if (bloodRiftHeal > 0) {
                pHP.heal(bloodRiftHeal);
            }

            // EntropyBeam: VoidBeam + EntropyAnchor -- kills reduce entropy by 5%
            if (RelicSynergy::isEntropyBeamActive(relics, combat.currentRanged) &&
                combat.currentAttack == AttackType::Ranged) {
                m_entropy.reduceEntropy(m_entropy.getMaxEntropy() * 0.05f);
            }

            // StormScatter: RiftShotgun + ChainLightning -- kills chain to 2 extra enemies
            if (RelicSynergy::isStormScatterActive(relics, combat.currentRanged) &&
                combat.currentAttack == AttackType::Ranged) {
                float chainDmg = 15.0f;
                int chainsLeft = 2;
                auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
                Vec2 killPos = playerT.getCenter();
                int dim = m_dimManager.getCurrentDimension();
                m_entities.forEach([&](Entity& nearby) {
                    if (chainsLeft <= 0) return;
                    if (nearby.getTag().find("enemy") == std::string::npos) return;
                    if (!nearby.isAlive()) return;
                    if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                    if (nearby.dimension != 0 && nearby.dimension != dim) return;
                    auto& nt = nearby.getComponent<TransformComponent>();
                    float dx = nt.getCenter().x - killPos.x;
                    float dy = nt.getCenter().y - killPos.y;
                    if (dx * dx + dy * dy < 120.0f * 120.0f) {
                        nearby.getComponent<HealthComponent>().takeDamage(chainDmg);
                        chainsLeft--;
                        m_particles.burst(nt.getCenter(), 8, {255, 255, 80, 255}, 150.0f, 2.0f);
                    }
                });
                if (chainsLeft < 2) {
                    AudioManager::instance().play(SFX::ElectricChain);
                }
            }
        }
    }
}

void PlayState::updatePostCombat(float dt) {
    // Lore: Walker Scourge - 50+ kills
    if (enemiesKilled >= 50) {
        if (auto* lore = game->getLoreSystem()) {
            if (!lore->isDiscovered(LoreID::WalkerScourge)) {
                lore->discover(LoreID::WalkerScourge);
                AudioManager::instance().play(SFX::LoreDiscover);
            }
        }
    }

    // Lore: Void Hunger - survive entropy > 90%
    if (m_entropy.getEntropy() > 90.0f) {
        if (auto* lore = game->getLoreSystem()) {
            if (!lore->isDiscovered(LoreID::VoidHunger)) {
                lore->discover(LoreID::VoidHunger);
                AudioManager::instance().play(SFX::LoreDiscover);
            }
        }
    }

    if (m_combatSystem.killedMiniBoss) {
        game->getAchievements().unlock("mini_boss_hunter");
        m_combatSystem.killedMiniBoss = false;
    }
    if (m_combatSystem.killedElemental) {
        game->getAchievements().unlock("elemental_slayer");
        m_combatSystem.killedElemental = false;
    }

    // Echo of Self: reward when mirror enemy is defeated
    if (m_echoSpawned && !m_echoRewarded) {
        bool echoAlive = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_echo" && e.isAlive()) echoAlive = true;
        });
        if (!echoAlive) {
            m_echoRewarded = true;
            m_echoSpawned = false;
            // Reward scales with story stage (stage already incremented after fight)
            int echoStage = std::min(m_npcStoryProgress[static_cast<int>(NPCType::EchoOfSelf)] - 1, 2);
            echoStage = std::max(0, echoStage);
            int echoShards = 40 + echoStage * 25; // 40, 65, 90
            game->getUpgradeSystem().addRiftShards(echoShards);
            shardsCollected += echoShards;
            m_player->damageBoostTimer = 30.0f + echoStage * 15.0f;
            m_player->damageBoostMultiplier = 1.2f + echoStage * 0.1f;
            if (echoStage >= 2) {
                // Stage 2 bonus: heal to full
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.heal(hp.maxHP);
            }
            AudioManager::instance().play(SFX::LevelComplete);
            m_camera.shake(10.0f + echoStage * 5.0f, 0.4f);
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(pPos, 30 + echoStage * 10, {220, 120, 255, 255}, 250.0f + echoStage * 50.0f, 4.0f);
            m_particles.burst(pPos, 15 + echoStage * 8, {255, 215, 0, 255}, 180.0f + echoStage * 40.0f, 3.0f);
        }
    }

    // Skill achievement kill tracking + kill feed (before clear)
    {
        int rangedKillsThisFrame = 0;
        int chainKillsThisFrame = 0;
        for (auto& ke : m_combatSystem.killEvents) {
            if (ke.wasAerial) m_aerialKillsThisRun++;
            if (ke.wasDash) {
                m_dashKillsThisRun++;
                m_dashKillsThisRunForWeapon++;
            }
            if (ke.wasCharged) m_chargedKillsThisRun++;
            if (ke.wasRanged) rangedKillsThisFrame++;
            // AoE kills (slam = AoE)
            if (ke.wasSlam) chainKillsThisFrame++;
            addKillFeedEntry(ke);
        }
        // RiftShotgun unlock: 3+ kills from one ranged attack (e.g. shotgun spread or chain)
        int aoeThisFrame = rangedKillsThisFrame + chainKillsThisFrame;
        if (aoeThisFrame >= 3) {
            m_aoeKillCountThisRun = aoeThisFrame; // triggers unlock check
        }
    }
    updateKillFeed(dt);

    // Combat challenge tracking
    updateCombatChallenge(dt);
    m_combatSystem.killEvents.clear();

    // Event chain tracking
    updateEventChain(dt);

    m_combatSystem.killCount = 0;

    // Achievement checks (update(dt) already called above at line ~1056)
    auto& ach = game->getAchievements();

    if (roomsCleared >= 10) ach.unlock("room_clearer");
    if (roomsCleared >= 20) ach.unlock("survivor");
    if (m_currentDifficulty >= 5) ach.unlock("unstoppable");
    if (m_dashCount >= 100) ach.unlock("dash_master");
    if (m_dimManager.getSwitchCount() >= 50) ach.unlock("dimension_hopper");
    if (game->getUpgradeSystem().getRiftShards() >= 1000) ach.unlock("shard_hoarder");

    // Combo check
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        if (combat.comboCount > m_bestCombo) m_bestCombo = combat.comboCount;
        if (combat.comboCount >= 10) ach.unlock("combo_king");
        if (combat.comboCount >= 15) ach.unlock("combo_legend");
    }

    // Skill achievements
    if (m_aerialKillsThisRun >= 5) ach.unlock("aerial_ace");
    if (m_dashKillsThisRun >= 10) ach.unlock("dash_slayer");
    if (m_chargedKillsThisRun >= 3) ach.unlock("charged_fury");
    if (m_dimManager.getResonanceTier() >= 3) ach.unlock("resonance_master");

    // Full upgrade check
    for (auto& u : game->getUpgradeSystem().getUpgrades()) {
        if (u.currentLevel >= u.maxLevel) {
            ach.unlock("full_upgrade");
            break;
        }
    }

    // Meta-progression unlock checks (class + weapon unlocks)
    checkUnlockConditions();
    updateUnlockNotifications(dt);
}

// --- NPC Quest progress tracking ---

void PlayState::updateQuestProgress() {
    // Tick completion notification timer (runs even after quest is done)
    if (m_questCompleteTimer > 0) {
        m_questCompleteTimer -= 1.0f / 60.0f; // Approximate dt (called from fixed update)
    }

    if (!m_activeQuest.active || m_activeQuest.completed) return;

    // Track kills since quest was accepted
    if (m_activeQuest.targetKills > 0) {
        m_activeQuest.currentKills = enemiesKilled - m_questKillSnapshot;
        if (m_activeQuest.currentKills >= m_activeQuest.targetKills) {
            completeQuest();
        }
    }

    // Track rifts since quest was accepted
    if (m_activeQuest.targetRifts > 0) {
        m_activeQuest.currentRifts = riftsRepaired - m_questRiftSnapshot;
        if (m_activeQuest.currentRifts >= m_activeQuest.targetRifts) {
            completeQuest();
        }
    }
}

void PlayState::completeQuest() {
    if (!m_activeQuest.active) return;
    m_activeQuest.active = false;
    m_activeQuest.completed = true;

    // Grant shard reward
    if (m_activeQuest.shardReward > 0) {
        game->getUpgradeSystem().addRiftShards(m_activeQuest.shardReward);
        shardsCollected += m_activeQuest.shardReward;
    }

    // Grant entropy reduction (Lost Engineer quest)
    if (m_activeQuest.entropyReduction > 0) {
        m_entropy.reduceEntropy(m_activeQuest.entropyReduction);
    }

    // Visual/audio feedback
    AudioManager::instance().play(SFX::ShrineBlessing);
    if (m_player && m_player->getEntity()->hasComponent<TransformComponent>()) {
        Vec2 pos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_particles.burst(pos, 30, {255, 215, 80, 255}, 200.0f, 4.0f);
        m_particles.burst(pos, 15, {100, 255, 180, 255}, 150.0f, 3.0f);
    }
    m_camera.shake(6.0f, 0.25f);
    m_questCompleteTimer = 3.0f; // Show completion popup for 3 seconds

    // Helpful Traveler: complete 5 NPC quests total across one run
    m_questsCompletedTotal++;
    if (m_questsCompletedTotal >= 5) {
        game->getAchievements().unlock("quest_helper");
    }
}
