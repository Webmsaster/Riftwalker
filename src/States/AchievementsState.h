#pragma once
#include "GameState.h"

class AchievementsState : public GameState {
public:
    void enter() override;
    void exit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    int m_scrollOffset = 0;
    int m_maxScroll = 0;
    float m_animTimer = 0;
};
