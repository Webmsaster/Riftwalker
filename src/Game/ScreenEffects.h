#pragma once
#include <SDL2/SDL.h>
#include <string>

class ScreenEffects {
public:
    void update(float dt);
    void render(SDL_Renderer* renderer, int screenW, int screenH);

    // Triggers
    void triggerKillFlash();
    void triggerBossIntro(const char* bossName);
    void triggerDimensionRipple();

    // Continuous states
    void setHP(float hpPercent) { m_hpPercent = hpPercent; }
    void setEntropy(float entropy) { m_entropy = entropy; }
    void setVoidStorm(bool active) { m_voidStormActive = active; }

private:
    float m_time = 0.0f;

    // Vignette (always on)
    float m_hpPercent = 1.0f;

    // Kill flash
    float m_killFlashTimer = 0.0f;

    // Boss intro
    float m_bossIntroTimer = 0.0f;
    std::string m_bossName;

    // Dimension ripple
    float m_rippleTimer = 0.0f;

    // Entropy glitch
    float m_entropy = 0.0f;
    float m_glitchTimer = 0.0f;

    // Void Storm
    bool m_voidStormActive = false;
};
