#pragma once
#include "Window.h"
#include "Timer.h"
#include "InputManager.h"
#include "AudioManager.h"
#include "ResourceManager.h"
#include "States/GameState.h"
#include "Game/UpgradeSystem.h"
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <unordered_map>
#include <stack>

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();
    void quit() { m_running = false; }
    void shutdown();

    // State management
    void changeState(StateID id);
    void pushState(StateID id);
    void popState();

    // Accessors
    const InputManager& getInput() const { return m_input; }
    InputManager& getInputMutable() { return m_input; }
    UpgradeSystem& getUpgradeSystem() { return m_upgrades; }
    TTF_Font* getFont() const { return m_font; }
    int getFPS() const { return m_timer.getFPS(); }
    SDL_Renderer* getRenderer() const { return m_window ? m_window->getSDLRenderer() : nullptr; }
    Window* getWindow() const { return m_window.get(); }

    static constexpr int SCREEN_WIDTH = 1280;
    static constexpr int SCREEN_HEIGHT = 720;

private:
    void handleEvents();
    void update();
    void render();
    void loadSaveData();
    void saveSaveData();

    std::unique_ptr<Window> m_window;
    Timer m_timer;
    InputManager m_input;
    UpgradeSystem m_upgrades;
    TTF_Font* m_font = nullptr;

    // State stack
    std::unordered_map<StateID, std::unique_ptr<GameState>> m_states;
    std::stack<GameState*> m_stateStack;
    GameState* m_currentState = nullptr;

    bool m_running = false;

    // Screen transition
    enum class TransitionState { None, FadeOut, FadeIn };
    TransitionState m_transition = TransitionState::None;
    float m_transitionTimer = 0;
    float m_transitionDuration = 0.25f;
    StateID m_pendingState = StateID::Menu;
    bool m_pendingPush = false;
    Uint8 m_transitionAlpha = 0;
};
