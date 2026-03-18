#pragma once
#include "GameState.h"

class DifficultySelectState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void handleConfirm();
    int m_selected = 1; // Default: Normal
    float m_time = 0;
};
