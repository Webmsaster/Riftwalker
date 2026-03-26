#include "GameOverState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>

extern bool g_autoSmokeTest;
extern bool g_autoPlaytest;

int GameOverState::s_deathCause = 0;
int GameOverState::s_floorsCleared = 0;
int GameOverState::s_killCount = 0;
float GameOverState::s_runTime = 0.0f;

void GameOverState::enter() {
    if (g_autoSmokeTest || g_autoPlaytest) {
        game->quit();
        return;
    }
    m_timer = 0;
    m_glitchIntensity = 1.0f;

    // Select subtitle based on death cause
    switch (s_deathCause) {
        case 1:  m_subtitleText = LOC("gameover.death_hp"); break;
        case 2:  m_subtitleText = LOC("gameover.death_entropy"); break;
        case 3:  m_subtitleText = LOC("gameover.death_rift"); break;
        case 4:  m_subtitleText = LOC("gameover.death_time"); break;
        default: m_subtitleText = LOC("gameover.death_entropy"); break;
    }
}

void GameOverState::handleEvent(const SDL_Event& event) {
    if ((event.type == SDL_KEYDOWN || event.type == SDL_CONTROLLERBUTTONDOWN) && m_timer > 1.5f) {
        game->changeState(StateID::Menu);
    }
}

void GameOverState::update(float dt) {
    m_timer += dt;
    // Glitch settles over time
    if (m_timer < 3.0f) {
        m_glitchIntensity = std::max(0.0f, 1.0f - m_timer * 0.3f);
    } else {
        m_glitchIntensity = 0.05f;
    }
}

void GameOverState::render(SDL_Renderer* renderer) {
    // Dark red-tinted background
    SDL_SetRenderDrawColor(renderer, 8, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Scanlines
    for (int y = 0; y < SCREEN_HEIGHT; y += 3) {
        Uint8 a = static_cast<Uint8>(8 + 5 * (y % 6 == 0 ? 1 : 0));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
    }

    // Random glitch bars
    if (m_glitchIntensity > 0.1f) {
        int barCount = static_cast<int>(m_glitchIntensity * 8);
        for (int i = 0; i < barCount; i++) {
            int gy = std::rand() % SCREEN_HEIGHT;
            int gh = 1 + std::rand() % 4;
            int gx = std::rand() % 200 - 100;
            Uint8 gr = static_cast<Uint8>(std::rand() % 60);
            Uint8 ga = static_cast<Uint8>(40 + std::rand() % 60);
            SDL_SetRenderDrawColor(renderer, 100 + gr, 10, 10, ga);
            SDL_Rect bar = {gx, gy, SCREEN_WIDTH + 200, gh};
            SDL_RenderFillRect(renderer, &bar);
        }
    }

    // Red vignette effect
    Uint8 vigAlpha = static_cast<Uint8>(std::min(255.0f, 40.0f + m_timer * 15.0f));
    // Top
    SDL_SetRenderDrawColor(renderer, 80, 0, 0, vigAlpha);
    SDL_Rect vigTop = {0, 0, SCREEN_WIDTH, 80};
    SDL_RenderFillRect(renderer, &vigTop);
    // Bottom
    SDL_Rect vigBot = {0, SCREEN_HEIGHT - 80, SCREEN_WIDTH, 80};
    SDL_RenderFillRect(renderer, &vigBot);
    // Sides
    SDL_Rect vigL = {0, 0, 60, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &vigL);
    SDL_Rect vigR = {SCREEN_WIDTH - 60, 0, 60, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &vigR);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Main title with color channel separation (glitch effect)
    SDL_Surface* textSurf = TTF_RenderText_Blended(font, LOC("gameover.title"), {255, 255, 255, 255});
    if (textSurf) {
        int tw = textSurf->w * 2;
        int th = textSurf->h * 2;
        int tx = SCREEN_WIDTH / 2 - tw / 2;
        int ty = 280;

        // Glitch offset
        int glitchOff = static_cast<int>(m_glitchIntensity * 20.0f *
            (std::rand() % 2 == 0 ? 1.0f : -1.0f));

        // Red channel (offset left)
        SDL_Texture* redTex = SDL_CreateTextureFromSurface(renderer, textSurf);
        if (redTex) {
            SDL_SetTextureColorMod(redTex, 255, 30, 30);
            SDL_SetTextureAlphaMod(redTex, 180);
            SDL_SetTextureBlendMode(redTex, SDL_BLENDMODE_ADD);
            int redOff = static_cast<int>(m_glitchIntensity * 6);
            SDL_Rect rr = {tx - redOff + glitchOff, ty, tw, th};
            SDL_RenderCopy(renderer, redTex, nullptr, &rr);
            SDL_DestroyTexture(redTex);
        }

        // Blue channel (offset right)
        SDL_Texture* blueTex = SDL_CreateTextureFromSurface(renderer, textSurf);
        if (blueTex) {
            SDL_SetTextureColorMod(blueTex, 30, 30, 255);
            SDL_SetTextureAlphaMod(blueTex, 120);
            SDL_SetTextureBlendMode(blueTex, SDL_BLENDMODE_ADD);
            int blueOff = static_cast<int>(m_glitchIntensity * 6);
            SDL_Rect br = {tx + blueOff + glitchOff, ty + 1, tw, th};
            SDL_RenderCopy(renderer, blueTex, nullptr, &br);
            SDL_DestroyTexture(blueTex);
        }

        // Main white text
        SDL_Texture* mainTex = SDL_CreateTextureFromSurface(renderer, textSurf);
        if (mainTex) {
            SDL_SetTextureColorMod(mainTex, 220, 40, 40);
            SDL_Rect mr = {tx + glitchOff, ty, tw, th};
            SDL_RenderCopy(renderer, mainTex, nullptr, &mr);
            SDL_DestroyTexture(mainTex);
        }

        SDL_FreeSurface(textSurf);
    }

    // Subtitle with entropy warning
    if (m_timer > 0.5f) {
        float subAlpha = std::min(1.0f, (m_timer - 0.5f) * 2.0f);
        Uint8 sa = static_cast<Uint8>(180 * subAlpha);
        SDL_Color sc = {180, 60, 60, sa};
        SDL_Surface* ss = TTF_RenderText_Blended(font, m_subtitleText, sc);
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_SetTextureAlphaMod(st, sa);
                SDL_Rect sr = {SCREEN_WIDTH / 2 - ss->w / 2, 370, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &sr);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }
    }

    // Run stats (floors, kills, time)
    if (m_timer > 0.8f) {
        float statsAlpha = std::min(1.0f, (m_timer - 0.8f) * 2.0f);
        Uint8 sa = static_cast<Uint8>(160 * statsAlpha);
        SDL_Color statCol = {160, 100, 100, sa};

        char buf[128];
        int mins = static_cast<int>(s_runTime) / 60;
        int secs = static_cast<int>(s_runTime) % 60;
        std::snprintf(buf, sizeof(buf), LOC("gameover.floor_stats"),
                      s_floorsCleared, s_killCount, mins, secs);

        SDL_Surface* ss = TTF_RenderText_Blended(font, buf, statCol);
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_SetTextureAlphaMod(st, sa);
                SDL_Rect sr = {SCREEN_WIDTH / 2 - ss->w / 2, 398, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &sr);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }
    }

    // Entropy bar (full/overloaded)
    if (m_timer > 1.0f) {
        float barAlpha = std::min(1.0f, (m_timer - 1.0f) * 2.0f);
        Uint8 ba = static_cast<Uint8>(200 * barAlpha);

        int barX = 440, barY = 410, barW = 400, barH = 16;
        SDL_SetRenderDrawColor(renderer, 30, 10, 10, ba);
        SDL_Rect barBg = {barX, barY, barW, barH};
        SDL_RenderFillRect(renderer, &barBg);

        // Overloaded fill (pulsing red)
        float pulse = 0.7f + 0.3f * std::sin(m_timer * 6.0f);
        Uint8 fillR = static_cast<Uint8>(200 * pulse);
        SDL_SetRenderDrawColor(renderer, fillR, 20, 20, ba);
        SDL_Rect barFill = {barX, barY, barW, barH};
        SDL_RenderFillRect(renderer, &barFill);

        SDL_SetRenderDrawColor(renderer, 180, 40, 40, ba);
        SDL_RenderDrawRect(renderer, &barBg);

        // "CRITICAL" text
        SDL_Color critColor = {255, static_cast<Uint8>(60 * pulse), 40, ba};
        SDL_Surface* cs = TTF_RenderText_Blended(font, LOC("gameover.critical"), critColor);
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_SetTextureAlphaMod(ct, ba);
                SDL_Rect cr = {SCREEN_WIDTH / 2 - cs->w / 2, barY + barH + 5, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }

    // Continue prompt
    if (m_timer > 1.5f) {
        float promptAlpha = std::min(1.0f, (m_timer - 1.5f));
        float blink = 0.5f + 0.5f * std::sin(m_timer * 3.0f);
        Uint8 pa = static_cast<Uint8>(180 * promptAlpha * blink);
        SDL_Color c2 = {120, 100, 140, pa};
        SDL_Surface* s2 = TTF_RenderText_Blended(font, LOC("gameover.continue"), c2);
        if (s2) {
            SDL_Texture* t2 = SDL_CreateTextureFromSurface(renderer, s2);
            if (t2) {
                SDL_Rect r2 = {SCREEN_WIDTH / 2 - s2->w / 2, 500, s2->w, s2->h};
                SDL_RenderCopy(renderer, t2, nullptr, &r2);
                SDL_DestroyTexture(t2);
            }
            SDL_FreeSurface(s2);
        }
    }
}
