// PlayStateRunLifecycle.cpp -- Split from PlayState.cpp (run lifecycle: start, level gen, upgrades, end)
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
#include <algorithm>
#include <string>

extern bool g_smokeRegression;
extern int g_forcedRunSeed;
extern int g_smokeCompletedFloor;
extern int g_smokeFailureCode;
extern char g_smokeFailureReason[256];

namespace {
constexpr bool kUseAiFinalBackgroundArtTest = false;
constexpr const char* kDimAFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dima_run01_s1103_fin.png";
constexpr const char* kDimBFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dimb_run01_s2103_fin.png";
constexpr const char* kBossFinalBackgroundPath =
    "assets/ai/finals/boss/run01/rw_boss_run01_f03_s3120_fin.png";
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
    // Seed mt19937-based RNG in game systems for reproducibility
    RelicSystem::seed(static_cast<uint32_t>(m_runSeed));
    game->getRunBuffSystem().seed(static_cast<uint32_t>(m_runSeed));
    if (g_smokeRegression) {
        std::srand(static_cast<unsigned int>(m_runSeed));
    }
    g_dailyRunActive = false; // Reset flag
    enemiesKilled = 0;
    riftsRepaired = 0;
    roomsCleared = 0;
    shardsCollected = 0;
    m_activeQuest = NPCQuest{};
    m_questCompleteTimer = 0;
    m_questKillSnapshot = 0;
    m_questRiftSnapshot = 0;
    m_collapsing = false;
    m_riftDimensionHintTimer = 0;
    m_riftDimensionHintRequiredDim = 0;
    m_tutorialTimer = 0;
    m_tutorialHintIndex = 0;
    m_tutorialHintShowTimer = 0;
    std::memset(m_tutorialHintDone, 0, sizeof(m_tutorialHintDone));
    // Tutorial only active on very first run, disabled for automated modes
    m_tutorialActive = (game->getUpgradeSystem().totalRuns == 0)
                       && !m_smokeTest && !m_playtest;
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
    m_usedNonScytheMelee = false;
    m_questsCompletedTotal = 0;
    m_dimensionSwitches = 0;
    m_totalDamageTaken = 0;
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

    // Zone-based theme pair: Zone 0 themes for first run
    auto themes = WorldTheme::getZonePair(0, m_runSeed);
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
    m_killStreakTimer = 0;
    m_killStreakCount = 0;
    m_killStreakDisplayTimer = 0;
    m_killStreakText.clear();
    m_playerDying = false;
    m_deathSequenceTimer = 0;
    m_deathCause = 0;

    // Reset dynamic level events
    m_dynamicEvent = {};
    m_dynamicEventCooldown = 40.0f;
    m_dynamicEventsTriggered = 0;
    m_dynamicEventsEnabled = true;
    m_dynamicEventAnnouncementTimer = 0;

    generateLevel();
    m_combatSystem.setPlayer(m_player.get());
    m_combatSystem.setDimensionManager(&m_dimManager);
    m_combatSystem.setSuitEntropy(&m_entropy);
    m_hud.setCombatSystem(&m_combatSystem);
    m_hud.setNGPlusTier(m_ngPlusTier);
    m_aiSystem.setLevel(m_level.get());
    m_aiSystem.setPlayer(m_player.get());

    // Activate run intro overlay (skip for bots and subsequent NG+ runs)
    m_runIntroActive = true;
    m_runIntroTimer = 0;
    if (m_playtest || m_smokeTest) m_runIntroActive = false;
}

void PlayState::generateLevel() {
    m_entities.clear();
    m_particles.clear();
    m_damageNumbers.clear();
    m_damageIndicators.clear();
    m_trails.clear();

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

    // Boss every 6 floors (zone boss) or every 3rd floor in zone (mid-boss)
    m_isBossLevel = isBossFloor(m_currentDifficulty) || isMidBossFloor(m_currentDifficulty);
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
    m_usedNonScytheMelee = false; // Reset per-floor for entropy_master achievement

    // Dynamic level events: disable on boss levels, reset cooldown
    m_dynamicEventsEnabled = !m_isBossLevel;
    m_dynamicEvent = {};
    m_dynamicEventCooldown = 30.0f + static_cast<float>(std::rand() % 31);
    m_dynamicEventAnnouncementTimer = 0;

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

    // Zone-based wave sizing: Zone 0=3-4, Zone 1=4-5, Zone 2=5-6, Zone 3=6-7, Zone 4=7-8
    int waveZone = getZone(m_currentDifficulty);
    int waveFloorInZone = getFloorInZone(m_currentDifficulty);
    int waveSize = 3 + waveZone + (waveFloorInZone > 3 ? 1 : 0);
    if (isBreatherFloor(m_currentDifficulty)) waveSize = std::max(waveSize - 1, 3);
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
    // Mini-boss: zone-scaled chance (not on boss levels)
    bool miniBossSpawned = false;

    // Zone-based elite and mini-boss chances, boosted by NG+ tier
    auto zoneScale = getZoneScaling(m_currentDifficulty);
    int eliteChance = static_cast<int>(zoneScale.eliteChance);
    int miniBossChance = static_cast<int>(zoneScale.miniBossChance);
    if (m_ngPlusTier >= 2) eliteChance = std::min(eliteChance + 15, 60);
    if (m_ngPlusTier >= 4) miniBossChance = 100;

    if (!m_spawnWaves.empty()) {
        for (auto& sp : m_spawnWaves[0]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            // Initial wave: no spawn flicker (enemies already present when level starts)
            if (e.hasComponent<AIComponent>()) e.getComponent<AIComponent>().spawnTimer = 0;
            // Theme-specific variant: stat mods, element affinity, color tint
            applyThemeVariant(e, sp.dimension);
            // NG+ HP/DMG scaling applied after theme variant
            applyNGPlusModifiers(e);
            // Elemental variant chance: 25% from Zone 1+, only if theme didn't set element
            if (getZone(m_currentDifficulty) >= 1 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && e.getComponent<AIComponent>().element == EnemyElement::None
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Zone-based elite modifier
            if (getZone(m_currentDifficulty) >= 1 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < eliteChance) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 9);
                Enemy::makeElite(e, mod);
            }
            // Zone-based mini-boss chance (not on boss floors)
            if (!miniBossSpawned && !m_isBossLevel
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

    // Zone-based difficulty scaling (logarithmic growth)
    {
        auto scaling = getZoneScaling(m_currentDifficulty);
        hp.maxHP *= scaling.hpMult;
        hp.currentHP = hp.maxHP;
        combat.meleeAttack.damage *= scaling.dmgMult;
        combat.rangedAttack.damage *= scaling.dmgMult;
        ai.chaseSpeed *= scaling.speedMult;
        ai.patrolSpeed *= scaling.speedMult;
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
    // BALANCE R2: Base HP 120 -> 150 (playtest: 100% death rate at F5-6, need more survivability)
    hp.maxHP = 150.0f + upgrades.getMaxHPBonus() + achBonus.maxHPBonus;
    hp.currentHP = hp.maxHP;
    hp.armor = upgrades.getArmorBonus() + achBonus.armorBonus;

    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    // BALANCE: Base melee 20 -> 25, ranged 12 -> 15 (matches Player.cpp change)
    combat.meleeAttack.damage = 25.0f * upgrades.getMeleeDamageMultiplier() * achBonus.meleeDamageMult * achBonus.allDamageMult;
    combat.rangedAttack.damage = 15.0f * upgrades.getRangedDamageMultiplier() * achBonus.rangedDamageMult * achBonus.allDamageMult;

    m_dimManager.switchCooldown = 0.5f * upgrades.getSwitchCooldownMultiplier() * achBonus.switchCooldownMult;
    m_entropy.passiveDecay = upgrades.getEntropyDecay() * achBonus.entropyDecayMult;
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

void PlayState::spawnBoss() {
    if (!m_level) return;

    // Spawn boss near the center of the level
    Vec2 exitPos = m_level->getExitPoint();
    Vec2 spawnPos = {exitPos.x - 200.0f, exitPos.y - 64.0f};

    int dim = m_dimManager.getCurrentDimension();
    int bossDiff = m_currentDifficulty;
    if (g_selectedDifficulty == GameDifficulty::Hard) bossDiff += 1;

    // Zone-based boss selection:
    // Mid-boss floors (3, 9, 15, 21, 27): lighter boss from rotation
    // Zone boss floors (6, 12, 18, 24, 30): full boss
    int zone = getZone(m_currentDifficulty);
    bool isMidBoss = isMidBossFloor(m_currentDifficulty);

    if (m_currentDifficulty >= 30) {
        // Floor 30: final confrontation — Void Sovereign
        Enemy::createVoidSovereign(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
        m_screenEffects.triggerBossIntro("VOID SOVEREIGN", "The last chain binding the Rift");
    } else if (zone == 4 && isMidBoss) {
        // Floor 27 mid-boss: Entropy Incarnate as penultimate challenge
        Enemy::createEntropyIncarnate(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
        m_screenEffects.triggerBossIntro("ENTROPY INCARNATE", "A shadow of greater power");
    } else if (isMidBoss) {
        // Mid-boss floors: rotate lighter bosses (Guardian, Wyrm) scaled to zone
        int midBossType = zone % 2;
        if (midBossType == 1) {
            Enemy::createVoidWyrm(m_entities, {spawnPos.x, spawnPos.y - 40.0f}, dim, bossDiff);
            m_screenEffects.triggerBossIntro("VOID WYRM", "A shadow of greater power");
        } else {
            Enemy::createBoss(m_entities, spawnPos, dim, bossDiff);
            m_screenEffects.triggerBossIntro("RIFT GUARDIAN", "A shadow of greater power");
        }
    } else {
        // Zone boss floors (6, 12, 18, 24): each zone has a signature boss
        switch (zone) {
            case 0: // Zone 1 boss (Floor 6): Rift Guardian
                Enemy::createBoss(m_entities, spawnPos, dim, bossDiff);
                m_screenEffects.triggerBossIntro("RIFT GUARDIAN", "The dimension's immune response");
                break;
            case 1: // Zone 2 boss (Floor 12): Dimensional Architect
                Enemy::createDimensionalArchitect(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
                m_screenEffects.triggerBossIntro("DIMENSIONAL ARCHITECT", "It rewrites reality itself");
                break;
            case 2: // Zone 3 boss (Floor 18): Temporal Weaver
                Enemy::createTemporalWeaver(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
                m_screenEffects.triggerBossIntro("TEMPORAL WEAVER", "Past and future converge");
                break;
            case 3: // Zone 4 boss (Floor 24): Entropy Incarnate
                Enemy::createEntropyIncarnate(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
                m_screenEffects.triggerBossIntro("ENTROPY INCARNATE", "Chaos given form and purpose");
                break;
            default: // Zone 5 boss (Floor 30): handled above
                Enemy::createVoidSovereign(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
                m_screenEffects.triggerBossIntro("VOID SOVEREIGN", "The last chain binding the Rift");
                break;
        }
    }

    // Activate boss intro freeze (skip for automated bots)
    m_bossIntroActive = true;
    if (m_smokeTest || m_playtest) {
        m_bossIntroActive = false;
        m_screenEffects.cancelBossIntro();
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

void PlayState::requestRestart() {
    m_pendingRestart = true;
    game->popState(); // Pop the pause overlay so update() runs
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
        summary->dimensionSwitches = m_dimensionSwitches;
        summary->damageTaken = m_totalDamageTaken;
        summary->aerialKills = m_aerialKillsThisRun;
        summary->dashKills = m_dashKillsThisRun;
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
