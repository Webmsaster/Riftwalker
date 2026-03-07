#pragma once
#include <set>
#include "GameState.h"
#include "ECS/EntityManager.h"
#include "Core/Camera.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/CollisionSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/ParticleSystem.h"
#include "Systems/AISystem.h"
#include "Systems/CombatSystem.h"
#include "Game/Player.h"
#include "Game/Level.h"
#include "Game/LevelGenerator.h"
#include "Game/DimensionManager.h"
#include "Game/SuitEntropy.h"
#include "Game/RiftPuzzle.h"
#include "Game/WorldTheme.h"
#include "Game/SecretRoom.h"
#include "Game/RandomEvent.h"
#include "Game/RelicSystem.h"
#include "Game/ChallengeMode.h"
#include "Game/Bestiary.h"
#include "Game/NPCSystem.h"
#include "Game/AscensionSystem.h"
#include "Game/TrailEmitter.h"
#include "Game/ScreenEffects.h"
#include "Core/MusicSystem.h"
#include "UI/HUD.h"
#include <memory>
#include <vector>

struct FloatingDamageNumber {
    Vec2 position;
    float value;
    float lifetime;
    float maxLifetime;
    bool isPlayerDamage;
    bool isCritical = false;
};

class PlayState : public GameState {
public:
    void enter() override;
    void exit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

    // Run stats
    int enemiesKilled = 0;
    int riftsRepaired = 0;
    int roomsCleared = 0;
    int shardsCollected = 0;

private:
    void startNewRun();
    void generateLevel();
    void spawnEnemies();
    void applyThemeVariant(Entity& e, int dimension);
    void handlePuzzleInput(int action);
    void checkRiftInteraction();
    void checkExitReached();
    void applyUpgrades();
    void renderBackground(SDL_Renderer* renderer);

    EntityManager m_entities;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<Level> m_level;
    Camera m_camera{SCREEN_WIDTH, SCREEN_HEIGHT};
    LevelGenerator m_levelGen;

    // Systems
    PhysicsSystem m_physics;
    CollisionSystem m_collision;
    RenderSystem m_renderSys;
    ParticleSystem m_particles;
    AISystem m_aiSystem;
    CombatSystem m_combatSystem;

    // Game mechanics
    DimensionManager m_dimManager;
    SuitEntropy m_entropy;
    HUD m_hud;

    // Puzzles
    std::unique_ptr<RiftPuzzle> m_activePuzzle;
    int m_nearRiftIndex = -1;
    // FIX: Track repaired rifts per level to prevent re-repair and fix collapse timing
    std::set<int> m_repairedRiftIndices;
    int m_levelRiftsRepaired = 0;

    // World themes
    WorldTheme m_themeA;
    WorldTheme m_themeB;

    // Run state
    int m_currentDifficulty = 1;
    int m_runSeed = 0;
    bool m_levelComplete = false;
    float m_levelCompleteTimer = 0;

    // Collapse timer (escape phase)
    bool m_collapsing = false;
    float m_collapseTimer = 0;
    float m_collapseMaxTime = 60.0f;

    // FIX: Exit locked hint when player reaches exit before repairing all rifts
    float m_exitLockedHintTimer = 0;

    // Ambient effects
    float m_ambientDustTimer = 0;

    // Combat feel
    float m_hitFreezeTimer = 0;
    float m_spikeDmgCooldown = 0;

    // Death sequence: brief dramatic pause before endRun
    bool m_playerDying = false;
    float m_deathSequenceTimer = 0;
    float m_deathSequenceDuration = 1.2f; // seconds of death freeze
    float m_achievementShardMult = 1.0f; // Achievement shard drop bonus
    int m_lastComboMilestone = 0;
    float m_comboMilestoneFlash = 0;

    // Floating damage numbers
    std::vector<FloatingDamageNumber> m_damageNumbers;
    void updateDamageNumbers(float dt);
    void renderDamageNumbers(SDL_Renderer* renderer, TTF_Font* font);

    // Spawn wave system
    std::vector<std::vector<Level::SpawnPoint>> m_spawnWaves;
    int m_currentWave = 0;
    float m_waveTimer = 0;
    float m_waveDelay = 5.0f;
    bool m_waveActive = false;
    void updateSpawnWaves(float dt);

    // Tutorial hints (context-based)
    void renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font);
    void renderKeyBox(SDL_Renderer* renderer, TTF_Font* font, const char* key, int x, int y, Uint8 alpha);
    float m_tutorialTimer = 0;
    int m_tutorialHintIndex = 0;      // Current hint to show
    float m_tutorialHintShowTimer = 0; // Time current hint has been visible
    bool m_tutorialHintDone[8] = {};   // Track completed hints
    bool m_hasMovedThisRun = false;
    bool m_hasJumpedThisRun = false;
    bool m_hasDashedThisRun = false;
    bool m_hasAttackedThisRun = false;
    int m_dashCount = 0; // for dash master achievement

    // Skill achievement tracking (per-run)
    int m_aerialKillsThisRun = 0;
    int m_dashKillsThisRun = 0;
    int m_chargedKillsThisRun = 0;
    bool m_tookDamageThisLevel = false;

    // Teleporter cooldown (reset per run)
    float m_teleportCooldown = 0;

    // Shop / Run buffs
    bool m_pendingLevelGen = false;
    void applyRunBuffs();

    // Secret rooms & events
    void checkBreakableWalls();
    void checkSecretRoomDiscovery();
    void updateSecretRoomHints(float dt);
    float m_secretHintTimer = 0;
    void checkEventInteraction();
    void renderRandomEvents(SDL_Renderer* renderer, TTF_Font* font);
    int m_nearEventIndex = -1;

    // Boss system
    bool m_isBossLevel = false;
    bool m_bossDefeated = false;
    bool m_voidSovereignDefeated = false;
    void spawnBoss();
    void renderBossHealthBar(SDL_Renderer* renderer, TTF_Font* font);
    void endRun();

    // Relic system
    bool m_showRelicChoice = false;
    std::vector<RelicID> m_relicChoices;
    int m_relicChoiceSelected = 0;
    void showRelicChoice();
    void selectRelic(int index);
    void renderRelicChoice(SDL_Renderer* renderer, TTF_Font* font);
    void renderRelicBar(SDL_Renderer* renderer, TTF_Font* font);

    // Challenge mode
    float m_challengeTimer = 0;       // Speedrun countdown
    int m_endlessScore = 0;           // Endless mode score
    float m_mutatorFluxTimer = 0;     // DimensionFlux auto-switch timer
    void applyChallengeModifiers();   // Apply at run start
    void renderChallengeHUD(SDL_Renderer* renderer, TTF_Font* font);

    // Combat Challenges (per-room micro-goals)
    enum class CombatChallengeType {
        AerialKill,       // Kill enemy while airborne
        MultiKill,        // Kill 3+ enemies within 4 seconds
        DashKill,         // Kill with dash attack
        ComboFinisher,    // Kill with 3rd melee combo hit
        ChargedKill,      // Kill with charged attack
        NoDamageWave,     // Survive current wave without taking damage
        COUNT
    };
    struct CombatChallenge {
        CombatChallengeType type = CombatChallengeType::AerialKill;
        const char* name = "";
        const char* desc = "";
        int targetCount = 1;
        int currentCount = 0;
        float timer = -1;       // countdown for timed challenges
        float maxTimer = -1;
        int shardReward = 25;
        bool active = false;
        bool completed = false;
    };
    CombatChallenge m_combatChallenge;
    float m_challengeCompleteTimer = 0;  // show completion popup
    int m_challengesCompleted = 0;
    bool m_tookDamageThisWave = false;
    void startCombatChallenge();
    void updateCombatChallenge(float dt);
    void renderCombatChallenge(SDL_Renderer* renderer, TTF_Font* font);

    // NPC system
    int m_nearNPCIndex = -1;
    bool m_showNPCDialog = false;
    int m_npcDialogChoice = 0;
    bool m_echoSpawned = false;     // Echo of Self mirror enemy spawned
    bool m_echoRewarded = false;    // Reward already given
    int m_npcStoryProgress[static_cast<int>(NPCType::COUNT)] = {}; // per-type encounter count this run
    int getNPCStoryStage(NPCType type) const {
        int idx = static_cast<int>(type);
        return (idx >= 0 && idx < static_cast<int>(NPCType::COUNT)) ?
            std::min(m_npcStoryProgress[idx], 2) : 0;
    }
    void checkNPCInteraction();
    void renderNPCs(SDL_Renderer* renderer, TTF_Font* font);
    void renderNPCDialog(SDL_Renderer* renderer, TTF_Font* font);
    void handleNPCDialogChoice(int npcIndex, int choice);

    // Ascension system
    void applyAscensionModifiers();

    // Debug overlay (F3 toggle, F4 snapshot)
    bool m_showDebugOverlay = false;
    bool m_pendingSnapshot = false;
    void renderDebugOverlay(SDL_Renderer* renderer, TTF_Font* font);
    void writeBalanceSnapshot();

    // Balance tracking (for run summary)
    struct BalanceStats {
        float peakDmgRaw = 0;
        float peakDmgClamped = 0;
        float peakSpdRaw = 0;
        float peakSpdClamped = 0;
        float cdFloorTime = 0;
        float activePlayTime = 0;
        int peakResidueZones = 0;
        float peakVoidHunger = 0;
        float finalVoidHunger = 0;
    } m_balanceStats;
    void updateBalanceTracking(float dt);

    // Smoke test (F6) - auto-play for integration testing
    bool m_smokeTest = false;
    float m_smokeActionTimer = 0;
    float m_smokeDimTimer = 0;
    float m_smokeRunTime = 0;
    void updateSmokeTest(float dt);

    // Human-like playtest mode (--playtest)
    bool m_playtest = false;
    int m_playtestRun = 0;           // Current run number
    int m_playtestMaxRuns = 10;      // Max attempts before giving up
    float m_playtestReactionTimer = 0; // Human reaction delay
    float m_playtestThinkTimer = 0;    // Pause between decisions
    float m_playtestLastHP = 0;        // Track HP changes for logging
    int m_playtestDeaths = 0;          // Total deaths across runs
    int m_playtestBestLevel = 0;       // Highest level reached
    float m_playtestRunTimer = 0;      // Time in current run
    bool m_playtestRunActive = false;  // Is a run currently in progress
    void updatePlaytest(float dt);
    void playtestLog(const char* fmt, ...);
    void playtestStartRun();
    void playtestOnDeath();
    void playtestWriteReport();
    // Per-run tracking (reset in playtestStartRun)
    int m_ptLastLoggedLevel = 0;
    int m_ptLastLoggedRifts = 0;
    int m_ptLastCompletedLevel = 0;
    float m_ptStuckTimer = 0;
    Vec2 m_ptLastCheckPos = {0, 0};
    float m_ptDimSwitchCD = 0;
    float m_ptDimExploreTimer = 0;
    float m_ptLastEntropyLog = 0;
    int m_ptFallCount = 0;
    // Progress tracking for smart teleport
    float m_ptNoProgressTimer = 0;
    float m_ptBestDistToTarget = 99999.0f;
    int m_ptNoProgressSkips = 0;
    int m_ptSkipRiftMask = 0;

    // Event chains (multi-level quest lines)
    EventChain m_eventChain;
    bool m_chainEventSpawned = false;   // Chain event placed this level
    int m_chainEventIndex = -1;         // Index in level's random events
    float m_chainNotifyTimer = 0;       // Stage advance notification
    float m_chainRewardTimer = 0;       // Completion reward popup
    int m_chainRewardShards = 0;
    void updateEventChain(float dt);
    void spawnChainEvent();
    void advanceChain();
    void completeChain();
    void renderEventChain(SDL_Renderer* renderer, TTF_Font* font);

    // Visual polish (Stufe 4)
    TrailSystem m_trails;
    ScreenEffects m_screenEffects;
    MusicSystem m_musicSystem;
    float m_moveTrailTimer = 0.0f;
    float m_runTime = 0.0f; // Total run time
    bool m_isDailyRun = false;
};
