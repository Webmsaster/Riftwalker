#pragma once
#include "GameState.h"
#include <SDL2/SDL_ttf.h>

class SplashState : public GameState {
public:
    void enter() override;
    void exit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void transitionToMenu();

    float m_time = 0.0f;
    bool m_skipped = false;

    // Fade timing
    static constexpr float TITLE_FADE_DURATION = 1.5f;   // Title fades in over 1.5s
    static constexpr float SUBTITLE_START = 1.0f;         // Subtitle begins at 1.0s
    static constexpr float SUBTITLE_FADE_DURATION = 1.0f; // Subtitle fades in over 1.0s
    static constexpr float TOTAL_DURATION = 2.5f;         // Auto-transition at 2.5s

    // Glow shimmer
    float m_glowPhase = 0.0f;
};
