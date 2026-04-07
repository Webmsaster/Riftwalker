// PlayStateUpdateCombat.cpp -- Combat update helpers split from PlayStateUpdate.cpp
// Hazard damage, status effects, kill effects, post-combat, quest progress
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
#include "Game/Bestiary.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include "Game/WeaponSystem.h"
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <algorithm>

void PlayState::updateEnemyHazardDamage(float dt) {
    // Enemy hazard damage: enemies on spikes/fire/laser take damage
    int curDim = m_dimManager.getCurrentDimension();
    int ts = m_level->getTileSize();
    m_entities.forEach([&](Entity& e) {
        if (!e.isEnemy) return;
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
                m_usedMeleeThisFloor = true;
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

                // Death explosion particles (dramatic multi-layer burst)
                Vec2 deathPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                const auto& classColor = ClassSystem::getData(m_player->playerClass).color;
                // Core burst: class-colored (large, fast)
                m_particles.burst(deathPos, 55, {classColor.r, classColor.g, classColor.b, 255}, 380.0f, 7.0f);
                // Fire layer: red-orange embers
                m_particles.burst(deathPos, 35, {255, 60, 40, 255}, 280.0f, 5.5f);
                // Hot core: bright white-yellow sparks
                m_particles.burst(deathPos, 25, {255, 255, 200, 255}, 320.0f, 4.5f);
                // Smoke layer: dark particles that linger
                m_particles.burst(deathPos, 20, {60, 50, 50, 180}, 150.0f, 8.0f);
                // Expanding ring (denser, larger radius)
                for (int i = 0; i < 24; i++) {
                    float angle = i * 6.283185f / 24.0f;
                    float radius = 12.0f + static_cast<float>(std::rand() % 6);
                    Vec2 ringPos = {deathPos.x + std::cos(angle) * radius, deathPos.y + std::sin(angle) * radius};
                    m_particles.burst(ringPos, 3, {255, 100, 60, 220}, 220.0f, 3.5f);
                }
                // Secondary expanding ring (outer, fainter)
                for (int i = 0; i < 16; i++) {
                    float angle = (i + 0.5f) * 6.283185f / 16.0f;
                    Vec2 outerPos = {deathPos.x + std::cos(angle) * 20.0f, deathPos.y + std::sin(angle) * 20.0f};
                    m_particles.burst(outerPos, 2, {classColor.r, classColor.g, classColor.b, 150}, 160.0f, 4.0f);
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

    // Kill slow-motion: trigger on boss/mini-boss kills or multi-kills
    for (auto& ke : m_combatSystem.killEvents) {
        if (ke.wasBoss) {
            m_killSlowMoTimer = 0.4f; // Longer slow-mo for boss
            m_killSlowMoScale = 0.4f;
        } else if (ke.wasMiniBoss && m_killSlowMoTimer <= 0) {
            m_killSlowMoTimer = 0.3f;
            m_killSlowMoScale = 0.4f;
        }
    }
    // Multi-kill slow-mo (3+ kills in one frame)
    if (m_combatSystem.killCount >= 3 && m_killSlowMoTimer <= 0) {
        m_killSlowMoTimer = 0.25f;
        m_killSlowMoScale = 0.5f;
    }

    // Award XP per kill (base 15, elite/miniboss/boss give more)
    if (m_player) {
        for (auto& ke : m_combatSystem.killEvents) {
            int xpAward = 15;
            if (ke.wasElite)    xpAward = 30;
            if (ke.wasMiniBoss) xpAward = 50;
            if (ke.wasBoss)     xpAward = 120;
            m_player->addXP(xpAward);
        }
        // Check for level-up celebration
        if (m_player->levelUpPending) {
            m_player->levelUpPending = false;
            m_levelUpTimer = 1.5f;
            m_levelUpFlashTimer = 0.3f;
            m_levelUpDisplayLevel = m_player->level;
            // Gold particle burst around player
            Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().position;
            SDL_Color gold = {255, 215, 40, 255};
            m_particles.burst(playerPos, 30, gold, 140.0f, 3.5f);
            // Small screen shake
            m_camera.shake(3.0f, 0.2f);
            AudioManager::instance().play(SFX::LevelComplete);
        }
    }

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
            AudioManager::instance().play(SFX::ComboMilestone);
        } else if (m_killStreakCount >= 8) {
            m_killStreakText = "UNSTOPPABLE!";
            m_killStreakColor = {255, 160, 30, 255};
            m_killStreakDisplayTimer = 1.5f;
            AudioManager::instance().play(SFX::ComboMilestone);
        } else if (m_killStreakCount >= 5) {
            m_killStreakText = "RAMPAGE!";
            m_killStreakColor = {255, 220, 50, 255};
            m_killStreakDisplayTimer = 1.5f;
            AudioManager::instance().play(SFX::ComboMilestone);
        } else if (m_killStreakCount >= 3) {
            m_killStreakText = "TRIPLE KILL!";
            m_killStreakColor = {255, 255, 255, 255};
            m_killStreakDisplayTimer = 1.5f;
            AudioManager::instance().play(SFX::ComboMilestone);
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
                    if (!nearby.isEnemy) return;
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

    // Lore: new enemy type discoveries (first kill triggers)
    if (auto* lore = game->getLoreSystem()) {
        m_entities.forEach([&](Entity& e) {
            if (!e.isAlive() || !e.hasComponent<AIComponent>()) return;
            auto& ai = e.getComponent<AIComponent>();
            if (e.hasComponent<HealthComponent>() &&
                e.getComponent<HealthComponent>().currentHP <= 0) {
                if (ai.enemyType == EnemyType::Swarmer && !lore->isDiscovered(LoreID::SwarmNature)) {
                    lore->discover(LoreID::SwarmNature);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
                if (ai.enemyType == EnemyType::GravityWell && !lore->isDiscovered(LoreID::GravityAnomaly)) {
                    lore->discover(LoreID::GravityAnomaly);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
                if (ai.enemyType == EnemyType::Mimic && ai.mimicRevealed && !lore->isDiscovered(LoreID::MimicDeception)) {
                    lore->discover(LoreID::MimicDeception);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }
        });
    }

    // Lore: Rift Ecology — discover 5+ secret rooms
    if (m_level) {
        int discoveredSecrets = 0;
        for (const auto& sr : m_level->getSecretRooms())
            if (sr.discovered) discoveredSecrets++;
        if (discoveredSecrets >= 5) {
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::RiftEcology)) {
                    lore->discover(LoreID::RiftEcology);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
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
            if (e.isEnemyEcho && e.isAlive()) echoAlive = true;
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

    // Relic achievements
    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
        auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
        if (static_cast<int>(relics.relics.size()) >= 6) ach.unlock("relic_collector");
        if (RelicSynergy::getActiveSynergyCount(relics) >= 3) ach.unlock("synergy_hunter");
        // Count cursed relics
        int cursedCount = 0;
        for (auto& r : relics.relics) {
            if (RelicSystem::isCursed(r.id)) cursedCount++;
        }
        if (cursedCount >= 3) ach.unlock("cursed_survivor");
    }

    // Combat: kill count milestone
    if (enemiesKilled >= 50) ach.unlock("kill_streak_50");

    // Lore completion
    if (game->getLoreSystem() && game->getLoreSystem()->discoveredCount() >= 12) {
        ach.unlock("lore_hunter");
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

// =========================================================================
// Death ghost effects: shrink + fade + white flash after enemy death
// =========================================================================

void PlayState::updateDeathEffects(float dt) {
    for (int i = 0; i < m_deathEffectCount; ) {
        auto& de = m_deathEffects[i];
        de.timer += dt;
        if (de.timer >= de.maxLife) {
            // Remove by swap with last
            m_deathEffects[i] = m_deathEffects[--m_deathEffectCount];
        } else {
            i++;
        }
    }
}

void PlayState::renderDeathEffects(SDL_Renderer* renderer) {
    if (m_deathEffectCount <= 0) return;

    Vec2 cam = m_camera.getPosition();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < m_deathEffectCount; i++) {
        auto& de = m_deathEffects[i];
        float t = de.timer / de.maxLife; // 0 → 1

        // Phase 1 (0-15%): white flash, full size
        // Phase 2 (15-100%): shrink to 0, fade out
        float scale, alpha, whiteness;
        if (t < 0.15f) {
            float flashT = t / 0.15f;
            scale = 1.0f + 0.15f * (1.0f - flashT); // Slight expand then settle
            alpha = 1.0f;
            whiteness = 1.0f - flashT; // 1→0 (full white → normal color)
        } else {
            float shrinkT = (t - 0.15f) / 0.85f; // 0→1
            // Ease-in: accelerating shrink (slow start, fast end)
            float eased = shrinkT * shrinkT;
            scale = 1.0f - eased * 0.9f; // Shrink to 10% size
            alpha = 1.0f - eased;
            whiteness = 0;
        }

        if (alpha < 0.02f || scale < 0.05f) continue;

        float w = de.width * scale;
        float h = de.height * scale;
        float sx = de.position.x - w * 0.5f - cam.x;
        float sy = de.position.y - h * 0.5f - cam.y;

        // Cull off-screen
        if (sx + w < -20 || sx > SCREEN_WIDTH + 20 || sy + h < -20 || sy > SCREEN_HEIGHT + 20)
            continue;

        SDL_Rect dstRect = {
            static_cast<int>(sx), static_cast<int>(sy),
            std::max(1, static_cast<int>(w)), std::max(1, static_cast<int>(h))
        };

        Uint8 drawAlpha = static_cast<Uint8>(alpha * 255);

        if (de.texture) {
            // Render sprite with white flash + fade
            Uint8 r = static_cast<Uint8>(std::min(255, static_cast<int>(de.color.r + whiteness * (255 - de.color.r))));
            Uint8 g = static_cast<Uint8>(std::min(255, static_cast<int>(de.color.g + whiteness * (255 - de.color.g))));
            Uint8 b = static_cast<Uint8>(std::min(255, static_cast<int>(de.color.b + whiteness * (255 - de.color.b))));

            SDL_SetTextureColorMod(de.texture, r, g, b);
            SDL_SetTextureAlphaMod(de.texture, drawAlpha);
            SDL_SetTextureBlendMode(de.texture, SDL_BLENDMODE_BLEND);

            SDL_RendererFlip flip = de.flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderCopyEx(renderer, de.texture, &de.srcRect, &dstRect, 0.0, nullptr, flip);

            // Additive white glow overlay during flash phase
            if (whiteness > 0.2f) {
                SDL_SetTextureBlendMode(de.texture, SDL_BLENDMODE_ADD);
                Uint8 glowA = static_cast<Uint8>(whiteness * 180 * alpha);
                SDL_SetTextureColorMod(de.texture, 255, 255, 255);
                SDL_SetTextureAlphaMod(de.texture, glowA);
                SDL_RenderCopyEx(renderer, de.texture, &de.srcRect, &dstRect, 0.0, nullptr, flip);
                SDL_SetTextureBlendMode(de.texture, SDL_BLENDMODE_BLEND);
            }
        } else {
            // Fallback: colored rectangle with white flash
            Uint8 r = static_cast<Uint8>(std::min(255, static_cast<int>(de.color.r + whiteness * (255 - de.color.r))));
            Uint8 g = static_cast<Uint8>(std::min(255, static_cast<int>(de.color.g + whiteness * (255 - de.color.g))));
            Uint8 b = static_cast<Uint8>(std::min(255, static_cast<int>(de.color.b + whiteness * (255 - de.color.b))));
            SDL_SetRenderDrawColor(renderer, r, g, b, drawAlpha);
            SDL_RenderFillRect(renderer, &dstRect);
        }

        // Outer glow halo during death (expanding, fading)
        if (de.isBoss || de.isElite) {
            float glowRadius = w * (1.0f + t * 0.5f);
            Uint8 glowA = static_cast<Uint8>(alpha * 40);
            SDL_SetRenderDrawColor(renderer, de.color.r, de.color.g, de.color.b, glowA);
            SDL_Rect glow = {
                static_cast<int>(sx - (glowRadius - w) * 0.5f),
                static_cast<int>(sy - (glowRadius - h) * 0.5f),
                static_cast<int>(glowRadius),
                static_cast<int>(glowRadius * h / w)
            };
            SDL_RenderFillRect(renderer, &glow);
        }
    }
}

// =========================================================================
// Foreground fog particles: atmospheric depth layer
// =========================================================================

void PlayState::initFogParticles() {
    for (int i = 0; i < MAX_FOG_PARTICLES; i++) {
        auto& f = m_fogParticles[i];
        f.x = static_cast<float>(std::rand() % SCREEN_WIDTH);
        f.y = static_cast<float>(std::rand() % SCREEN_HEIGHT);
        f.vx = 8.0f + static_cast<float>(std::rand() % 20); // Slow drift right
        if (std::rand() % 2) f.vx = -f.vx; // Some drift left
        f.width = 240.0f + static_cast<float>(std::rand() % 360);
        f.height = 60.0f + static_cast<float>(std::rand() % 100);
        f.alpha = 0.02f + static_cast<float>(std::rand() % 30) / 1000.0f; // Very subtle: 0.02-0.05
        f.alphaPhase = static_cast<float>(std::rand() % 628) / 100.0f; // Random phase
    }
    m_fogInitialized = true;
}

void PlayState::updateFogParticles(float dt) {
    if (!m_fogInitialized) initFogParticles();
    for (int i = 0; i < MAX_FOG_PARTICLES; i++) {
        auto& f = m_fogParticles[i];
        f.x += f.vx * dt;
        f.alphaPhase += dt * 0.5f;
        // Wrap around screen
        if (f.x > SCREEN_WIDTH + f.width) f.x = -f.width;
        if (f.x < -f.width) f.x = SCREEN_WIDTH + f.width * 0.5f;
    }
}

void PlayState::renderFogParticles(SDL_Renderer* renderer) {
    if (!m_fogInitialized) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int dim = m_dimManager.getCurrentDimension();
    // Fog color: cool blue-gray for dim A, warm amber-gray for dim B
    Uint8 fogR = (dim == 1) ? 140 : 180;
    Uint8 fogG = (dim == 1) ? 150 : 160;
    Uint8 fogB = (dim == 1) ? 180 : 140;

    for (int i = 0; i < MAX_FOG_PARTICLES; i++) {
        auto& f = m_fogParticles[i];
        float pulse = 0.7f + 0.3f * std::sin(f.alphaPhase);
        Uint8 a = static_cast<Uint8>(f.alpha * pulse * 255);
        if (a < 2) continue;

        SDL_SetRenderDrawColor(renderer, fogR, fogG, fogB, a);

        // Draw multiple overlapping rects for soft edges
        int ix = static_cast<int>(f.x);
        int iy = static_cast<int>(f.y);
        int iw = static_cast<int>(f.width);
        int ih = static_cast<int>(f.height);

        // Core
        SDL_Rect core = {ix, iy, iw, ih};
        SDL_RenderFillRect(renderer, &core);

        // Soft edge (larger, dimmer)
        SDL_SetRenderDrawColor(renderer, fogR, fogG, fogB, static_cast<Uint8>(a / 3));
        SDL_Rect outer = {ix - iw / 4, iy - ih / 3, iw + iw / 2, ih + ih * 2 / 3};
        SDL_RenderFillRect(renderer, &outer);
    }
}
