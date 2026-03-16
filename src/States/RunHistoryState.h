#pragma once
#include "GameState.h"

class RunHistoryState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    int m_scrollOffset = 0;
    float m_time = 0;
};
