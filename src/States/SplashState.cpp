#include "SplashState.h"
#include "Core/Game.h"
#include <cmath>
#include <algorithm>

// Auto-play / smoke-test globals (defined in main.cpp)
extern bool g_autoSmokeTest;
extern bool g_autoPlaytest;

void SplashState::enter() {
    m_time = 0.0f;
    m_skipped = false;
    m_glowPhase = 0.0f;

    // Skip splash entirely in automated modes
    if (g_autoSmokeTest || g_autoPlaytest) {
        m_skipped = true;
    }
}

void SplashState::exit() {
    // Nothing to clean up
}

void SplashState::handleEvent(const SDL_Event& event) {
    // Any key or mouse press skips to menu (after 1s to prevent accidental skip)
    if (m_time < 1.0f) return;
    if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_CONTROLLERBUTTONDOWN) {
        transitionToMenu();
    }
}

void SplashState::update(float dt) {
    if (m_skipped) {
        transitionToMenu();
        return;
    }

    m_time += dt;
    m_glowPhase += dt * 2.5f; // Shimmer speed

    if (m_time >= TOTAL_DURATION) {
        transitionToMenu();
    }
}

void SplashState::transitionToMenu() {
    if (m_skipped) {
        // For auto-skip, go directly without fade transition
        // We still use changeState which does a fade, but that's fine
    }
    m_skipped = true; // Prevent double-transition
    game->changeState(StateID::Menu);
}

void SplashState::render(SDL_Renderer* renderer) {
    // Black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (m_skipped || !game->getFont()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // --- Dimension glow shimmer behind title ---
    float glowAlpha = std::min(m_time / TITLE_FADE_DURATION, 1.0f);
    if (glowAlpha > 0.0f) {
        // Purple/blue shimmer - oscillates between colors
        float shimmer = (std::sin(m_glowPhase) + 1.0f) * 0.5f; // 0..1
        Uint8 gr = static_cast<Uint8>(80 + shimmer * 40);       // 80-120 (purple)
        Uint8 gg = static_cast<Uint8>(30 + (1.0f - shimmer) * 30); // 30-60
        Uint8 gb = static_cast<Uint8>(140 + shimmer * 60);      // 140-200 (blue)
        Uint8 ga = static_cast<Uint8>(glowAlpha * 50);

        // Soft glow rectangle behind title area
        int glowW = 1200;
        int glowH = 180;
        int cx = SCREEN_WIDTH / 2;
        int cy = SCREEN_HEIGHT / 2 - 30;
        SDL_Rect glowRect = { cx - glowW / 2, cy - glowH / 2, glowW, glowH };
        SDL_SetRenderDrawColor(renderer, gr, gg, gb, ga);
        SDL_RenderFillRect(renderer, &glowRect);

        // Slightly smaller, brighter inner glow
        int innerW = 900;
        int innerH = 120;
        SDL_Rect innerRect = { cx - innerW / 2, cy - innerH / 2, innerW, innerH };
        Uint8 ia = static_cast<Uint8>(glowAlpha * 30);
        SDL_SetRenderDrawColor(renderer, gr, gg, gb, ia);
        SDL_RenderFillRect(renderer, &innerRect);
    }

    // --- Title text: "RIFTWALKER" ---
    float titleAlpha = std::min(m_time / TITLE_FADE_DURATION, 1.0f);
    if (titleAlpha > 0.0f) {
        Uint8 ta = static_cast<Uint8>(titleAlpha * 255);
        SDL_Color titleColor = { 220, 200, 255, ta }; // Light purple-white

        TTF_Font* font = game->getFont();

        // Render title at larger size if possible - use the existing font
        // We'll render it centered
        SDL_Surface* surface = TTF_RenderText_Blended(font, "R I F T W A L K E R", titleColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, ta);
                // Scale up the title text 3x for a bigger look
                int scaledW = surface->w * 3;
                int scaledH = surface->h * 3;
                SDL_Rect dst = {
                    SCREEN_WIDTH / 2 - scaledW / 2,
                    SCREEN_HEIGHT / 2 - scaledH / 2 - 30,
                    scaledW,
                    scaledH
                };
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    // --- Subtitle: "A Dimension-Shifting Roguelike" ---
    if (m_time > SUBTITLE_START) {
        float subProgress = (m_time - SUBTITLE_START) / SUBTITLE_FADE_DURATION;
        float subAlpha = std::min(subProgress, 1.0f);
        Uint8 sa = static_cast<Uint8>(subAlpha * 200); // Slightly dimmer than title
        SDL_Color subColor = { 160, 140, 200, sa }; // Muted purple

        TTF_Font* font = game->getFont();
        SDL_Surface* surface = TTF_RenderText_Blended(font, "A Dimension-Shifting Roguelike", subColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, sa);
                // Scale subtitle 1.5x
                int scaledW = static_cast<int>(surface->w * 1.5f);
                int scaledH = static_cast<int>(surface->h * 1.5f);
                SDL_Rect dst = {
                    SCREEN_WIDTH / 2 - scaledW / 2,
                    SCREEN_HEIGHT / 2 + 30,
                    scaledW,
                    scaledH
                };
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    // --- "Press any key" hint after title is fully visible ---
    if (m_time > TITLE_FADE_DURATION + 0.3f) {
        float hintPulse = (std::sin(m_time * 3.0f) + 1.0f) * 0.5f;
        Uint8 ha = static_cast<Uint8>(80 + hintPulse * 120); // More visible pulse
        SDL_Color hintColor = { 160, 140, 200, ha };

        TTF_Font* font = game->getFont();
        SDL_Surface* surface = TTF_RenderText_Blended(font, "Press any key", hintColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, ha);
                int pw = static_cast<int>(surface->w * 1.5f);
                int ph = static_cast<int>(surface->h * 1.5f);
                SDL_Rect dst = {
                    SCREEN_WIDTH / 2 - pw / 2,
                    SCREEN_HEIGHT - 120,
                    pw,
                    ph
                };
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}
