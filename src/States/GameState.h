#pragma once
#include <SDL2/SDL.h>

class Game;

class GameState {
public:
    virtual ~GameState() = default;
    virtual void enter() {}
    virtual void exit() {}
    virtual void handleEvent(const SDL_Event& event) {}
    virtual void update(float dt) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;

    Game* game = nullptr;
};

enum class StateID {
    Menu,
    Play,
    Pause,
    Upgrade,
    RunSummary,
    GameOver,
    Options,
    DifficultySelect,
    Keybindings,
    Achievements,
    Shop,
    ChallengeSelect,
    Bestiary
};

// Global difficulty setting (shared between states)
enum class GameDifficulty { Easy = 0, Normal = 1, Hard = 2 };
inline GameDifficulty g_selectedDifficulty = GameDifficulty::Normal;
