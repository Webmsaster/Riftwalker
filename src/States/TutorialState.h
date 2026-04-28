#pragma once
#include "GameState.h"
#include "UI/Button.h"
#include <vector>
#include <string>

class TutorialState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

    // Set to true before opening Tutorial when user clicks "New Run" as a
    // first-time player — Tutorial will then continue to ClassSelect instead
    // of returning to Menu. Default false (opened via Tutorial button from Menu).
    static bool s_openedFromNewRun;

private:
    void renderPage(SDL_Renderer* renderer, TTF_Font* font);
    void renderPageIndicator(SDL_Renderer* renderer);
    void renderKeyIcon(SDL_Renderer* renderer, int x, int y, int w, int h,
                       const char* label, TTF_Font* font);
    void nextPage();
    void prevPage();
    void finish();

    int m_page = 0;
    static constexpr int PAGE_COUNT = 9;
    float m_fadeIn = 0;
    float m_time = 0;
    float m_pageTransition = 0; // 0=stable, >0=transitioning
    int m_transitionDir = 0;    // +1=forward, -1=backward

    // Particle background
    struct Particle {
        float x, y, vx, vy, life, maxLife, size;
        Uint8 r, g, b;
    };
    std::vector<Particle> m_particles;
};
