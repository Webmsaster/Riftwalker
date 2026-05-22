#pragma once
#include "GameState.h"

class GameOverState : public GameState {
public:
    void enter() override;
    void exit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

    // Set before changeState(StateID::GameOver) to display correct death cause
    // 0=entropy, 1=HP depleted, 2=entropy(legacy), 3=collapse, 4=time expired
    static int s_deathCause;
    static int s_floorsCleared;
    static int s_killCount;
    static float s_runTime;

private:
    float m_timer = 0;
    float m_glitchIntensity = 1.0f;
    const char* m_subtitleText = nullptr;

    // Cached title texture (static string) — built once, reused for the 3
    // glitch channels each frame instead of rasterizing 3x/frame at 60fps.
    SDL_Texture* m_titleTex = nullptr;
    int m_titleW = 0;
    int m_titleH = 0;
};
