#include "PlayState.h"
#include "Core/Game.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Components/AbilityComponent.h"
#include "Core/AudioManager.h"
#include "Game/AchievementSystem.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/LoreSystem.h"
#include "Game/DailyRun.h"
#include "States/EndingState.h"
#include "States/RunSummaryState.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

extern bool g_autoSmokeTest;

void PlayState::enter() {
    if (g_autoSmokeTest) {
        m_smokeTest = true;
        m_showDebugOverlay = true;
        SDL_Log("[SMOKE] Smoke test auto-enabled via --smoke flag");
    }
    startNewRun();
}

void PlayState::exit() {
    AudioManager::instance().stopAmbient();
    m_entities.clear();
    m_particles.clear();
    m_player.reset();
    m_level.reset();
    m_activePuzzle.reset();
}

void PlayState::startNewRun() {
    m_entities.clear();
    m_particles.clear();

    m_currentDifficulty = 1;
    m_isDailyRun = g_dailyRunActive;
    m_runSeed = m_isDailyRun ? DailyRun::getTodaySeed() : std::rand();
    g_dailyRunActive = false; // Reset flag
    enemiesKilled = 0;
    riftsRepaired = 0;
    roomsCleared = 0;
    shardsCollected = 0;
    m_collapsing = false;
    m_tutorialTimer = 0;
    m_tutorialHintIndex = 0;
    m_tutorialHintShowTimer = 0;
    std::memset(m_tutorialHintDone, 0, sizeof(m_tutorialHintDone));
    m_hasMovedThisRun = false;
    m_hasJumpedThisRun = false;
    m_hasDashedThisRun = false;
    m_hasAttackedThisRun = false;
    m_dashCount = 0;
    m_pendingLevelGen = false;
    m_showRelicChoice = false;
    m_relicChoices.clear();
    m_relicChoiceSelected = 0;
    m_voidSovereignDefeated = false;
    m_trails.clear();
    m_moveTrailTimer = 0.0f;
    m_runTime = 0.0f;
    m_nearNPCIndex = -1;
    m_showNPCDialog = false;
    m_npcDialogChoice = 0;
    m_balanceStats = {};
    m_combatSystem.voidResonanceProcs = 0;

    // Reset run buffs
    game->getRunBuffSystem().reset();

    // Random theme pair
    auto themes = WorldTheme::getRandomPair(m_runSeed);
    m_themeA = themes.first;
    m_themeB = themes.second;

    m_dimManager = DimensionManager();
    m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
    m_dimManager.particles = &m_particles;
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());

    m_entropy = SuitEntropy();
    m_combatSystem.setParticleSystem(&m_particles);
    m_combatSystem.setCamera(&m_camera);
    m_aiSystem.setParticleSystem(&m_particles);
    m_aiSystem.setCamera(&m_camera);
    m_aiSystem.setCombatSystem(&m_combatSystem);
    m_hitFreezeTimer = 0;
    m_spikeDmgCooldown = 0;

    generateLevel();
    m_combatSystem.setPlayer(m_player.get());
    m_aiSystem.setLevel(m_level.get());
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
    m_player->playerClass = g_selectedClass;
    m_player->applyClassStats();
    applyUpgrades();
    applyAscensionModifiers();

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
    // FIX: Reset per-level rift tracking
    m_repairedRiftIndices.clear();
    m_levelRiftsRepaired = 0;
    // FIX: Reset collapse state for new level
    m_collapsing = false;
    m_collapseTimer = 0;

    // Start ambient music
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
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

    if (!m_spawnWaves.empty()) {
        for (auto& sp : m_spawnWaves[0]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            // Elemental variant chance: 25% at difficulty 3+, type based on RNG
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Elite modifier: 15% chance per enemy at difficulty 3+
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < 15) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 6);
                Enemy::makeElite(e, mod);
            }
            // Mini-boss: promote first eligible enemy
            if (!miniBossSpawned && !m_isBossLevel && m_currentDifficulty >= 2
                && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && std::rand() % 100 < 15) {
                Enemy::makeMiniBoss(e);
                miniBossSpawned = true;
            }
        }
        m_currentWave = 1;
        m_waveActive = true;
        m_waveTimer = m_waveDelay;
    }
}

void PlayState::applyUpgrades() {
    if (!m_player) return;
    auto& upgrades = game->getUpgradeSystem();

    m_player->moveSpeed = 250.0f * upgrades.getMoveSpeedMultiplier();
    m_player->jumpForce = -420.0f * upgrades.getJumpMultiplier();
    m_player->dashCooldown = 0.5f * upgrades.getDashCooldownMultiplier();
    m_player->maxJumps = 2 + upgrades.getExtraJumps();

    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    hp.maxHP = 100.0f + upgrades.getMaxHPBonus();
    hp.currentHP = hp.maxHP;
    hp.armor = upgrades.getArmorBonus();

    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    combat.meleeAttack.damage = 20.0f * upgrades.getMeleeDamageMultiplier();
    combat.rangedAttack.damage = 12.0f * upgrades.getRangedDamageMultiplier();

    m_dimManager.switchCooldown = 0.5f * upgrades.getSwitchCooldownMultiplier();
    m_entropy.passiveDecay = upgrades.getEntropyDecay();
    m_combatSystem.setCritChance(upgrades.getCritChance());
    m_combatSystem.setComboBonus(upgrades.getComboBonus());

    // Ability upgrades
    if (m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float cdMult = upgrades.getAbilityCooldownMultiplier();
        float pwrMult = upgrades.getAbilityPowerMultiplier();
        abil.abilities[0].cooldown = 6.0f * cdMult;
        abil.abilities[1].cooldown = 10.0f * cdMult;
        abil.abilities[2].cooldown = 8.0f * cdMult;
        abil.slamDamage = 60.0f * pwrMult;
        abil.shieldBurstDamage = 25.0f * pwrMult;
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
            auto options = NPCSystem::getDialogOptions(npcs[m_nearNPCIndex].type);
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

    // Track balance stats every frame
    updateBalanceTracking(dt);

    // Smoke test: auto-play for integration testing
    if (m_smokeTest) updateSmokeTest(dt);

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
        if (m_challengeTimer <= 0) {
            endRun(); // Time's up
            return;
        }
    }

    // Mutator: DimensionFlux - auto switch every 15s
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::DimensionFlux) {
            m_mutatorFluxTimer += dt;
            if (m_mutatorFluxTimer >= 15.0f) {
                m_mutatorFluxTimer = 0;
                m_dimManager.switchDimension();
                m_camera.shake(6.0f, 0.2f);
            }
        }
    }

    // Active puzzle takes priority
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->update(dt);
        return;
    }

    auto& input = game->getInput();

    // Dimension switch
    if (input.isActionPressed(Action::DimensionSwitch)) {
        // Check if dimension is locked by Void Sovereign
        bool dimLocked = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_boss" && e.hasComponent<AIComponent>()) {
                auto& bossAi = e.getComponent<AIComponent>();
                if (bossAi.bossType == 4 && bossAi.vsDimLockActive > 0) dimLocked = true;
            }
        });
        if (dimLocked) {
            m_camera.shake(3.0f, 0.1f); // Feedback: can't switch
        } else {
        m_dimManager.switchDimension();
        m_entropy.onDimensionSwitch();
        AudioManager::instance().play(SFX::DimensionSwitch);
        AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
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

        // Voidwalker passive: Rift Affinity - heal on dim-switch
        if (m_player && m_player->playerClass == PlayerClass::Voidwalker) {
            const auto& voidData = ClassSystem::getData(PlayerClass::Voidwalker);
            auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
            hp.heal(voidData.switchHeal);
            if (m_player->particles) {
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_player->particles->burst(pPos, 8, {60, 200, 255, 200}, 60.0f, 2.0f);
            }
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
        } // end else (not dimLocked)
    }

    // Forced dimension switch from entropy
    if (m_entropy.shouldForceSwitch()) {
        m_dimManager.switchDimension();
        m_entropy.clearForceSwitch();
        m_camera.shake(8.0f, 0.3f);
        AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
    }

    m_dimManager.playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_dimManager.update(dt);

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
    }
    m_screenEffects.setEntropy(m_entropy.getEntropy());
    // Check if Void Storm is active
    bool stormActive = false;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "enemy_boss" && e.hasComponent<AIComponent>()) {
            auto& bossAi = e.getComponent<AIComponent>();
            if (bossAi.bossType == 4 && bossAi.vsStormActive > 0) stormActive = true;
        }
    });
    m_screenEffects.setVoidStorm(stormActive);
    m_screenEffects.update(dt);

    // Music system
    {
        int nearEnemies = 0;
        bool bossActive = false;
        Vec2 pPos = m_player ? m_player->getEntity()->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().substr(0, 5) == "enemy") {
                if (e.getTag() == "enemy_boss") bossActive = true;
                auto& et = e.getComponent<TransformComponent>();
                float dx = et.getCenter().x - pPos.x;
                float dy = et.getCenter().y - pPos.y;
                if (dx * dx + dy * dy < 400.0f * 400.0f) nearEnemies++;
            }
        });
        float hp = m_player ? m_player->getEntity()->getComponent<HealthComponent>().getPercent() : 1.0f;
        m_musicSystem.update(dt, nearEnemies, bossActive, hp, m_entropy.getEntropy());
    }

    // Run time tracking
    m_runTime += dt;

    // Player update
    m_player->update(dt, input);

    // Update tile timers (crumbling etc.)
    if (m_level) m_level->updateTiles(dt);

    // Physics - pass current dimension so player (dimension=0) collides with correct tiles
    m_physics.update(m_entities, dt, m_level.get(), m_dimManager.getCurrentDimension());

    // AI
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_aiSystem.update(m_entities, dt, playerPos, m_dimManager.getCurrentDimension());

    // Void Sovereign Phase 3: auto dimension switch
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() != "enemy_boss") return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& bossAi = e.getComponent<AIComponent>();
        if (bossAi.bossType == 4 && bossAi.vsForceDimSwitch) {
            bossAi.vsForceDimSwitch = false;
            m_dimManager.switchDimension();
            m_camera.shake(6.0f, 0.2f);
            AudioManager::instance().play(SFX::DimensionSwitch);
            m_screenEffects.triggerDimensionRipple();
        }
    });

    // Collision
    m_collision.update(m_entities, m_dimManager.getCurrentDimension());

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
    }
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
                    if (m_currentDifficulty >= 10 && bossIdx >= 4) {
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

            // Track Void Sovereign defeat for ending sequence
            if (m_currentDifficulty >= 10 && bossIdx >= 4) {
                m_voidSovereignDefeated = true;
            }

            // Boss kill -> Relic choice (3 from pool)
            showRelicChoice();
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

        // Spike & fire damage (cooldown-based)
        if (m_spikeDmgCooldown <= 0 && m_level->inBounds(footX, footY)) {
            const auto& tile = m_level->getTile(footX, footY, dim);
            if (tile.type == TileType::Spike) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                playerHP.takeDamage(hazardDmg(15.0f));
                m_entropy.addEntropy(5.0f);
                m_spikeDmgCooldown = 0.5f;
                m_camera.shake(6.0f, 0.2f);
                AudioManager::instance().play(SFX::SpikeDamage);
                if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
                    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                    phys.velocity.y = -250.0f;
                }
                m_particles.burst(playerT.getCenter(), 15, {255, 80, 40, 255}, 150.0f, 3.0f);
                m_hud.triggerDamageFlash();
            } else if (tile.type == TileType::Fire) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                playerHP.takeDamage(hazardDmg(10.0f));
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

        // Laser beam damage (separate cooldown via same timer)
        if (m_spikeDmgCooldown <= 0) {
            if (m_level->isInLaserBeam(playerT.getCenter().x, playerT.getCenter().y, dim)) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                playerHP.takeDamage(hazardDmg(20.0f));
                m_entropy.addEntropy(8.0f);
                m_spikeDmgCooldown = 0.3f;
                m_camera.shake(8.0f, 0.25f);
                AudioManager::instance().play(SFX::LaserHit);
                m_particles.burst(playerT.getCenter(), 20, {255, 50, 50, 255}, 200.0f, 3.0f);
                m_hud.triggerDamageFlash();
            }
        }

        // Conveyor belt push (always active, no damage)
        if (m_player->getEntity()->hasComponent<PhysicsBody>() && m_level->inBounds(footX, footY)) {
            int convDir = 0;
            if (m_level->isOnConveyor(footX, footY, dim, convDir)) {
                auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                phys.velocity.x += convDir * 120.0f * dt;
            }
        }

        // Teleporter: teleport player to paired tile
        {
            int centerTX = static_cast<int>(playerT.getCenter().x) / m_level->getTileSize();
            int centerTY = static_cast<int>(playerT.getCenter().y) / m_level->getTileSize();
            if (m_level->inBounds(centerTX, centerTY)) {
                const auto& tile = m_level->getTile(centerTX, centerTY, dim);
                if (tile.type == TileType::Teleporter) {
                    m_teleportCooldown -= dt;
                    if (m_teleportCooldown <= 0) {
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
                playerHP.takeDamage(dotDmg(5.0f));
                m_player->burnDmgTimer = 0.3f;
                m_particles.burst(playerT.getCenter(), 4, {255, 120, 30, 200}, 60.0f, 1.5f);
            }
        }
        if (m_player->isPoisoned()) {
            m_player->poisonDmgTimer -= dt;
            if (m_player->poisonDmgTimer <= 0) {
                playerHP.takeDamage(dotDmg(3.0f));
                m_player->poisonDmgTimer = 0.5f;
                m_particles.burst(playerT.getCenter(), 3, {80, 200, 40, 200}, 40.0f, 1.5f);
            }
        }
    }

    // HUD flash update
    m_hud.updateFlash(dt);

    // Tutorial timer + action tracking
    m_tutorialTimer += dt;
    if (m_player && m_player->getEntity()->hasComponent<PhysicsBody>()) {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        if (std::abs(phys.velocity.x) > 10.0f) m_hasMovedThisRun = true;
        if (!phys.onGround) m_hasJumpedThisRun = true;
    }
    if (m_player && m_player->isDashing) m_hasDashedThisRun = true;
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        if (m_player->getEntity()->getComponent<CombatComponent>().isAttacking) m_hasAttackedThisRun = true;
    }

    // Entropy
    m_entropy.update(dt);
    if (m_entropy.isCritical()) {
        // Suit crash - run over
        AudioManager::instance().play(SFX::SuitEntropyCritical);
        endRun();
        return;
    }

    // Relic timed effects update
    if (m_player->getEntity()->hasComponent<RelicComponent>()) {
        auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
        RelicSystem::updateTimedEffects(relics, dt);

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
            AudioManager::instance().play(SFX::PlayerDeath);
            endRun();
            return;
        }
    }
    skipDeath:

    // Check breakable walls (dash/charged attack destroys them)
    checkBreakableWalls();

    // Check secret room discovery
    checkSecretRoomDiscovery();

    // Check random event interaction
    checkEventInteraction();

    // Check NPC interaction
    checkNPCInteraction();

    // Check rift interaction
    checkRiftInteraction();

    // Check if player reached exit
    checkExitReached();

    // Camera follow player with look-ahead
    Vec2 camTarget = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    Vec2 playerVel = m_player->getEntity()->hasComponent<PhysicsBody>() ?
        m_player->getEntity()->getComponent<PhysicsBody>().velocity : Vec2{0, 0};
    m_camera.follow(camTarget, dt, playerVel);
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
            m_currentDifficulty++;
            roomsCleared++;
            m_runSeed += 100;

            // Pick new theme pair every 3 levels
            if (m_currentDifficulty % 3 == 0) {
                auto themes = WorldTheme::getRandomPair(m_runSeed);
                m_themeA = themes.first;
                m_themeB = themes.second;
                m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
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

    // Generate level after returning from shop
    if (m_pendingLevelGen) {
        m_pendingLevelGen = false;
        applyRunBuffs();
        generateLevel();
    }

    // Collapse mechanic
    if (m_collapsing) {
        m_collapseTimer += dt;
        float urgency = m_collapseTimer / m_collapseMaxTime;
        if (urgency > 0.5f) {
            m_camera.shake(urgency * 3.0f, 0.1f);
        }
        if (m_collapseTimer >= m_collapseMaxTime) {
            endRun();
            return;
        }
    }

    // Track dash count for achievement
    if (m_player && m_player->isDashing && m_player->dashTimer >= m_player->dashDuration - 0.02f) {
        m_dashCount++;
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
                float killHeal = RelicSystem::getKillHeal(relics);
                if (killHeal > 0) {
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
                float vpHeal = RelicSystem::getVoidPactHeal(relics);
                if (vpHeal > 0) {
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
    m_combatSystem.killCount = 0;

    // Achievement checks
    auto& ach = game->getAchievements();
    ach.update(dt);

    if (roomsCleared >= 10) ach.unlock("room_clearer");
    if (roomsCleared >= 20) ach.unlock("survivor");
    if (m_currentDifficulty >= 5) ach.unlock("unstoppable");
    if (m_dashCount >= 100) ach.unlock("dash_master");
    if (m_dimManager.getSwitchCount() >= 50) ach.unlock("dimension_hopper");
    if (game->getUpgradeSystem().getRiftShards() >= 1000) ach.unlock("shard_hoarder");

    // Combo check
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        if (combat.comboCount >= 10) ach.unlock("combo_king");
    }

    // Full upgrade check
    for (auto& u : game->getUpgradeSystem().getUpgrades()) {
        if (u.currentLevel >= u.maxLevel) {
            ach.unlock("full_upgrade");
            break;
        }
    }
}

void PlayState::checkRiftInteraction() {
    if (!m_player || !m_level) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto rifts = m_level->getRiftPositions();

    m_nearRiftIndex = -1;
    for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
        // FIX: Skip already-repaired rifts
        if (m_repairedRiftIndices.count(i)) continue;
        float dx = playerPos.x - rifts[i].x;
        float dy = playerPos.y - rifts[i].y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 60.0f) {
            m_nearRiftIndex = i;
            break;
        }
    }

    if (m_nearRiftIndex >= 0 && game->getInput().isActionPressed(Action::Interact)) {
        // Puzzle type progression: early = Timing (easiest), mid = Sequence, late = Alignment
        int puzzleType;
        if (m_currentDifficulty <= 1) {
            puzzleType = 0; // Always Timing for first levels
        } else if (m_currentDifficulty <= 3) {
            puzzleType = (riftsRepaired % 2 == 0) ? 0 : 1; // Mix Timing and Sequence
        } else {
            int roll = std::rand() % 100;
            if (roll < 20) puzzleType = 0;       // 20% Timing
            else if (roll < 55) puzzleType = 1;  // 35% Sequence
            else puzzleType = 2;                 // 45% Alignment
        }
        m_activePuzzle = std::make_unique<RiftPuzzle>(
            static_cast<PuzzleType>(puzzleType), m_currentDifficulty);

        int riftIdx = m_nearRiftIndex; // FIX: capture rift index for lambda
        m_activePuzzle->onComplete = [this, riftIdx](bool success) {
            if (success) {
                AudioManager::instance().play(SFX::RiftRepair);
                riftsRepaired++;
                // FIX: Mark this rift as repaired and track per-level count
                m_repairedRiftIndices.insert(riftIdx);
                m_levelRiftsRepaired++;
                m_entropy.onRiftRepaired();
                game->getAchievements().unlock("rift_walker");
                float riftShardMult = game->getRunBuffSystem().getShardMultiplier();
                if (m_player && m_player->getEntity()->hasComponent<RelicComponent>())
                    riftShardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
                int riftShards = static_cast<int>((10 + m_currentDifficulty * 5) * riftShardMult);
                shardsCollected += riftShards;
                game->getUpgradeSystem().addRiftShards(riftShards);
                game->getUpgradeSystem().totalRiftsRepaired++;

                m_particles.burst(m_dimManager.playerPos, 40,
                    {150, 80, 255, 255}, 250.0f, 6.0f);
                m_camera.shake(5.0f, 0.3f);

                // FIX: Start collapse after all rifts in THIS level repaired
                int totalRifts = static_cast<int>(m_level->getRiftPositions().size());
                if (m_levelRiftsRepaired >= totalRifts) {
                    m_collapsing = true;
                    m_collapseTimer = 0;
                }
            } else {
                AudioManager::instance().play(SFX::RiftFail);
                m_entropy.addEntropy(10.0f);
                m_camera.shake(3.0f, 0.2f);
            }
            m_activePuzzle.reset();
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
        m_levelComplete = true;
        m_levelCompleteTimer = 0;
        AudioManager::instance().play(SFX::LevelComplete);
        float shardMult = (g_selectedDifficulty == GameDifficulty::Easy) ? 1.5f :
                          (g_selectedDifficulty == GameDifficulty::Hard) ? 0.75f : 1.0f;
        shardMult *= game->getRunBuffSystem().getShardMultiplier();
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

void PlayState::renderBackground(SDL_Renderer* renderer) {
    auto& bgColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.background : m_themeB.colors.background;
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    Vec2 camPos = m_camera.getPosition();
    Uint32 ticks = SDL_GetTicks();

    // Layer 1: Distant stars (slow parallax)
    for (int i = 0; i < 60; i++) {
        // Deterministic pseudo-random positions based on index
        int baseX = ((i * 7919 + 104729) % 2560) - 640;
        int baseY = ((i * 6271 + 73856) % 1440) - 360;

        float parallax = 0.05f;
        int sx = baseX - static_cast<int>(camPos.x * parallax) % 2560;
        int sy = baseY - static_cast<int>(camPos.y * parallax) % 1440;
        // Wrap around screen
        sx = ((sx % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH;
        sy = ((sy % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT;

        float twinkle = 0.5f + 0.5f * std::sin(ticks * 0.002f + i * 1.7f);
        Uint8 brightness = static_cast<Uint8>(30 + 40 * twinkle);
        int size = (i % 3 == 0) ? 2 : 1;
        SDL_SetRenderDrawColor(renderer, brightness, brightness,
                               static_cast<Uint8>(brightness + 30), 255);
        SDL_Rect star = {sx, sy, size, size};
        SDL_RenderFillRect(renderer, &star);
    }

    // Layer 2: Grid lines (medium parallax, gives depth)
    float gridParallax = 0.15f;
    int gridSpacing = 120;
    int gridOffX = static_cast<int>(camPos.x * gridParallax) % gridSpacing;
    int gridOffY = static_cast<int>(camPos.y * gridParallax) % gridSpacing;

    Uint8 gridAlpha = 15;
    SDL_SetRenderDrawColor(renderer,
        static_cast<Uint8>(bgColor.r + 30 > 255 ? 255 : bgColor.r + 30),
        static_cast<Uint8>(bgColor.g + 30 > 255 ? 255 : bgColor.g + 30),
        static_cast<Uint8>(bgColor.b + 30 > 255 ? 255 : bgColor.b + 30),
        gridAlpha);

    for (int x = -gridOffX; x < SCREEN_WIDTH; x += gridSpacing) {
        SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    }
    for (int y = -gridOffY; y < SCREEN_HEIGHT; y += gridSpacing) {
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
    }

    // Layer 3: Floating dimension particles (close parallax)
    auto& dimColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.solid : m_themeB.colors.solid;
    for (int i = 0; i < 20; i++) {
        float speed = 0.3f + (i % 5) * 0.1f;
        int baseX = ((i * 4513 + 23143) % 1600) - 160;
        int baseY = ((i * 3251 + 51749) % 900) - 90;

        float px = baseX - camPos.x * speed;
        float py = baseY - camPos.y * speed;
        // Gentle floating motion
        py += std::sin(ticks * 0.001f + i * 2.3f) * 15.0f;

        int screenX = ((static_cast<int>(px) % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH;
        int screenY = ((static_cast<int>(py) % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT;

        int size = 2 + i % 3;
        Uint8 pa = static_cast<Uint8>(20 + 15 * std::sin(ticks * 0.0015f + i));
        SDL_SetRenderDrawColor(renderer, dimColor.r, dimColor.g, dimColor.b, pa);
        SDL_Rect particle = {screenX, screenY, size, size};
        SDL_RenderFillRect(renderer, &particle);
    }
}

void PlayState::render(SDL_Renderer* renderer) {
    // Parallax background
    renderBackground(renderer);

    if (m_level) {
        m_level->render(renderer, m_camera,
                        m_dimManager.getCurrentDimension(),
                        m_dimManager.getBlendAlpha());
    }

    // Render entities
    m_renderSys.render(renderer, m_entities, m_camera,
                       m_dimManager.getCurrentDimension(),
                       m_dimManager.getBlendAlpha());

    // Trails (before particles)
    m_trails.render(renderer);

    // Particles
    m_particles.render(renderer, m_camera);

    // Dimension switch visual effect
    m_dimManager.applyVisualEffect(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Entropy visual effects
    m_entropy.applyVisualEffects(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Collapse warning
    if (m_collapsing) {
        float urgency = m_collapseTimer / m_collapseMaxTime;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint8 a = static_cast<Uint8>(urgency * 100);
        SDL_SetRenderDrawColor(renderer, 255, 30, 0, a);
        SDL_Rect border = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderDrawRect(renderer, &border);

        // Collapse timer text
        TTF_Font* font = game->getFont();
        if (font) {
            float remaining = m_collapseMaxTime - m_collapseTimer;
            if (remaining < 0) remaining = 0;
            int secs = static_cast<int>(remaining);
            int tenths = static_cast<int>((remaining - secs) * 10);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "COLLAPSE: %d.%d", secs, tenths);

            // Pulsing red-white text
            Uint8 pulse = static_cast<Uint8>(200 + 55 * std::sin(SDL_GetTicks() * 0.01f));
            SDL_Color tc = {pulse, static_cast<Uint8>(pulse / 4), 0, 255};
            SDL_Surface* ts = TTF_RenderText_Blended(font, buf, tc);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    SDL_Rect tr = {640 - ts->w / 2, 50, ts->w, ts->h};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
        }
    }

    // Near rift indicator
    if (m_nearRiftIndex >= 0 && !m_activePuzzle) {
        TTF_Font* font = game->getFont();
        if (font) {
            SDL_Color c = {180, 130, 255, 200};
            SDL_Surface* s = TTF_RenderText_Blended(font, "Press F to repair rift", c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 500, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Off-screen rift direction indicators
    if (m_level && !m_activePuzzle) {
        auto rifts = m_level->getRiftPositions();
        Vec2 camPos = m_camera.getPosition();
        float halfW = 640.0f, halfH = 360.0f;

        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            float sx = (rifts[i].x - camPos.x) * m_camera.zoom + halfW;
            float sy = (rifts[i].y - camPos.y) * m_camera.zoom + halfH;

            // Only show if off-screen
            if (sx >= -10 && sx <= 1290 && sy >= -10 && sy <= 730) continue;

            // Clamp to screen edge with margin
            float cx = std::max(25.0f, std::min(1255.0f, sx));
            float cy = std::max(25.0f, std::min(695.0f, sy));

            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.005f + i * 1.5f);
            Uint8 pa = static_cast<Uint8>(120 + 80 * pulse);

            // Diamond indicator
            SDL_SetRenderDrawColor(renderer, 150, 80, 255, pa);
            SDL_Rect ind = {static_cast<int>(cx) - 5, static_cast<int>(cy) - 5, 10, 10};
            SDL_RenderFillRect(renderer, &ind);

            // Arrow pointing toward rift
            float angle = std::atan2(sy - cy, sx - cx);
            int ax = static_cast<int>(cx + std::cos(angle) * 14);
            int ay = static_cast<int>(cy + std::sin(angle) * 14);
            SDL_SetRenderDrawColor(renderer, 180, 120, 255, pa);
            SDL_RenderDrawLine(renderer, static_cast<int>(cx), static_cast<int>(cy), ax, ay);
        }
    }

    // Puzzle overlay
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    // Level complete: rift warp transition effect
    if (m_levelComplete) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        float progress = m_levelCompleteTimer / 2.0f; // 0 to 1 over 2 seconds
        if (progress > 1.0f) progress = 1.0f;
        Uint32 ticks = SDL_GetTicks();

        // Growing dark overlay
        Uint8 overlayA = static_cast<Uint8>(progress * 180);
        SDL_SetRenderDrawColor(renderer, 10, 5, 20, overlayA);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);

        // Expanding rift circle from center
        int maxRadius = 400;
        int radius = static_cast<int>(progress * maxRadius);
        float pulse = 0.7f + 0.3f * std::sin(ticks * 0.01f);
        Uint8 riftA = static_cast<Uint8>((1.0f - progress * 0.5f) * 120 * pulse);

        // Rift ring (multiple concentric rings)
        for (int r = radius - 6; r <= radius + 6; r += 3) {
            if (r < 0) continue;
            for (int angle = 0; angle < 60; angle++) {
                float a = angle * 6.283185f / 60.0f + ticks * 0.002f;
                int px = 640 + static_cast<int>(std::cos(a) * r);
                int py = 360 + static_cast<int>(std::sin(a) * r);
                if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT) continue;
                SDL_SetRenderDrawColor(renderer, 180, 100, 255, riftA);
                SDL_Rect dot = {px - 1, py - 1, 3, 3};
                SDL_RenderFillRect(renderer, &dot);
            }
        }

        // Scanlines converging toward center
        if (progress > 0.3f) {
            int lineCount = static_cast<int>((progress - 0.3f) * 20);
            for (int i = 0; i < lineCount; i++) {
                int y = ((i * 4513 + ticks / 40) % SCREEN_HEIGHT);
                Uint8 la = static_cast<Uint8>(progress * 60);
                SDL_SetRenderDrawColor(renderer, 150, 80, 220, la);
                SDL_Rect line = {0, y, SCREEN_WIDTH, 1};
                SDL_RenderFillRect(renderer, &line);
            }
        }

        // Text banner
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(180 * std::min(1.0f, progress * 3.0f)));
        SDL_Rect banner = {0, 330, SCREEN_WIDTH, 60};
        SDL_RenderFillRect(renderer, &banner);

        TTF_Font* font = game->getFont();
        if (font) {
            float textAlpha = std::min(1.0f, progress * 3.0f);
            Uint8 ta = static_cast<Uint8>(textAlpha * 255);
            SDL_Color c = {140, 255, 180, ta};
            SDL_Surface* s = TTF_RenderText_Blended(font, "RIFT STABILIZED - Warping to next dimension...", c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 348, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Random events (NPCs in world space)
    renderRandomEvents(renderer, game->getFont());

    // NPCs
    renderNPCs(renderer, game->getFont());

    // NPC dialog overlay
    if (m_showNPCDialog) {
        renderNPCDialog(renderer, game->getFont());
    }

    // Tutorial hints (first 3 levels only)
    renderTutorialHints(renderer, game->getFont());

    // Floating damage numbers
    renderDamageNumbers(renderer, game->getFont());

    // Boss HP bar at top of screen
    if (m_isBossLevel && !m_bossDefeated) {
        renderBossHealthBar(renderer, game->getFont());
    }

    // Combo milestone golden flash overlay
    if (m_comboMilestoneFlash > 0) {
        float alpha = m_comboMilestoneFlash / 0.4f;
        Uint8 a = static_cast<Uint8>(alpha * 60);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, a);
        SDL_Rect fullScreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fullScreen);
    }

    // Screen effects (vignette, low-HP pulse, kill flash, boss intro, ripple, glitch)
    m_screenEffects.render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Damage flash overlay
    m_hud.renderFlash(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // HUD (on top of everything)
    m_hud.render(renderer, game->getFont(), m_player.get(), &m_entropy, &m_dimManager,
                 SCREEN_WIDTH, SCREEN_HEIGHT, game->getFPS(), game->getUpgradeSystem().getRiftShards());

    // Relic bar (bottom left, below buffs)
    renderRelicBar(renderer, game->getFont());

    // Challenge HUD (left side, below main HUD)
    renderChallengeHUD(renderer, game->getFont());

    // Minimap (bottom right corner)
    m_hud.renderMinimap(renderer, m_level.get(), m_player.get(), &m_dimManager, SCREEN_WIDTH, SCREEN_HEIGHT, &m_entities);

    // Achievement notification popup
    auto* notif = game->getAchievements().getActiveNotification();
    TTF_Font* achFont = game->getFont();
    if (notif && achFont) {
        float t = notif->timer / notif->duration;
        float slideIn = std::min(1.0f, (1.0f - t) * 5.0f); // fast slide in
        float fadeOut = std::min(1.0f, t * 3.0f); // fade out at end
        float alpha = slideIn * fadeOut;
        Uint8 a = static_cast<Uint8>(alpha * 220);

        int popW = 300, popH = 40;
        int popX = 640 - popW / 2;
        int popY = 660 - static_cast<int>(slideIn * 20);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 40, 25, a);
        SDL_Rect bg = {popX, popY, popW, popH};
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 100, 220, 120, a);
        SDL_RenderDrawRect(renderer, &bg);

        // Trophy icon
        SDL_SetRenderDrawColor(renderer, 255, 200, 50, a);
        SDL_Rect trophy = {popX + 10, popY + 10, 16, 16};
        SDL_RenderFillRect(renderer, &trophy);

        // Text
        char achText[128];
        snprintf(achText, sizeof(achText), "Achievement: %s", notif->name.c_str());
        SDL_Color tc = {200, 255, 210, a};
        SDL_Surface* ts = TTF_RenderText_Blended(achFont, achText, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_SetTextureAlphaMod(tt, a);
                SDL_Rect tr = {popX + 34, popY + (popH - ts->h) / 2, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }

    // Debug overlay (F3)
    if (m_showDebugOverlay) {
        renderDebugOverlay(renderer, game->getFont());
    }

    // Relic choice overlay
    if (m_showRelicChoice) {
        renderRelicChoice(renderer, game->getFont());
    }
}

void PlayState::renderDebugOverlay(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || !m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // Compute raw damage multiplier (before cap)
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

    // Compute raw attack speed multiplier (before cap)
    float rawSpd = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) rawSpd += 0.15f;
    }
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0)
        rawSpd += relics.riftConduitStacks * 0.10f;
    float clampedSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    // Switch CD with floor detection
    float baseCD = 0.5f;
    float cdMult = RelicSystem::getSwitchCooldownMult(relics);
    float finalCD = m_dimManager.switchCooldown;
    bool cdFloored = (baseCD * cdMult) < finalCD + 0.001f && relics.hasRelic(RelicID::RiftMantle);

    // Zone count
    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
    });

    // Gameplay paused detection
    bool gameplayPaused = m_showRelicChoice || m_showNPCDialog
        || (m_activePuzzle && m_activePuzzle->isActive());

    // Stability derived values
    float stabRate = RelicSynergy::getStabilityDmgPerSec(relics);
    float stabMax = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
    float stabPct = std::min(relics.stabilityTimer * stabRate, stabMax) * 100.0f;

    int synergyCount = RelicSynergy::getActiveSynergyCount(relics);
    float dmgTakenMult = RelicSystem::getDamageTakenMult(relics, curDim);

    // Build lines into stack buffer
    struct Line { char text[96]; SDL_Color color; };
    Line lines[15];
    int lineCount = 0;

    auto addLine = [&](SDL_Color c, const char* fmt, ...) {
        if (lineCount >= 15) return;
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(lines[lineCount].text, sizeof(lines[0].text), fmt, args);
        va_end(args);
        lines[lineCount].color = c;
        lineCount++;
    };

    SDL_Color cHeader = {255, 220, 80, 255};
    SDL_Color cNormal = {200, 220, 240, 255};
    SDL_Color cWarn   = {255, 100, 80, 255};
    SDL_Color cGreen  = {100, 255, 140, 255};

    addLine(cHeader, "=== RELIC SAFETY RAILS (F3/F4) ===");
    addLine(gameplayPaused ? cWarn : cGreen, "Paused: %s", gameplayPaused ? "YES" : "NO");
    addLine(rawDmg > clampedDmg + 0.001f ? cWarn : cNormal,
            "DMG Mult: %.2fx raw -> %.2fx clamped", rawDmg, clampedDmg);
    addLine(rawSpd > clampedSpd + 0.001f ? cWarn : cNormal,
            "ATK Speed: %.2fx raw -> %.2fx clamped", rawSpd, clampedSpd);
    addLine(dmgTakenMult > 1.001f ? cWarn : dmgTakenMult < 0.999f ? cGreen : cNormal,
            "Damage Taken Mult: %.2fx", dmgTakenMult);
    if (cdFloored)
        addLine(cWarn, "Switch CD: %.2fs x%.2f = %.2fs FLOOR", baseCD, cdMult, finalCD);
    else
        addLine(cNormal, "Switch CD: %.2fs x%.2f = %.2fs", baseCD, cdMult, finalCD);
    addLine(cNormal, "VoidHunger: %.0f%% / %.0f%% cap",
            relics.voidHungerBonus * 100.0f, 40.0f);
    addLine(cNormal, "VoidRes ICD: %.2fs remain", std::max(0.0f, relics.voidResonanceProcCD));
    addLine(zoneCount > 0 ? cHeader : cNormal,
            "Residue: %d/%d zones, spawn ICD %.2fs",
            zoneCount, RelicSystem::getMaxResidueZones(),
            std::max(0.0f, relics.dimResidueSpawnCD));
    addLine(cNormal, "Stability: %.1fs (%.0f%% DMG)",
            relics.stabilityTimer, stabPct);
    addLine(cNormal, "Conduit: %d stacks, %.1fs left",
            relics.riftConduitStacks, std::max(0.0f, relics.riftConduitTimer));
    addLine(cNormal, "Dim: %d | Synergies: %d | Seed: %d",
            curDim, synergyCount, m_runSeed);
    addLine({140, 140, 160, 255}, "F4: snapshot to balance_snapshots.csv");

    // Render panel
    int panelX = 10, panelY = 190;
    int lineH = 16, padX = 8, padY = 4;
    int panelW = 360;
    int panelH = padY * 2 + lineCount * lineH;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 255, 220, 80, 120);
    SDL_RenderDrawRect(renderer, &bg);

    for (int i = 0; i < lineCount; i++) {
        SDL_Surface* s = TTF_RenderText_Blended(font, lines[i].text, lines[i].color);
        if (!s) continue;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        if (t) {
            SDL_Rect dst = {panelX + padX, panelY + padY + i * lineH, s->w, s->h};
            SDL_RenderCopy(renderer, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
    }

    // Process pending F4 snapshot (only on keypress, not per frame)
    if (m_pendingSnapshot) {
        m_pendingSnapshot = false;
        writeBalanceSnapshot();
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

void PlayState::showRelicChoice() {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    m_relicChoices = RelicSystem::generateChoice(m_currentDifficulty, relics.relics);
    if (m_relicChoices.empty()) return;
    m_showRelicChoice = true;
    m_relicChoiceSelected = 0;
}

void PlayState::selectRelic(int index) {
    if (index < 0 || index >= static_cast<int>(m_relicChoices.size())) return;
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    RelicID choice = m_relicChoices[index];
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    relics.addRelic(choice);

    // Apply stat effects
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    RelicSystem::applyStatEffects(relics, *m_player, hp, combat);

    // RiftMantle: reduce dimension switch cooldown (clamped by safety rail)
    m_dimManager.switchCooldown = std::max(0.20f, 0.5f * RelicSystem::getSwitchCooldownMult(relics));

    // TimeDilator: -30% ability cooldowns (applied once on pickup)
    if (choice == RelicID::TimeDilator && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float cdMult = RelicSystem::getAbilityCooldownMultiplier(relics);
        abil.abilities[0].cooldown *= cdMult;
        abil.abilities[1].cooldown *= cdMult;
        abil.abilities[2].cooldown *= cdMult;
    }

    // Visual/audio feedback
    AudioManager::instance().play(SFX::RiftRepair);
    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto& data = RelicSystem::getRelicData(choice);
    m_particles.burst(pPos, 30, data.glowColor, 200.0f, 4.0f);
    m_particles.burst(pPos, 15, {255, 255, 255, 255}, 150.0f, 3.0f);

    m_showRelicChoice = false;
    m_relicChoices.clear();
}

void PlayState::renderRelicChoice(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_relicChoices.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Dark overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Title
    {
        SDL_Color tc = {255, 215, 100, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "Choose a Relic", tc);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {640 - s->w / 2, 180, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Relic cards
    int cardW = 200;
    int cardH = 260;
    int gap = 30;
    int totalW = static_cast<int>(m_relicChoices.size()) * cardW + (static_cast<int>(m_relicChoices.size()) - 1) * gap;
    int startX = 640 - totalW / 2;
    int cardY = 230;

    for (int i = 0; i < static_cast<int>(m_relicChoices.size()); i++) {
        int cx = startX + i * (cardW + gap);
        bool selected = (i == m_relicChoiceSelected);
        auto& data = RelicSystem::getRelicData(m_relicChoices[i]);

        // Card background
        SDL_SetRenderDrawColor(renderer, 15, 15, 25, 220);
        SDL_Rect cardBg = {cx, cardY, cardW, cardH};
        SDL_RenderFillRect(renderer, &cardBg);

        // Border (glow when selected)
        Uint8 borderA = selected ? 255 : 100;
        SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, borderA);
        SDL_RenderDrawRect(renderer, &cardBg);
        if (selected) {
            SDL_Rect outer = {cx - 2, cardY - 2, cardW + 4, cardH + 4};
            SDL_RenderDrawRect(renderer, &outer);
        }

        // Relic icon (colored orb)
        int orbX = cx + cardW / 2;
        int orbY = cardY + 50;
        int orbR = 24;
        for (int oy = -orbR; oy <= orbR; oy++) {
            for (int ox = -orbR; ox <= orbR; ox++) {
                if (ox * ox + oy * oy <= orbR * orbR) {
                    float dist = std::sqrt(static_cast<float>(ox * ox + oy * oy)) / orbR;
                    Uint8 a = static_cast<Uint8>((1.0f - dist * 0.5f) * 255);
                    SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, a);
                    SDL_RenderDrawPoint(renderer, orbX + ox, orbY + oy);
                }
            }
        }

        // Tier text
        const char* tierText = "COMMON";
        SDL_Color tierColor = {180, 180, 180, 255};
        if (data.tier == RelicTier::Rare) { tierText = "RARE"; tierColor = {80, 180, 255, 255}; }
        else if (data.tier == RelicTier::Legendary) { tierText = "LEGENDARY"; tierColor = {255, 200, 50, 255}; }
        {
            SDL_Surface* s = TTF_RenderText_Blended(font, tierText, tierColor);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 90, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Name
        {
            SDL_Color nc = {255, 255, 255, 255};
            SDL_Surface* s = TTF_RenderText_Blended(font, data.name, nc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 120, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Description
        {
            SDL_Color dc = {180, 180, 200, 220};
            SDL_Surface* s = TTF_RenderText_Blended(font, data.description, dc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 160, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Key hint
        char keyBuf[8];
        std::snprintf(keyBuf, sizeof(keyBuf), "[%d]", i + 1);
        {
            SDL_Color kc = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 120, 180};
            SDL_Surface* s = TTF_RenderText_Blended(font, keyBuf, kc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + cardH - 35, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }
}

void PlayState::renderRelicBar(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    if (relics.relics.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int iconSize = 20;
    int gap = 4;
    int startX = 15;
    int startY = SCREEN_HEIGHT - 50;

    for (int i = 0; i < static_cast<int>(relics.relics.size()) && i < 12; i++) {
        auto& data = RelicSystem::getRelicData(relics.relics[i].id);
        int ix = startX + i * (iconSize + gap);

        // Background
        SDL_SetRenderDrawColor(renderer, 10, 10, 20, 180);
        SDL_Rect bg = {ix, startY, iconSize, iconSize};
        SDL_RenderFillRect(renderer, &bg);

        // Colored orb
        Uint8 r = data.glowColor.r, g = data.glowColor.g, b = data.glowColor.b;
        if (relics.relics[i].consumed) { r /= 3; g /= 3; b /= 3; }
        SDL_SetRenderDrawColor(renderer, r, g, b, 200);
        SDL_Rect orb = {ix + 3, startY + 3, iconSize - 6, iconSize - 6};
        SDL_RenderFillRect(renderer, &orb);

        // Border
        Uint8 borderTierA = 120;
        if (data.tier == RelicTier::Rare) borderTierA = 180;
        if (data.tier == RelicTier::Legendary) borderTierA = 255;
        SDL_SetRenderDrawColor(renderer, r, g, b, borderTierA);
        SDL_RenderDrawRect(renderer, &bg);
    }
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

    // Spawn next wave when: most enemies dead OR timer expired
    m_waveTimer -= dt;
    if (aliveEnemies <= 1 || m_waveTimer <= 0) {
        // Spawn next wave
        for (auto& sp : m_spawnWaves[m_currentWave]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Elite modifier in wave spawns
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < 15) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 6);
                Enemy::makeElite(e, mod);
            }
        }
        m_currentWave++;
        m_waveTimer = m_waveDelay;

        // Warning SFX for new wave
        AudioManager::instance().play(SFX::CollapseWarning);
    }
}

void PlayState::renderDamageNumbers(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    for (auto& dn : m_damageNumbers) {
        float t = dn.lifetime / dn.maxLifetime; // 1.0 → 0.0
        Uint8 alpha = static_cast<Uint8>(255 * t);

        // Color: red for player damage, orange for crits, yellow for normal enemy damage
        SDL_Color color;
        if (dn.isPlayerDamage) {
            color = {255, 60, 40, alpha};
        } else if (dn.isCritical) {
            color = {255, 180, 40, alpha};
        } else {
            color = {255, 240, 100, alpha};
        }

        char buf[32];
        if (dn.isCritical) {
            std::snprintf(buf, sizeof(buf), "CRIT! %.0f", dn.value);
        } else {
            std::snprintf(buf, sizeof(buf), "%.0f", dn.value);
        }

        // Convert world position to screen position
        SDL_FRect worldRect = {dn.position.x - 10, dn.position.y - 8, 20, 16};
        SDL_Rect screenRect = m_camera.worldToScreen(worldRect);

        SDL_Surface* surface = TTF_RenderText_Blended(font, buf, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                // Scale up for crits and large damage
                float scale = 1.0f;
                if (dn.isCritical) {
                    scale = 1.5f;
                } else if (dn.value > 20) {
                    scale = 1.3f;
                }
                SDL_Rect dst = {screenRect.x - static_cast<int>(surface->w * scale) / 2,
                               screenRect.y,
                               static_cast<int>(surface->w * scale),
                               static_cast<int>(surface->h * scale)};
                SDL_SetTextureAlphaMod(texture, alpha);
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void PlayState::renderKeyBox(SDL_Renderer* renderer, TTF_Font* font,
                              const char* key, int x, int y, Uint8 alpha) {
    // Render a key label as a bordered box
    SDL_Surface* surface = TTF_RenderText_Blended(font, key, {255, 255, 255, alpha});
    if (!surface) return;
    int pad = 4;
    SDL_Rect box = {x - pad, y - 2, surface->w + pad * 2, surface->h + 4};
    SDL_SetRenderDrawColor(renderer, 40, 50, 80, static_cast<Uint8>(alpha * 0.8f));
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 120, 160, 220, alpha);
    SDL_RenderDrawRect(renderer, &box);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void PlayState::renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_currentDifficulty > 3) return;

    // Context-based hint system: show hints when conditions are met
    const char* hint = nullptr;
    const char* keyLabel = nullptr;
    bool conditionMet = false; // True = hint should be dismissed

    if (m_currentDifficulty == 1) {
        if (!m_tutorialHintDone[0]) {
            // Show movement hint after 1.5s of inactivity
            if (m_tutorialTimer > 1.5f && !m_hasMovedThisRun) {
                hint = "to move";
                keyLabel = "WASD";
            }
            if (m_hasMovedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[1]) {
            hint = "to jump";
            keyLabel = "SPACE";
            if (m_hasJumpedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[2]) {
            hint = "to dash through danger";
            keyLabel = "SHIFT";
            if (m_hasDashedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[3]) {
            // Show attack hint when any enemy is nearby
            bool enemyNear = false;
            if (m_player) {
                Vec2 pp = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_entities.forEach([&](Entity& e) {
                    if (e.getTag().find("enemy") != std::string::npos &&
                        e.hasComponent<TransformComponent>()) {
                        Vec2 ep = e.getComponent<TransformComponent>().getCenter();
                        float d = std::sqrt((pp.x-ep.x)*(pp.x-ep.x) + (pp.y-ep.y)*(pp.y-ep.y));
                        if (d < 250.0f) enemyNear = true;
                    }
                });
            }
            if (enemyNear) {
                hint = "melee attack    ";
                keyLabel = "J";
            }
            if (m_hasAttackedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[4]) {
            hint = "to switch dimensions - some walls only exist in one!";
            keyLabel = "E";
            if (m_dimManager.getSwitchCount() > 0) conditionMet = true;
        }
    } else if (m_currentDifficulty == 2) {
        if (!m_tutorialHintDone[5]) {
            // Show rift hint when near a rift
            if (m_nearRiftIndex >= 0) {
                hint = "to repair the rift";
                keyLabel = "F";
            } else if (m_tutorialTimer > 5.0f) {
                hint = "Find glowing rifts on the map";
                keyLabel = nullptr;
            }
            if (riftsRepaired > 0) conditionMet = true;
        } else if (!m_tutorialHintDone[6]) {
            if (m_entropy.getPercent() > 0.4f) {
                hint = "Watch your Entropy bar - too much and your suit crashes!";
                keyLabel = nullptr;
                m_tutorialHintShowTimer += 1.0f / 60.0f;
                if (m_tutorialHintShowTimer > 5.0f) conditionMet = true;
            }
        }
    } else if (m_currentDifficulty == 3) {
        if (!m_tutorialHintDone[7]) {
            hint = "Chain attacks for combos - more hits = more damage!";
            keyLabel = nullptr;
            if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
                if (m_player->getEntity()->getComponent<CombatComponent>().comboCount >= 3)
                    conditionMet = true;
            }
            m_tutorialHintShowTimer += 1.0f / 60.0f;
            if (m_tutorialHintShowTimer > 8.0f) conditionMet = true;
        }
    }

    // Mark completed hints
    if (conditionMet) {
        // Find current hint index and mark it
        for (int i = 0; i < 8; i++) {
            if (!m_tutorialHintDone[i]) {
                m_tutorialHintDone[i] = true;
                m_tutorialHintShowTimer = 0;
                break;
            }
        }
    }

    if (!hint) return;

    // Fade in over 0.3s
    m_tutorialHintShowTimer += 1.0f / 60.0f;
    float alpha = std::min(1.0f, m_tutorialHintShowTimer / 0.3f);
    Uint8 a = static_cast<Uint8>(alpha * 200);

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(a * 0.4f));
    SDL_Rect bg = {240, 140, 800, 36};
    SDL_RenderFillRect(renderer, &bg);

    int textX = 640;
    if (keyLabel) {
        // Render key box + hint text side by side
        SDL_Surface* hintSurf = TTF_RenderText_Blended(font, hint, {180, 220, 255, a});
        SDL_Surface* keySurf = TTF_RenderText_Blended(font, keyLabel, {255, 255, 255, 255});
        int totalW = (keySurf ? keySurf->w + 16 : 0) + (hintSurf ? hintSurf->w : 0);
        int startX = 640 - totalW / 2;

        if (keySurf) {
            renderKeyBox(renderer, font, keyLabel, startX, 148, a);
            startX += keySurf->w + 16;
            SDL_FreeSurface(keySurf);
        }
        if (hintSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, hintSurf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect dst = {startX, 148, hintSurf->w, hintSurf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(hintSurf);
        }
    } else {
        // Just hint text centered
        SDL_Color color = {180, 220, 255, a};
        SDL_Surface* surface = TTF_RenderText_Blended(font, hint, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, a);
                SDL_Rect dst = {640 - surface->w / 2, 148, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
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

    // Void Sovereign at difficulty 10+ (after completing all 4 boss rotations)
    int bossIndex = m_currentDifficulty / 3; // 0-based boss encounter index
    if (m_currentDifficulty >= 10 && bossIndex >= 4) {
        // Spawn Void Sovereign as final boss
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

void PlayState::renderBossHealthBar(SDL_Renderer* renderer, TTF_Font* font) {
    // Find boss entity
    Entity* boss = nullptr;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "enemy_boss") boss = &e;
    });
    if (!boss || !boss->hasComponent<HealthComponent>()) return;

    auto& hp = boss->getComponent<HealthComponent>();
    int bossPhase = 1;
    if (boss->hasComponent<AIComponent>()) {
        bossPhase = boss->getComponent<AIComponent>().bossPhase;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Bar background
    int barW = 400;
    int barH = 12;
    int barX = 640 - barW / 2;
    int barY = 20;

    // Dark frame
    SDL_SetRenderDrawColor(renderer, 20, 10, 30, 220);
    SDL_Rect frame = {barX - 2, barY - 2, barW + 4, barH + 4};
    SDL_RenderFillRect(renderer, &frame);

    // Background
    SDL_SetRenderDrawColor(renderer, 50, 25, 50, 200);
    SDL_Rect bg = {barX, barY, barW, barH};
    SDL_RenderFillRect(renderer, &bg);

    // HP fill (color changes per phase and boss type)
    float pct = hp.getPercent();
    int fillW = static_cast<int>(barW * pct);
    int bt = 0;
    if (boss->hasComponent<AIComponent>()) bt = boss->getComponent<AIComponent>().bossType;
    Uint8 r, g, b;
    if (bt == 4) {
        // Void Sovereign: dark purple/magenta tones
        switch (bossPhase) {
            case 1: r = 120; g = 0; b = 180; break;
            case 2: r = 180; g = 0; b = 150; break;
            case 3: r = 255; g = 0; b = 120; break;
            default: r = 120; g = 0; b = 180; break;
        }
    } else if (bt == 3) {
        // Temporal Weaver: golden/amber tones
        switch (bossPhase) {
            case 1: r = 200; g = 170; b = 60; break;
            case 2: r = 230; g = 180; b = 40; break;
            case 3: r = 255; g = 200; b = 30; break;
            default: r = 200; g = 170; b = 60; break;
        }
    } else if (bt == 2) {
        // Dimensional Architect: blue/purple tones
        switch (bossPhase) {
            case 1: r = 80; g = 140; b = 255; break;
            case 2: r = 140; g = 100; b = 255; break;
            case 3: r = 200; g = 60; b = 255; break;
            default: r = 80; g = 140; b = 255; break;
        }
    } else if (bt == 1) {
        // Void Wyrm: green tones
        switch (bossPhase) {
            case 1: r = 40; g = 180; b = 120; break;
            case 2: r = 60; g = 220; b = 80; break;
            case 3: r = 120; g = 255; b = 40; break;
            default: r = 40; g = 180; b = 120; break;
        }
    } else {
        // Rift Guardian: purple/orange/red tones
        switch (bossPhase) {
            case 1: r = 200; g = 50; b = 180; break;
            case 2: r = 220; g = 120; b = 40; break;
            case 3: r = 255; g = 40; b = 40; break;
            default: r = 200; g = 50; b = 180; break;
        }
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, 240);
    SDL_Rect fill = {barX, barY, fillW, barH};
    SDL_RenderFillRect(renderer, &fill);

    // Phase markers at 66% and 33%
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
    SDL_RenderDrawLine(renderer, barX + barW * 2 / 3, barY, barX + barW * 2 / 3, barY + barH);
    SDL_RenderDrawLine(renderer, barX + barW / 3, barY, barX + barW / 3, barY + barH);

    // Boss name (dynamic based on boss type)
    if (font) {
        int bt = 0;
        if (boss->hasComponent<AIComponent>()) bt = boss->getComponent<AIComponent>().bossType;
        const char* bossName = (bt == 4) ? "VOID SOVEREIGN" : (bt == 3) ? "TEMPORAL WEAVER" : (bt == 2) ? "DIMENSIONAL ARCHITECT" : (bt == 1) ? "VOID WYRM" : "RIFT GUARDIAN";
        SDL_Color tc = (bt == 4) ? SDL_Color{180, 80, 255, 220} : (bt == 3) ? SDL_Color{220, 190, 100, 220} : (bt == 2) ? SDL_Color{160, 180, 255, 220} : (bt == 1) ? SDL_Color{180, 255, 200, 220} : SDL_Color{220, 180, 255, 220};
        SDL_Surface* ts = TTF_RenderText_Blended(font, bossName, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {640 - ts->w / 2, barY + barH + 4, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
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

void PlayState::checkBreakableWalls() {
    if (!m_player || !m_level) return;

    // Only break walls during dash or charged attack
    bool canBreak = m_player->isDashing || m_player->isChargingAttack;
    if (!canBreak) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    int tileSize = m_level->getTileSize();
    int currentDim = m_dimManager.getCurrentDimension();

    // Check tiles around player
    int cx = static_cast<int>(pt.getCenter().x) / tileSize;
    int cy = static_cast<int>(pt.getCenter().y) / tileSize;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int tx = cx + dx;
            int ty = cy + dy;
            if (!m_level->inBounds(tx, ty)) continue;

            auto& tile = m_level->getTile(tx, ty, currentDim);
            if (tile.type == TileType::Breakable) {
                // Destroy the breakable wall
                Tile empty;
                empty.type = TileType::Empty;
                m_level->setTile(tx, ty, currentDim, empty);
                // Also break in other dimension
                int otherDim = (currentDim == 1) ? 2 : 1;
                auto& otherTile = m_level->getTile(tx, ty, otherDim);
                if (otherTile.type == TileType::Breakable) {
                    m_level->setTile(tx, ty, otherDim, empty);
                }

                AudioManager::instance().play(SFX::BreakableWall);
                m_camera.shake(6.0f, 0.2f);

                // Debris particles
                Vec2 breakPos = {static_cast<float>(tx * tileSize + tileSize / 2),
                                 static_cast<float>(ty * tileSize + tileSize / 2)};
                m_particles.burst(breakPos, 20, tile.color, 200.0f, 3.0f);
                m_particles.burst(breakPos, 10, {200, 180, 160, 255}, 150.0f, 2.0f);
            }
        }
    }
}

void PlayState::checkSecretRoomDiscovery() {
    if (!m_player || !m_level) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    int tileSize = m_level->getTileSize();
    int px = static_cast<int>(pt.getCenter().x) / tileSize;
    int py = static_cast<int>(pt.getCenter().y) / tileSize;

    for (auto& sr : m_level->getSecretRooms()) {
        if (sr.discovered) continue;

        // Check if player is inside the secret room bounds
        if (px >= sr.tileX && px < sr.tileX + sr.width &&
            py >= sr.tileY && py < sr.tileY + sr.height) {
            sr.discovered = true;
            AudioManager::instance().play(SFX::SecretRoomDiscover);
            // Lore: The Rift - discovered via secret room
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::TheRift)) {
                    lore->discover(LoreID::TheRift);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }
            m_camera.shake(4.0f, 0.15f);

            Vec2 roomCenter = {
                static_cast<float>((sr.tileX + sr.width / 2) * tileSize),
                static_cast<float>((sr.tileY + sr.height / 2) * tileSize)
            };
            m_particles.burst(roomCenter, 30, {255, 215, 60, 255}, 200.0f, 4.0f);
            m_particles.burst(roomCenter, 15, {180, 150, 255, 255}, 150.0f, 3.0f);

            // Apply rewards based on type
            if (!sr.completed) {
                sr.completed = true;
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();

                switch (sr.type) {
                    case SecretRoomType::TreasureVault: {
                        int shards = sr.shardReward;
                        shards = static_cast<int>(shards * game->getRunBuffSystem().getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        hp.currentHP = std::min(hp.maxHP, hp.currentHP + sr.hpReward);
                        break;
                    }
                    case SecretRoomType::ShrineRoom: {
                        // 50/50 buff or curse
                        AudioManager::instance().play(SFX::ShrineActivate);
                        if (std::rand() % 2 == 0) {
                            // Blessing: +20 max HP
                            AudioManager::instance().play(SFX::ShrineBlessing);
                            hp.maxHP += 20;
                            hp.currentHP = std::min(hp.maxHP, hp.currentHP + 20);
                            m_particles.burst(roomCenter, 20, {100, 255, 100, 255}, 150.0f, 3.0f);
                        } else {
                            // Curse: +15% entropy
                            AudioManager::instance().play(SFX::ShrineCurse);
                            m_entropy.addEntropy(15.0f);
                            m_particles.burst(roomCenter, 20, {200, 50, 50, 255}, 150.0f, 3.0f);
                        }
                        break;
                    }
                    case SecretRoomType::DimensionCache: {
                        int shards = sr.shardReward * 2; // Double reward
                        shards = static_cast<int>(shards * game->getRunBuffSystem().getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        break;
                    }
                    case SecretRoomType::AncientWeapon:
                        // Temporary damage boost
                        m_player->damageBoostTimer = 30.0f; // 30 seconds
                        m_player->damageBoostMultiplier = 2.0f;
                        break;
                    case SecretRoomType::ChallengeRoom:
                        // Reward given after clearing enemies (checked by wave system)
                        break;
                }
            }
        }
    }
}

void PlayState::checkEventInteraction() {
    if (!m_player || !m_level) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 playerCenter = pt.getCenter();
    int currentDim = m_dimManager.getCurrentDimension();
    const auto& input = game->getInput();

    m_nearEventIndex = -1;

    auto& events = m_level->getRandomEvents();
    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        float dx = playerCenter.x - event.position.x;
        float dy = playerCenter.y - event.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 60.0f) {
            m_nearEventIndex = i;

            // Interact with Up/W
            if (input.isActionPressed(Action::Jump) || input.isActionPressed(Action::MoveUp)) {
                // Not jump - use a dedicated check. Just use E key via event
            }

            // Interact with Enter or E (check raw key)
            if (input.isActionPressed(Action::Interact)) {
                event.used = true;
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                auto& buffs = game->getRunBuffSystem();

                switch (event.type) {
                    case RandomEventType::Merchant:
                        AudioManager::instance().play(SFX::MerchantGreet);
                        // Buy a random available buff (cheaper)
                        {
                            auto offerings = buffs.generateShopOffering(m_currentDifficulty);
                            if (!offerings.empty()) {
                                auto& pick = offerings[0];
                                int shards = game->getUpgradeSystem().getRiftShards();
                                int discountCost = pick.cost * 3 / 4; // 25% cheaper
                                if (shards >= discountCost) {
                                    game->getUpgradeSystem().addRiftShards(-discountCost);
                                    // Bypass purchase() shard deduction - pass dummy with enough
                                    int dummy = pick.cost;
                                    buffs.purchase(pick.id, dummy);
                                    AudioManager::instance().play(SFX::Pickup);
                                    m_particles.burst(event.position, 15, {255, 215, 60, 255}, 120.0f, 2.0f);
                                } else {
                                    AudioManager::instance().play(SFX::RiftFail);
                                    event.used = false; // Can try again
                                }
                            }
                        }
                        break;

                    case RandomEventType::Shrine:
                        AudioManager::instance().play(SFX::ShrineActivate);
                        if (std::rand() % 2 == 0) {
                            AudioManager::instance().play(SFX::ShrineBlessing);
                            hp.maxHP += 15;
                            hp.currentHP = std::min(hp.maxHP, hp.currentHP + 15);
                            m_particles.burst(event.position, 20, {100, 255, 150, 255}, 150.0f, 3.0f);
                        } else {
                            AudioManager::instance().play(SFX::ShrineCurse);
                            m_entropy.addEntropy(10.0f);
                            hp.currentHP = std::max(1.0f, hp.currentHP - 10);
                            m_particles.burst(event.position, 15, {200, 50, 50, 255}, 150.0f, 3.0f);
                        }
                        break;

                    case RandomEventType::DimensionalAnomaly:
                        AudioManager::instance().play(SFX::AnomalySpawn);
                        // Spawn bonus enemies directly (don't re-spawn all)
                        for (int e = 0; e < 3 + m_currentDifficulty; e++) {
                            float sx = event.position.x + (std::rand() % 200 - 100);
                            float sy = event.position.y - 50 - (std::rand() % 100);
                            int etype = std::rand() % std::min(10, 3 + m_currentDifficulty);
                            Enemy::createByType(m_entities, etype, {sx, sy}, currentDim);
                        }
                        m_particles.burst(event.position, 25, {180, 100, 255, 255}, 200.0f, 4.0f);
                        break;

                    case RandomEventType::RiftEcho: {
                        int shards = event.reward;
                        shards = static_cast<int>(shards * buffs.getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(event.position, 20, {200, 150, 255, 255}, 180.0f, 3.0f);
                        break;
                    }

                    case RandomEventType::SuitRepairStation:
                        hp.currentHP = hp.maxHP;
                        m_entropy.reduceEntropy(30.0f);
                        AudioManager::instance().play(SFX::RiftRepair);
                        m_particles.burst(event.position, 25, {100, 255, 100, 255}, 150.0f, 3.0f);
                        break;

                    case RandomEventType::GamblingRift: {
                        int cost = event.cost;
                        int shards = game->getUpgradeSystem().getRiftShards();
                        if (shards >= cost) {
                            game->getUpgradeSystem().addRiftShards(-cost);
                            // Random reward: 0x (40%), 2x (35%), 4x (25%)
                            int roll = std::rand() % 100;
                            int reward = 0;
                            if (roll < 40) {
                                reward = 0;
                                AudioManager::instance().play(SFX::RiftFail);
                                m_particles.burst(event.position, 10, {100, 100, 100, 255}, 80.0f, 2.0f);
                            } else if (roll < 75) {
                                reward = cost * 2;
                                AudioManager::instance().play(SFX::Pickup);
                                m_particles.burst(event.position, 15, {200, 170, 255, 255}, 120.0f, 3.0f);
                            } else {
                                reward = cost * 4;
                                AudioManager::instance().play(SFX::LevelComplete);
                                m_particles.burst(event.position, 30, {255, 215, 60, 255}, 200.0f, 4.0f);
                            }
                            reward = static_cast<int>(reward * buffs.getShardMultiplier());
                            shardsCollected += reward;
                            game->getUpgradeSystem().addRiftShards(reward);
                        } else {
                            AudioManager::instance().play(SFX::RiftFail);
                            event.used = false;
                        }
                        break;
                    }
                }
            }
            break; // Only interact with nearest event
        }
    }
}

void PlayState::renderRandomEvents(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& events = m_level->getRandomEvents();

    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        SDL_FRect worldRect = {event.position.x - 16, event.position.y - 32, 32, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        // Skip if off screen
        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 2.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC body
        SDL_Color npcColor;
        switch (event.type) {
            case RandomEventType::Merchant:           npcColor = {80, 200, 80, 255}; break;
            case RandomEventType::Shrine:             npcColor = {180, 120, 255, 255}; break;
            case RandomEventType::DimensionalAnomaly: npcColor = {200, 50, 200, 255}; break;
            case RandomEventType::RiftEcho:           npcColor = {150, 150, 255, 255}; break;
            case RandomEventType::SuitRepairStation:  npcColor = {100, 255, 150, 255}; break;
            case RandomEventType::GamblingRift:       npcColor = {255, 200, 50, 255}; break;
        }

        // Glow
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i);
        Uint8 ga = static_cast<Uint8>(40 * glow);
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, ga);
        SDL_Rect glowRect = {sr.x - 6, sr.y - 6, sr.w + 12, sr.h + 12};
        SDL_RenderFillRect(renderer, &glowRect);

        // Body
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 220);
        SDL_RenderFillRect(renderer, &sr);

        // Eye
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        SDL_Rect eye = {sr.x + sr.w / 2 - 2, sr.y + 8, 4, 4};
        SDL_RenderFillRect(renderer, &eye);

        // Icon on top based on type
        int iconY = sr.y - 6;
        int iconX = sr.x + sr.w / 2;
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 180);
        switch (event.type) {
            case RandomEventType::Merchant:
                // Coin
                SDL_RenderDrawLine(renderer, iconX - 4, iconY, iconX + 4, iconY);
                SDL_RenderDrawLine(renderer, iconX, iconY - 4, iconX, iconY + 4);
                break;
            case RandomEventType::Shrine:
                // Diamond
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX + 5, iconY);
                SDL_RenderDrawLine(renderer, iconX + 5, iconY, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX, iconY + 5, iconX - 5, iconY);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX, iconY - 5);
                break;
            case RandomEventType::DimensionalAnomaly: {
                // Exclamation
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 1);
                SDL_Rect dot = {iconX - 1, iconY + 3, 2, 2};
                SDL_RenderFillRect(renderer, &dot);
                break;
            }
            default:
                // Star
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX + 5, iconY);
                break;
        }

        // Interaction hint when near
        if (i == m_nearEventIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", event.getName());
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y - 24, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }
}

void PlayState::endRun() {
    auto& upgrades = game->getUpgradeSystem();
    upgrades.totalRuns++;
    upgrades.addRunRecord(roomsCleared, enemiesKilled, riftsRepaired,
                          shardsCollected, m_currentDifficulty);
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

    // Void Sovereign defeated -> Ending sequence
    if (m_voidSovereignDefeated) {
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
    } else {
        // Copy balance stats to run summary
        if (auto* summary = dynamic_cast<RunSummaryState*>(game->getState(StateID::RunSummary))) {
            summary->enemiesKilled = enemiesKilled;
            summary->riftsRepaired = riftsRepaired;
            summary->roomsCleared = roomsCleared;
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
        game->changeState(StateID::RunSummary);
    }
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

void PlayState::renderChallengeHUD(SDL_Renderer* renderer, TTF_Font* font) {
    if (g_activeChallenge == ChallengeID::None && g_activeMutators[0] == MutatorID::None) return;
    if (!font) return;

    int y = 90;
    // Challenge name
    if (g_activeChallenge != ChallengeID::None) {
        const auto& cd = ChallengeMode::getChallengeData(g_activeChallenge);
        SDL_Color cc = {255, 200, 60, 200};
        SDL_Surface* s = TTF_RenderText_Blended(font, cd.name, cc);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {15, y, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
        y += 20;

        // Speedrun timer
        if (g_activeChallenge == ChallengeID::Speedrun && m_challengeTimer > 0) {
            int mins = static_cast<int>(m_challengeTimer) / 60;
            int secs = static_cast<int>(m_challengeTimer) % 60;
            char timerText[32];
            std::snprintf(timerText, sizeof(timerText), "%d:%02d", mins, secs);
            SDL_Color tc = (m_challengeTimer < 60) ? SDL_Color{255, 60, 60, 255} : SDL_Color{200, 200, 200, 220};
            SDL_Surface* ts = TTF_RenderText_Blended(font, timerText, tc);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    SDL_Rect tr = {15, y, ts->w, ts->h};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
            y += 20;
        }

        // Endless score
        if (g_activeChallenge == ChallengeID::EndlessRift) {
            char scoreText[32];
            std::snprintf(scoreText, sizeof(scoreText), "Score: %d", m_endlessScore);
            SDL_Surface* ss = TTF_RenderText_Blended(font, scoreText, SDL_Color{200, 180, 255, 220});
            if (ss) {
                SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
                if (st) {
                    SDL_Rect sr = {15, y, ss->w, ss->h};
                    SDL_RenderCopy(renderer, st, nullptr, &sr);
                    SDL_DestroyTexture(st);
                }
                SDL_FreeSurface(ss);
            }
            y += 20;
        }
    }

    // Active mutators
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::None) continue;
        const auto& md = ChallengeMode::getMutatorData(g_activeMutators[i]);
        SDL_Surface* ms = TTF_RenderText_Blended(font, md.name, SDL_Color{180, 180, 220, 160});
        if (ms) {
            SDL_Texture* mt = SDL_CreateTextureFromSurface(renderer, ms);
            if (mt) {
                SDL_Rect mr = {15, y, ms->w, ms->h};
                SDL_RenderCopy(renderer, mt, nullptr, &mr);
                SDL_DestroyTexture(mt);
            }
            SDL_FreeSurface(ms);
        }
        y += 16;
    }
}

// ============ NPC System ============

void PlayState::checkNPCInteraction() {
    if (!m_player || !m_level || m_showNPCDialog) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 playerCenter = pt.getCenter();
    int currentDim = m_dimManager.getCurrentDimension();
    const auto& input = game->getInput();

    m_nearNPCIndex = -1;

    auto& npcs = m_level->getNPCs();
    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        float dx = playerCenter.x - npc.position.x;
        float dy = playerCenter.y - npc.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 60.0f) {
            m_nearNPCIndex = i;

            if (input.isActionPressed(Action::Interact)) {
                m_showNPCDialog = true;
                m_npcDialogChoice = 0;
                AudioManager::instance().play(SFX::MenuConfirm);
            }
            break;
        }
    }
}

void PlayState::renderNPCs(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& npcs = m_level->getNPCs();

    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        SDL_FRect worldRect = {npc.position.x - 12, npc.position.y - 32, 24, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 3.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC color by type
        SDL_Color col;
        switch (npc.type) {
            case NPCType::RiftScholar:  col = {100, 180, 255, 255}; break; // Blue
            case NPCType::DimRefugee:   col = {255, 180, 80, 255}; break;  // Orange
            case NPCType::LostEngineer: col = {180, 255, 120, 255}; break; // Green
            case NPCType::EchoOfSelf:   col = {220, 120, 255, 255}; break; // Purple
            default:                    col = {200, 200, 200, 255}; break;
        }

        // Glow aura
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i * 2.0f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(35 * glow));
        SDL_Rect glowR = {sr.x - 8, sr.y - 8, sr.w + 16, sr.h + 16};
        SDL_RenderFillRect(renderer, &glowR);

        // Body (hooded figure shape)
        SDL_SetRenderDrawColor(renderer, col.r / 2, col.g / 2, col.b / 2, 220);
        SDL_RenderFillRect(renderer, &sr); // Robe

        // Head (smaller rect on top)
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 240);
        SDL_Rect head = {sr.x + sr.w / 4, sr.y + 2, sr.w / 2, sr.h / 3};
        SDL_RenderFillRect(renderer, &head);

        // Eyes
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        int eyeY = sr.y + sr.h / 5;
        SDL_Rect leftEye = {sr.x + sr.w / 3 - 2, eyeY, 3, 3};
        SDL_Rect rightEye = {sr.x + sr.w * 2 / 3 - 1, eyeY, 3, 3};
        SDL_RenderFillRect(renderer, &leftEye);
        SDL_RenderFillRect(renderer, &rightEye);

        // Type-specific detail
        switch (npc.type) {
            case NPCType::RiftScholar: {
                SDL_SetRenderDrawColor(renderer, 100, 180, 255, 160);
                SDL_Rect book = {sr.x + sr.w / 2 - 4, sr.y - 12, 8, 6};
                SDL_RenderFillRect(renderer, &book);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 12, sr.x + sr.w / 2, sr.y - 6);
                break;
            }
            case NPCType::DimRefugee: {
                SDL_SetRenderDrawColor(renderer, 255, 200, 60, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 14, sr.x + sr.w / 2 + 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 + 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 2);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 2, sr.x + sr.w / 2 - 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 14);
                break;
            }
            case NPCType::LostEngineer: {
                SDL_SetRenderDrawColor(renderer, 180, 255, 120, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 4, sr.y - 12, sr.x + sr.w / 2 + 4, sr.y - 4);
                break;
            }
            case NPCType::EchoOfSelf: {
                SDL_SetRenderDrawColor(renderer, 220, 120, 255, static_cast<Uint8>(100 * glow));
                SDL_Rect mirrorR = {sr.x - 2, sr.y, sr.w + 4, sr.h};
                SDL_RenderDrawRect(renderer, &mirrorR);
                break;
            }
            default: break;
        }

        // Interaction hint
        if (i == m_nearNPCIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", npc.name);
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y - 28, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }
}

void PlayState::renderNPCDialog(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_nearNPCIndex < 0 || !m_level) return;

    auto& npcs = m_level->getNPCs();
    if (m_nearNPCIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[m_nearNPCIndex];

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Dialog box
    int boxW = 500, boxH = 260;
    int boxX = 640 - boxW / 2, boxY = 360 - boxH / 2;

    SDL_SetRenderDrawColor(renderer, 20, 15, 35, 240);
    SDL_Rect box = {boxX, boxY, boxW, boxH};
    SDL_RenderFillRect(renderer, &box);

    // Border
    SDL_SetRenderDrawColor(renderer, 120, 80, 200, 200);
    SDL_RenderDrawRect(renderer, &box);

    // NPC name
    SDL_Color nameColor = {180, 140, 255, 255};
    SDL_Surface* ns = TTF_RenderText_Blended(font, npc.name, nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            SDL_Rect nr = {boxX + 20, boxY + 15, ns->w, ns->h};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    // Dialog text
    SDL_Color textColor = {200, 200, 220, 255};
    SDL_Surface* ds = TTF_RenderText_Blended(font, npc.greeting, textColor);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            SDL_Rect dr = {boxX + 20, boxY + 45, ds->w, ds->h};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }

    // Lore line
    SDL_Surface* ls = TTF_RenderText_Blended(font, npc.dialogLine, SDL_Color{160, 160, 180, 200});
    if (ls) {
        SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
        if (lt) {
            SDL_Rect lr = {boxX + 20, boxY + 70, ls->w, ls->h};
            SDL_RenderCopy(renderer, lt, nullptr, &lr);
            SDL_DestroyTexture(lt);
        }
        SDL_FreeSurface(ls);
    }

    // Dialog options
    auto options = NPCSystem::getDialogOptions(npc.type);
    int optY = boxY + 110;
    for (int i = 0; i < static_cast<int>(options.size()); i++) {
        bool selected = (i == m_npcDialogChoice);
        SDL_Color oc = selected ? SDL_Color{255, 220, 100, 255} : SDL_Color{180, 180, 200, 200};

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 80, 60, 120, 100);
            SDL_Rect selR = {boxX + 15, optY - 2, boxW - 30, 22};
            SDL_RenderFillRect(renderer, &selR);

            // Arrow
            SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
            SDL_RenderDrawLine(renderer, boxX + 20, optY + 8, boxX + 28, optY + 4);
            SDL_RenderDrawLine(renderer, boxX + 28, optY + 4, boxX + 20, optY);
        }

        SDL_Surface* os = TTF_RenderText_Blended(font, options[i], oc);
        if (os) {
            SDL_Texture* ot = SDL_CreateTextureFromSurface(renderer, os);
            if (ot) {
                SDL_Rect or_ = {boxX + 35, optY, os->w, os->h};
                SDL_RenderCopy(renderer, ot, nullptr, &or_);
                SDL_DestroyTexture(ot);
            }
            SDL_FreeSurface(os);
        }
        optY += 26;
    }

    // Controls hint
    SDL_Surface* hs = TTF_RenderText_Blended(font, "[W/S] Navigate  [Enter] Select  [Esc] Close",
                                              SDL_Color{120, 120, 140, 150});
    if (hs) {
        SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
        if (ht) {
            SDL_Rect hr = {boxX + boxW / 2 - hs->w / 2, boxY + boxH - 25, hs->w, hs->h};
            SDL_RenderCopy(renderer, ht, nullptr, &hr);
            SDL_DestroyTexture(ht);
        }
        SDL_FreeSurface(hs);
    }
}

void PlayState::handleNPCDialogChoice(int npcIndex, int choice) {
    if (!m_level || npcIndex < 0) return;
    auto& npcs = m_level->getNPCs();
    if (npcIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[npcIndex];
    auto options = NPCSystem::getDialogOptions(npc.type);

    // Last option is always [Leave]
    if (choice == static_cast<int>(options.size()) - 1) {
        m_showNPCDialog = false;
        return;
    }

    if (!m_player->getEntity()->hasComponent<HealthComponent>() ||
        !m_player->getEntity()->hasComponent<CombatComponent>()) return;
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();

    // Lore: Forgotten Craft - NPC interaction
    if (auto* lore = game->getLoreSystem()) {
        if (!lore->isDiscovered(LoreID::ForgottenCraft)) {
            lore->discover(LoreID::ForgottenCraft);
            AudioManager::instance().play(SFX::LoreDiscover);
        }
    }

    switch (npc.type) {
        case NPCType::RiftScholar:
            // Give tip + small shard bonus
            AudioManager::instance().play(SFX::Pickup);
            game->getUpgradeSystem().addRiftShards(15);
            shardsCollected += 15;
            m_particles.burst(npc.position, 12, {100, 180, 255, 255}, 100.0f, 2.0f);
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::DimRefugee:
            if (choice == 0) {
                // Trade 20 HP for 50 Shards
                if (hp.currentHP > 25) {
                    hp.currentHP -= 20;
                    game->getUpgradeSystem().addRiftShards(50);
                    shardsCollected += 50;
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 15, {255, 200, 60, 255}, 120.0f, 2.0f);
                } else {
                    AudioManager::instance().play(SFX::RiftFail);
                    m_showNPCDialog = false;
                    return;
                }
            } else if (choice == 1) {
                // Swap random relic (just give bonus shards if no relics)
                game->getUpgradeSystem().addRiftShards(30);
                shardsCollected += 30;
                AudioManager::instance().play(SFX::Pickup);
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::LostEngineer:
            // Upgrade weapon (+30% damage temporarily)
            combat.meleeAttack.damage *= 1.3f;
            combat.rangedAttack.damage *= 1.3f;
            AudioManager::instance().play(SFX::Pickup);
            m_particles.burst(npc.position, 20, {180, 255, 120, 255}, 150.0f, 3.0f);
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::EchoOfSelf:
            if (choice == 0) {
                // Fight! Spawn a tough enemy as "mirror match"
                Vec2 spawnPos = {npc.position.x + 60, npc.position.y};
                auto& mirror = Enemy::createByType(m_entities, static_cast<int>(EnemyType::Charger),
                                                   spawnPos, m_dimManager.getCurrentDimension());
                auto& mirrorHP = mirror.getComponent<HealthComponent>();
                mirrorHP.maxHP = hp.maxHP;
                mirrorHP.currentHP = mirrorHP.maxHP;
                auto& mirrorCombat = mirror.getComponent<CombatComponent>();
                mirrorCombat.meleeAttack.damage = combat.meleeAttack.damage;
                Enemy::makeElite(mirror, EliteModifier::Berserker);
                AudioManager::instance().play(SFX::AnomalySpawn);
                m_particles.burst(npc.position, 25, {220, 120, 255, 255}, 200.0f, 3.0f);
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        default:
            m_showNPCDialog = false;
            break;
    }
}

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

void PlayState::updateSmokeTest(float dt) {
    if (!m_player || !m_level) return;

    m_smokeRunTime += dt;
    m_smokeActionTimer -= dt;
    m_smokeDimTimer -= dt;

    // God mode: keep HP full
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    hp.currentHP = hp.maxHP;

    // Periodic status log every 5 seconds
    static float lastStatusLog = 0;
    if (m_smokeRunTime - lastStatusLog >= 5.0f) {
        lastStatusLog = m_smokeRunTime;
        Vec2 pos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        auto rifts = m_level->getRiftPositions();
        SDL_Log("[SMOKE] t=%.0fs pos=(%.0f,%.0f) vel=(%.0f,%.0f) rifts=%d ground=%d",
                m_smokeRunTime, pos.x, pos.y, phys.velocity.x, phys.velocity.y,
                static_cast<int>(rifts.size()), phys.onGround ? 1 : 0);
        fflush(stderr);
    }

    // Log level start once
    static int lastLoggedLevel = -1;
    if (m_currentDifficulty != lastLoggedLevel) {
        lastLoggedLevel = m_currentDifficulty;
        auto rifts = m_level->getRiftPositions();
        SDL_Log("[SMOKE] Level %d started | %d rifts | %.0f,%.0f exit",
                m_currentDifficulty, static_cast<int>(rifts.size()),
                m_level->getExitPoint().x, m_level->getExitPoint().y);
    }

    // Auto-quit after completing level 1 (level 2 starts = level 1 done)
    if (m_currentDifficulty > 1) {
        SDL_Log("[SMOKE] Level 1 completed successfully in %.1fs! Quitting.", m_smokeRunTime);
        writeBalanceSnapshot();
        game->quit();
        return;
    }

    // Timeout safety: quit if stuck for 120s
    if (m_smokeRunTime > 120.0f) {
        SDL_Log("[SMOKE] TIMEOUT after 120s on level %d — possible stuck/bug", m_currentDifficulty);
        writeBalanceSnapshot();
        game->quit();
        return;
    }

    // Auto-select relic when choice appears
    if (m_showRelicChoice && !m_relicChoices.empty()) {
        int pick = std::rand() % static_cast<int>(m_relicChoices.size());
        SDL_Log("[SMOKE] Picked relic: %d", static_cast<int>(m_relicChoices[pick]));
        selectRelic(pick);
        return;
    }

    // Auto-dismiss NPC dialog
    if (m_showNPCDialog) {
        m_showNPCDialog = false;
        return;
    }

    // Auto-solve active puzzle
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        switch (m_activePuzzle->getType()) {
            case PuzzleType::Timing:
                // Press confirm (action 4) — even if not in sweet spot,
                // it just won't count. Spam it every frame.
                m_activePuzzle->handleInput(4);
                break;
            case PuzzleType::Sequence:
                // Feed the correct sequence directly
                if (!m_activePuzzle->isShowingSequence()) {
                    int idx = static_cast<int>(m_activePuzzle->getPlayerInput().size());
                    if (idx < static_cast<int>(m_activePuzzle->getSequence().size())) {
                        m_activePuzzle->handleInput(m_activePuzzle->getSequence()[idx]);
                    }
                }
                break;
            case PuzzleType::Alignment:
                // Rotate until aligned
                if (m_activePuzzle->getCurrentRotation() != m_activePuzzle->getTargetRotation()) {
                    m_activePuzzle->handleInput(1); // rotate right
                }
                break;
        }
        return; // Don't move while solving puzzle
    }

    // Navigate toward exit or nearest rift
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    Vec2 target = m_level->getExitPoint();

    // If rifts remain and not collapsing, go to nearest rift first
    auto rifts = m_level->getRiftPositions();
    float nearestRiftDist = 99999.0f;
    if (!m_collapsing) {
        for (auto& r : rifts) {
            float dx = r.x - playerPos.x;
            float dy = r.y - playerPos.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < nearestRiftDist) {
                nearestRiftDist = d;
                target = r;
            }
        }
    }

    // FIX: Use input injection instead of direct velocity manipulation.
    // Direct velocity was overridden by Player::handleMovement + PhysicsSystem friction.
    float dirX = target.x - playerPos.x;
    float dirY = target.y - playerPos.y;
    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
    auto& inputMut = game->getInputMutable();

    // FIX: Stuck detection — if position barely changed in 3s, move toward exit instead
    static Vec2 lastStuckCheckPos = {0, 0};
    static float stuckTimer = 0;
    float movedDist = std::sqrt((playerPos.x - lastStuckCheckPos.x) * (playerPos.x - lastStuckCheckPos.x)
                              + (playerPos.y - lastStuckCheckPos.y) * (playerPos.y - lastStuckCheckPos.y));
    if (movedDist < 30.0f) {
        stuckTimer += dt;
    } else {
        stuckTimer = 0;
        lastStuckCheckPos = playerPos;
    }

    // If stuck for 3s, ignore rift and head for exit; also try random jumps
    bool isStuck = stuckTimer > 3.0f;
    if (isStuck) {
        Vec2 exitPos = m_level->getExitPoint();
        dirX = exitPos.x - playerPos.x;
        dirY = exitPos.y - playerPos.y;
        // Reset stuck timer periodically so we re-evaluate
        if (stuckTimer > 6.0f) {
            stuckTimer = 0;
            lastStuckCheckPos = playerPos;
        }
    }

    // Horizontal movement: also move if target is mainly above/below (need to navigate platforms)
    if (dirX > 30.0f) {
        inputMut.injectActionPress(Action::MoveRight);
    } else if (dirX < -30.0f) {
        inputMut.injectActionPress(Action::MoveLeft);
    } else if (std::abs(dirY) > 40.0f) {
        // Target is above/below but close in X — move right toward exit to find platforms
        Vec2 exitPos = m_level->getExitPoint();
        if (exitPos.x > playerPos.x + 30.0f)
            inputMut.injectActionPress(Action::MoveRight);
        else if (exitPos.x < playerPos.x - 30.0f)
            inputMut.injectActionPress(Action::MoveLeft);
        else
            inputMut.injectActionPress(std::rand() % 2 ? Action::MoveRight : Action::MoveLeft);
    }

    // FIX: jumpForce is already negative (-420), so use input injection for jumps.
    bool blocked = (phys.onWallLeft || phys.onWallRight) ||
                   (std::abs(phys.velocity.x) < 10.0f && std::abs(dirX) > 50.0f);
    bool targetAbove = dirY < -40.0f;
    if (phys.onGround && (blocked || targetAbove || isStuck || (std::rand() % 60 == 0))) {
        inputMut.injectActionPress(Action::Jump);
    }
    // FIX: Wall-jump when sliding on wall — essential for navigating platformer terrain
    if (m_player->isWallSliding) {
        inputMut.injectActionPress(Action::Jump);
    }
    // Double-jump in air when target is above or stuck
    if (!phys.onGround && !m_player->isWallSliding && m_player->jumpsRemaining > 0
        && (targetAbove || isStuck || (std::rand() % 20 == 0))) {
        inputMut.injectActionPress(Action::Jump);
    }

    // Interact with rift when near (inject Interact input so normal update handles it)
    if (nearestRiftDist < 60.0f && !m_activePuzzle && m_nearRiftIndex >= 0) {
        game->getInputMutable().injectActionPress(Action::Interact);
        SDL_Log("[SMOKE] Interacting with rift %d (dist=%.0f)", m_nearRiftIndex, nearestRiftDist);
    }

    // Attack nearby enemies
    if (m_smokeActionTimer <= 0) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        float nearestEnemy = 99999.0f;
        Vec2 enemyDir = {1.0f, 0.0f};
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos || !e.isAlive()) return;
            if (!e.hasComponent<TransformComponent>()) return;
            auto& et = e.getComponent<TransformComponent>();
            float dx = et.getCenter().x - playerPos.x;
            float dy = et.getCenter().y - playerPos.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < nearestEnemy) {
                nearestEnemy = d;
                float len = d;
                enemyDir = (len > 0) ? Vec2{dx / len, dy / len} : Vec2{1.0f, 0.0f};
            }
        });

        if (nearestEnemy < 80.0f) {
            combat.startAttack(AttackType::Melee, enemyDir);
            m_smokeActionTimer = 0.25f;
        } else if (nearestEnemy < 300.0f) {
            combat.startAttack(AttackType::Ranged, enemyDir);
            m_smokeActionTimer = 0.4f;
        } else {
            m_smokeActionTimer = 0.1f;
        }
    }

    // FIX: Use input injection for dimension switch to trigger all associated effects
    if (m_smokeDimTimer <= 0) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_smokeDimTimer = 5.0f + static_cast<float>(std::rand() % 30) * 0.1f;
    }

    // FIX: Use input injection for dash instead of direct state manipulation
    if (std::rand() % 120 == 0 && phys.onGround && m_player->dashCooldownTimer <= 0) {
        inputMut.injectActionPress(Action::Dash);
    }
}
