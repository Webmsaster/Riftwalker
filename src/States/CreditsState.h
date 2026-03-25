#pragma once
#include "States/GameState.h"
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

class CreditsState : public GameState {
public:
    void enter() override;
    void exit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    struct Particle {
        float x, y, vy;
        float life, maxLife;
        Uint8 r, g, b;
        float size;
    };

    TTF_Font* m_fontTitle = nullptr;
    TTF_Font* m_fontBody = nullptr;
    std::vector<std::string> m_lines;
    float m_scrollY = 0.0f;
    float m_time = 0.0f;
    std::vector<Particle> m_particles;
};
