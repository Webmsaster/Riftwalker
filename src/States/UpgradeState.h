#pragma once
#include "GameState.h"
#include "UI/Button.h"
#include <vector>

class UpgradeState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderText(SDL_Renderer* renderer, TTF_Font* font,
                    const char* text, int x, int y, SDL_Color color);

    int m_selectedUpgrade = 0;
    int m_scrollOffset = 0;
    float m_time = 0;
    float m_flashTimer = 0;
    static constexpr int VISIBLE_ITEMS = 8;
};
