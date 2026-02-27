#pragma once
#include "GameState.h"
#include "Game/AscensionSystem.h"
#include "UI/Button.h"
#include <vector>

class MenuState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderPortal(SDL_Renderer* renderer);
    void renderTitle(SDL_Renderer* renderer, TTF_Font* font);

    std::vector<Button> m_buttons;
    int m_selectedButton = 0;
    float m_time = 0;
    float m_fadeIn = 0;

    struct Particle {
        float x, y, vx, vy;
        float life, maxLife;
        Uint8 r, g, b;
        float size;
    };
    std::vector<Particle> m_bgParticles;
};
