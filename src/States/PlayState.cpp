#include "PlayState.h"
#include <tracy/Tracy.hpp>
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
#include "States/EndingState.h"
#include "States/RunSummaryState.h"
#include "States/DailyLeaderboardState.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdarg>

// Centralized music track paths
static const char* const ZONE_TRACKS[] = {
    "assets/music/zone1.ogg",
    "assets/music/zone2.ogg",
    "assets/music/zone3.ogg",
};
static constexpr const char* BOSS_TRACK = "assets/music/boss_theme.ogg";
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

// g_smokeFile and smokeLog defined in PlayStateSmokeBot.cpp
// g_playtestFile defined in PlayStatePlaytest.cpp
extern FILE* g_smokeFile;
extern FILE* g_playtestFile;
extern void smokeLog(const char* fmt, ...);


void PlayState::enter() {
    if (g_autoSmokeTest) {
        m_smokeTest = true;
        // Open persistent log file
        if (g_smokeFile) fclose(g_smokeFile);
        g_smokeFile = fopen("smoke_test.log", "w");
        if (g_smokeFile) {
            fprintf(g_smokeFile, "=== RIFTWALKER SMOKE TEST ===\n");
            fflush(g_smokeFile);
        }
    }
    if (g_autoPlaytest) {
        extern int g_playtestRuns;
        extern int g_playtestProfile;
        extern char g_playtestOutputFile[];
        m_playtest = true;
        m_playtestMaxRuns = g_playtestRuns;
        if (g_playtestFile) fclose(g_playtestFile);
        g_playtestFile = fopen(g_playtestOutputFile, "w");
        m_playtestRun = 0;
        m_playtestDeaths = 0;
        m_playtestBestLevel = 0;
        static const char* profileNames[] = {"balanced", "aggressive", "defensive", "speedrun"};
        playtestLog("========================================");
        playtestLog("  RIFTWALKER BALANCE PLAYTEST");
        playtestLog("  Profile: %s | Runs: %d", profileNames[g_playtestProfile], m_playtestMaxRuns);
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
    for (auto& d : m_damageNumbers) {
        if (d.cachedText)   SDL_DestroyTexture(d.cachedText);
        if (d.cachedShadow) SDL_DestroyTexture(d.cachedShadow);
    }
    m_damageNumbers.clear();
    // Free cached UI textures so they don't leak when PlayState is re-entered
    if (m_killStreakCachedTex) { SDL_DestroyTexture(m_killStreakCachedTex); m_killStreakCachedTex = nullptr; }
    if (m_levelUpLabelCachedTex) { SDL_DestroyTexture(m_levelUpLabelCachedTex); m_levelUpLabelCachedTex = nullptr; }
    if (m_levelUpNumCachedTex) { SDL_DestroyTexture(m_levelUpNumCachedTex); m_levelUpNumCachedTex = nullptr; }
    m_killStreakCachedKey.clear();
    m_levelUpLabelCachedKey.clear();
    m_levelUpNumCachedKey.clear();
    m_entities.clear();
    m_particles.clear();
    m_player.reset();
    m_level.reset();
    m_activePuzzle.reset();
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
            bool hasQuest = m_activeQuest.active || m_activeQuest.completed;
            auto options = NPCSystem::getDialogOptions(npcs[m_nearNPCIndex].type, stage, hasQuest);
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

    // Gamepad support for in-game menus (relic choice, NPC dialog, puzzle)
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        auto btn = event.cbutton.button;

        if (btn == SDL_CONTROLLER_BUTTON_START) {
            if (!m_showRelicChoice && !(m_activePuzzle && m_activePuzzle->isActive())) {
                game->pushState(StateID::Pause);
            }
            return;
        }

        // Relic choice
        if (m_showRelicChoice && !m_relicChoices.empty()) {
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                m_relicChoiceSelected = (m_relicChoiceSelected - 1 + static_cast<int>(m_relicChoices.size()))
                                        % static_cast<int>(m_relicChoices.size());
            } else if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                m_relicChoiceSelected = (m_relicChoiceSelected + 1)
                                        % static_cast<int>(m_relicChoices.size());
            } else if (btn == SDL_CONTROLLER_BUTTON_A) {
                selectRelic(m_relicChoiceSelected);
            }
            return;
        }

        // NPC dialog
        if (m_showNPCDialog && m_nearNPCIndex >= 0) {
            auto& npcs = m_level->getNPCs();
            if (m_nearNPCIndex >= static_cast<int>(npcs.size())) { m_showNPCDialog = false; return; }
            int stage = getNPCStoryStage(npcs[m_nearNPCIndex].type);
            bool hasQuest = m_activeQuest.active || m_activeQuest.completed;
            auto options = NPCSystem::getDialogOptions(npcs[m_nearNPCIndex].type, stage, hasQuest);
            int optCount = static_cast<int>(options.size());
            if (optCount == 0) { m_showNPCDialog = false; return; }
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                m_npcDialogChoice = (m_npcDialogChoice - 1 + optCount) % optCount;
                AudioManager::instance().play(SFX::MenuSelect);
            } else if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                m_npcDialogChoice = (m_npcDialogChoice + 1) % optCount;
                AudioManager::instance().play(SFX::MenuSelect);
            } else if (btn == SDL_CONTROLLER_BUTTON_A) {
                handleNPCDialogChoice(m_nearNPCIndex, m_npcDialogChoice);
            } else if (btn == SDL_CONTROLLER_BUTTON_B) {
                m_showNPCDialog = false;
            }
            return;
        }

        // Puzzle input
        if (m_activePuzzle && m_activePuzzle->isActive()) {
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP)    handlePuzzleInput(0);
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) handlePuzzleInput(1);
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)  handlePuzzleInput(2);
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT)  handlePuzzleInput(3);
            if (btn == SDL_CONTROLLER_BUTTON_A)          handlePuzzleInput(4);
            if (btn == SDL_CONTROLLER_BUTTON_B) m_activePuzzle.reset();
            return;
        }
    }
}

void PlayState::update(float dt) {
    ZoneScopedN("PlayStateUpdate");
    if (!m_player || !m_level) return;

    // Quick restart requested from pause menu
    if (m_pendingRestart) {
        m_pendingRestart = false;
        startNewRun();
        return;
    }

    // Generate level FIRST after returning from shop (before any gameplay logic)
    // This prevents stale state (old collapsing/exit) from triggering false completions
    if (m_pendingLevelGen) {
        m_pendingLevelGen = false;

        // Save player state before level regeneration (relics + HP + smith upgrades carry over)
        m_savedRelics.clear();
        m_savedHP = 0;
        m_savedMaxHP = 0;
        m_savedVoidHungerBonus = 0;
        m_savedSmithMeleeDmgMult = 1.0f;
        m_savedSmithRangedDmgMult = 1.0f;
        m_savedSmithAtkSpdMult = 1.0f;
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
            m_savedSmithMeleeDmgMult = m_player->smithMeleeDmgMult;
            m_savedSmithRangedDmgMult = m_player->smithRangedDmgMult;
            m_savedSmithAtkSpdMult = m_player->smithAtkSpdMult;
        }

        generateLevel();
        // Update system pointers to new Player and Level (prevent dangling after regeneration)
        m_combatSystem.setPlayer(m_player.get());
        m_aiSystem.setLevel(m_level.get());
        m_aiSystem.setPlayer(m_player.get());
        applyRunBuffs();

        // Restore blacksmith weapon upgrades (per-run, persist across levels)
        if (m_player) {
            m_player->smithMeleeDmgMult = m_savedSmithMeleeDmgMult;
            m_player->smithRangedDmgMult = m_savedSmithRangedDmgMult;
            m_player->smithAtkSpdMult = m_savedSmithAtkSpdMult;
        }

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

    // Death sequence: dramatic freeze frame + slow-mo + zoom + desaturate
    if (m_playerDying) {
        float progress = 1.0f - (m_deathSequenceTimer / m_deathSequenceDuration);

        // Phase 1 (0-12%): Freeze frame — complete time stop for dramatic impact
        // Phase 2 (12-85%): Slow-mo at 15% speed
        // Phase 3 (85-100%): Ramp back to normal speed for transition
        float deathSlowdown;
        if (progress < 0.12f) {
            deathSlowdown = 0.0f; // Complete freeze
        } else if (progress < 0.85f) {
            deathSlowdown = 0.15f; // Very slow motion
        } else {
            deathSlowdown = 0.15f + (progress - 0.85f) / 0.15f * 0.85f; // Ramp to 1.0
        }
        float slowDt = dt * deathSlowdown;

        m_deathSequenceTimer -= dt; // Real-time countdown (not slowed)

        // Camera zoom: smoothly zoom in during death
        float targetZoom = 5.0f + 2.4f * std::min(1.0f, progress * 2.0f);
        m_camera.zoom += (targetZoom - m_camera.zoom) * std::min(1.0f, dt * 4.0f);

        m_camera.update(slowDt);       // camera shake continues (slowed)
        m_particles.update(slowDt);    // death particles animate (slowed)
        m_screenEffects.update(slowDt);
        updateDeathEffects(slowDt);    // death ghosts animate (slowed)

        // Slow-motion particle drip during death (enhanced: more particles, wider spread)
        if (m_player && m_deathSequenceTimer > 0.2f) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 center = pt.getCenter();
            Uint8 alpha = static_cast<Uint8>(80 + 140 * progress);
            // Primary ember drip
            m_particles.burst(
                {center.x + static_cast<float>((std::rand() % 40) - 20),
                 center.y + static_cast<float>((std::rand() % 30) - 15)},
                2 + static_cast<int>(progress * 4),
                {255, static_cast<Uint8>(60 + 100 * (1.0f - progress)), 40, alpha},
                50.0f + progress * 100.0f, 2.5f + progress * 4.0f);
            // Secondary soul-fragment particles (white/blue, float upward)
            if (std::rand() % 3 == 0) {
                m_particles.burst(
                    {center.x + static_cast<float>((std::rand() % 20) - 10),
                     center.y - 5.0f},
                    1 + static_cast<int>(progress * 2),
                    {200, 220, 255, static_cast<Uint8>(60 + 80 * progress)},
                    30.0f + progress * 40.0f, 1.5f + progress * 2.0f);
            }
        }

        if (m_deathSequenceTimer <= 0) {
            m_playerDying = false;
            m_camera.zoom = 5.0f; // Reset zoom before transitioning
            if (m_playtest) {
                playtestOnDeath();
            } else {
                endRun();
            }
        }
        return;
    }

    // Boss intro title card: freeze gameplay while the card is showing
    if (m_bossIntroActive) {
        m_screenEffects.update(dt);  // Animate the title card
        m_particles.update(dt);      // Ambient particles keep going
        if (!m_screenEffects.isBossIntroActive()) {
            m_bossIntroActive = false;
        }
        return;
    }

    // Run intro overlay: freeze gameplay while atmospheric text plays
    if (m_runIntroActive) {
        m_runIntroTimer += dt;
        m_particles.update(dt);      // Ambient particles keep going
        m_screenEffects.update(dt);
        if (m_runIntroTimer >= kRunIntroDuration) {
            m_runIntroActive = false;
        }
        return;
    }

    // Track balance stats every frame
    updateBalanceTracking(dt);

    // Smoke test: auto-play for integration testing
    if (m_smokeTest) updateSmokeTest(dt);
    // Playtest: human-like auto-play for balance testing
    if (m_playtest) updatePlaytest(dt);

    // Hit-freeze: brief pause on melee hit for impact feel.
    // Uses max() so a big charged hit still extends an already-draining freeze
    // from a previous normal hit (multi-hit frames felt thin with discard semantics).
    float freeze = m_combatSystem.consumeHitFreeze();
    if (freeze > 0) {
        m_hitFreezeTimer = std::max(m_hitFreezeTimer, freeze);
    }
    if (m_hitFreezeTimer > 0) {
        m_hitFreezeTimer -= dt;
        m_camera.update(dt);       // still update camera shake during freeze
        m_particles.update(dt);    // particles keep going
        return;
    }

    // Kill slow-motion: time dilation after significant kills
    if (m_killSlowMoTimer > 0) {
        m_killSlowMoTimer -= dt;
        float progress = 1.0f - (m_killSlowMoTimer / 0.3f); // 0→1
        // Ease out: start slow, ramp back to normal
        m_killSlowMoScale = 0.4f + 0.6f * (progress * progress);
        if (m_killSlowMoTimer <= 0) m_killSlowMoScale = 1.0f;
        dt *= m_killSlowMoScale;
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

    updateDimensionSwitch();
    m_dimManager.playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_dimManager.update(dt);

    updateDimensionEffects(dt);

    // Run time tracking
    m_runTime += dt;

    // Player update
    m_player->update(dt, game->getInput());

    // Update tile timers (crumbling etc.)
    if (m_level) m_level->updateTiles(dt);

    // Physics - pass current dimension so player (dimension=0) collides with correct tiles
    m_physics.update(m_entities, dt, m_level.get(), m_dimManager.getCurrentDimension());

    updatePlayerPhysicsEffects(dt);

    // AI
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_aiSystem.update(m_entities, dt, playerPos, m_dimManager.getCurrentDimension());

    updateTechnomancerEntities(dt);

    updateBossEffects(dt);

    // Collision
    m_collision.update(m_entities, m_dimManager.getCurrentDimension());

    updateEnemyHazardDamage(dt);

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
            if (m_isBossLevel) m_tookDamageThisBoss = true;
            m_totalDamageTaken += static_cast<int>(evt.damage);
            // Screen shake + damage flash + hurt SFX on enemy combat hits
            // Skip if source already handled feedback (hazards, DoT)
            if (!evt.feedbackHandled) {
                m_camera.shake(6.0f, 0.15f);
                m_hud.triggerDamageFlash();
                AudioManager::instance().play(SFX::PlayerHurt);
                game->getInputMutable().rumble(0.6f, 150);
                // Chromatic aberration: stronger for big hits
                float dmgRatio = std::min(1.0f, evt.damage / 30.0f);
                m_screenEffects.triggerDamageChromaticAberration(0.5f + 0.5f * dmgRatio);
            }
            // Directional damage indicator: show red edge flash toward damage source
            if (m_player && (evt.sourcePos.x != 0 || evt.sourcePos.y != 0)) {
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                float dx = evt.sourcePos.x - pPos.x;
                float dy = evt.sourcePos.y - pPos.y;
                if (dx * dx + dy * dy > 1.0f) { // Avoid zero-length direction
                    float angle = std::atan2(dy, dx);
                    m_damageIndicators.push_back({angle, 0.8f, 0.8f});
                }
            }
        }
    }

    // Consume parry events for "PARRY!" floating text + achievement tracking
    for (auto& pe : m_combatSystem.parryEvents) {
        m_parryCountThisRun++;
        if (m_parryCountThisRun >= 20) {
            game->getAchievements().unlock("parry_master");
        }
        FloatingDamageNumber num;
        num.position = {pe.position.x, pe.position.y - 20.0f};
        num.value = 0;
        num.isBuff = true;
        num.buffText = LOC("hud.parry");
        num.lifetime = 1.0f;
        num.maxLifetime = 1.0f;
        num.isPlayerDamage = false;
        m_damageNumbers.push_back(num);
    }
    m_combatSystem.parryEvents.clear();

    // Consume reflect events for "REFLECT!" floating text
    for (auto& re : m_combatSystem.reflectEvents) {
        FloatingDamageNumber num;
        num.position = {re.position.x, re.position.y - 30.0f};
        num.value = 0;
        num.isBuff = true;
        num.buffText = LOC("hud.reflect");
        num.lifetime = 1.2f;
        num.maxLifetime = 1.2f;
        num.isPlayerDamage = false;
        m_damageNumbers.push_back(num);
    }
    m_combatSystem.reflectEvents.clear();

    // Wave/area clear celebration: all waves spawned + all enemies dead
    if (m_combatSystem.killCount > 0 && !m_isBossLevel && !m_waveClearTriggered) {
        int aliveAfterCombat = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.isEnemy) aliveAfterCombat++;
        });
        if (aliveAfterCombat == 0 && m_currentWave >= static_cast<int>(m_spawnWaves.size())) {
            m_waveClearTriggered = true;
            m_waveClearTimer = 2.0f;
            m_waveClearFlashTimer = 0.15f; // screen brightness pulse
            m_combatSystem.addHitFreeze(0.15f);
            m_camera.shake(10.0f, 0.3f);
            game->getInputMutable().rumble(0.5f, 200);
            AudioManager::instance().play(SFX::ShrineBlessing);
            if (m_player) {
                // XP bonus for clearing the wave
                constexpr int waveClearXP = 25;
                m_player->addXP(waveClearXP);
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                // Floating "+25 XP" indicator
                FloatingDamageNumber xpNum;
                xpNum.position = {pPos.x, pPos.y - 30.0f};
                xpNum.value = 0;
                xpNum.isBuff = true;
                xpNum.buffText = "+25 XP";
                xpNum.lifetime = 1.2f;
                xpNum.maxLifetime = 1.2f;
                xpNum.isPlayerDamage = false;
                m_damageNumbers.push_back(xpNum);
                // Cyan/white celebration particles
                m_particles.burst(pPos, 40, {0, 255, 255, 255}, 300.0f, 5.0f);
                m_particles.burst(pPos, 20, {220, 255, 255, 255}, 200.0f, 4.0f);
            }
        }
    }
    if (m_waveClearTimer > 0) m_waveClearTimer -= dt;
    if (m_waveClearFlashTimer > 0) m_waveClearFlashTimer -= dt;
    if (m_weaponSwitchDisplayTimer > 0) m_weaponSwitchDisplayTimer -= dt;

    // Kill streak timers
    if (m_killStreakTimer > 0) {
        m_killStreakTimer -= dt;
        if (m_killStreakTimer <= 0) {
            m_killStreakCount = 0;
        }
    }
    if (m_killStreakDisplayTimer > 0) m_killStreakDisplayTimer -= dt;

    // Level-up celebration timers
    if (m_levelUpTimer > 0) m_levelUpTimer -= dt;
    if (m_levelUpFlashTimer > 0) m_levelUpFlashTimer -= dt;

    // Zone transition banner timer
    if (m_zoneTransitionActive) {
        m_zoneTransitionTimer += dt;
        if (m_zoneTransitionTimer >= 3.0f) {
            m_zoneTransitionActive = false;
        }
    }

    updateDamageNumbers(dt);
    updateDamageIndicators(dt);

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

        // Combo Finisher availability: at 5+ combo, open a 2s window to trigger finisher
        if (combo >= 5 && m_player->finisherCooldown <= 0
            && m_player->finisherAvailable == Player::FinisherTier::None
            && !m_player->finisherExecuting) {
            m_player->finisherAvailable = Player::FinisherTier::Minor;
            m_player->finisherAvailableTimer = m_player->finisherAvailableMaxTime;
        }

        // Combo break: clear finisher availability
        if (combo == 0) {
            m_lastComboMilestone = 0;
            if (m_player->finisherAvailable != Player::FinisherTier::None) {
                m_player->finisherAvailable = Player::FinisherTier::None;
                m_player->finisherAvailableTimer = 0;
            }
        }
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
        float magnetRangeSq = magnetRange * magnetRange;
        m_entities.forEach([&](Entity& e) {
            if (!e.isPickup) return;
            if (!e.hasComponent<TransformComponent>() || !e.hasComponent<PhysicsBody>()) return;

            auto& t = e.getComponent<TransformComponent>();
            Vec2 pickupPos = t.getCenter();
            float dx = pPos.x - pickupPos.x;
            float dy = pPos.y - pickupPos.y;
            float distSq = dx * dx + dy * dy;

            // Out of range: skip (no sqrt needed)
            if (distSq >= magnetRangeSq || distSq <= 25.0f) return;
            float dist = std::sqrt(distSq);
            float strength = 300.0f * (1.0f - dist / magnetRange);
            auto& phys = e.getComponent<PhysicsBody>();
            phys.velocity.x += (dx / dist) * strength * dt;
            phys.velocity.y += (dy / dist) * strength * dt;
        });
    }

    // Entities
    m_entities.update(dt);
    m_entities.refresh();

    // Check boss defeated
    if (m_isBossLevel && !m_bossDefeated) {
        bool bossAlive = false;
        m_entities.forEach([&](Entity& e) {
            if (e.isBoss) bossAlive = true;
        });
        if (!bossAlive) {
            m_bossDefeated = true;
            // Resume zone music after boss kill
            {
                int trackIdx = std::clamp(getZone(m_currentDifficulty) - 1, 0, 2);
                AudioManager::instance().playMusic(ZONE_TRACKS[trackIdx]);
            }
            // Dramatic zoom-in on boss kill
            m_camera.zoomTarget = 5.6f;
            m_camera.zoomSpeed = 3.0f;
            // Boss kill rewards
            float bossShardMult = game->getRunBuffSystem().getShardMultiplier();
            bossShardMult *= m_achievementShardMult;
            if (m_player->getEntity()->hasComponent<RelicComponent>())
                bossShardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
            // Boss shards scale with zone: zone bosses give more than mid-bosses
            int baseShards = isBossFloor(m_currentDifficulty) ? 80 : 50;
            int bossShards = static_cast<int>((baseShards + getZone(m_currentDifficulty) * 30) * bossShardMult);
            shardsCollected += bossShards;
            game->getUpgradeSystem().addRiftShards(bossShards);
            AudioManager::instance().play(SFX::LevelComplete);
            m_camera.shake(15.0f, 0.5f);
            Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(playerPos, 60, {255, 200, 100, 255}, 350.0f, 6.0f);

            // Boss achievements
            game->getAchievements().unlock("boss_slayer");
            if (!m_tookDamageThisBoss) {
                game->getAchievements().unlock("no_damage_boss");
            }
            int bossIdx = m_currentDifficulty / 3;
            // wyrm_hunter: fires only on actual Void Wyrm kills.
            // Void Wyrm spawns on mid-boss floors where zone % 2 == 1 (floors 9, 21),
            // see mid-boss rotation in PlayStateRunLifecycle.cpp::spawnBossForFloor.
            if (isMidBossFloor(m_currentDifficulty) && getZone(m_currentDifficulty) % 2 == 1) {
                game->getAchievements().unlock("wyrm_hunter");
            }

            // Lore triggers on boss kill
            {
                auto* lore = game->getLoreSystem();
                if (lore) {
                    int bossTypeForLore = bossIdx % 4;
                    if (m_currentDifficulty >= 30) {
                        // Void Sovereign killed (final boss at floor 30)
                        lore->discover(LoreID::SovereignTruth);
                        Bestiary::onBossKill(4);
                        Bestiary::save("bestiary_save.dat");
                        AudioManager::instance().play(SFX::LoreDiscover);
                    } else if (getZone(m_currentDifficulty) >= 3 && isBossFloor(m_currentDifficulty)) {
                        // Entropy Incarnate killed (zone 4 boss at floor 24)
                        Bestiary::onBossKill(5);
                        Bestiary::save("bestiary_save.dat");
                        lore->discover(LoreID::EntropyIncarnate);
                        AudioManager::instance().play(SFX::LoreDiscover);
                    } else {
                        Bestiary::onBossKill(bossTypeForLore);
                        Bestiary::save("bestiary_save.dat");
                        if (bossTypeForLore == 0) lore->discover(LoreID::BossMemory1);
                        else if (bossTypeForLore == 1) lore->discover(LoreID::BossMemory2);
                        else if (bossTypeForLore == 2) lore->discover(LoreID::BossMemory3);
                        else if (bossTypeForLore == 3) lore->discover(LoreID::BossMemory4);
                    }
                }
            }

            bool finalBossDefeated = (m_currentDifficulty >= 30);

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

    updatePlayerHazardDamage(dt);

    updateStatusEffects(dt);
    if (m_playerDying) return; // Entropy critical death triggered in updateStatusEffects

    if (!updatePlayerDeath()) return; // Player dying - wait for death sequence

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

    // Death ghost effects + foreground fog
    updateDeathEffects(dt);
    updateFogParticles(dt);

    // DimResidue damage zones: tick down lifetime and deal AoE damage
    int curDim = m_dimManager.getCurrentDimension();
    float residueDps = 15.0f; // Default DPS
    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
        residueDps = RelicSynergy::getResidueDamage(m_player->getEntity()->getComponent<RelicComponent>());
    }
    m_entities.forEach([&](Entity& zone) {
        if (!zone.isDimResidue) return;
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
            if (!enemy.isEnemy) return;
            if (enemy.dimension != 0 && enemy.dimension != curDim) return;
            if (!enemy.hasComponent<TransformComponent>() || !enemy.hasComponent<HealthComponent>()) return;
            auto& et = enemy.getComponent<TransformComponent>();
            float dx = et.getCenter().x - zCenter.x;
            float dy = et.getCenter().y - zCenter.y;
            if (dx * dx + dy * dy < 48.0f * 48.0f) {
                enemy.getComponent<HealthComponent>().takeDamage(residueDps * dt);
            }
        });
    });

    // Ambient theme particles (subtle atmospheric effects matching world theme)
    m_ambientDustTimer += dt;
    if (m_ambientDustTimer >= 0.4f) {
        m_ambientDustTimer = 0;
        if (m_player) {
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            const auto& theme = (m_dimManager.getCurrentDimension() == 1) ? m_themeA : m_themeB;
            switch (theme.id) {
                case ThemeID::VictorianClockwork:
                    // Golden clockwork dust drifting down
                    m_particles.ambientThemeParticle(pPos, {180, 150, 80, 100}, 270.0f,
                        12.0f, 2.0f, 3.5f, 20.0f);
                    break;
                case ThemeID::DeepOcean:
                    // Light blue bubbles floating up
                    m_particles.ambientThemeParticle(pPos, {100, 180, 255, 90}, 90.0f,
                        18.0f, 2.5f, 4.0f, -30.0f);
                    break;
                case ThemeID::NeonCity:
                    // Neon sparks flickering (random bright colors)
                    {
                        Uint8 nr = (std::rand() % 2) ? 255 : 50;
                        Uint8 ng = (std::rand() % 2) ? 255 : 50;
                        Uint8 nb = (std::rand() % 2) ? 255 : 50;
                        m_particles.ambientThemeParticle(pPos, {nr, ng, nb, 140}, 0.0f,
                            35.0f, 1.5f, 1.2f, 0.0f);
                    }
                    break;
                case ThemeID::AncientRuins:
                    // Sandy dust motes drifting slowly
                    m_particles.ambientThemeParticle(pPos, {190, 170, 130, 70}, 250.0f,
                        8.0f, 1.5f, 4.0f, 10.0f);
                    break;
                case ThemeID::CrystalCavern:
                    // Shimmering crystal sparkles
                    m_particles.ambientThemeParticle(pPos, {200, 220, 255, 130}, 90.0f,
                        10.0f, 1.0f, 2.0f, -5.0f);
                    break;
                case ThemeID::BioMechanical:
                    // Green toxic drips falling
                    m_particles.ambientThemeParticle(pPos, {80, 220, 60, 110}, 270.0f,
                        25.0f, 2.0f, 2.5f, 60.0f);
                    break;
                case ThemeID::FrozenWasteland:
                    // White snowflakes drifting down with slight wind
                    m_particles.ambientThemeParticle(pPos, {230, 240, 255, 100}, 250.0f,
                        14.0f, 2.0f, 5.0f, 8.0f);
                    break;
                case ThemeID::VolcanicCore:
                    // Orange embers rising
                    m_particles.ambientThemeParticle(pPos, {255, 130, 30, 120}, 90.0f,
                        20.0f, 2.0f, 3.0f, -40.0f);
                    break;
                case ThemeID::FloatingIslands:
                    // Faint wind wisps (horizontal drift)
                    m_particles.ambientThemeParticle(pPos, {200, 220, 240, 60}, 0.0f,
                        22.0f, 1.5f, 3.5f, -10.0f);
                    break;
                case ThemeID::VoidRealm:
                    // Purple void particles swirling
                    m_particles.ambientThemeParticle(pPos, {160, 60, 220, 110}, 180.0f,
                        16.0f, 2.5f, 3.0f, 0.0f);
                    break;
                case ThemeID::SpaceWestern:
                    // Dusty orange particles drifting with wind
                    m_particles.ambientThemeParticle(pPos, {210, 160, 90, 80}, 350.0f,
                        12.0f, 2.0f, 3.5f, 15.0f);
                    break;
                case ThemeID::Biopunk:
                    // Green bioluminescent spores floating up
                    m_particles.ambientThemeParticle(pPos, {100, 255, 80, 100}, 100.0f,
                        10.0f, 2.0f, 4.5f, -20.0f);
                    break;
            }
        }
    }

    // Ambient rift particles — energy leaking upward from unrepaired rifts
    m_riftAmbientTimer += dt;
    if (m_riftAmbientTimer >= 0.2f && m_level) {
        m_riftAmbientTimer = 0;
        int curDim = m_dimManager.getCurrentDimension();
        auto rifts = m_level->getRiftPositions();
        Vec2 cam = m_camera.getPosition();
        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            if (m_repairedRiftIndices.count(i)) continue;
            if (!m_level->isRiftActiveInDimension(i, curDim)) continue;
            Vec2 center = {rifts[i].x + 16.0f, rifts[i].y + 16.0f};
            // Cull off-screen rifts
            float sx = center.x - cam.x, sy = center.y - cam.y;
            if (sx < -200 || sx > SCREEN_WIDTH + 200 || sy < -200 || sy > SCREEN_HEIGHT + 200) continue;
            int reqDim = m_level->getRiftRequiredDimension(i);
            SDL_Color col = (reqDim == 2) ? SDL_Color{220, 50, 50, 140}   // red for dim-B
                                          : SDL_Color{160, 60, 220, 140}; // purple for dim-A / neutral
            // Enhanced: swirling upward particles + center glow
            m_particles.ambientThemeParticle(center, col, 90.0f, 25.0f, 3.0f, 2.5f, -20.0f, 20.0f);
            m_particles.ambientThemeParticle(center, col, 90.0f, 15.0f, 2.0f, 1.8f, -10.0f, 16.0f);
            // White core sparkle
            m_particles.ambientThemeParticle(center, {255, 255, 255, 80}, 90.0f, 10.0f, 1.5f, 1.0f, -5.0f, 8.0f);
        }
    }

    // Exit beacon — visual cue so the player can find the exit
    if (m_level) {
        Vec2 exitPos = m_level->getExitPoint();
        exitPos.x += 16.0f; // center on tile
        exitPos.y += 16.0f;
        m_exitBeaconTimer -= dt;
        if (m_collapsing) {
            // Active exit: bright green rising particles
            if (m_exitBeaconTimer <= 0) {
                m_exitBeaconTimer = 0.15f;
                m_particles.directionalBurst(exitPos, 2, {80, 255, 80, 200}, 90.0f, 30.0f, 80.0f, 3.0f);
                m_particles.burst(exitPos, 1, {200, 255, 200, 150}, 40.0f, 2.0f);
            }
        } else {
            // Inactive exit: dim gray hint particles
            if (m_exitBeaconTimer <= 0) {
                m_exitBeaconTimer = 0.6f;
                m_particles.ambientThemeParticle(exitPos, {120, 120, 120, 60}, 90.0f, 15.0f, 1.5f, 3.0f, -10.0f);
            }
        }
    }

    // Level complete transition
    if (m_levelComplete) {
        m_levelCompleteTimer += dt;
        float transitionTime = m_playtest ? 0.1f : 2.0f; // Skip transition delay for playtest bots
        if (m_levelCompleteTimer >= transitionTime) {
            roomsCleared++;

            if (m_voidSovereignDefeated && !m_endlessMode) {
                endRun();
                return;
            }

            m_currentDifficulty++;

            // BossRush: skip non-boss floors (advance to next boss/mid-boss)
            if (g_activeChallenge == ChallengeID::BossRush) {
                while (!isBossFloor(m_currentDifficulty) && !isMidBossFloor(m_currentDifficulty)) {
                    m_currentDifficulty++;
                    if (m_currentDifficulty > 200) break; // Safety cap
                }
            }
            m_runSeed += 100;

            // Pick new theme pair when entering a new zone or every 3 floors within a zone
            int prevZone = getZone(m_currentDifficulty - 1);
            int newZone = getZone(m_currentDifficulty);
            if (newZone != prevZone || getFloorInZone(m_currentDifficulty) % 3 == 1) {
                auto themes = WorldTheme::getZonePair(newZone, m_runSeed);
                m_themeA = themes.first;
                m_themeB = themes.second;
                m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
                AudioManager::instance().playThemeAmbient(static_cast<int>(m_themeA.id));
            }

            // Zone-based music (switch track on zone change)
            if (newZone != prevZone) {
                int trackIdx = std::clamp(newZone - 1, 0, 2);
                AudioManager::instance().playMusic(ZONE_TRACKS[trackIdx]);
            }

            // Trigger zone transition banner when entering a new zone
            if (newZone != prevZone) {
                static const char* zoneNameKeys[] = {
                    "zone.1.name", "zone.2.name", "zone.3.name",
                    "zone.4.name", "zone.5.name"
                };
                static const char* zoneTaglineKeys[] = {
                    "zone.1.tagline", "zone.2.tagline", "zone.3.tagline",
                    "zone.4.tagline", "zone.5.tagline"
                };
                int zi = std::clamp(newZone, 0, 4);
                m_zoneTransitionName = LOC(zoneNameKeys[zi]);
                m_zoneTransitionTagline = LOC(zoneTaglineKeys[zi]);
                m_zoneTransitionNumber = zi + 1;
                m_zoneTransitionTimer = 0;
                m_zoneTransitionActive = true;
                AudioManager::instance().play(SFX::RiftRepair);
                // Skip banner for automated bots
                if (m_playtest || m_smokeTest) m_zoneTransitionActive = false;
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
        // ShardStorm mutator: 3x shard drops
        if (g_activeMutators[0] == MutatorID::ShardStorm || g_activeMutators[1] == MutatorID::ShardStorm)
            shardMult *= 3.0f;
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
            emitBuffPickup({100, 180, 255, 220}, LOC("pickup.shield"));
            m_player->pickupShieldPending = false;
        }
        if (m_player->pickupSpeedPending) {
            emitBuffPickup({255, 255, 80, 220}, LOC("pickup.speed"));
            m_player->pickupSpeedPending = false;
        }
        if (m_player->pickupDamagePending) {
            emitBuffPickup({255, 80, 80, 220}, LOC("pickup.damage"));
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

        // Weapon switch visual feedback (Q/E cycling)
        if (m_player->weaponSwitchPending > 0) {
            auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
            bool isMelee = (m_player->weaponSwitchPending == 1);
            WeaponID wid = isMelee ? combat.currentMelee : combat.currentRanged;
            m_weaponSwitchName = WeaponSystem::getWeaponName(wid);
            m_weaponSwitchDisplayTimer = 1.5f;
            m_weaponSwitchIsMelee = isMelee;

            // Particle burst at player position in weapon color
            SDL_Color burstColor = isMelee
                ? SDL_Color{255, 180, 60, 200}   // Orange/white for melee
                : SDL_Color{60, 180, 255, 200};   // Blue/cyan for ranged
            m_particles.burst(pPos, 8, burstColor, 100.0f, 2.5f);

            m_player->weaponSwitchPending = 0;
        }
    }

    updateKillEffects();

    // Consume death ghost effects from combat system
    {
        auto effects = m_combatSystem.consumeDeathEffects();
        for (auto& de : effects) {
            if (m_deathEffectCount < MAX_DEATH_EFFECTS) {
                m_deathEffects[m_deathEffectCount++] = de;
            }
        }
    }

    updateQuestProgress();
    updatePostCombat(dt);
}
