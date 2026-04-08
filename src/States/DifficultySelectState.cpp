#include "DifficultySelectState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/ClassSystem.h"
#include "Game/UpgradeSystem.h"
#include "UI/UITextures.h"
#include <cmath>
#include <cstdio>

void DifficultySelectState::enter() {
    m_selected = static_cast<int>(g_selectedDifficulty);
    m_time = 0;
}

void DifficultySelectState::handleConfirm() {
    g_selectedDifficulty = static_cast<GameDifficulty>(m_selected);
    AudioManager::instance().play(SFX::MenuConfirm);
    if (game->getUpgradeSystem().getMaxUnlockedNGPlus() >= 1) {
        game->changeState(StateID::NGPlusSelect);
    } else {
        g_selectedNGPlus = 0;
        game->changeState(StateID::Play);
    }
}

void DifficultySelectState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_selected = (m_selected - 1 + 3) % 3;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_selected = (m_selected + 1) % 3;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                handleConfirm();
                break;
            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::Menu);
                break;
            default: break;
        }
    }

    // Card layout — mirrors render() calculations exactly
    int cardH = 220;
    int cardW = 1000;
    int cardX = SCREEN_WIDTH / 2 - cardW / 2;
    int startY = 380;

    if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x, my = event.motion.y;
        for (int i = 0; i < 3; i++) {
            int y = startY + i * (cardH + 30);
            if (mx >= cardX && mx < cardX + cardW && my >= y && my < y + cardH) {
                if (i != m_selected) {
                    m_selected = i;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                break;
            }
        }
    }

    // Mouse wheel scrolling
    if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0)
            m_selected = (m_selected - 1 + 3) % 3;
        else if (event.wheel.y < 0)
            m_selected = (m_selected + 1) % 3;
        AudioManager::instance().play(SFX::MenuSelect);
    }

    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        for (int i = 0; i < 3; i++) {
            int y = startY + i * (cardH + 30);
            if (mx >= cardX && mx < cardX + cardW && my >= y && my < y + cardH) {
                m_selected = i;
                handleConfirm();
                break;
            }
        }
    }
}

void DifficultySelectState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    if (input.isActionPressed(Action::MenuUp)) {
        m_selected = (m_selected - 1 + 3) % 3;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_selected = (m_selected + 1) % 3;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Confirm)) {
        handleConfirm();
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
    }
}

void DifficultySelectState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 10, 8, 20, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Grid
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 20);
    for (int x = 0; x < SCREEN_WIDTH; x += 160) SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 160) SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Class indicator at top
    {
        const auto& classData = ClassSystem::getData(g_selectedClass);
        char classText[64];
        std::snprintf(classText, sizeof(classText), LOC("difficulty.class"), classData.name);
        SDL_Surface* cs = TTF_RenderUTF8_Blended(font, classText, classData.color);
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_Rect cr = {SCREEN_WIDTH / 2 - cs->w / 2, 120, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }

    // Title
    {
        SDL_Color c = {140, 100, 220, 255};
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, LOC("difficulty.title"), c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int tw = static_cast<int>(s->w * 1.5f);
                int th = static_cast<int>(s->h * 1.5f);
                SDL_Rect r = {SCREEN_WIDTH / 2 - tw / 2, 200, tw, th};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    struct DiffOption {
        const char* name;
        const char* desc;
        SDL_Color color;
    };
    DiffOption options[] = {
        {LOC("difficulty.easy"), LOC("difficulty.easy_desc"), {80, 200, 80, 255}},
        {LOC("difficulty.normal"), LOC("difficulty.normal_desc"), {200, 200, 80, 255}},
        {LOC("difficulty.hard"), LOC("difficulty.hard_desc"), {220, 60, 60, 255}}
    };

    int startY = 380;
    int cardH = 220;
    int cardW = 1000;
    int cardX = SCREEN_WIDTH / 2 - cardW / 2;

    for (int i = 0; i < 3; i++) {
        int y = startY + i * (cardH + 30);
        bool selected = (i == m_selected);

        // Card background
        Uint8 bgA = selected ? 80 : 30;
        SDL_Rect card = {cardX, y, cardW, cardH};
        if (!renderPanelBg(renderer, card, bgA, selected ? "assets/textures/ui/panel_light.png" : "assets/textures/ui/panel_dark.png")) {
            SDL_SetRenderDrawColor(renderer, 30, 25, 50, bgA);
            SDL_RenderFillRect(renderer, &card);
        }

        // Left accent
        SDL_SetRenderDrawColor(renderer, options[i].color.r, options[i].color.g,
                               options[i].color.b, selected ? static_cast<Uint8>(220) : static_cast<Uint8>(80));
        SDL_Rect accent = {cardX, y, 8, cardH};
        SDL_RenderFillRect(renderer, &accent);

        // Selection border
        if (selected) {
            float pulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
            Uint8 borderA = static_cast<Uint8>(200 * pulse);
            SDL_SetRenderDrawColor(renderer, options[i].color.r, options[i].color.g,
                                   options[i].color.b, borderA);
            SDL_RenderDrawRect(renderer, &card);
        }

        // Name
        SDL_Color nameColor = selected ? options[i].color :
            SDL_Color{static_cast<Uint8>(options[i].color.r / 2),
                      static_cast<Uint8>(options[i].color.g / 2),
                      static_cast<Uint8>(options[i].color.b / 2), 255};
        SDL_Surface* ns = TTF_RenderUTF8_Blended(font, options[i].name, nameColor);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                int nw = static_cast<int>(ns->w * 1.3f);
                int nh = static_cast<int>(ns->h * 1.3f);
                SDL_Rect nr = {cardX + 40, y + 30, nw, nh};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Description
        SDL_Color descColor = selected ? SDL_Color{180, 175, 200, 220} : SDL_Color{100, 95, 120, 150};
        SDL_Surface* ds = TTF_RenderUTF8_Blended(font, options[i].desc, descColor);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {cardX + 44, y + 116, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }
    }

    // Navigation hint
    {
        SDL_Color nc = {60, 55, 85, 140};
        SDL_Surface* ns = TTF_RenderUTF8_Blended(font, LOC("difficulty.nav_hint"), nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {SCREEN_WIDTH / 2 - ns->w / 2, SCREEN_HEIGHT - 60, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }
}
