// PlayStateCombatChallenge.cpp -- Split from PlayState.cpp (combat challenges, rifts, balance tracking, waves)
#include "PlayState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
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

extern bool g_smokeRegression;
extern int g_smokeCompletedFloor;
extern void smokeLog(const char* fmt, ...);

void PlayState::checkRiftInteraction() {
    if (!m_player || !m_level) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto rifts = m_level->getRiftPositions();
    int currentDim = m_dimManager.getCurrentDimension();

    m_nearRiftIndex = -1;
    float nearestRiftDistSq = 60.0f * 60.0f;
    for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
        // FIX: Skip already-repaired rifts
        if (m_repairedRiftIndices.count(i)) continue;
        float dx = playerPos.x - rifts[i].x;
        float dy = playerPos.y - rifts[i].y;
        float distSq = dx * dx + dy * dy;
        if (distSq < nearestRiftDistSq) {
            nearestRiftDistSq = distSq;
            m_nearRiftIndex = i;
        }
    }

    if (m_nearRiftIndex >= 0 && game->getInput().isActionPressed(Action::Interact)) {
        if (m_smokeTest) {
            const auto& topology = m_level->getTopology();
            if (m_nearRiftIndex < static_cast<int>(topology.rifts.size())) {
                const auto& topoRift = topology.rifts[m_nearRiftIndex];
                smokeLog("[SMOKE] Rift attempt idx=%d room=%d tile=(%d,%d) dims=%d%d requiredDim=%d validated=%d genValidated=%d pos=(%.0f,%.0f) dim=%d",
                         m_nearRiftIndex,
                         topoRift.roomIndex,
                         topoRift.tileX,
                         topoRift.tileY,
                         topoRift.accessibleDimensions[1] ? 1 : 0,
                         topoRift.accessibleDimensions[2] ? 1 : 0,
                         topoRift.requiredDimension,
                         topoRift.validated ? 1 : 0,
                         topology.validation.passed ? 1 : 0,
                         playerPos.x,
                         playerPos.y,
                         currentDim);
            }
        }

        int requiredDim = m_level->getRiftRequiredDimension(m_nearRiftIndex);
        if (requiredDim != 0 && requiredDim != currentDim) {
            m_riftDimensionHintTimer = 2.0f;
            m_riftDimensionHintRequiredDim = requiredDim;
            SDL_Color hintColor = (requiredDim == 2)
                ? SDL_Color{255, 90, 145, 255}
                : SDL_Color{90, 180, 255, 255};
            m_particles.burst(rifts[m_nearRiftIndex], 10, hintColor, 80.0f, 1.4f);
            if (m_smokeTest) {
                smokeLog("[SMOKE] Rift blocked idx=%d currentDim=%d requiredDim=%d",
                         m_nearRiftIndex,
                         currentDim,
                         requiredDim);
            }
            return;
        }

        // Puzzle type progression: early = Timing (easiest), mid = Sequence, late = Alignment/Pattern
        int puzzleType;
        int pzZone = getZone(m_currentDifficulty);
        if (pzZone == 0 && getFloorInZone(m_currentDifficulty) <= 2) {
            puzzleType = 0; // Always Timing for earliest levels
        } else if (pzZone == 0) {
            puzzleType = (riftsRepaired % 2 == 0) ? 0 : 1; // Mix Timing and Sequence
        } else {
            int roll = std::rand() % 100;
            if (roll < 15) puzzleType = 0;       // 15% Timing
            else if (roll < 40) puzzleType = 1;  // 25% Sequence
            else if (roll < 70) puzzleType = 2;  // 30% Alignment
            else puzzleType = 3;                 // 30% Pattern
        }
        m_activePuzzle = std::make_unique<RiftPuzzle>(
            static_cast<PuzzleType>(puzzleType), m_currentDifficulty);

        int riftIdx = m_nearRiftIndex; // FIX: capture rift index for lambda
        m_activePuzzle->onComplete = [this, riftIdx](bool success) {
            if (success) {
                AudioManager::instance().play(SFX::RiftRepair);
                int requiredDim = m_level ? m_level->getRiftRequiredDimension(riftIdx) : 0;
                const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
                riftsRepaired++;
                // FIX: Mark this rift as repaired and track per-level count
                m_repairedRiftIndices.insert(riftIdx);
                m_levelRiftsRepaired++;
                m_entropy.onRiftRepaired();
                if (requiredDim == 2) {
                    m_entropy.reduceEntropy(shiftBalance.dimBEntropyRepairBonus);
                }
                game->getAchievements().unlock("rift_walker");
                float riftShardMult = game->getRunBuffSystem().getShardMultiplier();
                riftShardMult *= m_achievementShardMult;
                if (m_player && m_player->getEntity()->hasComponent<RelicComponent>())
                    riftShardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
                int riftShards = static_cast<int>((10 + m_currentDifficulty * 5) * riftShardMult);
                if (requiredDim == 2) {
                    riftShards = static_cast<int>(riftShards * shiftBalance.dimBShardMultiplier + 0.5f);
                }
                shardsCollected += riftShards;
                game->getUpgradeSystem().addRiftShards(riftShards);
                game->getUpgradeSystem().totalRiftsRepaired++;

                SDL_Color repairColor = (requiredDim == 2)
                    ? SDL_Color{255, 90, 145, 255}
                    : SDL_Color{90, 180, 255, 255};
                m_particles.burst(m_dimManager.playerPos, 40,
                    repairColor, 250.0f, 6.0f);
                m_camera.shake(5.0f, 0.3f);

                // Start collapse after all rifts in THIS level repaired
                // (skip if already collapsing, e.g. boss kill already triggered it)
                int totalRifts = static_cast<int>(m_level->getRiftPositions().size());
                if (m_smokeTest) {
                    smokeLog("[SMOKE] Rift repaired idx=%d repaired=%d/%d collapsing=%d",
                             riftIdx,
                             m_levelRiftsRepaired,
                             totalRifts,
                             m_collapsing ? 1 : 0);
                }
                if (m_levelRiftsRepaired >= totalRifts && !m_collapsing) {
                    m_collapsing = true;
                    m_collapseTimer = 0;
                    m_level->setExitActive(true);
                    // Dramatic collapse start: the rift is sealed, escape now!
                    m_camera.shake(15.0f, 0.5f);
                    m_camera.flash(0.3f, 255, 255, 255);
                    m_particles.burst(m_dimManager.playerPos, 60,
                        {255, 200, 255, 255}, 300.0f, 4.0f);
                    AudioManager::instance().play(SFX::BossShieldBurst);
                    m_combatSystem.addHitFreeze(0.15f);
                }
            } else {
                AudioManager::instance().play(SFX::RiftFail);
                m_entropy.addEntropy(10.0f);
                m_camera.shake(3.0f, 0.2f);
                if (m_smokeTest) {
                    smokeLog("[SMOKE] Rift puzzle failed idx=%d", riftIdx);
                }
            }
        };

        m_activePuzzle->activate();
    }
}

void PlayState::checkExitReached() {
    if (!m_player || !m_level || m_levelComplete) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    Vec2 exitPos = m_level->getExitPoint();
    float dx = playerPos.x - exitPos.x;
    float dy = playerPos.y - exitPos.y;

    if (dx * dx + dy * dy < 50.0f * 50.0f) {
        // FIX: Exit only works after all rifts are repaired (collapse started).
        // This enforces the core game loop: repair rifts -> collapse -> escape.
        if (!m_collapsing) {
            // Show hint to player that exit is locked
            m_exitLockedHintTimer = 2.0f;
            AudioManager::instance().play(SFX::RiftFail);
            m_camera.shake(2.0f, 0.1f);
            return;
        }
        m_levelComplete = true;
        m_levelCompleteTimer = 0;
        // Persist bestiary progress on level exit
        Bestiary::save("bestiary_save.dat");
        // Flawless Floor: completed level without taking damage
        if (!m_tookDamageThisLevel) {
            game->getAchievements().unlock("flawless_floor");
        }
        // Entropy Master: completed floor using only Entropy Scythe (no other melee)
        if (m_hasAttackedThisRun && !m_usedNonScytheMelee) {
            game->getAchievements().unlock("entropy_master");
        }
        // Ranged Only: complete 5 floors without using melee attacks
        if (!m_usedMeleeThisFloor) {
            m_rangedOnlyFloors++;
            if (m_rangedOnlyFloors >= 5) {
                game->getAchievements().unlock("ranged_only");
            }
        }
        if (m_smokeTest) {
            smokeLog("[SMOKE] LEVEL %d COMPLETE! (%.1fs)", m_currentDifficulty, m_smokeRunTime);
            if (g_smokeRegression) {
                g_smokeCompletedFloor = std::max(g_smokeCompletedFloor, m_currentDifficulty);
            }
        }
        AudioManager::instance().play(SFX::LevelComplete);
        m_camera.shake(10.0f, 0.3f);
        m_combatSystem.addHitFreeze(0.1f);
        // Celebration particles at player position
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_particles.burst(pPos, 40, {140, 255, 180, 255}, 250.0f, 5.0f);
        m_particles.burst(pPos, 20, {255, 215, 100, 255}, 180.0f, 3.0f);
        float shardMult = (g_selectedDifficulty == GameDifficulty::Easy) ? 1.5f :
                          (g_selectedDifficulty == GameDifficulty::Hard) ? 0.75f : 1.0f;
        shardMult *= game->getRunBuffSystem().getShardMultiplier();
        shardMult *= m_achievementShardMult;
        if (m_player->getEntity()->hasComponent<RelicComponent>())
            shardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
        int shards = static_cast<int>((20 + m_currentDifficulty * 10) * shardMult);
        shardsCollected += shards;
        game->getUpgradeSystem().addRiftShards(shards);
    }
}

void PlayState::handlePuzzleInput(int action) {
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->handleInput(action);
    }
}




void PlayState::updateBalanceTracking(float dt) {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    bool gameplayActive = !m_showRelicChoice && !m_showNPCDialog
        && !(m_activePuzzle && m_activePuzzle->isActive());
    if (gameplayActive) m_balanceStats.activePlayTime += dt;

    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // Raw DMG multiplier (mirrors debug overlay logic)
    float rawDmg = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::BloodFrenzy && hpPct < 0.3f) rawDmg += 0.25f;
        if (r.id == RelicID::BerserkerCore) rawDmg += 0.50f;
        if (r.id == RelicID::VoidHunger) rawDmg += relics.voidHungerBonus;
        if (r.id == RelicID::DualityGem && (curDim == 1 || RelicSynergy::isDualNatureActive(relics)))
            rawDmg += 0.25f;
        if (r.id == RelicID::StabilityMatrix) {
            float rate = RelicSynergy::getStabilityDmgPerSec(relics);
            float maxB = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
            rawDmg += std::min(relics.stabilityTimer * rate, maxB);
        }
    }
    rawDmg *= RelicSynergy::getPhaseHunterDamageMult(relics);
    float clampedDmg = RelicSystem::getDamageMultiplier(relics, hpPct, curDim);

    // Raw ATK speed multiplier
    float rawSpd = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) rawSpd += 0.15f;
    }
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0)
        rawSpd += relics.riftConduitStacks * 0.10f;
    float clampedSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    // Track peaks
    m_balanceStats.peakDmgRaw = std::max(m_balanceStats.peakDmgRaw, rawDmg);
    m_balanceStats.peakDmgClamped = std::max(m_balanceStats.peakDmgClamped, clampedDmg);
    m_balanceStats.peakSpdRaw = std::max(m_balanceStats.peakSpdRaw, rawSpd);
    m_balanceStats.peakSpdClamped = std::max(m_balanceStats.peakSpdClamped, clampedSpd);

    // CD floor tracking
    if (gameplayActive && relics.hasRelic(RelicID::RiftMantle)) {
        float baseCD = 0.5f;
        float cdMult = RelicSystem::getSwitchCooldownMult(relics);
        float finalCD = m_dimManager.switchCooldown;
        if ((baseCD * cdMult) < finalCD + 0.001f)
            m_balanceStats.cdFloorTime += dt;
    }

    // Residue zone count
    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.isDimResidue && e.isAlive()) zoneCount++;
    });
    m_balanceStats.peakResidueZones = std::max(m_balanceStats.peakResidueZones, zoneCount);

    // VoidHunger tracking
    m_balanceStats.peakVoidHunger = std::max(m_balanceStats.peakVoidHunger, relics.voidHungerBonus);
    m_balanceStats.finalVoidHunger = relics.voidHungerBonus;
}

void PlayState::writeBalanceSnapshot() {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    auto& upgrades = game->getUpgradeSystem();
    auto achBonus = game->getAchievements().getUnlockedBonuses();
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // --- Relic damage multiplier (clamped by RelicSystem) ---
    float relicDmg = RelicSystem::getDamageMultiplier(relics, hpPct, curDim);
    float relicAtkSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    // --- Upgrade multipliers ---
    float upgMelee = upgrades.getMeleeDamageMultiplier();
    float upgRanged = upgrades.getRangedDamageMultiplier();
    float upgMoveSpd = upgrades.getMoveSpeedMultiplier();

    // --- Achievement multipliers ---
    float achMelee = achBonus.meleeDamageMult * achBonus.allDamageMult;
    float achRanged = achBonus.rangedDamageMult * achBonus.allDamageMult;
    float achCrit = achBonus.critChanceBonus;

    // --- Class multipliers ---
    float classDmg = m_player->getClassDamageMultiplier();
    float classAtkSpd = m_player->getClassAttackSpeedMultiplier();

    // --- Resonance ---
    int resTier = m_dimManager.getResonanceTier();
    float resDmg = m_dimManager.getResonanceDamageMult();

    // --- Weapon mastery ---
    int meleeIdx = static_cast<int>(combat.currentMelee);
    int rangedIdx = static_cast<int>(combat.currentRanged);
    int meleeKills = (meleeIdx >= 0 && meleeIdx < static_cast<int>(WeaponID::COUNT)) ? m_combatSystem.weaponKills[meleeIdx] : 0;
    int rangedKills = (rangedIdx >= 0 && rangedIdx < static_cast<int>(WeaponID::COUNT)) ? m_combatSystem.weaponKills[rangedIdx] : 0;
    auto meleeBonus = WeaponSystem::getMasteryBonus(meleeKills);
    auto rangedBonus = WeaponSystem::getMasteryBonus(rangedKills);

    // --- Smith NPC bonuses ---
    float smithMelee = m_player->smithMeleeDmgMult;
    float smithRanged = m_player->smithRangedDmgMult;
    float smithAtkSpd = m_player->smithAtkSpdMult;

    // --- Combined effective multipliers ---
    float effectiveMelee = relicDmg * upgMelee * achMelee * classDmg * meleeBonus.damageMult * smithMelee * resDmg;
    float effectiveRanged = relicDmg * upgRanged * achRanged * classDmg * rangedBonus.damageMult * smithRanged * resDmg;
    float effectiveAtkSpd = relicAtkSpd * classAtkSpd * smithAtkSpd;

    // --- Switch cooldown ---
    float baseCD = 0.5f;
    float cdMult = RelicSystem::getSwitchCooldownMult(relics);
    float finalCD = m_dimManager.switchCooldown;

    // --- Relic-specific tracking ---
    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.isDimResidue && e.isAlive()) zoneCount++;
    });

    float stabRate = RelicSynergy::getStabilityDmgPerSec(relics);
    float stabMax = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
    float stabPct = std::min(relics.stabilityTimer * stabRate, stabMax) * 100.0f;
    int synergyCount = RelicSynergy::getActiveSynergyCount(relics);

    // --- Player state ---
    int momentum = m_player->momentumStacks;
    int kills = m_combatSystem.killCount;

    const char* className = "Unknown";
    switch (m_player->playerClass) {
        case PlayerClass::Voidwalker: className = "Voidwalker"; break;
        case PlayerClass::Berserker:  className = "Berserker"; break;
        case PlayerClass::Phantom:    className = "Phantom"; break;
        default: break;
    }

    // Write header if file doesn't exist or is empty
    bool needHeader = false;
    {
        FILE* check = std::fopen("balance_snapshots.csv", "r");
        if (!check) {
            needHeader = true;
        } else {
            std::fseek(check, 0, SEEK_END);
            if (std::ftell(check) == 0) needHeader = true;
            std::fclose(check);
        }
    }

    FILE* f = std::fopen("balance_snapshots.csv", "a");
    if (!f) return;

    if (needHeader) {
        std::fprintf(f, "seed,difficulty,class,time,floor,dim,kills,hp_pct,"
            "eff_melee,eff_ranged,eff_atkspd,"
            "relic_dmg,relic_atkspd,"
            "upg_melee,upg_ranged,upg_movespd,"
            "ach_melee,ach_ranged,ach_crit,"
            "class_dmg,class_atkspd,"
            "res_tier,res_dmg,"
            "mastery_melee_kills,mastery_melee_dmg,mastery_ranged_kills,mastery_ranged_dmg,"
            "smith_melee,smith_ranged,smith_atkspd,"
            "momentum_stacks,synergies,"
            "switch_base,switch_mult,switch_final,"
            "voidhunger_pct,stability_pct,"
            "conduit_stacks\n");
    }

    std::fprintf(f,
        "%d,%d,%s,%.1f,%d,%d,%d,%.1f,"
        "%.3f,%.3f,%.3f,"
        "%.3f,%.3f,"
        "%.3f,%.3f,%.3f,"
        "%.3f,%.3f,%.3f,"
        "%.3f,%.3f,"
        "%d,%.3f,"
        "%d,%.3f,%d,%.3f,"
        "%.3f,%.3f,%.3f,"
        "%d,%d,"
        "%.2f,%.2f,%.2f,"
        "%.1f,%.1f,"
        "%d\n",
        m_runSeed, m_currentDifficulty, className, m_runTime,
        m_currentDifficulty, curDim, kills, hpPct * 100.0f,
        effectiveMelee, effectiveRanged, effectiveAtkSpd,
        relicDmg, relicAtkSpd,
        upgMelee, upgRanged, upgMoveSpd,
        achMelee, achRanged, achCrit,
        classDmg, classAtkSpd,
        resTier, resDmg,
        meleeKills, meleeBonus.damageMult, rangedKills, rangedBonus.damageMult,
        smithMelee, smithRanged, smithAtkSpd,
        momentum, synergyCount,
        baseCD, cdMult, finalCD,
        relics.voidHungerBonus * 100.0f, stabPct,
        relics.riftConduitStacks);

    std::fclose(f);
}





void PlayState::updateDamageNumbers(float dt) {
    for (auto& dn : m_damageNumbers) {
        dn.lifetime -= dt;
        dn.position.y -= 60.0f * dt; // Float upward
    }
    // Destroy cached textures of expired numbers, then remove
    for (auto& d : m_damageNumbers) {
        if (d.lifetime <= 0) {
            if (d.cachedText)   { SDL_DestroyTexture(d.cachedText);   d.cachedText = nullptr; }
            if (d.cachedShadow) { SDL_DestroyTexture(d.cachedShadow); d.cachedShadow = nullptr; }
        }
    }
    m_damageNumbers.erase(
        std::remove_if(m_damageNumbers.begin(), m_damageNumbers.end(),
            [](const FloatingDamageNumber& d) { return d.lifetime <= 0; }),
        m_damageNumbers.end());
}

void PlayState::updateSpawnWaves(float dt) {
    if (m_currentWave >= static_cast<int>(m_spawnWaves.size())) return;

    // Count alive enemies
    int aliveEnemies = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.isEnemy) aliveEnemies++;
    });

    // NoDamageWave challenge: check BEFORE spawning next wave (otherwise new
    // enemies make aliveEnemies > 0 and the later check in updateCombatChallenge fails)
    if (aliveEnemies == 0 && enemiesKilled > 0 && !m_tookDamageThisWave &&
        m_combatChallenge.active && !m_combatChallenge.completed &&
        m_combatChallenge.type == CombatChallengeType::NoDamageWave) {
        m_combatChallenge.currentCount = 1;
    }

    // Spawn next wave when: most enemies dead OR timer expired
    m_waveTimer -= dt;
    if (aliveEnemies <= 1 || m_waveTimer <= 0) {
        // Spawn next wave
        // Zone-based elite chance in wave spawns
        auto waveZoneScale = getZoneScaling(m_currentDifficulty);
        int waveEliteChance = static_cast<int>(waveZoneScale.eliteChance);
        if (m_ngPlusTier >= 2) waveEliteChance = std::min(waveEliteChance + 15, 60);
        for (auto& sp : m_spawnWaves[m_currentWave]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            // Theme-specific variant
            applyThemeVariant(e, sp.dimension);
            // NG+ scaling on wave enemies
            applyNGPlusModifiers(e);
            if (getZone(m_currentDifficulty) >= 1 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && e.getComponent<AIComponent>().element == EnemyElement::None
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Zone-based elite modifier in wave spawns (12 modifiers; %12 covers all).
            if (getZone(m_currentDifficulty) >= 1 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < waveEliteChance) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 12);
                Enemy::makeElite(e, mod);
            }
        }
        m_currentWave++;
        m_waveTimer = m_waveDelay;
        m_tookDamageThisWave = false;

        // Start new combat challenge if previous one completed or inactive
        if (!m_combatChallenge.active) {
            startCombatChallenge();
        }

        // Warning SFX for new wave
        AudioManager::instance().play(SFX::CollapseWarning);
    }
}


// ============ Combat Challenges ============

void PlayState::startCombatChallenge() {
    // Pick a random challenge type
    int typeCount = static_cast<int>(CombatChallengeType::COUNT);
    auto type = static_cast<CombatChallengeType>(std::rand() % typeCount);

    m_combatChallenge = {};
    m_combatChallenge.type = type;
    m_combatChallenge.active = true;
    m_tookDamageThisWave = false;

    switch (type) {
        case CombatChallengeType::AerialKill:
            m_combatChallenge.name = LOC("cc.aerial.name");
            m_combatChallenge.desc = LOC("cc.aerial.desc");
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 20;
            break;
        case CombatChallengeType::MultiKill:
            m_combatChallenge.name = LOC("cc.multi.name");
            m_combatChallenge.desc = LOC("cc.multi.desc");
            m_combatChallenge.targetCount = 3;
            m_combatChallenge.timer = 4.0f;
            m_combatChallenge.maxTimer = 4.0f;
            m_combatChallenge.shardReward = 35;
            break;
        case CombatChallengeType::DashKill:
            m_combatChallenge.name = LOC("cc.dash.name");
            m_combatChallenge.desc = LOC("cc.dash.desc");
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 20;
            break;
        case CombatChallengeType::ComboFinisher:
            m_combatChallenge.name = LOC("cc.combo.name");
            m_combatChallenge.desc = LOC("cc.combo.desc");
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 25;
            break;
        case CombatChallengeType::ChargedKill:
            m_combatChallenge.name = LOC("cc.charged.name");
            m_combatChallenge.desc = LOC("cc.charged.desc");
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 25;
            break;
        case CombatChallengeType::NoDamageWave:
            m_combatChallenge.name = LOC("cc.nodmg.name");
            m_combatChallenge.desc = LOC("cc.nodmg.desc");
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 40;
            break;
        default: break;
    }
}

void PlayState::updateCombatChallenge(float dt) {
    // Completion popup timer
    if (m_challengeCompleteTimer > 0) {
        m_challengeCompleteTimer -= dt;
    }

    if (!m_combatChallenge.active || m_combatChallenge.completed) return;

    // Note: m_tookDamageThisWave is set directly when player takes damage
    // (combat hits, hazards, DOTs) — no framerate-dependent checks needed

    // Process kill events from CombatSystem
    for (auto& ke : m_combatSystem.killEvents) {
        switch (m_combatChallenge.type) {
            case CombatChallengeType::AerialKill:
                if (ke.wasAerial) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::MultiKill:
                m_combatChallenge.currentCount++;
                // Reset timer on first kill of the sequence
                if (m_combatChallenge.currentCount == 1) {
                    m_combatChallenge.timer = m_combatChallenge.maxTimer;
                }
                break;
            case CombatChallengeType::DashKill:
                if (ke.wasDash) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::ComboFinisher:
                if (ke.wasComboFinisher) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::ChargedKill:
                if (ke.wasCharged) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::NoDamageWave:
                // Tracked via wave clear, not per kill
                break;
            default: break;
        }
    }

    // MultiKill timer countdown (only after first kill)
    if (m_combatChallenge.type == CombatChallengeType::MultiKill &&
        m_combatChallenge.currentCount > 0 && m_combatChallenge.currentCount < m_combatChallenge.targetCount) {
        m_combatChallenge.timer -= dt;
        if (m_combatChallenge.timer <= 0) {
            // Failed: reset progress
            m_combatChallenge.currentCount = 0;
            m_combatChallenge.timer = m_combatChallenge.maxTimer;
        }
    }

    // NoDamageWave: check on wave clear (all enemies dead)
    if (m_combatChallenge.type == CombatChallengeType::NoDamageWave) {
        int aliveEnemies = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.isEnemy) aliveEnemies++;
        });
        if (aliveEnemies == 0 && enemiesKilled > 0 && !m_tookDamageThisWave) {
            m_combatChallenge.currentCount = 1;
        }
    }

    // Check completion
    if (m_combatChallenge.currentCount >= m_combatChallenge.targetCount) {
        m_combatChallenge.completed = true;
        m_combatChallenge.active = false;
        m_challengeCompleteTimer = 2.5f;
        m_challengesCompleted++;

        // Reward shards
        if (game) {
            game->getUpgradeSystem().addRiftShards(m_combatChallenge.shardReward);
            shardsCollected += m_combatChallenge.shardReward;
        }

        // Particles + SFX
        if (m_player) {
            auto& t = m_player->getEntity()->getComponent<TransformComponent>();
            m_particles.burst(t.getCenter(), 20, {255, 215, 0, 255}, 200.0f, 4.0f);
            m_particles.burst(t.getCenter(), 12, {255, 255, 180, 255}, 120.0f, 3.0f);
        }
        AudioManager::instance().play(SFX::ShrineBlessing);
        m_camera.shake(4.0f, 0.12f);
    }
}
