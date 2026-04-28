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
#include <vector>

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
            if (e.isBoss && e.hasComponent<AIComponent>()) {
                auto& bossAi = e.getComponent<AIComponent>();
                if (bossAi.bossType == 4 && bossAi.vsDimLockActive > 0) dimLocked = true;
                if (bossAi.bossType == 5 && bossAi.eiDimLockActive > 0) dimLocked = true;
            }
        });
        if (dimLocked) {
            m_camera.shake(3.0f, 0.1f);
            AudioManager::instance().play(SFX::RiftFail); // Audio feedback: can't switch
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
            // Visceral feedback: subtle camera shake on the switch itself
            m_camera.shake(4.0f, 0.15f);
            // Expanding particle ring from player center at switch moment
            if (m_player) {
                Vec2 dpos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                SDL_Color dimColor = (m_dimManager.getCurrentDimension() == 1)
                    ? SDL_Color{100, 180, 255, 255}
                    : SDL_Color{220, 100, 180, 255};
                for (int i = 0; i < 12; i++) {
                    float angle = i * 30.0f;
                    m_particles.directionalBurst(dpos, 3, dimColor, angle, 20.0f, 250.0f, 3.0f);
                }
            }

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
                        if (e.isDimResidue && e.isAlive()) zoneCount++;
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
    // Pickup magnet: shards within ~150px accelerate toward the player so
    // post-fight cleanup isn't fiddly. Distance check uses squared distance
    // to avoid sqrt; pull strength tapers with distance for a soft fall-off.
    // Gravity is toggled per-frame so a shard that briefly entered the magnet
    // zone and then left does NOT lose gravity permanently (regression catch).
    if (m_player && m_player->getEntity()) {
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        constexpr float kMagnetRange = 150.0f;
        constexpr float kMagnetRangeSq = kMagnetRange * kMagnetRange;
        constexpr float kMagnetStrength = 600.0f; // px/s^2 toward player at edge
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() != "pickup_shard") return;
            if (!e.hasComponent<TransformComponent>() ||
                !e.hasComponent<PhysicsBody>()) return;
            // Only pull shards in the player's dimension (or dim-0 'both')
            if (e.dimension != 0 &&
                e.dimension != m_dimManager.getCurrentDimension()) return;
            auto& sphys = e.getComponent<PhysicsBody>();
            auto& tt = e.getComponent<TransformComponent>();
            Vec2 sp = tt.getCenter();
            float dx = pPos.x - sp.x;
            float dy = pPos.y - sp.y;
            float d2 = dx * dx + dy * dy;
            if (d2 > kMagnetRangeSq || d2 < 1.0f) {
                // Out of magnet range: ensure gravity is restored (shard may
                // have entered/exited the zone earlier in the run).
                sphys.useGravity = true;
                return;
            }
            float d = std::sqrt(d2);
            float t = 1.0f - (d / kMagnetRange); // 1 close, 0 at edge
            float pull = kMagnetStrength * t * dt;
            sphys.velocity.x += (dx / d) * pull;
            sphys.velocity.y += (dy / d) * pull;
            // Disable gravity inside magnet zone so the pull dominates and
            // shards don't sink while being attracted across uneven terrain.
            sphys.useGravity = false;
            // Visual trail: rare purple sparkle behind the shard so the magnet
            // effect reads as "pulled". Throttled by the squared distance so
            // close shards spawn fewer particles (avoids noise near pickup).
            if ((std::rand() & 0x1F) == 0 && t > 0.15f) {
                Vec2 tail = {sp.x - (dx / d) * 6.0f,
                             sp.y - (dy / d) * 6.0f};
                Uint8 tailA = static_cast<Uint8>(120 + 80 * t);
                m_particles.burst(tail, 1,
                    {180, 130, 255, tailA}, 18.0f, 1.5f);
            }
        });
    }

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
                // Tactile heartbeat: soft rumble pulse on each beat. Scales
                // with urgency (subtle at 25% -> firm near 0%). Brief 80ms
                // pulse so it feels like a heartbeat, not constant rumble.
                float rumbleStrength = 0.10f + urgency * 0.30f; // 0.10..0.40
                game->getInputMutable().rumble(rumbleStrength, 80);
            }
        } else {
            m_heartbeatTimer = 0; // Reset so it plays immediately when entering low HP
        }
    }
    m_screenEffects.setEntropy(m_entropy.getEntropy());

    // Combined boss/enemy scan (storm flags + music + nearEnemies in one forEach)
    bool stormActive = false;
    int nearEnemies = 0;
    bool bossActive = false;
    int bossPhase = 1;
    Vec2 pPos = m_player ? m_player->getEntity()->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
    m_entities.forEach([&](Entity& e) {
        if (e.isEnemy) {
            if (e.isBoss && e.hasComponent<AIComponent>()) {
                bossActive = true;
                auto& bossAi = e.getComponent<AIComponent>();
                bossPhase = bossAi.bossPhase;
                if (bossAi.bossType == 4 && bossAi.vsStormActive > 0) stormActive = true;
                if (bossAi.bossType == 5 && bossAi.eiStormActive > 0) stormActive = true;
            }
            if (!e.hasComponent<TransformComponent>()) return;
            auto& et = e.getComponent<TransformComponent>();
            float dx = et.getCenter().x - pPos.x;
            float dy = et.getCenter().y - pPos.y;
            if (dx * dx + dy * dy < 400.0f * 400.0f) nearEnemies++;
        }
    });
    m_screenEffects.setVoidStorm(stormActive);
    m_screenEffects.update(dt);

    // Music system
    {
        float hp = m_player ? m_player->getEntity()->getComponent<HealthComponent>().getPercent() : 1.0f;
        m_musicSystem.update(dt, nearEnemies, bossActive, hp, m_entropy.getEntropy());
        AudioManager::instance().updateMusicLayers(m_musicSystem);

        // Procedural music: boss track switching and phase changes.
        // setBossActive early-exits if bossActive is unchanged, so we also call
        // setBossPhase explicitly to catch mid-fight phase escalation.
        m_musicSystem.setBossActive(bossActive, bossPhase);
        if (bossActive) {
            m_musicSystem.setBossPhase(bossPhase);
        }
    }
}

void PlayState::updatePlayerPhysicsEffects(float dt) {
    // Landing effects: camera shake + dust proportional to fall distance & speed
    {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        if (phys.onGround && !phys.wasOnGround && phys.landingImpactSpeed > 250.0f) {
            auto& tr = m_player->getEntity()->getComponent<TransformComponent>();
            float fallDist = tr.position.y - m_player->fallStartY;
            float t = std::min((phys.landingImpactSpeed - 250.0f) / 550.0f, 1.0f);

            // Camera shake gated by fall distance: <200px none, 200-400 subtle, >400 strong
            if (fallDist > 200.0f) {
                float distFactor = std::min((fallDist - 200.0f) / 200.0f, 1.0f); // 0-1 over 200-400px
                float shakeAmt = 2.0f + distFactor * 3.0f;  // 2-5px
                float shakeDur = 0.10f + distFactor * 0.05f; // 0.10-0.15s
                m_camera.shake(shakeAmt, shakeDur);
            }
            game->getInputMutable().rumble(0.2f + t * 0.4f, 80 + static_cast<int>(t * 120));

            // Landing squash: sprite goes wide+short on impact
            auto& spr = m_player->getEntity()->getComponent<SpriteComponent>();
            spr.landingSquashTimer = 0.15f;
            spr.landingSquashIntensity = 0.1f + t * 0.15f; // 0.1-0.25 based on fall speed

            // Landing dust cloud at feet -- bigger for harder landings
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
        if (!e.isEnemy) return;
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

    // Pre-build alive-enemy list once so the turret loop is O(turrets * enemies)
    // instead of O(turrets * ALL entities). Reusable static buffer to avoid alloc.
    struct EnemyCacheEntry { Entity* e; Vec2 center; };
    static thread_local std::vector<EnemyCacheEntry> s_enemyCache;
    s_enemyCache.clear();
    m_entities.forEach([&](Entity& e) {
        if (!e.isEnemy) return;
        if (!e.isAlive() || !e.hasComponent<TransformComponent>() || !e.hasComponent<HealthComponent>()) return;
        if (e.dimension != 0 && e.dimension != curDim) return;
        if (e.getComponent<HealthComponent>().currentHP <= 0) return;
        s_enemyCache.push_back({&e, e.getComponent<TransformComponent>().getCenter()});
    });

    // Combined turret + trap pass: single entity scan handles both player-owned
    // entity types. Trap branch decays lifetime & counts; turret branch adds
    // targeting + auto-fire on top of the same decay logic.
    int trapCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (!e.isAlive()) return;
        const bool isTurret = e.isPlayerTurret;
        const bool isTrap   = e.isPlayerTrap;
        if (!isTurret && !isTrap) return;
        if (!e.hasComponent<HealthComponent>()) return;

        auto& hp = e.getComponent<HealthComponent>();
        hp.currentHP -= dt; // decay lifetime (shared by turret + trap)
        if (hp.currentHP <= 0) {
            auto& tt = e.getComponent<TransformComponent>();
            if (isTurret) {
                m_particles.burst(tt.getCenter(), 8, {230, 180, 50, 150}, 60.0f, 2.0f);
            } else {
                m_particles.burst(tt.getCenter(), 6, {255, 200, 50, 120}, 40.0f, 1.5f);
            }
            e.destroy();
            return;
        }
        if (isTrap) {
            trapCount++;
            return;
        }

        // Turret-only: targeting + auto-fire.
        turretCount++;
        if (!e.hasComponent<AIComponent>() || !e.hasComponent<CombatComponent>()) return;
        auto& ai = e.getComponent<AIComponent>();
        auto& tt = e.getComponent<TransformComponent>();
        Vec2 turretPos = tt.getCenter();

        // Find nearest enemy in range using the cached list (squared comparison)
        Entity* nearestEnemy = nullptr;
        float nearestDistSq = ai.detectRange * ai.detectRange;
        for (auto& ec : s_enemyCache) {
            float dx = ec.center.x - turretPos.x;
            float dy = ec.center.y - turretPos.y;
            float distSq = dx * dx + dy * dy;
            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearestEnemy = ec.e;
            }
        }

        // Auto-fire at nearest enemy
        ai.attackTimer -= dt;
        if (nearestEnemy && ai.attackTimer <= 0) {
            auto& combat = e.getComponent<CombatComponent>();
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
            if (e.hasComponent<SpriteComponent>()) {
                e.getComponent<SpriteComponent>().flipX = (dir.x < 0);
            }
        }
    });
    m_player->activeTurrets = turretCount;
    m_player->activeTraps = trapCount;
}

void PlayState::updateBossEffects(float dt) {
    // Combined boss-effects pass: VoidSovereign (type 4) + EntropyIncarnate (type 5).
    // Single forEach over all entities; per-entity isBoss / isEntropyMinion
    // flags are the cheap gates. Boss + entropy-minion-death scans were two
    // walks; merged into one with branch on flag.
    m_entities.forEach([&](Entity& e) {
        if (e.isEntropyMinion) {
            if (!e.hasComponent<HealthComponent>()) return;
            auto& mhp = e.getComponent<HealthComponent>();
            if (mhp.currentHP <= 0 && e.isAlive()) {
                m_entropy.addEntropy(10.0f);
                if (e.hasComponent<TransformComponent>()) {
                    Vec2 mPos = e.getComponent<TransformComponent>().getCenter();
                    m_particles.burst(mPos, 20, {140, 0, 200, 255}, 150.0f, 4.0f);
                }
            }
            return;
        }
        if (!e.isBoss) return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& bossAi = e.getComponent<AIComponent>();

        // VoidSovereign Phase 3: auto dimension switch
        if (bossAi.bossType == 4 && bossAi.vsForceDimSwitch) {
            bossAi.vsForceDimSwitch = false;
            m_dimManager.switchDimension(true);
            m_camera.shake(6.0f, 0.2f);
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
            m_screenEffects.triggerDimensionRipple();
        }

        if (bossAi.bossType != 5) return;

        // Entropy Pulse: add entropy when pulse just fired
        if (bossAi.eiPulseFired) {
            bossAi.eiPulseFired = false;
            float entropyAdd = (bossAi.bossPhase >= 3) ? 20.0f : 15.0f;
            if (m_player && e.hasComponent<TransformComponent>()) {
                auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
                Vec2 bossPos = e.getComponent<TransformComponent>().getCenter();
                Vec2 pPos = playerT.getCenter();
                float ddx = pPos.x - bossPos.x;
                float ddy = pPos.y - bossPos.y;
                if (ddx * ddx + ddy * ddy < 200.0f * 200.0f) {
                    m_entropy.addEntropy(entropyAdd);
                }
            }
        }

        // Entropy Storm: continuous entropy drain while player in range
        if (bossAi.eiStormActive > 0 && m_player) {
            auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 bossPos = e.getComponent<TransformComponent>().getCenter();
            Vec2 pPos = playerT.getCenter();
            float ddx = pPos.x - bossPos.x;
            float ddy = pPos.y - bossPos.y;
            if (ddx * ddx + ddy * ddy < 150.0f * 150.0f) {
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

        // Entropy Overload: if player entropy > 70, boss heals 5% max HP/s.
        // BossEntropyIncarnate increments eiOverloadHealAccum unconditionally
        // every frame at phase>=3 (it doesn't know the player's entropy).
        // Without the else-branch reset below, a long phase-3 fight below
        // threshold would accumulate pending heal ticks that all burst out
        // the moment entropy briefly crosses 70 — could fully reset the kill.
        if (bossAi.bossPhase >= 3) {
            if (m_entropy.getEntropy() > 70.0f) {
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
            } else {
                // Below threshold: discard any pending heal ticks.
                bossAi.eiOverloadHealAccum = 0;
            }
        }

        // Final Stand: massive +30 entropy applied once on trigger
        if (bossAi.eiFinalStandTriggered && !bossAi.eiFinalStandEntropyApplied) {
            bossAi.eiFinalStandEntropyApplied = true;
            m_entropy.addEntropy(30.0f);
        }
    });
}

