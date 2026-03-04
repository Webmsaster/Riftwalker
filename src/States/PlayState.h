#pragma once
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
    void handlePuzzleInput(int action);
    void checkRiftInteraction();
    void checkExitReached();
    void applyUpgrades();
    void renderBackground(SDL_Renderer* renderer);

    EntityManager m_entities;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<Level> m_level;
    Camera m_camera{1280, 720};
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

    // Ambient effects
    float m_ambientDustTimer = 0;

    // Combat feel
    float m_hitFreezeTimer = 0;
    float m_spikeDmgCooldown = 0;
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

    // Shop / Run buffs
    bool m_pendingLevelGen = false;
    void applyRunBuffs();

    // Secret rooms & events
    void checkBreakableWalls();
    void checkSecretRoomDiscovery();
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

    // NPC system
    int m_nearNPCIndex = -1;
    bool m_showNPCDialog = false;
    int m_npcDialogChoice = 0;
    void checkNPCInteraction();
    void renderNPCs(SDL_Renderer* renderer, TTF_Font* font);
    void renderNPCDialog(SDL_Renderer* renderer, TTF_Font* font);
    void handleNPCDialogChoice(int npcIndex, int choice);

    // Ascension system
    void applyAscensionModifiers();

    // Visual polish (Stufe 4)
    TrailSystem m_trails;
    ScreenEffects m_screenEffects;
    MusicSystem m_musicSystem;
    float m_moveTrailTimer = 0.0f;
    float m_runTime = 0.0f; // Total run time
    bool m_isDailyRun = false;
};
