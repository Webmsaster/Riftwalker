#pragma once
#include "GameState.h"
#include "UI/Button.h"
#include <vector>

class PauseState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderRunStats(SDL_Renderer* renderer, TTF_Font* font);

    std::vector<Button> m_buttons;
    int m_selectedButton = 0;
    float m_time = 0;
    bool m_confirmAbandon = false;
};
