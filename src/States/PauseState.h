#pragma once
#include "GameState.h"
#include "UI/Button.h"
#include <vector>

class PauseState : public GameState {
public:
    void enter() override;
    void exit() override;
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

    // Cached title texture (static "pause.title" string) — built once per
    // pause session, reused for glow + main layers each frame. Freed on exit()
    // so a language change between sessions rebuilds it.
    SDL_Texture* m_titleTex = nullptr;
    int m_titleW = 0;
    int m_titleH = 0;
};
