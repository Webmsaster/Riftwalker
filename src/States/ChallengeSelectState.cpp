#include "ChallengeSelectState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "UI/UITextures.h"
#include <cmath>
#include <cstdio>

void ChallengeSelectState::enter() {
    m_selectedChallenge = 0;
    m_inMutatorSection = false;
    m_focusedMutator = 0;
    m_time = 0;
    g_activeChallenge = ChallengeID::None;
    g_activeMutators[0] = MutatorID::None;
    g_activeMutators[1] = MutatorID::None;
}

void ChallengeSelectState::handleEvent(const SDL_Event& event) {
    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
        return;
    }

    // Mouse hover for challenges and mutators
    int challengeCount = ChallengeMode::getChallengeCount();
    int mutatorCount = ChallengeMode::getMutatorCount();

    if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x, my = event.motion.y;
        // Challenge cards (left side)
        for (int i = 0; i < challengeCount; i++) {
            int y = 200 + i * 170;
            if (mx >= 80 && mx < 1080 && my >= y && my < y + 160) {
                if (m_inMutatorSection || i != m_selectedChallenge) {
                    m_inMutatorSection = false;
                    m_selectedChallenge = i;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                return;
            }
        }
        // Mutator cards (right side)
        for (int i = 0; i < mutatorCount; i++) {
            int y = 260 + i * 110;
            if (mx >= 1400 && mx < 2440 && my >= y && my < y + 100) {
                if (!m_inMutatorSection || i != m_focusedMutator) {
                    m_inMutatorSection = true;
                    m_focusedMutator = i;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                return;
            }
        }
    }

    // Mouse click for challenges and mutators
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        for (int i = 0; i < challengeCount; i++) {
            int y = 200 + i * 170;
            if (mx >= 80 && mx < 1080 && my >= y && my < y + 160) {
                m_inMutatorSection = false;
                m_selectedChallenge = i;
                g_activeChallenge = static_cast<ChallengeID>(m_selectedChallenge + 1);
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::ClassSelect);
                return;
            }
        }
        for (int i = 0; i < mutatorCount; i++) {
            int y = 260 + i * 110;
            if (mx >= 1400 && mx < 2440 && my >= y && my < y + 100) {
                m_inMutatorSection = true;
                m_focusedMutator = i;
                MutatorID mid = static_cast<MutatorID>(i + 1);
                bool isActive = (g_activeMutators[0] == mid || g_activeMutators[1] == mid);
                if (isActive) {
                    if (g_activeMutators[0] == mid) g_activeMutators[0] = MutatorID::None;
                    if (g_activeMutators[1] == mid) g_activeMutators[1] = MutatorID::None;
                } else {
                    if (g_activeMutators[0] == MutatorID::None) g_activeMutators[0] = mid;
                    else if (g_activeMutators[1] == MutatorID::None) g_activeMutators[1] = mid;
                }
                AudioManager::instance().play(SFX::MenuConfirm);
                return;
            }
        }
    }

    if (event.type != SDL_KEYDOWN) return;

    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
            AudioManager::instance().play(SFX::MenuSelect);
            if (m_inMutatorSection) {
                m_focusedMutator = (m_focusedMutator - 1 + mutatorCount) % mutatorCount;
            } else {
                m_selectedChallenge = (m_selectedChallenge - 1 + challengeCount) % challengeCount;
            }
            break;

        case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
            AudioManager::instance().play(SFX::MenuSelect);
            if (m_inMutatorSection) {
                m_focusedMutator = (m_focusedMutator + 1) % mutatorCount;
            } else {
                m_selectedChallenge = (m_selectedChallenge + 1) % challengeCount;
            }
            break;

        case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT:
            if (!m_inMutatorSection) {
                m_inMutatorSection = true;
                AudioManager::instance().play(SFX::MenuSelect);
            }
            break;

        case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:
            if (m_inMutatorSection) {
                m_inMutatorSection = false;
                AudioManager::instance().play(SFX::MenuSelect);
            }
            break;

        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
            AudioManager::instance().play(SFX::MenuConfirm);
            if (m_inMutatorSection) {
                // Toggle mutator (max 2)
                MutatorID mid = static_cast<MutatorID>(m_focusedMutator + 1);
                bool isActive = (g_activeMutators[0] == mid || g_activeMutators[1] == mid);
                if (isActive) {
                    // Remove
                    if (g_activeMutators[0] == mid) g_activeMutators[0] = MutatorID::None;
                    if (g_activeMutators[1] == mid) g_activeMutators[1] = MutatorID::None;
                } else {
                    // Add to first empty slot
                    if (g_activeMutators[0] == MutatorID::None)
                        g_activeMutators[0] = mid;
                    else if (g_activeMutators[1] == MutatorID::None)
                        g_activeMutators[1] = mid;
                }
            } else {
                // Select challenge and start run
                g_activeChallenge = static_cast<ChallengeID>(m_selectedChallenge + 1);
                game->changeState(StateID::ClassSelect);
            }
            break;

        case SDL_SCANCODE_ESCAPE:
            AudioManager::instance().play(SFX::MenuConfirm);
            game->changeState(StateID::Menu);
            break;

        default: break;
    }
}

void ChallengeSelectState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    int challengeCount = ChallengeMode::getChallengeCount();
    int mutatorCount = ChallengeMode::getMutatorCount();

    if (input.isActionPressed(Action::MenuUp)) {
        AudioManager::instance().play(SFX::MenuSelect);
        if (m_inMutatorSection) {
            m_focusedMutator = (m_focusedMutator - 1 + mutatorCount) % mutatorCount;
        } else {
            m_selectedChallenge = (m_selectedChallenge - 1 + challengeCount) % challengeCount;
        }
    }
    if (input.isActionPressed(Action::MenuDown)) {
        AudioManager::instance().play(SFX::MenuSelect);
        if (m_inMutatorSection) {
            m_focusedMutator = (m_focusedMutator + 1) % mutatorCount;
        } else {
            m_selectedChallenge = (m_selectedChallenge + 1) % challengeCount;
        }
    }
    if (input.isActionPressed(Action::MenuRight)) {
        if (!m_inMutatorSection) {
            m_inMutatorSection = true;
            AudioManager::instance().play(SFX::MenuSelect);
        }
    }
    if (input.isActionPressed(Action::MenuLeft)) {
        if (m_inMutatorSection) {
            m_inMutatorSection = false;
            AudioManager::instance().play(SFX::MenuSelect);
        }
    }
    if (input.isActionPressed(Action::Confirm)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        if (m_inMutatorSection) {
            MutatorID mid = static_cast<MutatorID>(m_focusedMutator + 1);
            bool isActive = (g_activeMutators[0] == mid || g_activeMutators[1] == mid);
            if (isActive) {
                if (g_activeMutators[0] == mid) g_activeMutators[0] = MutatorID::None;
                if (g_activeMutators[1] == mid) g_activeMutators[1] = MutatorID::None;
            } else {
                if (g_activeMutators[0] == MutatorID::None)
                    g_activeMutators[0] = mid;
                else if (g_activeMutators[1] == MutatorID::None)
                    g_activeMutators[1] = mid;
            }
        } else {
            g_activeChallenge = static_cast<ChallengeID>(m_selectedChallenge + 1);
            game->changeState(StateID::ClassSelect);
        }
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
    }
}

void ChallengeSelectState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 10, 8, 20, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Grid
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 20);
    for (int x = 0; x < SCREEN_WIDTH; x += 160) SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 160) SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Title
    {
        SDL_Color c = {220, 160, 60, 255};
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, LOC("challenges.title"), c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int tw = static_cast<int>(s->w * 1.5f);
                int th = static_cast<int>(s->h * 1.5f);
                SDL_Rect r = {SCREEN_WIDTH / 2 - tw / 2, 80, tw, th};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Challenge cards (left side)
    int cardW = 1000;
    int cardH = 160;
    int cardX = 80;
    int startY = 200;

    int challengeCount = ChallengeMode::getChallengeCount();
    for (int i = 0; i < challengeCount; i++) {
        ChallengeID cid = static_cast<ChallengeID>(i + 1);
        const auto& data = ChallengeMode::getChallengeData(cid);
        bool selected = (!m_inMutatorSection && i == m_selectedChallenge);
        renderChallengeCard(renderer, font, data, cardX, startY + i * (cardH + 10),
                           cardW, cardH, selected);
    }

    // Mutator section (right side)
    {
        SDL_Color mc = {160, 140, 200, 200};
        SDL_Surface* ms = TTF_RenderUTF8_Blended(font, LOC("challenges.mutators"), mc);
        if (ms) {
            SDL_Texture* mt = SDL_CreateTextureFromSurface(renderer, ms);
            if (mt) {
                SDL_Rect mr = {1400, 200, ms->w, ms->h};
                SDL_RenderCopy(renderer, mt, nullptr, &mr);
                SDL_DestroyTexture(mt);
            }
            SDL_FreeSurface(ms);
        }
    }

    int mutX = 1400;
    int mutY = 260;
    int mutW = 1040;
    int mutH = 100;
    int mutatorCount = ChallengeMode::getMutatorCount();
    for (int i = 0; i < mutatorCount; i++) {
        MutatorID mid = static_cast<MutatorID>(i + 1);
        const auto& data = ChallengeMode::getMutatorData(mid);
        bool active = (g_activeMutators[0] == mid || g_activeMutators[1] == mid);
        bool focused = (m_inMutatorSection && i == m_focusedMutator);
        renderMutatorToggle(renderer, font, data, mutX, mutY + i * (mutH + 6),
                           mutW, mutH, active, focused);
    }

    // Navigation hint
    {
        SDL_Color nc = {60, 55, 85, 140};
        SDL_Surface* ns = TTF_RenderUTF8_Blended(font, LOC("challenges.nav_hint"), nc);
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

void ChallengeSelectState::renderChallengeCard(SDL_Renderer* renderer, TTF_Font* font,
                                                const ChallengeData& data, int x, int y,
                                                int w, int h, bool selected) {
    Uint8 bgA = selected ? 80 : 30;
    SDL_Rect card = {x, y, w, h};
    if (!renderPanelBg(renderer, card, bgA, selected ? "assets/textures/ui/panel_light.png" : "assets/textures/ui/panel_dark.png")) {
        SDL_SetRenderDrawColor(renderer, 30, 25, 50, bgA);
        SDL_RenderFillRect(renderer, &card);
    }

    SDL_Color accentColor = {220, 160, 60, 255};
    Uint8 accentA = selected ? 220 : 60;
    SDL_SetRenderDrawColor(renderer, accentColor.r, accentColor.g, accentColor.b, accentA);
    SDL_Rect accent = {x, y, 8, h};
    SDL_RenderFillRect(renderer, &accent);

    if (selected) {
        float pulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
        Uint8 borderA = static_cast<Uint8>(200 * pulse);
        SDL_SetRenderDrawColor(renderer, accentColor.r, accentColor.g, accentColor.b, borderA);
        SDL_RenderDrawRect(renderer, &card);
    }

    // Localized challenge name + description
    char cNameKey[32], cDescKey[32];
    std::snprintf(cNameKey, sizeof(cNameKey), "challenge.%d.name", static_cast<int>(data.id));
    std::snprintf(cDescKey, sizeof(cDescKey), "challenge.%d.desc", static_cast<int>(data.id));

    SDL_Color nameColor = selected ? accentColor : SDL_Color{130, 110, 50, 200};
    SDL_Surface* ns = TTF_RenderUTF8_Blended(font, LOC(cNameKey), nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            SDL_Rect nr = {x + 30, y + 20, ns->w, ns->h};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    SDL_Color descColor = selected ? SDL_Color{180, 175, 200, 220} : SDL_Color{100, 95, 120, 150};
    // Use word-wrapped rendering so long DE descriptions don't overflow the card
    Uint32 wrapW = 960; // card usable width after 34px left + 20px right padding
    SDL_Surface* ds = TTF_RenderUTF8_Blended_Wrapped(font, LOC(cDescKey), descColor, wrapW);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            SDL_Rect dr = {x + 34, y + 76, ds->w, ds->h};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }
}

void ChallengeSelectState::renderMutatorToggle(SDL_Renderer* renderer, TTF_Font* font,
                                                const MutatorData& data, int x, int y,
                                                int w, int h, bool active, bool focused) {
    Uint8 bgA = focused ? 60 : (active ? 50 : 20);
    SDL_Rect card = {x, y, w, h};
    if (!renderPanelBg(renderer, card, bgA)) {
        SDL_SetRenderDrawColor(renderer, active ? 30 : 20, active ? 40 : 15, active ? 25 : 30, bgA);
        SDL_RenderFillRect(renderer, &card);
    }

    // Checkbox
    SDL_Rect check = {x + 16, y + h / 2 - 14, 28, 28};
    SDL_SetRenderDrawColor(renderer, 120, 110, 150, focused ? static_cast<Uint8>(200) : static_cast<Uint8>(100));
    SDL_RenderDrawRect(renderer, &check);
    if (active) {
        SDL_SetRenderDrawColor(renderer, 100, 220, 100, 220);
        SDL_Rect fill = {check.x + 4, check.y + 4, 20, 20};
        SDL_RenderFillRect(renderer, &fill);
    }

    if (focused) {
        float pulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
        Uint8 borderA = static_cast<Uint8>(150 * pulse);
        SDL_SetRenderDrawColor(renderer, 160, 140, 200, borderA);
        SDL_RenderDrawRect(renderer, &card);
    }

    // Localized mutator name + description
    char mNameKey[32], mDescKey[32];
    std::snprintf(mNameKey, sizeof(mNameKey), "mutator.%d.name", static_cast<int>(data.id));
    std::snprintf(mDescKey, sizeof(mDescKey), "mutator.%d.desc", static_cast<int>(data.id));

    SDL_Color nameColor = active ? SDL_Color{100, 220, 100, 255} :
        (focused ? SDL_Color{180, 170, 210, 255} : SDL_Color{100, 95, 120, 180});
    SDL_Surface* ns = TTF_RenderUTF8_Blended(font, LOC(mNameKey), nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            SDL_Rect nr = {x + 60, y + 10, ns->w, ns->h};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    SDL_Color descColor = {120, 115, 140, static_cast<Uint8>(focused ? 180 : 100)};
    SDL_Surface* ds = TTF_RenderUTF8_Blended(font, LOC(mDescKey), descColor);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            SDL_Rect dr = {x + 64, y + 52, ds->w, ds->h};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }
}
