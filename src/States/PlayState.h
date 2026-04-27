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
#include "Game/WeaponSystem.h"
#include <memory>
#include <vector>

struct FloatingDamageNumber {
    Vec2 position;
    float value = 0;
    float lifetime = 0;
    float maxLifetime = 1.0f; // non-zero default prevents NaN in alpha math
    bool isPlayerDamage = false;
    bool isCritical = false;
    bool isHeal = false;
    bool isShard = false;
    bool isBuff = false;
    const char* buffText = nullptr; // label for buff pickups
    // Cached textures (created once on first render, reused with alpha mod)
    SDL_Texture* cachedText = nullptr;
    SDL_Texture* cachedShadow = nullptr;
    int texW = 0, texH = 0;
    float baseScale = 1.0f;
};

// Dynamic level events (random mid-level occurrences)
enum class DynamicEventType { DimensionStorm, EliteInvasion, TimeDilation, COUNT };
struct DynamicLevelEvent {
    DynamicEventType type = DynamicEventType::DimensionStorm;
    float duration = 0;
    float timer = 0;
    float effectTimer = 0;
    bool active = false;
    const char* name = "";
    SDL_Color color{255,255,255,255};
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

    // Getters for PauseState stats panel
    float getRunTime() const { return m_runTime; }
    int getBestCombo() const { return m_bestCombo; }
    int getCurrentDifficulty() const { return m_currentDifficulty; }
    int getNGPlusTier() const { return m_ngPlusTier; }
    Player* getPlayer() const { return m_player.get(); }
    CombatSystem& getCombatSystem() { return m_combatSystem; }
    bool isBossLevel() const { return m_isBossLevel; }
    void abandonRun();
    void requestRestart();  // Quick restart from pause menu

private:
    void startNewRun();
    void generateLevel();
    void spawnEnemies();
    void spawnCrates();
    void applyThemeVariant(Entity& e, int dimension);
    void applyNGPlusModifiers(Entity& e);  // Scale enemy stats for active NG+ tier
    void handlePuzzleInput(int action);
    void checkRiftInteraction();
    void checkExitReached();
    void applyUpgrades();
    void applyMutators();
    void renderBackground(SDL_Renderer* renderer);
    void renderBackgroundMidground(SDL_Renderer* renderer, const Vec2& camPos,
                                    Uint32 ticks, const SDL_Color& bgColor);

    // Render helper methods extracted from render()
    void renderCollapseWarning(SDL_Renderer* renderer);
    void renderDimensionBorder(SDL_Renderer* renderer);
    void renderRiftProgress(SDL_Renderer* renderer);
    void renderDeathSequence(SDL_Renderer* renderer);
    void renderAchievementNotification(SDL_Renderer* renderer, TTF_Font* font);
    void renderLoreNotification(SDL_Renderer* renderer, TTF_Font* font);
    void renderLevelCompleteTransition(SDL_Renderer* renderer);

    // Update helper methods (PlayStateUpdate.cpp)
    void updateDimensionSwitch();
    void updateDimensionEffects(float dt);
    void updatePlayerPhysicsEffects(float dt);
    void updateTechnomancerEntities(float dt);
    void updateBossEffects(float dt);
    void updateEnemyHazardDamage(float dt);
    void updatePlayerHazardDamage(float dt);
    void updateStatusEffects(float dt);
    bool updatePlayerDeath();   // returns true if player survived, false if dying
    void updateKillEffects();
    void updatePostCombat(float dt);

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
    SDL_Texture* m_dimATestBackground = nullptr;
    SDL_Texture* m_dimBTestBackground = nullptr;
    SDL_Texture* m_bossTestBackground = nullptr;

    // Run state
    int m_currentDifficulty = 1;
    int m_runSeed = 0;
    bool m_levelComplete = false;
    float m_levelCompleteTimer = 0;

    // NG+ modifiers applied at run start
    int m_ngPlusTier = 0;           // 0=Normal, 1-5=NG+ tier
    float m_ngPlusForceSwitchTimer = 0; // NG+4 forced dim-switch countdown

    // Collapse timer (escape phase)
    bool m_collapsing = false;
    float m_collapseTimer = 0;
    float m_collapseMaxTime = 60.0f;

    // FIX: Exit locked hint when player reaches exit before repairing all rifts
    float m_exitLockedHintTimer = 0;
    float m_riftDimensionHintTimer = 0;
    int m_riftDimensionHintRequiredDim = 0;

    // Weapon switch display (floating name + particle burst)
    float m_weaponSwitchDisplayTimer = 0;
    std::string m_weaponSwitchName;
    bool m_weaponSwitchIsMelee = false; // true=melee (orange), false=ranged (blue)
    void renderWeaponSwitchDisplay(SDL_Renderer* renderer, TTF_Font* font);

    // Ambient effects
    float m_ambientDustTimer = 0;
    float m_riftAmbientTimer = 0;
    float m_exitBeaconTimer = 0;
    float m_wallSlideDustTimer = 0;

    // Combat feel
    float m_hitFreezeTimer = 0;
    float m_killSlowMoTimer = 0;       // Time dilation: counts down from 0.3s
    float m_killSlowMoScale = 1.0f;    // Current time scale (1.0=normal, 0.4=slow)
    float m_spikeDmgCooldown = 0;
    float m_conveyorParticleTimer = 0;
    float m_heartbeatTimer = 0;  // Low HP heartbeat SFX interval

    // Kill streak notification
    float m_killStreakTimer = 0;          // time window for chaining kills (3s)
    int m_killStreakCount = 0;            // current streak count
    float m_killStreakDisplayTimer = 0;   // how long to show the streak text
    std::string m_killStreakText;         // e.g. "TRIPLE KILL!"
    SDL_Color m_killStreakColor{255, 255, 255, 255}; // color per tier
    void renderKillStreak(SDL_Renderer* renderer, TTF_Font* font);
    // Cached kill-streak texture — rebuilt only when text or color changes.
    // Eliminates TTF_Render + CreateTexture on every frame of a 3s display.
    SDL_Texture* m_killStreakCachedTex = nullptr;
    std::string m_killStreakCachedKey;
    int m_killStreakCachedW = 0;
    int m_killStreakCachedH = 0;

    // Cached boss-name texture (rebuilt only when boss type changes).
    // Boss HUD draws every frame during a fight; this saves a
    // TTF_RenderUTF8_Blended + CreateTextureFromSurface per frame.
    SDL_Texture* m_bossNameCachedTex = nullptr;
    int m_bossNameCachedType = -1; // -1 = invalid
    int m_bossNameCachedW = 0;
    int m_bossNameCachedH = 0;

    // Cached rift-counter texture (rebuilt only when rift counts change).
    // Rasterized every frame while rifts remain on a level.
    SDL_Texture* m_riftCounterCachedTex = nullptr;
    int m_riftCounterCachedW = 0;
    int m_riftCounterCachedH = 0;
    int m_riftCounterCachedRepaired = -1;
    int m_riftCounterCachedTotal = -1;
    int m_riftCounterCachedDimA = -1;
    int m_riftCounterCachedDimB = -1;

    // Cached challenge-HUD textures: name + per-mutator labels are
    // static for the run; only rebuild on challenge/mutator change.
    SDL_Texture* m_challengeNameCachedTex = nullptr;
    int m_challengeNameCachedID = -1;
    int m_challengeNameCachedW = 0;
    int m_challengeNameCachedH = 0;
    SDL_Texture* m_mutatorCachedTex[2] = {nullptr, nullptr};
    int m_mutatorCachedID[2] = {-1, -1};
    int m_mutatorCachedW[2] = {0, 0};
    int m_mutatorCachedH[2] = {0, 0};

    // Speedrun timer texture cache: text only changes per integer second.
    // Bucket by (mins*100 + secs) so the texture rebuilds at 1Hz instead of 60Hz.
    SDL_Texture* m_speedrunTimerCachedTex = nullptr;
    int m_speedrunTimerCachedKey = -1;
    int m_speedrunTimerCachedW = 0;
    int m_speedrunTimerCachedH = 0;
    bool m_speedrunTimerCachedRed = false;

    // Endless score text cache: rebuilds only when score changes.
    SDL_Texture* m_endlessScoreCachedTex = nullptr;
    int m_endlessScoreCachedValue = -1;
    int m_endlessScoreCachedW = 0;
    int m_endlessScoreCachedH = 0;

    // Wave/area clear celebration
    float m_waveClearTimer = 0;       // text overlay duration (2s)
    float m_waveClearFlashTimer = 0;  // screen brightness pulse (0.15s)
    bool m_waveClearTriggered = false; // prevent re-trigger until new enemies spawn

    // Run intro overlay (atmospheric story text at run start)
    bool m_runIntroActive = false;
    float m_runIntroTimer = 0;
    static constexpr float kRunIntroDuration = 3.5f;
    void renderRunIntro(SDL_Renderer* renderer, TTF_Font* font);

    // Death sequence: brief dramatic pause before endRun
    bool m_playerDying = false;
    float m_deathSequenceTimer = 0;
    float m_deathSequenceDuration = 1.2f; // seconds of death freeze
    int m_deathCause = 0; // 0=unknown, 1=HP, 2=entropy, 3=collapse, 4=speedrun, 5=victory
    float m_achievementShardMult = 1.0f; // Achievement shard drop bonus
    int m_lastComboMilestone = 0;
    float m_comboMilestoneFlash = 0;

    // Level-up celebration
    float m_levelUpTimer = 0;               // Countdown for "LEVEL UP!" text (1.5s)
    float m_levelUpFlashTimer = 0;          // Golden screen flash (0.3s)
    int m_levelUpDisplayLevel = 0;          // Level reached (for display)
    void updateLevelUp(float dt);
    void renderLevelUp(SDL_Renderer* renderer, TTF_Font* font);
    // Cached level-up textures — rebuilt only when text changes.
    SDL_Texture* m_levelUpLabelCachedTex = nullptr;
    std::string m_levelUpLabelCachedKey;
    int m_levelUpLabelCachedW = 0;
    int m_levelUpLabelCachedH = 0;
    SDL_Texture* m_levelUpNumCachedTex = nullptr;
    std::string m_levelUpNumCachedKey;
    int m_levelUpNumCachedW = 0;
    int m_levelUpNumCachedH = 0;

    // Floating damage numbers
    std::vector<FloatingDamageNumber> m_damageNumbers;
    void updateDamageNumbers(float dt);
    void renderDamageNumbers(SDL_Renderer* renderer, TTF_Font* font);

    // Directional damage indicators (red edge flash showing damage source direction)
    struct DamageIndicator {
        float angle = 0;    // radians, 0=right, PI/2=down, PI=left, -PI/2=up
        float timer = 0;
        float maxTimer = 1.0f; // non-zero default prevents NaN if ever default-constructed
    };
    std::vector<DamageIndicator> m_damageIndicators;
    void updateDamageIndicators(float dt);
    void renderDamageIndicators(SDL_Renderer* renderer);

    // Off-screen enemy proximity arrows at screen edges
    void renderOffscreenEnemyIndicators(SDL_Renderer* renderer);

    // Spawn wave system
    std::vector<std::vector<Level::SpawnPoint>> m_spawnWaves;
    int m_currentWave = 0;
    float m_waveTimer = 0;
    float m_waveDelay = 5.0f;
    bool m_waveActive = false;
    void updateSpawnWaves(float dt);

    // Tutorial hints (context-based, first run only)
    void renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font);
    void renderKeyBox(SDL_Renderer* renderer, TTF_Font* font, const char* key, int x, int y, Uint8 alpha);
    Uint32 m_frameTicks = 0;           // Per-frame SDL_GetTicks() cache (set at start of render)
    bool m_tutorialActive = false;     // true only on first run (totalRuns == 0), disabled for smoke/playtest
    float m_tutorialTimer = 0;
    int m_tutorialHintIndex = 0;      // Current hint to show
    float m_tutorialHintShowTimer = 0; // Time current hint has been visible
    bool m_tutorialHintDone[20] = {};  // Track completed hints (expanded)
    bool m_hasMovedThisRun = false;
    bool m_hasJumpedThisRun = false;
    bool m_hasDashedThisRun = false;
    bool m_hasAttackedThisRun = false;
    bool m_hasRangedThisRun = false;
    bool m_hasUsedAbilityThisRun = false;
    bool m_shownEntropyWarning = false;
    bool m_shownConveyorHint = false;
    bool m_shownDimPlatformHint = false;
    bool m_shownRelicHint = false;
    bool m_shownWallSlideHint = false;
    int m_dashCount = 0; // for dash master achievement

    // Skill achievement tracking (per-run)
    int m_aerialKillsThisRun = 0;
    int m_dashKillsThisRun = 0;
    int m_chargedKillsThisRun = 0;
    bool m_tookDamageThisLevel = false;
    int m_parryCountThisRun = 0;        // Achievement: parry_master (20 parries)
    bool m_tookDamageThisBoss = false;  // Achievement: no_damage_boss
    int m_rangedOnlyFloors = 0;         // Achievement: ranged_only (5 floors ranged-only)
    bool m_usedMeleeThisFloor = false;  // Tracking for ranged_only

    // Achievement tracking: entropy_master (only Entropy Scythe used on floor)
    bool m_usedNonScytheMelee = false;  // Set true if any non-Scythe melee attack fires
    int m_questsCompletedTotal = 0;     // Persistent count for quest_helper achievement

    // Extended run stats (for RunSummaryState)
    int m_dimensionSwitches = 0;
    int m_totalDamageTaken = 0;

    // Teleporter cooldown (reset per run)
    float m_teleportCooldown = 0;

    // Deferred actions (checked at top of update)
    bool m_pendingRestart = false;

    // Shop / Run buffs
    bool m_pendingLevelGen = false;
    void applyRunBuffs();

    // Carry-over state between levels (relics + HP persist across level transitions)
    std::vector<ActiveRelic> m_savedRelics;
    float m_savedHP = 0;
    float m_savedMaxHP = 0;
    float m_savedVoidHungerBonus = 0;
    // Blacksmith upgrades persist within a run but Player is recreated on level transition
    float m_savedSmithMeleeDmgMult = 1.0f;
    float m_savedSmithRangedDmgMult = 1.0f;
    float m_savedSmithAtkSpdMult = 1.0f;

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
    bool m_bossIntroActive = false; // Freeze gameplay during boss title card
    // Boss HP bar damage ghost + shake + phase flash (visual polish)
    float m_bossBarShakeTimer = 0;
    float m_bossBarGhostPct = 0;      // Previous HP% shown as fading white trail
    float m_bossBarGhostTimer = 0;
    int m_bossBarLastPhase = 0;
    float m_bossBarPhaseFlashTimer = 0;
    float m_bossBarLastPct = 1.0f;    // Track HP for damage detection
    void spawnBoss();
    void renderBossHealthBar(SDL_Renderer* renderer, TTF_Font* font);
    void endRun();
    void finalizeRun(bool abandoned);
    void populateRunSummary(int runShards, bool isNewRecord);

    // Relic system
    bool m_showRelicChoice = false;
    std::vector<RelicID> m_relicChoices;
    int m_relicChoiceSelected = 0;
    // Relic pickup celebration (flash + name card)
    float m_relicPickupFlashTimer = 0;
    float m_relicPickupFlashDuration = 1.4f;
    int m_relicPickupID = -1;
    SDL_Color m_relicPickupGlow{255, 255, 255, 255};
    void showRelicChoice();
    void showCursedRelicChoice();  // Cursed-only relic offer (secret rooms)
    void selectRelic(int index);
    void renderRelicChoice(SDL_Renderer* renderer, TTF_Font* font);
    void renderRelicPickupFlash(SDL_Renderer* renderer, TTF_Font* font);
    void renderRelicBar(SDL_Renderer* renderer, TTF_Font* font);

    // Challenge mode
    float m_challengeTimer = 0;       // Speedrun countdown
    bool m_endlessMode = false;       // EndlessRift challenge: infinite floors
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

    // NPC quest system (kill/collect quests)
    NPCQuest m_activeQuest;
    float m_questCompleteTimer = 0;   // Notification popup duration
    int m_questKillSnapshot = 0;      // enemiesKilled at quest start (track delta)
    int m_questRiftSnapshot = 0;      // riftsRepaired at quest start (track delta)
    void updateQuestProgress();
    void completeQuest();
    void renderQuestHUD(SDL_Renderer* renderer, TTF_Font* font);

    // Ascension system
    bool m_ascensionModifiersApplied = false;
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
    void playtestEndRun(bool success);
    void playtestWriteReport();
    // Per-run tracking (reset in playtestStartRun)
    int m_ptLastLoggedLevel = 0;
    int m_ptLastLoggedRifts = 0;
    int m_ptLastCompletedLevel = 0;
    float m_ptStuckTimer = 0;
    Vec2 m_ptLastCheckPos = {0, 0};

    // Playtest auto-screenshots (every 10s + on key events)
    float m_ptScreenshotTimer = 0;
    int m_ptScreenshotCount = 0;
    void playtestScreenshot(const char* event = nullptr);
    float m_ptDimSwitchCD = 0;
    float m_ptDimExploreTimer = 0;
    float m_ptLastEntropyLog = 0;
    int m_ptFallCount = 0;
    // Progress tracking for human-like stuck recovery (no teleport)
    float m_ptNoProgressTimer = 0;
    float m_ptBestDistToTarget = 99999.0f;
    int m_ptNoProgressSkips = 0;
    int m_ptSkipRiftMask = 0;
    int m_ptStuckPhase = 0;            // Escalating recovery: 0=none, 1=dimswitch, 2=reverse, 3=dash-escape, 4=spawn-return
    float m_ptStuckReverseTimer = 0;   // Timer for moving in opposite direction
    float m_ptFallStunTimer = 0;       // Brief stun after fall-respawn
    float m_ptFallInvincTimer = 0;     // Invincibility after fall-respawn
    // Per-floor balance tracking (accumulated across runs)
    struct FloorStats {
        // Survivability
        float damageTaken = 0;
        float dmgFromMelee = 0;    // Enemy melee hits
        float dmgFromRanged = 0;   // Enemy projectiles
        float dmgFromHazard = 0;   // Spikes/fire/laser
        float dmgFromBoss = 0;     // Boss-specific damage
        float dmgFromDoT = 0;      // Burn/poison/etc
        float healingReceived = 0; // From pickups/relics/shrines
        // Combat performance
        int enemiesKilled = 0;
        int meleeKills = 0;
        int rangedKills = 0;
        int abilityKills = 0;
        int comboFinishers = 0;    // 3-hit combos completed
        // Timing
        float timeSpent = 0;
        float timeInCombat = 0;    // Time with enemies nearby
        // State snapshots
        float hpAtStart = 0;
        float hpAtEnd = 0;
        float entropyAtEnd = 0;
        float playerDPS = 0;       // Approx player DPS this floor
        int shardsEarned = 0;
        int upgradesBought = 0;
        bool bossFloor = false;
        int timesPlayed = 0;       // How many runs reached this floor
    };
    static constexpr int kMaxTrackedFloors = 30;
    FloorStats m_ptFloorStats[kMaxTrackedFloors] = {};
    int m_ptFloorDeathCount[kMaxTrackedFloors] = {};  // Global across runs
    int m_ptDeathCause[5] = {};  // 0=enemy melee, 1=enemy ranged, 2=hazard, 3=boss, 4=entropy
    // Per-floor live tracking (reset each floor)
    float m_ptFloorDmgTaken = 0;
    float m_ptFloorDmgMelee = 0;
    float m_ptFloorDmgRanged = 0;
    float m_ptFloorDmgHazard = 0;
    float m_ptFloorDmgBoss = 0;
    float m_ptFloorHealing = 0;
    float m_ptFloorTimeStart = 0;
    float m_ptFloorCombatTime = 0;  // Time with enemies in range
    int m_ptFloorKillsStart = 0;
    int m_ptFloorShardsStart = 0;
    int m_ptFloorMeleeKills = 0;
    int m_ptFloorRangedKills = 0;
    float m_ptAbilityCD = 0;       // Ability usage cooldown
    float m_ptChargeTimer = 0;     // Charged attack hold timer
    bool m_ptCharging = false;     // Currently charging an attack
    // Combo tracking for finisher
    int m_ptLastComboCount = 0;    // Track combo for timed attacks
    float m_ptComboAttackTimer = 0;// Timer to pace combo hits
    // Wave preparation
    float m_ptWavePrepTimer = 0;   // Time spent preparing for wave
    // Run-to-run learning
    bool m_ptDefensiveFloor = false; // Activated on floors where bot died before
    // Shop upgrade tracking
    void playtestBuyUpgrades();    // Buy best upgrades between levels
    // Human input simulation
    float m_ptHumanReactionBase = 0.22f;  // Base reaction time (varies per "session")
    float m_ptHumanHesitation = 0;        // Brief freeze after taking damage
    float m_ptHumanDimDisorient = 0;      // Disorientation after dim-switch
    float m_ptHumanIdleTimer = 0;         // Occasional "thinking" pause
    float m_ptHumanAimJitter = 0;         // Aim offset for current attack
    int m_ptHumanLastMoveDir = 0;         // Smoothing: don't instantly reverse
    float m_ptHumanDirChangeDelay = 0;    // Delay before allowing direction change
    float m_ptHumanFatigue = 0;           // Builds over time, slows reactions
    float m_ptIntentionalDmgTimer = 0;    // Cooldown for intentional damage-taking (human imperfection)
    float m_ptDimMistakeTimer = 0;        // Timer for wrong-dimension exposure (human timing error)

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

    // Unlock notifications (golden popup when class/weapon earned mid-run)
    struct UnlockNotification {
        char text[128] = {};
        float timer = 0;
        float maxTimer = 4.0f;
    };
    static constexpr int MAX_UNLOCK_NOTIFS = 4;
    UnlockNotification m_unlockNotifs[MAX_UNLOCK_NOTIFS];
    int m_unlockNotifHead = 0;
    void pushUnlockNotification(const char* name, bool isWeapon);
    void updateUnlockNotifications(float dt);
    void renderUnlockNotifications(SDL_Renderer* renderer, TTF_Font* font);

    // Per-run unlock tracking (conditions that fire mid-run)
    bool m_unlockedBossWeaponThisRun = false;  // VoidHammer on first boss defeat
    int m_aoeKillCountThisRun = 0;             // for RiftShotgun: multi-kills in one attack frame
    int m_dashKillsThisRunForWeapon = 0;       // for GrapplingHook: dash kills this run
    void checkUnlockConditions();              // called each frame after kill events

    // Kill feed (bottom-right scrolling text)
    struct KillFeedEntry {
        char text[64] = {};
        float timer = 0;
        SDL_Color color{220, 220, 220, 255};
        SDL_Texture* cachedTex = nullptr;
        int texW = 0;
        int texH = 0;
    };
    static constexpr int MAX_KILL_FEED = 5;
    KillFeedEntry m_killFeed[5];
    int m_killFeedHead = 0; // circular buffer index
    void addKillFeedEntry(const KillEvent& ke);
    void updateKillFeed(float dt);
    void renderKillFeed(SDL_Renderer* renderer, TTF_Font* font);

    // Zone transition banner (non-blocking overlay on zone change)
    bool m_zoneTransitionActive = false;
    float m_zoneTransitionTimer = 0;
    std::string m_zoneTransitionName;
    std::string m_zoneTransitionTagline;
    int m_zoneTransitionNumber = 0;
    void renderZoneTransition(SDL_Renderer* renderer, TTF_Font* font);

    // Death ghost effects (enemy shrink+fade after death)
    static constexpr int MAX_DEATH_EFFECTS = 24;
    DeathEffect m_deathEffects[MAX_DEATH_EFFECTS];
    int m_deathEffectCount = 0;
    void updateDeathEffects(float dt);
    void renderDeathEffects(SDL_Renderer* renderer);

    // Foreground fog (atmospheric depth layer)
    struct FogParticle {
        float x = 0, y = 0;
        float vx = 0;
        float width = 0, height = 0;
        float alpha = 0;
        float alphaPhase = 0; // For pulsing
    };
    static constexpr int MAX_FOG_PARTICLES = 8;
    FogParticle m_fogParticles[MAX_FOG_PARTICLES];
    bool m_fogInitialized = false;
    void initFogParticles();
    void updateFogParticles(float dt);
    void renderFogParticles(SDL_Renderer* renderer);

    // Visual polish (Stufe 4)
    TrailSystem m_trails;
    ScreenEffects m_screenEffects;
    MusicSystem m_musicSystem;
    float m_moveTrailTimer = 0.0f;
    float m_runTime = 0.0f; // Total run time
    int m_bestCombo = 0;    // Peak combo count this run
    bool m_isDailyRun = false;

    // Dynamic level events (dimension storm, elite invasion, time dilation)
    DynamicLevelEvent m_dynamicEvent;
    float m_dynamicEventCooldown = 40.0f;
    int m_dynamicEventsTriggered = 0;
    bool m_dynamicEventsEnabled = true;
    float m_dynamicEventAnnouncementTimer = 0;
    void updateDynamicEvent(float dt);
    void triggerDynamicEvent();
    void endDynamicEvent();
    void renderDynamicEventOverlay(SDL_Renderer* renderer, TTF_Font* font);
};
