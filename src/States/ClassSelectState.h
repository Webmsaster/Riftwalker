#pragma once
#include "GameState.h"
#include "Game/ClassSystem.h"
#include <SDL2/SDL_ttf.h>

class ClassSelectState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderClassCard(SDL_Renderer* renderer, TTF_Font* font,
                         const ClassData& data, int x, int y, int w, int h, bool selected);
    void renderClassPreview(SDL_Renderer* renderer, const ClassData& data, int cx, int cy);

    int m_selected = 0;
    float m_time = 0;
    float m_previewBob = 0;
};
