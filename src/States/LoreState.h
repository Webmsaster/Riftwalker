#pragma once
#include "States/GameState.h"
#include "Game/LoreSystem.h"
#include <SDL2/SDL_ttf.h>

class LoreState : public GameState {
public:
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;
    void handleEvent(const SDL_Event& event) override;

private:
    TTF_Font* m_fontTitle = nullptr;
    TTF_Font* m_fontBody = nullptr;
    TTF_Font* m_fontSmall = nullptr;
    int m_selected = 0;
    float m_scrollY = 0.0f;
    float m_time = 0.0f;
    LoreSystem* m_lore = nullptr;
};
