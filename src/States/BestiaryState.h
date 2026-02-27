#pragma once
#include "GameState.h"
#include <SDL2/SDL_ttf.h>

class BestiaryState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderEnemyPreview(SDL_Renderer* renderer, int cx, int cy, int type, bool isBoss);

    int m_selected = 0;
    int m_totalEntries = 0;
    float m_time = 0;
    int m_scrollOffset = 0;
};
