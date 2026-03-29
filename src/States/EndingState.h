#pragma once
#include "States/GameState.h"
#include <SDL2/SDL_ttf.h>

class EndingState : public GameState {
public:
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;
    void handleEvent(const SDL_Event& event) override;

    // Set run stats before entering
    int finalScore = 0;
    int totalKills = 0;
    int maxDifficulty = 0;
    int ngPlusTier = 0;    // Ascension tier of this winning run
    int relicsCollected = 0;
    float totalTime = 0.0f;

private:
    TTF_Font* m_fontTitle = nullptr;
    TTF_Font* m_fontBody = nullptr;
    TTF_Font* m_fontSmall = nullptr;
    int m_phase = 0;        // 0=flash, 1=story, 2=stats, 3=thankyou
    int m_endingType = 0;   // 0=healer, 1=destroyer, 2=speedrunner
    float m_phaseTimer = 0.0f;
    float m_time = 0.0f;
    float m_storyScroll = 0.0f;
};
