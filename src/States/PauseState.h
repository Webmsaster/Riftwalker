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
    // Reset all confirm states except the one matching `keepIdx` (-1 = clear all).
    // Used so clicking a different action auto-cancels any pending confirm.
    void clearConfirmsExcept(int keepIdx);

    std::vector<Button> m_buttons;
    int m_selectedButton = 0;
    float m_time = 0;
    float m_slideInTimer = 0;   // 0->1 over SLIDE_IN_DURATION for entry animation
    bool m_confirmAbandon = false;
    bool m_confirmRestart = false;
};
