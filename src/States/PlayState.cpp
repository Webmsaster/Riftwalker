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
#include "States/EndingState.h"
#include "States/RunSummaryState.h"
#include "States/DailyLeaderboardState.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <algorithm>
#include <string>

extern bool g_autoSmokeTest;
extern bool g_autoPlaytest;
extern int g_forcedRunSeed;
extern bool g_smokeRegression;
extern int g_smokeTargetFloor;
extern float g_smokeMaxRuntime;
extern int g_smokeCompletedFloor;
extern int g_smokeFailureCode;
extern char g_smokeFailureReason[256];

// g_smokeFile, g_playtestFile, and smokeLog defined in PlayStateSmokeBot.cpp
extern FILE* g_smokeFile;
extern FILE* g_playtestFile;
extern void smokeLog(const char* fmt, ...);

namespace {
constexpr bool kUseAiFinalBackgroundArtTest = true;
constexpr const char* kDimAFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dima_run01_s1103_fin.png";
constexpr const char* kDimBFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dimb_run01_s2103_fin.png";
constexpr const char* kBossFinalBackgroundPath =
    "assets/ai/finals/boss/run01/rw_boss_run01_f03_s3120_fin.png";
}

void PlayState::enter() {
    if (g_autoSmokeTest) {
        m_smokeTest = true;
        m_showDebugOverlay = true;
        // Open persistent log file
        if (g_smokeFile) fclose(g_smokeFile);
        g_smokeFile = fopen("smoke_test.log", "w");
        if (g_smokeFile) {
            fprintf(g_smokeFile, "=== RIFTWALKER SMOKE TEST ===\n");
            fflush(g_smokeFile);
        }
    }
    if (g_autoPlaytest) {
        m_playtest = true;
        m_showDebugOverlay = true;
        if (g_playtestFile) fclose(g_playtestFile);
        g_playtestFile = fopen("playtest_report.log", "w");
        m_playtestRun = 0;
        m_playtestDeaths = 0;
        m_playtestBestLevel = 0;
        playtestLog("========================================");
        playtestLog("  RIFTWALKER BALANCE PLAYTEST");
        playtestLog("  Modus: Menschliche Simulation");
        playtestLog("  Max Versuche: %d", m_playtestMaxRuns);
        playtestLog("========================================");
    }
    startNewRun();
    if (m_playtest) {
        playtestStartRun();
    }
}

void PlayState::exit() {
    AudioManager::instance().stopAmbient();
    AudioManager::instance().stopMusicLayers();
    AudioManager::instance().stopThemeAmbient();
    m_musicSystem.cleanup(); // Stop and free procedural music tracks
    m_dimATestBackground = nullptr;
    m_dimBTestBackground = nullptr;
    m_bossTestBackground = nullptr;
    m_entities.clear();
    m_particles.clear();
    m_player.reset();
    m_level.reset();
    m_activePuzzle.reset();
}

void PlayState::startNewRun() {
    m_entities.clear();
    m_particles.clear();

    if (g_smokeRegression) {
        g_smokeCompletedFloor = 0;
        g_smokeFailureCode = 0;
        g_smokeFailureReason[0] = '\0';
    }

    m_currentDifficulty = 1;
    m_ngPlusTier = g_selectedNGPlus;      // Capture NG+ tier at run start
    m_ngPlusForceSwitchTimer = 30.0f;     // NG+4 forced dim-switch interval
    m_isDailyRun = g_dailyRunActive;
    m_runSeed = (g_forcedRunSeed >= 0)
        ? g_forcedRunSeed
        : (m_isDailyRun ? DailyRun::getTodaySeed() : std::rand());
    if (g_smokeRegression) {
        std::srand(static_cast<unsigned int>(m_runSeed));
    }
    g_dailyRunActive = false; // Reset flag
    enemiesKilled = 0;
    riftsRepaired = 0;
    roomsCleared = 0;
    shardsCollected = 0;
    m_collapsing = false;
    m_riftDimensionHintTimer = 0;
    m_riftDimensionHintRequiredDim = 0;
    m_tutorialTimer = 0;
    m_tutorialHintIndex = 0;
    m_tutorialHintShowTimer = 0;
    std::memset(m_tutorialHintDone, 0, sizeof(m_tutorialHintDone));
    m_hasMovedThisRun = false;
    m_hasJumpedThisRun = false;
    m_hasDashedThisRun = false;
    m_hasAttackedThisRun = false;
    m_hasRangedThisRun = false;
    m_hasUsedAbilityThisRun = false;
    m_shownEntropyWarning = false;
    m_shownConveyorHint = false;
    m_shownDimPlatformHint = false;
    m_shownRelicHint = false;
    m_shownWallSlideHint = false;
    m_dashCount = 0;
    m_aerialKillsThisRun = 0;
    m_dashKillsThisRun = 0;
    m_chargedKillsThisRun = 0;
    m_tookDamageThisLevel = false;
    m_pendingLevelGen = false;
    m_showRelicChoice = false;
    m_relicChoices.clear();
    m_savedRelics.clear();
    m_savedHP = 0;
    m_savedMaxHP = 0;
    m_savedVoidHungerBonus = 0;
    m_relicChoiceSelected = 0;
    m_voidSovereignDefeated = false;
    m_trails.clear();
    m_moveTrailTimer = 0.0f;
    m_runTime = 0.0f;
    m_bestCombo = 0;
    m_nearNPCIndex = -1;
    m_showNPCDialog = false;
    m_npcDialogChoice = 0;
    m_echoSpawned = false;
    m_echoRewarded = false;
    std::memset(m_npcStoryProgress, 0, sizeof(m_npcStoryProgress));
    m_combatChallenge = {};
    m_challengeCompleteTimer = 0;
    m_challengesCompleted = 0;
    m_tookDamageThisWave = false;
    m_balanceStats = {};
    m_combatSystem.voidResonanceProcs = 0;

    // Reset per-run unlock tracking
    m_unlockedBossWeaponThisRun = false;
    m_aoeKillCountThisRun = 0;
    m_dashKillsThisRunForWeapon = 0;
    std::memset(m_unlockNotifs, 0, sizeof(m_unlockNotifs));
    m_unlockNotifHead = 0;

    // Event chain: 30% chance to start a chain per run
    m_eventChain = {};
    m_chainEventSpawned = false;
    m_chainEventIndex = -1;
    m_chainNotifyTimer = 0;
    m_chainRewardTimer = 0;
    m_chainRewardShards = 0;
    if (std::rand() % 100 < 30) {
        m_eventChain.type = static_cast<EventChainType>(std::rand() % static_cast<int>(EventChainType::COUNT));
        m_eventChain.stage = 1;
        m_eventChain.startLevel = 1;
        m_chainNotifyTimer = 4.0f; // Show intro notification
    }

    // Reset run buffs
    game->getRunBuffSystem().reset();

    // Random theme pair
    auto themes = WorldTheme::getRandomPair(m_runSeed);
    m_themeA = themes.first;
    m_themeB = themes.second;
    m_dimATestBackground = nullptr;
    m_dimBTestBackground = nullptr;
    m_bossTestBackground = nullptr;
    if (kUseAiFinalBackgroundArtTest) {
        m_dimATestBackground = ResourceManager::instance().getTexture(kDimAFinalBackgroundPath);
        m_dimBTestBackground = ResourceManager::instance().getTexture(kDimBFinalBackgroundPath);
        m_bossTestBackground = ResourceManager::instance().getTexture(kBossFinalBackgroundPath);
        if (!m_dimATestBackground) {
            SDL_Log("AI test background missing for DIM-A: %s", kDimAFinalBackgroundPath);
        }
        if (!m_dimBTestBackground) {
            SDL_Log("AI test background missing for DIM-B: %s", kDimBFinalBackgroundPath);
        }
        if (!m_bossTestBackground) {
            SDL_Log("AI test background missing for boss floors: %s", kBossFinalBackgroundPath);
        }
    }

    m_dimManager = DimensionManager();
    m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
    m_dimManager.particles = &m_particles;
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
    AudioManager::instance().startMusicLayers();
    AudioManager::instance().playThemeAmbient(static_cast<int>(m_themeA.id));

    // Start procedural theme music
    m_musicSystem.setTheme(static_cast<int>(m_themeA.id), m_dimManager.getCurrentDimension());

    m_entropy = SuitEntropy();
    // NG+1: entropy decays 20% slower (passive decay reduced)
    if (m_ngPlusTier >= 1) {
        m_entropy.passiveDecay *= 0.8f;
    }
    m_combatSystem.resetRunState();
    m_combatSystem.setParticleSystem(&m_particles);
    m_combatSystem.setCamera(&m_camera);
    m_aiSystem.setParticleSystem(&m_particles);
    m_aiSystem.setCamera(&m_camera);
    m_aiSystem.setCombatSystem(&m_combatSystem);
    m_hitFreezeTimer = 0;
    m_spikeDmgCooldown = 0;
    m_waveClearTriggered = false;
    m_waveClearTimer = 0;
    m_playerDying = false;
    m_deathSequenceTimer = 0;
    m_deathCause = 0;

    generateLevel();
    m_combatSystem.setPlayer(m_player.get());
    m_combatSystem.setDimensionManager(&m_dimManager);
    m_hud.setCombatSystem(&m_combatSystem);
    m_hud.setNGPlusTier(m_ngPlusTier);
    m_aiSystem.setLevel(m_level.get());
    m_aiSystem.setPlayer(m_player.get());
}

void PlayState::generateLevel() {
    m_entities.clear();
    m_particles.clear();

    m_levelGen.setThemes(m_themeA, m_themeB);
    m_level = std::make_unique<Level>(m_levelGen.generate(m_currentDifficulty, m_runSeed + m_currentDifficulty));

    // Try to load tileset for sprite-based tile rendering
    m_level->loadTileset();

    // Create player with selected class
    m_player = std::make_unique<Player>(m_entities);
    m_player->particles = &m_particles;
    m_player->entityManager = &m_entities;
    m_player->combatSystemRef = &m_combatSystem;
    m_player->dimensionManager = &m_dimManager;
    m_player->playerClass = g_selectedClass;
    m_player->applyClassStats();
    applyUpgrades();
    applyAscensionModifiers();

    // Register heal callback for floating green numbers
    {
        auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
        hp.onHeal = [this](float amount) {
            if (amount < 1.0f) return; // Skip tiny heals to avoid spam
            if (!m_player) return;
            auto& t = m_player->getEntity()->getComponent<TransformComponent>();
            FloatingDamageNumber num;
            num.position = t.getCenter();
            num.position.x += static_cast<float>((std::rand() % 20) - 10); // Slight random offset
            num.value = amount;
            num.isPlayerDamage = false;
            num.isHeal = true;
            num.lifetime = 0.9f;
            num.maxLifetime = num.lifetime;
            m_damageNumbers.push_back(num);
        };
    }

    // Voidwalker: reduced switch cooldown
    if (g_selectedClass == PlayerClass::Voidwalker) {
        m_dimManager.switchCooldown = 0.5f * ClassSystem::getData(PlayerClass::Voidwalker).switchCDReduction;
    }

    auto& playerTransform = m_player->getEntity()->getComponent<TransformComponent>();
    playerTransform.position = m_level->getSpawnPoint();

    m_camera.setPosition(playerTransform.getCenter());
    m_camera.setBounds(0, 0, m_level->getPixelWidth(), m_level->getPixelHeight());

    spawnEnemies();

    // Boss every 3 levels
    m_isBossLevel = (m_currentDifficulty % 3 == 0);
    m_bossDefeated = false;
    if (m_isBossLevel) {
        spawnBoss();
    }

    m_levelComplete = false;
    m_levelCompleteTimer = 0;
    m_activePuzzle.reset();
    m_nearRiftIndex = -1;
    m_riftDimensionHintTimer = 0;
    m_riftDimensionHintRequiredDim = 0;
    // FIX: Reset per-level rift tracking
    m_repairedRiftIndices.clear();
    m_levelRiftsRepaired = 0;
    // FIX: Reset collapse state for new level
    m_collapsing = false;
    m_collapseTimer = 0;
    m_teleportCooldown = 0;
    m_tookDamageThisLevel = false;

    // Event chain: spawn chain-specific event this level
    m_chainEventSpawned = false;
    m_chainEventIndex = -1;
    if (m_eventChain.stage > 0 && !m_eventChain.completed) {
        spawnChainEvent();
    }

    // Start ambient music
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());

    // Update procedural music for new level theme
    m_musicSystem.setTheme(static_cast<int>(m_themeA.id), m_dimManager.getCurrentDimension());
}

void PlayState::spawnEnemies() {
    auto spawns = m_level->getEnemySpawns();

    // Difficulty modifier: Easy removes 30% of spawns, Hard adds 40% extra
    if (g_selectedDifficulty == GameDifficulty::Easy) {
        int removeCount = static_cast<int>(spawns.size() * 0.3f);
        for (int i = 0; i < removeCount && !spawns.empty(); i++) {
            spawns.pop_back();
        }
    } else if (g_selectedDifficulty == GameDifficulty::Hard) {
        int extraCount = static_cast<int>(spawns.size() * 0.4f);
        int origSize = static_cast<int>(spawns.size());
        spawns.reserve(spawns.size() + extraCount);
        for (int i = 0; i < extraCount; i++) {
            auto base = spawns[std::rand() % origSize]; // copy to avoid dangling ref
            Vec2 offset = {static_cast<float>((std::rand() % 100) - 50), 0};
            spawns.push_back({Vec2{base.position.x + offset.x, base.position.y}, base.enemyType, base.dimension});
        }
    }

    // Divide spawns into waves of 3-5 enemies
    m_spawnWaves.clear();
    m_currentWave = 0;
    m_waveTimer = 0;
    m_waveActive = false;
    m_waveClearTriggered = false;
    m_waveClearTimer = 0;

    int waveSize = 3 + (m_currentDifficulty > 3 ? 2 : m_currentDifficulty > 1 ? 1 : 0);
    std::vector<Level::SpawnPoint> wave;
    for (auto& sp : spawns) {
        wave.push_back(sp);
        if (static_cast<int>(wave.size()) >= waveSize) {
            m_spawnWaves.push_back(wave);
            wave.clear();
        }
    }
    if (!wave.empty()) m_spawnWaves.push_back(wave);

    // Spawn first wave immediately
    // Mini-boss: one per level at difficulty 2+, 15% chance (not on boss levels)
    bool miniBossSpawned = false;

    // NG+2: double elite spawn rate (base 15% -> 30%)
    int eliteChance = (m_ngPlusTier >= 2) ? 30 : 15;
    // NG+4: mini-bosses in every level (100% chance, not just 15%)
    int miniBossChance = (m_ngPlusTier >= 4) ? 100 : 15;

    if (!m_spawnWaves.empty()) {
        for (auto& sp : m_spawnWaves[0]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            // Initial wave: no spawn flicker (enemies already present when level starts)
            if (e.hasComponent<AIComponent>()) e.getComponent<AIComponent>().spawnTimer = 0;
            // Theme-specific variant: stat mods, element affinity, color tint
            applyThemeVariant(e, sp.dimension);
            // NG+ HP/DMG scaling applied after theme variant
            applyNGPlusModifiers(e);
            // Elemental variant chance: 25% at difficulty 3+, only if theme didn't set element
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && e.getComponent<AIComponent>().element == EnemyElement::None
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Elite modifier: chance increases in NG+2
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < eliteChance) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 9);
                Enemy::makeElite(e, mod);
            }
            // Mini-boss: NG+4 spawns one in every level
            if (!miniBossSpawned && !m_isBossLevel && m_currentDifficulty >= 2
                && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && std::rand() % 100 < miniBossChance) {
                Enemy::makeMiniBoss(e);
                miniBossSpawned = true;
            }
        }
        m_currentWave = 1;
        m_waveActive = true;
        m_waveTimer = m_waveDelay;
    }

    // Start first combat challenge for this level
    startCombatChallenge();
}

void PlayState::applyThemeVariant(Entity& e, int dimension) {
    // Apply theme-specific stat modifications and element affinity to enemies
    if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>() ||
        !e.hasComponent<CombatComponent>() || !e.hasComponent<SpriteComponent>()) return;

    const auto& theme = (dimension == 1) ? m_themeA : m_themeB;
    auto config = ThemeEnemyConfig::getConfig(theme.id);

    auto& ai = e.getComponent<AIComponent>();
    if (ai.enemyType == EnemyType::Boss) return; // Don't modify bosses

    // Apply theme stat modifiers
    auto& hp = e.getComponent<HealthComponent>();
    hp.maxHP *= config.hpMod;
    hp.currentHP = hp.maxHP;

    ai.patrolSpeed *= config.speedMod;
    ai.chaseSpeed *= config.speedMod;

    auto& combat = e.getComponent<CombatComponent>();
    combat.meleeAttack.damage *= config.damageMod;
    combat.rangedAttack.damage *= config.damageMod;

    // Theme element affinity: 40% chance theme element, 10% random other element (at diff 2+)
    if (config.preferredElement > 0 && ai.element == EnemyElement::None && m_currentDifficulty >= 2) {
        if (std::rand() % 100 < 40) {
            Enemy::applyElement(e, static_cast<EnemyElement>(config.preferredElement));
        }
    } else if (config.preferredElement == 0 && ai.element == EnemyElement::None && m_currentDifficulty >= 3) {
        // Themes without element affinity: small chance for random element
        if (std::rand() % 100 < 15) {
            Enemy::applyElement(e, static_cast<EnemyElement>(1 + std::rand() % 3));
        }
    }

    // Tint enemy sprite toward theme accent color for visual cohesion
    auto& sprite = e.getComponent<SpriteComponent>();
    const auto& accent = theme.colors.accent;
    // Blend 20% toward theme accent
    sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.8f + accent.r * 0.2f);
    sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.8f + accent.g * 0.2f);
    sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.8f + accent.b * 0.2f);

    // New Game+ scaling: enemies get stronger per NG+ tier
    if (g_newGamePlusLevel > 0) {
        float ngMult = 1.0f + g_newGamePlusLevel * 0.2f; // +20% per NG+ level
        hp.maxHP *= ngMult;
        hp.currentHP = hp.maxHP;
        combat.meleeAttack.damage *= (1.0f + g_newGamePlusLevel * 0.15f);
        combat.rangedAttack.damage *= (1.0f + g_newGamePlusLevel * 0.15f);
        ai.chaseSpeed *= (1.0f + g_newGamePlusLevel * 0.05f);
        // NG+ visual: slightly purple tint
        sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + g_newGamePlusLevel * 15));
        sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b + g_newGamePlusLevel * 20));
    }

    sprite.baseColor = sprite.color; // update base for wind-up restore
}

void PlayState::applyNGPlusModifiers(Entity& e) {
    // Apply NG+ HP and damage scaling to enemies. Bosses are included.
    if (m_ngPlusTier <= 0) return;
    if (!e.hasComponent<HealthComponent>() || !e.hasComponent<CombatComponent>()) return;

    // NG+1: +25% HP, +15% DMG
    // NG+3: cumulative to +50% HP total (applied on top of NG+1/2 chain)
    // NG+5: cumulative to +100% HP total
    float hpMult = 1.0f;
    float dmgMult = 1.0f;
    if (m_ngPlusTier >= 1) { hpMult *= 1.25f; dmgMult *= 1.15f; }
    if (m_ngPlusTier >= 3) { hpMult *= 1.20f; }   // reaches ~50% total vs base
    if (m_ngPlusTier >= 5) { hpMult *= 1.33f; }   // reaches ~100% total vs base

    auto& hp = e.getComponent<HealthComponent>();
    hp.maxHP *= hpMult;
    hp.currentHP = hp.maxHP;

    auto& combat = e.getComponent<CombatComponent>();
    combat.meleeAttack.damage *= dmgMult;
    combat.rangedAttack.damage *= dmgMult;

    // NG+3+: tint enemy HP bar gold to signal the tier
    if (e.hasComponent<SpriteComponent>()) {
        auto& sprite = e.getComponent<SpriteComponent>();
        // Subtle gold tint for NG+ enemies — blend toward gold without destroying theme color
        float tintStrength = 0.08f + m_ngPlusTier * 0.03f; // 0.11-0.23 at NG+1-5
        sprite.color.r = static_cast<Uint8>(std::min(255, static_cast<int>(sprite.color.r * (1.0f - tintStrength) + 255 * tintStrength)));
        sprite.color.g = static_cast<Uint8>(std::min(255, static_cast<int>(sprite.color.g * (1.0f - tintStrength) + 200 * tintStrength)));
        sprite.color.b = static_cast<Uint8>(std::min(255, static_cast<int>(sprite.color.b * (1.0f - tintStrength) + 30 * tintStrength)));
        sprite.baseColor = sprite.color;
    }
}

void PlayState::applyUpgrades() {
    if (!m_player) return;
    auto& upgrades = game->getUpgradeSystem();
    auto achBonus = game->getAchievements().getUnlockedBonuses();

    m_player->moveSpeed = 250.0f * upgrades.getMoveSpeedMultiplier() * achBonus.moveSpeedMult;
    m_player->jumpForce = -420.0f * upgrades.getJumpMultiplier();
    m_player->dashCooldown = 0.5f * upgrades.getDashCooldownMultiplier() * achBonus.dashCooldownMult;
    m_player->maxJumps = 2 + upgrades.getExtraJumps();
    // FIX: Apply WallSlide upgrade (was purchased but had no effect)
    m_player->wallSlideSpeed = 60.0f * upgrades.getWallSlideSpeedMultiplier();

    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    // BALANCE: Base HP 100 -> 120 (matches Player.cpp change)
    hp.maxHP = 120.0f + upgrades.getMaxHPBonus() + achBonus.maxHPBonus;
    hp.currentHP = hp.maxHP;
    hp.armor = upgrades.getArmorBonus() + achBonus.armorBonus;

    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    // BALANCE: Base melee 20 -> 25, ranged 12 -> 15 (matches Player.cpp change)
    combat.meleeAttack.damage = 25.0f * upgrades.getMeleeDamageMultiplier() * achBonus.meleeDamageMult * achBonus.allDamageMult;
    combat.rangedAttack.damage = 15.0f * upgrades.getRangedDamageMultiplier() * achBonus.rangedDamageMult * achBonus.allDamageMult;

    m_dimManager.switchCooldown = 0.5f * upgrades.getSwitchCooldownMultiplier() * achBonus.switchCooldownMult;
    m_entropy.passiveDecay = upgrades.getEntropyDecay();
    // FIX: EntropyResistance upgrade was purchased but never applied (same pattern as WallSlide)
    // Cap at 0.8 so entropy never becomes completely trivial
    m_entropy.upgradeResistance = 1.0f - std::min(upgrades.getEntropyResistance(), 0.8f);
    m_combatSystem.setCritChance(upgrades.getCritChance() + achBonus.critChanceBonus);
    m_combatSystem.setComboBonus(upgrades.getComboBonus() + achBonus.comboDamageBonus);

    // Achievement DOT reduction applied to player
    m_player->dotDurationMult = achBonus.dotDurationMult;

    // Store shard drop multiplier from achievements + NG+ bonus (+20% per tier)
    m_achievementShardMult = achBonus.shardDropMult * (1.0f + m_ngPlusTier * 0.20f);

    // Ability upgrades
    if (m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float cdMult = upgrades.getAbilityCooldownMultiplier();
        float pwrMult = upgrades.getAbilityPowerMultiplier();
        abil.abilities[0].cooldown = 6.0f * cdMult;
        abil.abilities[1].cooldown = 10.0f * cdMult;
        abil.abilities[2].cooldown = 8.0f * cdMult;
        abil.slamDamage = 60.0f * pwrMult * achBonus.allDamageMult;
        abil.shieldBurstDamage = 25.0f * pwrMult * achBonus.allDamageMult;
        abil.shieldMaxHits = 3 + upgrades.getShieldCapacityBonus();
    }
}

void PlayState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.scancode == SDL_SCANCODE_F3) {
            m_showDebugOverlay = !m_showDebugOverlay;
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_F4) {
            m_pendingSnapshot = true;
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_F6) {
            m_smokeTest = !m_smokeTest;
            m_showDebugOverlay = m_smokeTest; // Auto-enable F3 overlay
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_M) {
            m_hud.toggleMinimap();
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            if (m_showRelicChoice) {
                // Can't escape relic choice
                return;
            }
            if (m_activePuzzle && m_activePuzzle->isActive()) {
                m_activePuzzle.reset();
            } else {
                game->pushState(StateID::Pause);
            }
            return;
        }

        // Relic choice input
        if (m_showRelicChoice && !m_relicChoices.empty()) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:
                    m_relicChoiceSelected = (m_relicChoiceSelected - 1 + static_cast<int>(m_relicChoices.size()))
                                            % static_cast<int>(m_relicChoices.size());
                    break;
                case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT:
                    m_relicChoiceSelected = (m_relicChoiceSelected + 1)
                                            % static_cast<int>(m_relicChoices.size());
                    break;
                case SDL_SCANCODE_1: selectRelic(0); break;
                case SDL_SCANCODE_2: if (m_relicChoices.size() > 1) selectRelic(1); break;
                case SDL_SCANCODE_3: if (m_relicChoices.size() > 2) selectRelic(2); break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                    selectRelic(m_relicChoiceSelected);
                    break;
                default: break;
            }
            return;
        }

        // NPC dialog input
        // FIX: Added bounds check for m_nearNPCIndex against npcs.size()
        if (m_showNPCDialog && m_nearNPCIndex >= 0) {
            auto& npcs = m_level->getNPCs();
            if (m_nearNPCIndex >= static_cast<int>(npcs.size())) { m_showNPCDialog = false; return; }
            int stage = getNPCStoryStage(npcs[m_nearNPCIndex].type);
            auto options = NPCSystem::getDialogOptions(npcs[m_nearNPCIndex].type, stage);
            int optCount = static_cast<int>(options.size());
            if (optCount == 0) { m_showNPCDialog = false; return; }
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                    m_npcDialogChoice = (m_npcDialogChoice - 1 + optCount) % optCount;
                    AudioManager::instance().play(SFX::MenuSelect);
                    break;
                case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                    m_npcDialogChoice = (m_npcDialogChoice + 1) % optCount;
                    AudioManager::instance().play(SFX::MenuSelect);
                    break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                    handleNPCDialogChoice(m_nearNPCIndex, m_npcDialogChoice);
                    break;
                case SDL_SCANCODE_ESCAPE:
                    m_showNPCDialog = false;
                    break;
                default: break;
            }
            return;
        }

        // Puzzle input
        if (m_activePuzzle && m_activePuzzle->isActive()) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_W: case SDL_SCANCODE_UP:    handlePuzzleInput(0); break;
                case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT: handlePuzzleInput(1); break;
                case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:  handlePuzzleInput(2); break;
                case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:  handlePuzzleInput(3); break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE: handlePuzzleInput(4); break;
                default: break;
            }
            return;
        }
    }
}

void PlayState::update(float dt) {
    if (!m_player || !m_level) return;

    // Generate level FIRST after returning from shop (before any gameplay logic)
    // This prevents stale state (old collapsing/exit) from triggering false completions
    if (m_pendingLevelGen) {
        m_pendingLevelGen = false;

        // Save player state before level regeneration (relics + HP carry over)
        m_savedRelics.clear();
        m_savedHP = 0;
        m_savedMaxHP = 0;
        m_savedVoidHungerBonus = 0;
        if (m_player) {
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                auto& rc = m_player->getEntity()->getComponent<RelicComponent>();
                m_savedRelics = rc.relics;
                m_savedVoidHungerBonus = rc.voidHungerBonus;
            }
            if (m_player->getEntity()->hasComponent<HealthComponent>()) {
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                m_savedHP = hp.currentHP;
                m_savedMaxHP = hp.maxHP;
            }
        }

        generateLevel();
        // Update system pointers to new Player and Level (prevent dangling after regeneration)
        m_combatSystem.setPlayer(m_player.get());
        m_aiSystem.setLevel(m_level.get());
        m_aiSystem.setPlayer(m_player.get());
        applyRunBuffs();

        // Restore relics and HP to new player entity
        if (m_player && !m_savedRelics.empty()) {
            auto& rc = m_player->getEntity()->getComponent<RelicComponent>();
            rc.relics = m_savedRelics;
            rc.voidHungerBonus = m_savedVoidHungerBonus;

            // Re-apply relic stat effects (IronHeart HP, SwiftBoots speed, etc.)
            auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
            auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
            RelicSystem::applyStatEffects(rc, *m_player, hp, combat);

            // Re-apply relic-specific modifiers
            m_dimManager.switchCooldown = std::max(0.20f,
                0.5f * RelicSystem::getSwitchCooldownMult(rc));

            // TimeDilator: re-apply ability cooldown reduction
            if (rc.hasRelic(RelicID::TimeDilator) &&
                m_player->getEntity()->hasComponent<AbilityComponent>()) {
                auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
                float cdMult = RelicSystem::getAbilityCooldownMultiplier(rc);
                abil.abilities[0].cooldown *= cdMult;
                abil.abilities[1].cooldown *= cdMult;
                abil.abilities[2].cooldown *= cdMult;
            }

            // Carry over HP (clamped to new max)
            hp.currentHP = std::min(m_savedHP, hp.maxHP);

            // SoulLeech: -5 HP per level transition (applied after HP restore)
            float leechCost = RelicSystem::getSoulLeechLevelHPCost(rc);
            if (leechCost > 0 && hp.currentHP > leechCost) {
                hp.currentHP -= leechCost;
            }
        } else if (m_player && m_savedMaxHP > 0) {
            // No relics but still carry over HP
            auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
            hp.currentHP = std::min(m_savedHP, hp.maxHP);
        }
    }

    // Death sequence: dramatic freeze before ending the run
    if (m_playerDying) {
        m_deathSequenceTimer -= dt;
        m_camera.update(dt);       // camera shake continues
        m_particles.update(dt);    // death particles animate
        m_screenEffects.update(dt);

        // Slow-motion particle drip during death
        if (m_player && m_deathSequenceTimer > 0.3f) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 center = pt.getCenter();
            float progress = 1.0f - (m_deathSequenceTimer / m_deathSequenceDuration);
            Uint8 alpha = static_cast<Uint8>(80 + 120 * progress);
            m_particles.burst(
                {center.x + static_cast<float>((std::rand() % 30) - 15),
                 center.y + static_cast<float>((std::rand() % 20) - 10)},
                1 + static_cast<int>(progress * 3),
                {255, static_cast<Uint8>(60 + 100 * (1.0f - progress)), 40, alpha},
                40.0f + progress * 80.0f, 2.0f + progress * 3.0f);
        }

        if (m_deathSequenceTimer <= 0) {
            m_playerDying = false;
            if (m_playtest) {
                playtestOnDeath();
            } else {
                endRun();
            }
        }
        return;
    }

    // Track balance stats every frame
    updateBalanceTracking(dt);

    // Smoke test: auto-play for integration testing
    if (m_smokeTest) updateSmokeTest(dt);
    // Playtest: human-like auto-play for balance testing
    if (m_playtest) updatePlaytest(dt);

    // Hit-freeze: brief pause on melee hit for impact feel
    float freeze = m_combatSystem.consumeHitFreeze();
    if (freeze > 0 && m_hitFreezeTimer <= 0) {
        m_hitFreezeTimer = freeze;
    }
    if (m_hitFreezeTimer > 0) {
        m_hitFreezeTimer -= dt;
        m_camera.update(dt);       // still update camera shake during freeze
        m_particles.update(dt);    // particles keep going
        return;
    }

    // Relic choice popup takes priority
    if (m_showRelicChoice) {
        return; // Wait for player to pick a relic
    }

    // NPC dialog takes priority
    if (m_showNPCDialog) {
        return;
    }

    // Challenge mode: speedrun timer
    if (g_activeChallenge == ChallengeID::Speedrun && m_challengeTimer > 0) {
        m_challengeTimer -= dt;
        if (m_challengeTimer <= 0 && !m_playerDying) {
            m_playerDying = true;
            m_deathCause = 4; // Speedrun
            m_deathSequenceTimer = m_deathSequenceDuration;
            AudioManager::instance().play(SFX::PlayerDeath);
            m_camera.shake(15.0f, 0.6f);
            game->getInputMutable().rumble(1.0f, 500);
            if (m_player) {
                Vec2 pos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(pos, 30, {255, 200, 50, 255}, 250.0f, 5.0f);
            }
            m_hud.triggerDamageFlash();
            return;
        }
    }

    // Mutator: DimensionFlux - auto switch every 15s
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::DimensionFlux) {
            m_mutatorFluxTimer += dt;
            if (m_mutatorFluxTimer >= 15.0f) {
                m_mutatorFluxTimer = 0;
                m_dimManager.switchDimension(true);
                m_camera.shake(6.0f, 0.2f);
                AudioManager::instance().play(SFX::DimensionSwitch);
                AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
                m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
                m_screenEffects.triggerDimensionRipple();
            }
        }
    }

    // NG+4: forced dimension switch every 30 seconds
    if (m_ngPlusTier >= 4) {
        m_ngPlusForceSwitchTimer -= dt;
        if (m_ngPlusForceSwitchTimer <= 0) {
            m_ngPlusForceSwitchTimer = 30.0f;
            m_dimManager.switchDimension(true); // force = true, bypasses cooldown
            m_camera.shake(8.0f, 0.3f);
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            m_musicSystem.setDimension(m_dimManager.getCurrentDimension());
            m_screenEffects.triggerDimensionRipple();
        }
    }

    // Active puzzle takes priority
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->update(dt);
        return;
    }
    if (m_activePuzzle && !m_activePuzzle->isActive()) {
        m_activePuzzle.reset();
    }

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

    m_dimManager.playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_dimManager.update(dt);

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
                // Beat faster as HP gets lower (1.2s at 25% → 0.6s near 0%)
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

    // Run time tracking
    m_runTime += dt;

    // Player update
    m_player->update(dt, input);

    // Update tile timers (crumbling etc.)
    if (m_level) m_level->updateTiles(dt);

    // Physics - pass current dimension so player (dimension=0) collides with correct tiles
    m_physics.update(m_entities, dt, m_level.get(), m_dimManager.getCurrentDimension());

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

            // Landing dust cloud at feet — bigger for harder landings
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

    // AI
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_aiSystem.update(m_entities, dt, playerPos, m_dimManager.getCurrentDimension());

    // Technomancer: update player-owned turrets (lifetime + auto-fire)
    if (m_player->isTechnomancer()) {
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
    // (Handled by zombie sweep in CombatSystem — detect dying entropy minions here)
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

    // Collision
    m_collision.update(m_entities, m_dimManager.getCurrentDimension());

    // Enemy hazard damage: enemies on spikes/fire/laser take damage
    {
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

    // Combat
    m_combatSystem.update(m_entities, dt, m_dimManager.getCurrentDimension());

    // Consume damage events for floating numbers + achievement tracking
    auto dmgEvents = m_combatSystem.consumeDamageEvents();
    for (auto& evt : dmgEvents) {
        FloatingDamageNumber num;
        num.position = evt.position;
        num.value = evt.damage;
        num.isPlayerDamage = evt.isPlayerDamage;
        num.isCritical = evt.isCritical;
        num.lifetime = evt.isCritical ? 1.2f : 0.8f;
        num.maxLifetime = num.lifetime;
        m_damageNumbers.push_back(num);

        // Track enemy hits from player for achievements
        if (!evt.isPlayerDamage) {
            game->getAchievements().unlock("first_blood");
        }
        // Track player damage for flawless floor achievement + NoDamageWave challenge
        if (evt.isPlayerDamage) {
            m_tookDamageThisLevel = true;
            m_tookDamageThisWave = true;
            // Screen shake + damage flash + hurt SFX on enemy combat hits
            // Skip if source already handled feedback (hazards, DoT)
            if (!evt.feedbackHandled) {
                m_camera.shake(6.0f, 0.15f);
                m_hud.triggerDamageFlash();
                AudioManager::instance().play(SFX::PlayerHurt);
                game->getInputMutable().rumble(0.6f, 150);
            }
        }
    }

    // Wave/area clear celebration: all waves spawned + all enemies dead
    if (m_combatSystem.killCount > 0 && !m_isBossLevel && !m_waveClearTriggered) {
        int aliveAfterCombat = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") != std::string::npos) aliveAfterCombat++;
        });
        if (aliveAfterCombat == 0 && m_currentWave >= static_cast<int>(m_spawnWaves.size())) {
            m_waveClearTriggered = true;
            m_waveClearTimer = 2.0f;
            m_combatSystem.addHitFreeze(0.15f);
            m_camera.shake(10.0f, 0.3f);
            game->getInputMutable().rumble(0.5f, 200);
            AudioManager::instance().play(SFX::ShrineBlessing);
            if (m_player) {
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(pPos, 40, {255, 215, 0, 255}, 300.0f, 5.0f);
                m_particles.burst(pPos, 20, {255, 255, 200, 255}, 200.0f, 4.0f);
            }
        }
    }
    if (m_waveClearTimer > 0) m_waveClearTimer -= dt;

    updateDamageNumbers(dt);

    // Combo milestone check (3x, 5x, 7x, 10x)
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        int combo = combat.comboCount;
        int milestone = 0;
        if (combo >= 10) milestone = 10;
        else if (combo >= 7) milestone = 7;
        else if (combo >= 5) milestone = 5;
        else if (combo >= 3) milestone = 3;

        if (milestone > 0 && milestone != m_lastComboMilestone) {
            m_lastComboMilestone = milestone;
            m_comboMilestoneFlash = 0.4f;
            AudioManager::instance().play(SFX::ComboMilestone);
            if (m_player->particles) {
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                // Golden burst for milestone
                m_player->particles->burst(pPos, 20 + milestone * 2, {255, 215, 0, 255}, 180.0f, 4.0f);
                m_player->particles->burst(pPos, 10, {255, 255, 180, 255}, 120.0f, 3.0f);
            }
        }
        if (combo == 0) m_lastComboMilestone = 0;
    }
    if (m_comboMilestoneFlash > 0) m_comboMilestoneFlash -= dt;

    // Shard magnet: attract pickups toward player
    float magnetRange = game->getUpgradeSystem().getShardMagnetRange();
    // ShardMagnet relic: +50% pickup radius
    if (m_player->getEntity()->hasComponent<RelicComponent>()) {
        magnetRange *= RelicSystem::getShardMagnetMultiplier(
            m_player->getEntity()->getComponent<RelicComponent>());
    }
    if (magnetRange > 14.0f) { // Only if upgrade purchased or relic active
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("pickup") == std::string::npos) return;
            if (!e.hasComponent<TransformComponent>() || !e.hasComponent<PhysicsBody>()) return;

            auto& t = e.getComponent<TransformComponent>();
            Vec2 pickupPos = t.getCenter();
            float dx = pPos.x - pickupPos.x;
            float dy = pPos.y - pickupPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < magnetRange && dist > 5.0f) {
                float strength = 300.0f * (1.0f - dist / magnetRange);
                auto& phys = e.getComponent<PhysicsBody>();
                phys.velocity.x += (dx / dist) * strength * dt;
                phys.velocity.y += (dy / dist) * strength * dt;
            }
        });
    }

    // Entities
    m_entities.update(dt);
    m_entities.refresh();

    // Check boss defeated
    if (m_isBossLevel && !m_bossDefeated) {
        bool bossAlive = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_boss") bossAlive = true;
        });
        if (!bossAlive) {
            m_bossDefeated = true;
            // Boss kill rewards
            float bossShardMult = game->getRunBuffSystem().getShardMultiplier();
            bossShardMult *= m_achievementShardMult;
            if (m_player->getEntity()->hasComponent<RelicComponent>())
                bossShardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
            int bossShards = static_cast<int>((50 + m_currentDifficulty * 20) * bossShardMult);
            shardsCollected += bossShards;
            game->getUpgradeSystem().addRiftShards(bossShards);
            AudioManager::instance().play(SFX::LevelComplete);
            m_camera.shake(15.0f, 0.5f);
            Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(playerPos, 60, {255, 200, 100, 255}, 350.0f, 6.0f);

            // Boss achievements
            game->getAchievements().unlock("boss_slayer");
            int bossIdx = m_currentDifficulty / 3;
            if (bossIdx % 2 == 1) game->getAchievements().unlock("wyrm_hunter");

            // Lore triggers on boss kill
            {
                auto* lore = game->getLoreSystem();
                if (lore) {
                    int bossTypeForLore = bossIdx % 4;
                    if (m_currentDifficulty >= 11 && bossIdx >= 5) {
                        // Entropy Incarnate killed
                        Bestiary::onBossKill(5);
                        AudioManager::instance().play(SFX::LoreDiscover);
                    } else if (m_currentDifficulty >= 10 && bossIdx >= 4) {
                        // Void Sovereign killed
                        lore->discover(LoreID::SovereignTruth);
                        Bestiary::onBossKill(4);
                        AudioManager::instance().play(SFX::LoreDiscover);
                    } else {
                        Bestiary::onBossKill(bossTypeForLore);
                        if (bossTypeForLore == 0) lore->discover(LoreID::BossMemory1);
                        else if (bossTypeForLore == 1) lore->discover(LoreID::BossMemory2);
                        else if (bossTypeForLore == 2) lore->discover(LoreID::BossMemory3);
                        else if (bossTypeForLore == 3) lore->discover(LoreID::BossMemory4);
                    }
                }
            }

            bool finalBossDefeated = (m_currentDifficulty >= 10 && bossIdx >= 4);

            // Track Void Sovereign defeat for ending sequence
            if (finalBossDefeated) {
                m_voidSovereignDefeated = true;
                m_deathCause = 5; // Victory
                m_levelComplete = true;
                m_levelCompleteTimer = 0;
            } else {
                // Boss kill unlocks exit: trigger collapse so player can escape
                if (!m_collapsing) {
                    m_collapsing = true;
                    m_collapseTimer = 0;
                    m_level->setExitActive(true);
                }

                // Boss kill -> Relic choice (3 from pool)
                showRelicChoice();
            }
        }
    }

    // Hazard damage (spikes, fire, laser, conveyor)
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
                // BALANCE: Spike DMG 15 -> 10, entropy 5 -> 3 (playtest: main death cause in L1)
                float spikeDmg = hazardDmg(10.0f);
                playerHP.takeDamage(spikeDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), spikeDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(3.0f);
                m_spikeDmgCooldown = 0.5f;
                m_camera.shake(6.0f, 0.2f);
                AudioManager::instance().play(SFX::SpikeDamage);
                if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
                    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                    phys.velocity.y = -250.0f;
                }
                m_particles.burst(playerT.getCenter(), 15, {255, 80, 40, 255}, 150.0f, 3.0f);
                m_hud.triggerDamageFlash();
            } else if (tile.type == TileType::Fire && !playerHP.isInvincible()) {
                float fireDmg = hazardDmg(10.0f);
                playerHP.takeDamage(fireDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), fireDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(3.0f);
                m_spikeDmgCooldown = 0.4f;
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
                float laserDmg = hazardDmg(20.0f);
                playerHP.takeDamage(laserDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), laserDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(8.0f);
                m_spikeDmgCooldown = 0.3f;
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
        if (combat.isAttacking) m_hasAttackedThisRun = true;
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
        return;
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
                // Skip to buff check
                goto skipDeath;
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
            return; // Wait for death sequence to finish
        }
    }
    skipDeath:

    // Check breakable walls (dash/charged attack destroys them)
    checkBreakableWalls();

    // Check secret room discovery
    checkSecretRoomDiscovery();

    // Secret room proximity hints (ambient particles near undiscovered rooms)
    updateSecretRoomHints(dt);

    // Check random event interaction
    checkEventInteraction();

    // Check NPC interaction
    checkNPCInteraction();

    // Check rift interaction
    checkRiftInteraction();

    // Check if player reached exit
    checkExitReached();

    // Camera follow player with look-ahead + vertical dead zone
    Vec2 camTarget = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    bool playerGrounded = false;
    Vec2 playerVel = {0, 0};
    if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        playerVel = phys.velocity;
        playerGrounded = phys.onGround;
    }
    m_camera.follow(camTarget, dt, playerVel, playerGrounded);
    m_camera.shakeMultiplier = g_shakeIntensity; // apply settings slider
    m_camera.update(dt);

    // Spawn waves
    updateSpawnWaves(dt);

    // Particles
    m_particles.update(dt);

    // DimResidue damage zones: tick down lifetime and deal AoE damage
    int curDim = m_dimManager.getCurrentDimension();
    float residueDps = 15.0f; // Default DPS
    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
        residueDps = RelicSynergy::getResidueDamage(m_player->getEntity()->getComponent<RelicComponent>());
    }
    m_entities.forEach([&](Entity& zone) {
        if (zone.getTag() != "dim_residue") return;
        if (!zone.hasComponent<HealthComponent>()) return;
        auto& zHP = zone.getComponent<HealthComponent>();
        zHP.currentHP -= dt;
        if (zHP.currentHP <= 0) {
            zone.destroy();
            return;
        }
        // Only deal damage in matching dimension
        if (zone.dimension != 0 && zone.dimension != curDim) return;
        auto& zt = zone.getComponent<TransformComponent>();
        Vec2 zCenter = zt.getCenter();
        // Damage enemies in range
        m_entities.forEach([&](Entity& enemy) {
            if (enemy.getTag().find("enemy") == std::string::npos) return;
            if (enemy.dimension != 0 && enemy.dimension != curDim) return;
            if (!enemy.hasComponent<TransformComponent>() || !enemy.hasComponent<HealthComponent>()) return;
            auto& et = enemy.getComponent<TransformComponent>();
            float dx = et.getCenter().x - zCenter.x;
            float dy = et.getCenter().y - zCenter.y;
            if (std::sqrt(dx * dx + dy * dy) < 48.0f) {
                enemy.getComponent<HealthComponent>().takeDamage(residueDps * dt);
            }
        });
    });

    // Ambient dimension dust (subtle atmospheric particles)
    m_ambientDustTimer += dt;
    if (m_ambientDustTimer >= 0.3f) {
        m_ambientDustTimer = 0;
        if (m_player) {
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            SDL_Color dustColor = (m_dimManager.getCurrentDimension() == 1)
                ? SDL_Color{100, 140, 220, 80}   // Dim A: blue dust
                : SDL_Color{220, 120, 80, 80};   // Dim B: warm dust
            m_particles.ambientDust(pPos, dustColor, 300.0f);
        }
    }

    // Level complete transition
    if (m_levelComplete) {
        m_levelCompleteTimer += dt;
        if (m_levelCompleteTimer >= 2.0f) {
            roomsCleared++;

            if (m_voidSovereignDefeated) {
                endRun();
                return;
            }

            m_currentDifficulty++;
            m_runSeed += 100;

            // Pick new theme pair every 3 levels
            if (m_currentDifficulty % 3 == 0) {
                auto themes = WorldTheme::getRandomPair(m_runSeed);
                m_themeA = themes.first;
                m_themeB = themes.second;
                m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
                AudioManager::instance().playThemeAmbient(static_cast<int>(m_themeA.id));
            }

            // Open shop between levels (every level except first)
            game->setShopDifficulty(m_currentDifficulty);
            m_levelComplete = false;
            m_levelCompleteTimer = 0;
            m_pendingLevelGen = true;
            game->pushState(StateID::Shop);
            return; // Don't process further this frame; resume after shop closes
        }
    }

    // (pendingLevelGen moved to top of update to prevent stale state bugs)

    // Collapse mechanic
    if (m_collapsing) {
        m_collapseTimer += dt;
        float urgency = m_collapseTimer / m_collapseMaxTime;
        if (urgency > 0.5f) {
            m_camera.shake(urgency * 3.0f, 0.1f);
        }
        if (m_collapseTimer >= m_collapseMaxTime && !m_playerDying) {
            m_playerDying = true;
            m_deathCause = 3; // Collapse
            m_deathSequenceTimer = m_deathSequenceDuration;
            AudioManager::instance().play(SFX::PlayerDeath);
            m_camera.shake(20.0f, 0.8f);
            if (m_player) {
                Vec2 deathPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(deathPos, 30, {255, 100, 30, 255}, 250.0f, 5.0f);
                m_particles.burst(deathPos, 20, {255, 50, 20, 255}, 300.0f, 6.0f);
            }
            m_hud.triggerDamageFlash();
            return;
        }
    }

    // FIX: Decay exit locked hint timer
    if (m_exitLockedHintTimer > 0) m_exitLockedHintTimer -= dt;
    if (m_riftDimensionHintTimer > 0) m_riftDimensionHintTimer -= dt;

    // Track dash count for achievement
    if (m_player && m_player->isDashing && m_player->dashTimer >= m_player->dashDuration - 0.02f) {
        m_dashCount++;
    }

    // Consume shard pickups from item drops
    if (m_player && m_player->riftShardsCollected > 0) {
        int shards = m_player->riftShardsCollected;
        float shardMult = game->getRunBuffSystem().getShardMultiplier() * m_achievementShardMult;
        if (m_player->getEntity()->hasComponent<RelicComponent>())
            shardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
        int finalShards = static_cast<int>(shards * shardMult);
        shardsCollected += finalShards;
        game->getUpgradeSystem().addRiftShards(finalShards);
        m_player->riftShardsCollected = 0;

        // Pickup feedback: purple particles + floating shard count
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_particles.burst(pPos, 6, {180, 130, 255, 220}, 60.0f, 2.5f);
        FloatingDamageNumber num;
        num.position = {pPos.x + (std::rand() % 10 - 5.0f), pPos.y - 10.0f};
        num.value = static_cast<float>(finalShards);
        num.isShard = true;
        num.lifetime = 0.9f;
        num.maxLifetime = 0.9f;
        num.isPlayerDamage = false;
        m_damageNumbers.push_back(num);
    }

    // Consume buff pickup effects (shield/speed/damage)
    if (m_player) {
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        auto emitBuffPickup = [&](SDL_Color color, const char* label) {
            m_particles.burst(pPos, 10, color, 80.0f, 3.0f);
            FloatingDamageNumber num;
            num.position = {pPos.x, pPos.y - 16.0f};
            num.value = 0;
            num.isBuff = true;
            num.buffText = label;
            num.lifetime = 1.2f;
            num.maxLifetime = 1.2f;
            num.isPlayerDamage = false;
            m_damageNumbers.push_back(num);
        };
        if (m_player->pickupShieldPending) {
            emitBuffPickup({100, 180, 255, 220}, "SHIELD");
            m_player->pickupShieldPending = false;
        }
        if (m_player->pickupSpeedPending) {
            emitBuffPickup({255, 255, 80, 220}, "SPEED UP");
            m_player->pickupSpeedPending = false;
        }
        if (m_player->pickupDamagePending) {
            emitBuffPickup({255, 80, 80, 220}, "DMG UP");
            m_player->pickupDamagePending = false;
        }
        if (m_player->weaponPickupPending >= 0) {
            auto wid = static_cast<WeaponID>(m_player->weaponPickupPending);
            bool melee = WeaponSystem::isMelee(wid);
            SDL_Color wColor = melee ? SDL_Color{255, 180, 60, 220} : SDL_Color{60, 200, 255, 220};
            emitBuffPickup(wColor, WeaponSystem::getWeaponName(wid));
            m_camera.shake(10.0f, 0.3f);
            m_player->weaponPickupPending = -1;
        }
    }

    // Consume kill tracking from combat system
    if (m_combatSystem.killCount > 0) {
        enemiesKilled += m_combatSystem.killCount;
        game->getUpgradeSystem().totalEnemiesKilled += m_combatSystem.killCount;
        m_screenEffects.triggerKillFlash();

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

                // BloodRift: RiftBlade + BloodFrenzy — melee kills under 50% HP heal 5
                float bloodRiftHeal = RelicSynergy::getBloodRiftHeal(
                    relics, combat.currentMelee, pHP.getPercent());
                if (bloodRiftHeal > 0) {
                    pHP.heal(bloodRiftHeal);
                }

                // EntropyBeam: VoidBeam + EntropyAnchor — kills reduce entropy by 5%
                if (RelicSynergy::isEntropyBeamActive(relics, combat.currentRanged) &&
                    combat.currentAttack == AttackType::Ranged) {
                    m_entropy.reduceEntropy(m_entropy.getMaxEntropy() * 0.05f);
                }

                // StormScatter: RiftShotgun + ChainLightning — kills chain to 2 extra enemies
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

void PlayState::checkRiftInteraction() {
    if (!m_player || !m_level) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto rifts = m_level->getRiftPositions();
    int currentDim = m_dimManager.getCurrentDimension();

    m_nearRiftIndex = -1;
    float nearestRiftDist = 60.0f;
    for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
        // FIX: Skip already-repaired rifts
        if (m_repairedRiftIndices.count(i)) continue;
        float dx = playerPos.x - rifts[i].x;
        float dy = playerPos.y - rifts[i].y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearestRiftDist) {
            nearestRiftDist = dist;
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
        if (m_currentDifficulty <= 1) {
            puzzleType = 0; // Always Timing for first levels
        } else if (m_currentDifficulty <= 3) {
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
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 50.0f) {
        // FIX: Exit only works after all rifts are repaired (collapse started).
        // This enforces the core game loop: repair rifts -> collapse -> escape.
        if (!m_collapsing) {
            // Show hint to player that exit is locked
            m_exitLockedHintTimer = 2.0f;
            return;
        }
        m_levelComplete = true;
        m_levelCompleteTimer = 0;
        // Flawless Floor: completed level without taking damage
        if (!m_tookDamageThisLevel) {
            game->getAchievements().unlock("flawless_floor");
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
        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
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
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // Raw damage (same calc as overlay)
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

    // Raw attack speed
    float rawSpd = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) rawSpd += 0.15f;
    }
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0)
        rawSpd += relics.riftConduitStacks * 0.10f;
    float clampedSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    float baseCD = 0.5f;
    float cdMult = RelicSystem::getSwitchCooldownMult(relics);
    float finalCD = m_dimManager.switchCooldown;

    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
    });

    float stabRate = RelicSynergy::getStabilityDmgPerSec(relics);
    float stabMax = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
    float stabPct = std::min(relics.stabilityTimer * stabRate, stabMax) * 100.0f;
    int synergyCount = RelicSynergy::getActiveSynergyCount(relics);

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
        std::fprintf(f, "seed,difficulty,class,time,dim,synergies,"
            "dmg_raw,dmg_clamped,atk_raw,atk_clamped,"
            "switch_base,switch_mult,switch_final,"
            "voidhunger_pct,voidres_icd,"
            "residue_zones,residue_spawn_icd,"
            "stability_timer,stability_pct,"
            "conduit_stacks,conduit_timer\n");
    }

    std::fprintf(f,
        "%d,%d,%s,%.1f,%d,%d,"
        "%.3f,%.3f,%.3f,%.3f,"
        "%.2f,%.2f,%.2f,"
        "%.1f,%.2f,"
        "%d,%.2f,"
        "%.1f,%.1f,"
        "%d,%.1f\n",
        m_runSeed, m_currentDifficulty, className, m_runTime, curDim, synergyCount,
        rawDmg, clampedDmg, rawSpd, clampedSpd,
        baseCD, cdMult, finalCD,
        relics.voidHungerBonus * 100.0f, std::max(0.0f, relics.voidResonanceProcCD),
        zoneCount, std::max(0.0f, relics.dimResidueSpawnCD),
        relics.stabilityTimer, stabPct,
        relics.riftConduitStacks, std::max(0.0f, relics.riftConduitTimer));

    std::fclose(f);
}






void PlayState::updateDamageNumbers(float dt) {
    for (auto& dn : m_damageNumbers) {
        dn.lifetime -= dt;
        dn.position.y -= 60.0f * dt; // Float upward
    }
    // Remove expired
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
        if (e.getTag().find("enemy") != std::string::npos) aliveEnemies++;
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
        int waveEliteChance = (m_ngPlusTier >= 2) ? 30 : 15; // NG+2: doubled elite rate
        for (auto& sp : m_spawnWaves[m_currentWave]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            // Theme-specific variant
            applyThemeVariant(e, sp.dimension);
            // NG+ scaling on wave enemies
            applyNGPlusModifiers(e);
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && e.getComponent<AIComponent>().element == EnemyElement::None
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Elite modifier in wave spawns (NG+2 doubles rate)
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < waveEliteChance) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 9);
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




void PlayState::spawnBoss() {
    if (!m_level) return;

    // Spawn boss near the center of the level
    Vec2 exitPos = m_level->getExitPoint();
    Vec2 spawnPos = {exitPos.x - 200.0f, exitPos.y - 64.0f};

    int dim = m_dimManager.getCurrentDimension();
    int bossDiff = m_currentDifficulty;
    if (g_selectedDifficulty == GameDifficulty::Hard) bossDiff += 1;

    // Boss selection based on difficulty
    int bossIndex = m_currentDifficulty / 3; // 0-based boss encounter index
    if (m_currentDifficulty >= 11 && bossIndex >= 5) {
        // Entropy Incarnate at difficulty 11+ (after Void Sovereign rotation)
        Enemy::createEntropyIncarnate(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
        m_screenEffects.triggerBossIntro("ENTROPY INCARNATE");
    } else if (m_currentDifficulty >= 10 && bossIndex >= 4) {
        // Spawn Void Sovereign as final boss at difficulty 10+
        Enemy::createVoidSovereign(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
        m_screenEffects.triggerBossIntro("VOID SOVEREIGN");
    } else {
        // Rotate 4 boss types: Guardian -> Wyrm -> Architect -> Temporal Weaver
        int bossType = (bossIndex - 1) % 4;
        if (bossType == 3) {
            Enemy::createTemporalWeaver(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
            m_screenEffects.triggerBossIntro("TEMPORAL WEAVER");
        } else if (bossType == 2) {
            Enemy::createDimensionalArchitect(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
            m_screenEffects.triggerBossIntro("DIMENSIONAL ARCHITECT");
        } else if (bossType == 1) {
            Enemy::createVoidWyrm(m_entities, {spawnPos.x, spawnPos.y - 40.0f}, dim, bossDiff);
            m_screenEffects.triggerBossIntro("VOID WYRM");
        } else {
            Enemy::createBoss(m_entities, spawnPos, dim, bossDiff);
            m_screenEffects.triggerBossIntro("RIFT GUARDIAN");
        }
    }
}


void PlayState::applyRunBuffs() {
    if (!m_player) return;
    auto& buffs = game->getRunBuffSystem();

    // HP boost
    if (buffs.hasBuff(RunBuffID::MaxHPBoost)) {
        auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
        hp.maxHP += buffs.getMaxHPBoost();
        hp.currentHP = hp.maxHP;
    }

    // Crit bonus
    float extraCrit = buffs.getCritBonus();
    if (extraCrit > 0) {
        m_combatSystem.setCritChance(game->getUpgradeSystem().getCritChance() + extraCrit);
    }

    // Ability cooldown reduction
    if (buffs.hasBuff(RunBuffID::CooldownReduction) && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float mult = buffs.getAbilityCDMultiplier();
        abil.abilities[0].cooldown *= mult;
        abil.abilities[1].cooldown *= mult;
        abil.abilities[2].cooldown *= mult;
    }

    // Lifesteal
    m_combatSystem.setLifesteal(buffs.getLifesteal());

    // Element weapon (0=none, 1=fire, 2=ice, 3=electric)
    int elemType = 0;
    if (buffs.hasFireWeapon()) elemType = 1;
    else if (buffs.hasIceWeapon()) elemType = 2;
    else if (buffs.hasElectricWeapon()) elemType = 3;
    m_combatSystem.setElementWeapon(elemType);

    // Dash refresh on kill
    m_combatSystem.setDashRefreshOnKill(buffs.hasDashRefresh());

    // Entropy shield: reduce entropy gain
    m_entropy.entropyGainMultiplier = buffs.getEntropyGainMultiplier();
}






void PlayState::endRun() {
    finalizeRun(false);
}

void PlayState::abandonRun() {
    finalizeRun(true);
}

void PlayState::populateRunSummary(int runShards, bool isNewRecord) {
    if (auto* summary = dynamic_cast<RunSummaryState*>(game->getState(StateID::RunSummary))) {
        summary->enemiesKilled = enemiesKilled;
        summary->riftsRepaired = riftsRepaired;
        summary->roomsCleared = roomsCleared;
        summary->shardsEarned = runShards;
        summary->isNewRecord = isNewRecord;
        summary->runTime = m_runTime;
        summary->playerClass = g_selectedClass;
        summary->difficulty = m_currentDifficulty;
        summary->bestCombo = m_bestCombo;
        summary->deathCause = m_deathCause;
        summary->ngPlusTier = m_ngPlusTier;
        summary->relicsCollected = 0;
        if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
            summary->relicsCollected = static_cast<int>(
                m_player->getEntity()->getComponent<RelicComponent>().relics.size());
        }
        if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
            auto& cb = m_player->getEntity()->getComponent<CombatComponent>();
            summary->meleeWeapon = cb.currentMelee;
            summary->rangedWeapon = cb.currentRanged;
        }
        summary->peakDmgRaw = m_balanceStats.peakDmgRaw;
        summary->peakDmgClamped = m_balanceStats.peakDmgClamped;
        summary->peakSpdRaw = m_balanceStats.peakSpdRaw;
        summary->peakSpdClamped = m_balanceStats.peakSpdClamped;
        summary->cdFloorPercent = (m_balanceStats.activePlayTime > 0)
            ? (m_balanceStats.cdFloorTime / m_balanceStats.activePlayTime * 100.0f) : 0;
        summary->voidResProcs = m_combatSystem.voidResonanceProcs;
        summary->peakResidueZones = m_balanceStats.peakResidueZones;
        summary->peakVoidHunger = m_balanceStats.peakVoidHunger * 100.0f;
        summary->finalVoidHunger = m_balanceStats.finalVoidHunger * 100.0f;
    }
}

void PlayState::finalizeRun(bool abandoned) {
    if (abandoned) {
        game->changeState(StateID::Menu);
        return;
    }

    // Playtest mode: intercept endRun to restart instead of changing state
    if (m_playtest) {
        playtestOnDeath();
        return;
    }

    auto& upgrades = game->getUpgradeSystem();
    bool isNewRecord = (roomsCleared > upgrades.bestRoomReached && roomsCleared > 0);
    upgrades.totalRuns++;
    upgrades.addRunRecord(roomsCleared, enemiesKilled, riftsRepaired,
                          shardsCollected, m_currentDifficulty,
                          m_bestCombo, m_runTime, static_cast<int>(g_selectedClass), m_deathCause);
    upgrades.bestRoomReached = std::max(upgrades.bestRoomReached, roomsCleared);

    // Final unlock check at run end (catches anything missed mid-run)
    // Berserker: 50 total kills across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Berserker) &&
        upgrades.totalEnemiesKilled >= 50) {
        ClassSystem::unlock(PlayerClass::Berserker);
    }
    // Phantom: completed floor 3 (difficulty went past 3)
    if (!ClassSystem::isUnlocked(PlayerClass::Phantom) &&
        m_currentDifficulty > 3) {
        ClassSystem::unlock(PlayerClass::Phantom);
    }
    // Technomancer: 30 rifts repaired across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Technomancer) &&
        upgrades.totalRiftsRepaired >= 30) {
        ClassSystem::unlock(PlayerClass::Technomancer);
    }
    // VoidHammer: defeated any boss
    if (!WeaponSystem::isUnlocked(WeaponID::VoidHammer) && m_bossDefeated) {
        WeaponSystem::unlock(WeaponID::VoidHammer);
    }
    // PhaseDaggers: 10-hit combo
    if (!WeaponSystem::isUnlocked(WeaponID::PhaseDaggers) && m_bestCombo >= 10) {
        WeaponSystem::unlock(WeaponID::PhaseDaggers);
    }
    // VoidBeam: reach floor 5
    if (!WeaponSystem::isUnlocked(WeaponID::VoidBeam) && m_currentDifficulty >= 5) {
        WeaponSystem::unlock(WeaponID::VoidBeam);
    }
    // GrapplingHook: 5 dash kills in a run
    if (!WeaponSystem::isUnlocked(WeaponID::GrapplingHook) && m_dashKillsThisRun >= 5) {
        WeaponSystem::unlock(WeaponID::GrapplingHook);
    }

    // Check milestone rewards
    auto milestone = upgrades.checkMilestones();
    if (milestone.bonusShards > 0) {
        upgrades.addRiftShards(milestone.bonusShards);
    }

    // Save bestiary progress
    Bestiary::save("bestiary_save.dat");

    // Save lore progress
    if (auto* lore = game->getLoreSystem()) {
        lore->save("riftwalker_lore.dat");
    }

    // Ascension: award Rift Cores based on difficulty and ascension level
    int cores = m_currentDifficulty * (1 + AscensionSystem::currentLevel);
    AscensionSystem::riftCores += cores;
    AscensionSystem::save("ascension_save.dat");

    // Void Sovereign defeated -> NG+ and Ending sequence
    if (m_voidSovereignDefeated) {
        g_newGamePlusLevel++;
        // NG+ unlock: beating Normal unlocks NG+1, beating NG+X unlocks NG+X+1 (up to NG+5)
        int nextTier = m_ngPlusTier + 1;
        if (nextTier <= 5) {
            upgrades.unlockNGPlus(nextTier);
        }
        if (auto* lore = game->getLoreSystem()) {
            lore->discover(LoreID::FinalRevelation);
            lore->save("riftwalker_lore.dat");
        }
        // Set ending stats
        if (auto* ending = dynamic_cast<EndingState*>(game->getState(StateID::Ending))) {
            ending->finalScore = enemiesKilled * 10 + roomsCleared * 50 + riftsRepaired * 100;
            ending->totalKills = enemiesKilled;
            ending->maxDifficulty = m_currentDifficulty;
            int relicCount = 0;
            if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
                relicCount = static_cast<int>(m_player->getEntity()->getComponent<RelicComponent>().relics.size());
            }
            ending->relicsCollected = relicCount;
            ending->totalTime = m_runTime;
        }
        game->changeState(StateID::Ending);
        return;
    }

    // Record daily leaderboard entry when this was a daily run
    int runScore = calculateRunScore(roomsCleared, enemiesKilled, riftsRepaired,
                                     shardsCollected, m_bestCombo, m_deathCause);
    bool isNewDailyBest = false;
    if (m_isDailyRun) {
        DailyRun dailyRun;
        dailyRun.load("riftwalker_daily.dat");
        int prevBest = dailyRun.getTodayBest();

        DailyLeaderboardEntry entry;
        entry.score       = runScore;
        entry.floors      = roomsCleared;
        entry.kills       = enemiesKilled;
        entry.rifts       = riftsRepaired;
        entry.shards      = shardsCollected;
        entry.bestCombo   = m_bestCombo;
        entry.runTime     = m_runTime;
        entry.playerClass = static_cast<int>(g_selectedClass);
        entry.deathCause  = m_deathCause;
        auto dateStr = DailyRun::getTodayDate();
        strncpy(entry.date, dateStr.c_str(), sizeof(entry.date) - 1);
        entry.date[sizeof(entry.date) - 1] = 0; // null terminator

        dailyRun.addEntry(entry);
        dailyRun.save("riftwalker_daily.dat");

        isNewDailyBest = (runScore > prevBest);

        // Pre-configure DailyLeaderboardState with highlight info for this run
        if (auto* lb = dynamic_cast<DailyLeaderboardState*>(
                game->getState(StateID::DailyLeaderboard))) {
            lb->highlightScore = runScore;
            lb->isNewBest      = isNewDailyBest;
        }
    }

    populateRunSummary(shardsCollected, isNewRecord);
    // Pass daily run score data to RunSummaryState for prominent display
    if (auto* summary = dynamic_cast<RunSummaryState*>(game->getState(StateID::RunSummary))) {
        summary->dailyScore     = runScore;
        summary->isDailyRun     = m_isDailyRun;
        summary->isNewDailyBest = isNewDailyBest;
    }
    game->changeState(StateID::RunSummary);
}

void PlayState::applyChallengeModifiers() {
    if (g_activeChallenge == ChallengeID::None) return;
    const auto& cd = ChallengeMode::getChallengeData(g_activeChallenge);

    if (cd.playerMaxHP > 0 && m_player) {
        auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
        hp.maxHP = cd.playerMaxHP;
        hp.currentHP = cd.playerMaxHP;
    }
    if (cd.timeLimit > 0) {
        m_challengeTimer = cd.timeLimit;
    }

    // Apply mutators at run start
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::LowGravity) {
            // Reduce gravity for all entities (handled in physics)
        }
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
            m_combatChallenge.name = "Aerial Kill";
            m_combatChallenge.desc = "Kill an enemy while airborne";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 20;
            break;
        case CombatChallengeType::MultiKill:
            m_combatChallenge.name = "Triple Kill";
            m_combatChallenge.desc = "Kill 3 enemies within 4 seconds";
            m_combatChallenge.targetCount = 3;
            m_combatChallenge.timer = 4.0f;
            m_combatChallenge.maxTimer = 4.0f;
            m_combatChallenge.shardReward = 35;
            break;
        case CombatChallengeType::DashKill:
            m_combatChallenge.name = "Dash Slayer";
            m_combatChallenge.desc = "Kill an enemy with a dash attack";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 20;
            break;
        case CombatChallengeType::ComboFinisher:
            m_combatChallenge.name = "Combo Finisher";
            m_combatChallenge.desc = "Kill with a 3rd combo hit";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 25;
            break;
        case CombatChallengeType::ChargedKill:
            m_combatChallenge.name = "Heavy Hitter";
            m_combatChallenge.desc = "Kill with a charged attack";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 25;
            break;
        case CombatChallengeType::NoDamageWave:
            m_combatChallenge.name = "Untouchable";
            m_combatChallenge.desc = "Clear wave without taking damage";
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
            if (e.getTag().find("enemy") != std::string::npos) aliveEnemies++;
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


// ============ NPC System ============





// ============ Ascension System ============

void PlayState::applyAscensionModifiers() {
    if (AscensionSystem::currentLevel <= 0) return;

    // Lore: The Ascended - triggered on first ascended run
    if (auto* lore = game->getLoreSystem()) {
        if (!lore->isDiscovered(LoreID::AscendedOnes)) {
            lore->discover(LoreID::AscendedOnes);
        }
    }

    const auto& asc = AscensionSystem::getLevel(AscensionSystem::currentLevel);

    if (!m_player) return;
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();

    // Bonus: start shards
    if (asc.startShardBonus > 0) {
        int bonus = static_cast<int>(asc.startShardBonus * 100);
        game->getUpgradeSystem().addRiftShards(bonus);
    }

    // Bonus: shop discount (stored for later use in shop)
    // Bonus: shard gain multiplier (applied in combat loot)
    // Bonus: abilities start at 50% CD
    if (asc.abilitiesStartHalfCD && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        for (int i = 0; i < 3; i++) {
            abil.abilities[i].cooldownTimer = abil.abilities[i].cooldown * 0.5f;
        }
    }

    // Bonus: keep weapon upgrade from previous run (mark flag)
    // (Would require meta-save, simplified: +10% damage)
    if (asc.keepWeaponUpgrade) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        combat.meleeAttack.damage *= 1.1f;
        combat.rangedAttack.damage *= 1.1f;
    }

    // Malus: enemies get buffs (applied per-entity in spawnEnemies isn't practical,
    // so we scale globally via combat system multipliers)
    // These are applied when enemies spawn - we modify the base stats in spawnEnemies
    // For now, store on entropy system's passive modifier as a global scaling hint

    // Malus: enemy HP/DMG/Speed scaling - applied to enemies after spawn
    m_entities.forEach([&](Entity& e) {
        if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>()) return;
        auto& eHP = e.getComponent<HealthComponent>();
        eHP.maxHP *= asc.enemyHPMult;
        eHP.currentHP = eHP.maxHP;
        if (e.hasComponent<CombatComponent>()) {
            e.getComponent<CombatComponent>().meleeAttack.damage *= asc.enemyDMGMult;
        }
        if (e.hasComponent<PhysicsBody>()) {
            // Speed scaling for chase/patrol
            auto& ai = e.getComponent<AIComponent>();
            ai.chaseSpeed *= asc.enemySpeedMult;
            ai.patrolSpeed *= asc.enemySpeedMult;
        }
    });
}

// ====== Event Chain System ======






// ─── Kill Feed ───




// ─── Unlock Notifications ───




// ─── Unlock Condition Checks (called each frame after kill events) ───

