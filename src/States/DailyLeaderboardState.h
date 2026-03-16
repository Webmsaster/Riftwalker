#pragma once
#include "GameState.h"
#include "Game/DailyRun.h"
#include <vector>

class DailyLeaderboardState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

    // Set before entering to highlight a newly submitted score
    int  highlightScore = -1;  // -1 = no highlight
    bool isNewBest      = false;

private:
    float m_time        = 0.f;
    float m_fadeIn      = 0.f;
    float m_newBestPulse = 0.f;
    int   m_scrollOffset = 0;

    // Cached data, refreshed in enter()
    std::vector<DailyLeaderboardEntry> m_todayEntries;
    std::vector<DailyRun::DaySummary>  m_daySummaries;
    std::string m_todayDate;
    int m_todaySeed = 0;
};
