#pragma once
#include "Core/Camera.h"
#include "Systems/ParticleSystem.h"
#include <SDL2/SDL.h>

class DimensionManager {
public:
    DimensionManager();

    void switchDimension();
    void update(float dt);
    void applyVisualEffect(SDL_Renderer* renderer, int screenW, int screenH);

    int getCurrentDimension() const { return m_currentDim; }
    int getOtherDimension() const { return m_currentDim == 1 ? 2 : 1; }
    float getBlendAlpha() const { return m_blendAlpha; }
    bool isSwitching() const { return m_switching; }
    int getSwitchCount() const { return m_switchCount; }
    float getCooldownTimer() const { return m_cooldownTimer; }

    // Colors per dimension
    SDL_Color getDimColorA() const { return m_dimColorA; }
    SDL_Color getDimColorB() const { return m_dimColorB; }
    void setDimColors(SDL_Color a, SDL_Color b) { m_dimColorA = a; m_dimColorB = b; }

    float switchCooldown = 0.5f;
    float switchDuration = 0.3f;
    ParticleSystem* particles = nullptr;
    Vec2 playerPos;

private:
    int m_currentDim = 1;
    bool m_switching = false;
    float m_switchTimer = 0;
    float m_cooldownTimer = 0;
    float m_blendAlpha = 0; // 0 = fully in current, 1 = fully transitioning
    bool m_hasSwitchedMidpoint = false;
    int m_switchCount = 0;

    SDL_Color m_dimColorA{40, 60, 100, 255};  // Cool blue
    SDL_Color m_dimColorB{100, 40, 40, 255};  // Warm red

    // Glitch effect
    float m_glitchIntensity = 0;
};
