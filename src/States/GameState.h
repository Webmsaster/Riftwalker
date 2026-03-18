#pragma once
#include <SDL2/SDL.h>

class Game;

class GameState {
public:
    static constexpr int SCREEN_WIDTH = 1280;
    static constexpr int SCREEN_HEIGHT = 720;

    virtual ~GameState() = default;
    virtual void enter() {}
    virtual void exit() {}
    virtual void handleEvent([[maybe_unused]] const SDL_Event& event) {}
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
    ClassSelect,
    DifficultySelect,
    NGPlusSelect,
    Keybindings,
    Achievements,
    Shop,
    ChallengeSelect,
    Bestiary,
    Lore,
    Ending,
    RunHistory,
    DailyLeaderboard
};

// Global difficulty setting (shared between states)
enum class GameDifficulty { Easy = 0, Normal = 1, Hard = 2 };
inline GameDifficulty g_selectedDifficulty = GameDifficulty::Normal;

// New Game+ level (0 = first playthrough, 1+ = NG+ tiers)
inline int g_newGamePlusLevel = 0;

// Global NG+ tier (0 = Normal, 1-5 = NG+ tiers)
inline int g_selectedNGPlus = 0;
