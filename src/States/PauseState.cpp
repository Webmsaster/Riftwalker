#include "PauseState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include <cmath>

void PauseState::enter() {
    int cx = 640;
    int startY = 320;
    int btnW = 240;
    int btnH = 48;
    int gap = 12;

    m_buttons.clear();
    m_buttons.emplace_back(cx - btnW / 2, startY, btnW, btnH, "Resume");
    m_buttons.emplace_back(cx - btnW / 2, startY + btnH + gap, btnW, btnH, "Abandon Run");
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 2, btnW, btnH, "Quit to Menu");

    m_buttons[0].onClick = [this]() { game->popState(); };
    m_buttons[1].onClick = [this]() {
        game->getUpgradeSystem().totalRuns++;
        game->changeState(StateID::Menu);
    };
    m_buttons[2].onClick = [this]() { game->changeState(StateID::Menu); };

    m_selectedButton = 0;
    m_buttons[0].setSelected(true);
}

void PauseState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
                game->popState();
                break;
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_buttons[m_selectedButton].setSelected(false);
                m_selectedButton = (m_selectedButton - 1 + static_cast<int>(m_buttons.size())) % static_cast<int>(m_buttons.size());
                m_buttons[m_selectedButton].setSelected(true);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_buttons[m_selectedButton].setSelected(false);
                m_selectedButton = (m_selectedButton + 1) % static_cast<int>(m_buttons.size());
                m_buttons[m_selectedButton].setSelected(true);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                AudioManager::instance().play(SFX::MenuConfirm);
                if (m_buttons[m_selectedButton].onClick)
                    m_buttons[m_selectedButton].onClick();
                break;
            default: break;
        }
    }
}

void PauseState::update(float dt) {
    m_time += dt;
}

void PauseState::render(SDL_Renderer* renderer) {
    // Dark overlay with vignette
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    // Scanlines
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 30);
    for (int y = 0; y < SCREEN_HEIGHT; y += 3) {
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
    }

    // Animated border frame
    Uint32 ticks = SDL_GetTicks();
    float pulse = 0.5f + 0.5f * std::sin(ticks * 0.003f);
    Uint8 borderAlpha = static_cast<Uint8>(40 + 30 * pulse);

    // Top/bottom border bars
    SDL_SetRenderDrawColor(renderer, 120, 80, 200, borderAlpha);
    SDL_Rect topBar = {0, 0, SCREEN_WIDTH, 3};
    SDL_Rect botBar = {0, 717, SCREEN_WIDTH, 3};
    SDL_RenderFillRect(renderer, &topBar);
    SDL_RenderFillRect(renderer, &botBar);

    // Central panel background
    SDL_Rect panelBg = {340, 180, 600, 360};
    SDL_SetRenderDrawColor(renderer, 10, 10, 20, 200);
    SDL_RenderFillRect(renderer, &panelBg);
    SDL_SetRenderDrawColor(renderer, 80, 60, 140, 100);
    SDL_RenderDrawRect(renderer, &panelBg);

    // Corner accents
    int cornerSize = 12;
    SDL_SetRenderDrawColor(renderer, 150, 100, 255, static_cast<Uint8>(100 + 50 * pulse));
    // Top-left
    SDL_RenderDrawLine(renderer, 340, 180, 340 + cornerSize, 180);
    SDL_RenderDrawLine(renderer, 340, 180, 340, 180 + cornerSize);
    // Top-right
    SDL_RenderDrawLine(renderer, 940, 180, 940 - cornerSize, 180);
    SDL_RenderDrawLine(renderer, 940, 180, 940, 180 + cornerSize);
    // Bottom-left
    SDL_RenderDrawLine(renderer, 340, 540, 340 + cornerSize, 540);
    SDL_RenderDrawLine(renderer, 340, 540, 340, 540 - cornerSize);
    // Bottom-right
    SDL_RenderDrawLine(renderer, 940, 540, 940 - cornerSize, 540);
    SDL_RenderDrawLine(renderer, 940, 540, 940, 540 - cornerSize);

    // Title with glow
    TTF_Font* font = game->getFont();
    if (font) {
        // Glow layer
        SDL_Color glowC = {120, 80, 200, static_cast<Uint8>(60 + 40 * pulse)};
        SDL_Surface* gs = TTF_RenderText_Blended(font, "P A U S E D", glowC);
        if (gs) {
            SDL_Texture* gt = SDL_CreateTextureFromSurface(renderer, gs);
            if (gt) {
                SDL_SetTextureBlendMode(gt, SDL_BLENDMODE_ADD);
                SDL_Rect gr = {640 - gs->w - 2, 218, gs->w * 2 + 4, gs->h * 2 + 4};
                SDL_RenderCopy(renderer, gt, nullptr, &gr);
                SDL_DestroyTexture(gt);
            }
            SDL_FreeSurface(gs);
        }

        // Main title
        SDL_Color c = {200, 200, 230, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "P A U S E D", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {640 - s->w, 220, s->w * 2, s->h * 2};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }

        // Separator line under title
        SDL_SetRenderDrawColor(renderer, 100, 70, 180, 80);
        SDL_RenderDrawLine(renderer, 400, 290, 880, 290);
    }

    // Buttons
    for (auto& btn : m_buttons) {
        btn.render(renderer, font);
    }

    // Controls hint
    if (font) {
        SDL_Color hintC = {100, 100, 130, 120};
        SDL_Surface* hs = TTF_RenderText_Blended(font, "ESC to resume", hintC);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            if (ht) {
                SDL_Rect hr = {640 - hs->w / 2, 510, hs->w, hs->h};
                SDL_RenderCopy(renderer, ht, nullptr, &hr);
                SDL_DestroyTexture(ht);
            }
            SDL_FreeSurface(hs);
        }
    }
}
