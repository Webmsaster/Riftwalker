#pragma once
#include "Window.h"
#include "Timer.h"
#include "InputManager.h"
#include "AudioManager.h"
#include "ResourceManager.h"
#include "States/GameState.h"
#include "Game/UpgradeSystem.h"
#include "Game/AchievementSystem.h"
#include "Game/RunBuffSystem.h"
#include "Game/LoreSystem.h"
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <unordered_map>
#include <stack>

// Global audio/visual settings (defined in main.cpp, persisted in riftwalker_settings.cfg)
extern float g_sfxVolume;      // 0.0 - 1.0
extern float g_musicVolume;    // 0.0 - 1.0
extern float g_shakeIntensity; // 0.0 - 2.0 (camera shake multiplier)
extern float g_hudOpacity;     // 0.5 - 1.0

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();
    void quit() { m_running = false; }
    void shutdown();
    void saveSettings(); // persist audio/visual settings to riftwalker_settings.cfg

    // State management
    void changeState(StateID id);
    void pushState(StateID id);
    void popState();

    // Accessors
    const InputManager& getInput() const { return m_input; }
    InputManager& getInputMutable() { return m_input; }
    UpgradeSystem& getUpgradeSystem() { return m_upgrades; }
    AchievementSystem& getAchievements() { return m_achievements; }
    RunBuffSystem& getRunBuffSystem() { return m_runBuffs; }
    LoreSystem* getLoreSystem() { return &m_lore; }
    GameState* getState(StateID id) { auto it = m_states.find(id); return it != m_states.end() ? it->second.get() : nullptr; }
    void setShopDifficulty(int d) { m_shopDifficulty = d; }
    int getShopDifficulty() const { return m_shopDifficulty; }
    TTF_Font* getFont() const { return m_font; }
    int getFPS() const { return m_timer.getFPS(); }
    float getInterpolation() const { return m_timer.getInterpolation(); }
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
    AchievementSystem m_achievements;
    RunBuffSystem m_runBuffs;
    LoreSystem m_lore;
    int m_shopDifficulty = 1;
    TTF_Font* m_font = nullptr;

    // State stack
    std::unordered_map<StateID, std::unique_ptr<GameState>> m_states;
    std::stack<GameState*> m_stateStack;
    GameState* m_currentState = nullptr;

    bool m_running = false;
    bool m_shutdownDone = false;

    // Screen transition
    enum class TransitionState { None, FadeOut, FadeIn };
    TransitionState m_transition = TransitionState::None;
    float m_transitionTimer = 0;
    float m_transitionDuration = 0.3f;  // 0.3s per phase = 0.6s total
    StateID m_pendingState = StateID::Menu;
    bool m_pendingPush = false;
    Uint8 m_transitionAlpha = 0;

    // Transition style per state change
    enum class TransitionStyle { Rift, Death, Subtle };
    TransitionStyle m_transitionStyle = TransitionStyle::Rift;
    TransitionStyle pickTransitionStyle(StateID target) const;

    // Smooth easing (ease-in-out cubic)
    static float easeInOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
    }
};

// Accessibility globals
// 0=Off, 1=Deuteranopia (red-green), 2=Tritanopia (blue-yellow)
inline int g_colorBlindMode = 0;
inline float g_hudScale = 1.0f; // 0.75 - 1.5
inline bool g_crtEffect = false; // CRT scanline post-processing

// Color blind helper: remap colors for visibility
inline SDL_Color applyColorBlind(SDL_Color c) {
    if (g_colorBlindMode == 0) return c;
    if (g_colorBlindMode == 1) { // Deuteranopia: shift red->orange, green->blue
        if (c.r > 150 && c.g < 100) { // Red-ish → bright orange
            c.g = static_cast<Uint8>(std::min(255, c.r / 2 + 80));
            c.b = static_cast<Uint8>(std::min(255, 40));
        } else if (c.g > 150 && c.r < 100) { // Green-ish → cyan/blue
            c.b = c.g;
            c.g = static_cast<Uint8>(c.g / 2);
        }
    } else if (g_colorBlindMode == 2) { // Tritanopia: shift blue->red, yellow→pink
        if (c.b > 150 && c.r < 100) { // Blue-ish → magenta
            c.r = c.b;
            c.b = static_cast<Uint8>(c.b / 2);
        } else if (c.r > 150 && c.g > 150 && c.b < 80) { // Yellow-ish → pink
            c.g = static_cast<Uint8>(c.g / 3);
        }
    }
    return c;
}
