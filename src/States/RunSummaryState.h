#pragma once
#include "GameState.h"

class RunSummaryState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

    int enemiesKilled = 0;
    int riftsRepaired = 0;
    int roomsCleared = 0;
    int shardsEarned = 0;
    bool isNewRecord = false;

private:
    float m_fadeIn = 0;
    float m_time = 0;
    float m_statsTimer = 0; // controls sequential stat reveal
};
