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

    // Combat feel
    float m_hitFreezeTimer = 0;
    float m_spikeDmgCooldown = 0;

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

    // Tutorial hints
    void renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font);
    float m_tutorialTimer = 0;

    // Boss system
    bool m_isBossLevel = false;
    bool m_bossDefeated = false;
    void spawnBoss();
    void renderBossHealthBar(SDL_Renderer* renderer, TTF_Font* font);
    void endRun();
};
