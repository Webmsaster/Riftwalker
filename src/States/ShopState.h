#pragma once
#include "GameState.h"
#include "Game/RunBuffSystem.h"
#include <vector>

class ShopState : public GameState {
public:
    void enter() override;
    void exit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderCard(SDL_Renderer* renderer, const RunBuff& buff, int x, int y,
                    int w, int h, bool selected, bool affordable);

    std::vector<RunBuff> m_offerings;
    int m_selectedIndex = 0;
    float m_animTimer = 0;
    bool m_purchasedThisVisit = false;
};
