#pragma once
#include "GameState.h"

class GameOverState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    float m_timer = 0;
    float m_glitchIntensity = 1.0f;
};
