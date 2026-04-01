#include "KeybindingsState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include <cmath>

void KeybindingsState::enter() {
    m_selected = 0;
    m_listening = false;
    m_time = 0;
    m_blinkTimer = 0;
    refreshBindings();
}

void KeybindingsState::exit() {
    // Save bindings when leaving
    game->getInputMutable().saveBindings("riftwalker_bindings.cfg");
}

void KeybindingsState::refreshBindings() {
    m_items.clear();
    auto& input = game->getInput();
    for (auto action : InputManager::getBindableActions()) {
        BindingItem item;
        item.action = action;
        item.name = InputManager::getActionName(action);
        item.key = input.getKeyForAction(action);
        m_items.push_back(item);
    }
}

void KeybindingsState::confirmSelected() {
    if (isBackItem(m_selected)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Options);
    } else if (isResetItem(m_selected)) {
        game->getInputMutable().resetToDefaults();
        refreshBindings();
        AudioManager::instance().play(SFX::MenuConfirm);
    } else {
        // Start listening for new key
        m_listening = true;
        m_blinkTimer = 0;
        AudioManager::instance().play(SFX::MenuSelect);
    }
}

void KeybindingsState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        SDL_Scancode key = event.key.keysym.scancode;

        if (m_listening) {
            // Cancel rebind
            if (key == SDL_SCANCODE_ESCAPE) {
                m_listening = false;
                AudioManager::instance().play(SFX::MenuSelect);
                return;
            }

            // Block F11 (fullscreen toggle) and debug keys
            if (key == SDL_SCANCODE_F11 || key == SDL_SCANCODE_F3 ||
                key == SDL_SCANCODE_F4 || key == SDL_SCANCODE_F6) return;

            auto& input = game->getInputMutable();
            Action currentAction = m_items[m_selected].action;
            SDL_Scancode oldKey = m_items[m_selected].key;

            // Check for conflict and swap
            if (input.isKeyUsedByOtherAction(key, currentAction)) {
                Action conflicting = input.getActionForKey(key);
                input.rebindKey(conflicting, oldKey);
            }

            input.rebindKey(currentAction, key);
            m_listening = false;
            refreshBindings();
            AudioManager::instance().play(SFX::MenuConfirm);
            return;
        }

        // Normal navigation
        switch (key) {
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_selected = (m_selected - 1 + totalItems()) % totalItems();
                AudioManager::instance().play(SFX::MenuSelect);
                break;

            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_selected = (m_selected + 1) % totalItems();
                AudioManager::instance().play(SFX::MenuSelect);
                break;

            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                confirmSelected();
                break;

            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::Options);
                break;

            default: break;
        }
    }

    // Mouse hover: update selection (not while listening for a key)
    if (event.type == SDL_MOUSEMOTION && !m_listening) {
        int mx = event.motion.x, my = event.motion.y;
        const int startY = 120, itemH = 48, cardW = 500, cardX = SCREEN_WIDTH / 2 - cardW / 2;
        for (int i = 0; i < totalItems(); i++) {
            int y = startY + i * itemH;
            SDL_Rect card = {cardX, y, cardW, itemH - 6};
            if (mx >= card.x && mx < card.x + card.w && my >= card.y && my < card.y + card.h) {
                if (i != m_selected) {
                    m_selected = i;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                break;
            }
        }
    }

    // Mouse click: select + confirm (not while listening)
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && !m_listening) {
        int mx = event.button.x, my = event.button.y;
        const int startY = 120, itemH = 48, cardW = 500, cardX = SCREEN_WIDTH / 2 - cardW / 2;
        for (int i = 0; i < totalItems(); i++) {
            int y = startY + i * itemH;
            SDL_Rect card = {cardX, y, cardW, itemH - 6};
            if (mx >= card.x && mx < card.x + card.w && my >= card.y && my < card.y + card.h) {
                m_selected = i;
                confirmSelected();
                break;
            }
        }
    }
}

void KeybindingsState::update(float dt) {
    m_time += dt;
    if (m_listening) m_blinkTimer += dt;

    // Gamepad navigation (only when not listening for a key rebind)
    auto& input = game->getInput();
    if (!input.hasGamepad() || m_listening) return;

    if (input.isActionPressed(Action::MenuUp)) {
        m_selected = (m_selected - 1 + totalItems()) % totalItems();
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_selected = (m_selected + 1) % totalItems();
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Confirm)) {
        confirmSelected();
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Options);
    }
}

void KeybindingsState::render(SDL_Renderer* renderer) {
    // Dark background
    SDL_SetRenderDrawColor(renderer, 10, 8, 20, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Subtle grid
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 20);
    for (int x = 0; x < SCREEN_WIDTH; x += 80) SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 80) SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Title
    {
        SDL_Color c = {140, 100, 220, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "C O N T R O L S", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int tw = s->w * 2;
                int th = s->h * 2;
                SDL_Rect r = {SCREEN_WIDTH / 2 - tw / 2, 50, tw, th};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    int startY = 120;
    int itemH = 48;
    int cardW = 500;
    int cardX = SCREEN_WIDTH / 2 - cardW / 2;

    for (int i = 0; i < totalItems(); i++) {
        int y = startY + i * itemH;
        bool selected = (i == m_selected);
        bool isSpecial = isResetItem(i) || isBackItem(i);

        // Card background
        Uint8 bgA = selected ? 60 : 25;
        SDL_SetRenderDrawColor(renderer, 30, 25, 50, bgA);
        SDL_Rect card = {cardX, y, cardW, itemH - 6};
        SDL_RenderFillRect(renderer, &card);

        // Selection border
        if (selected) {
            float pulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
            Uint8 borderA = static_cast<Uint8>(180 * pulse);
            if (m_listening) {
                SDL_SetRenderDrawColor(renderer, 200, 120, 80, borderA);
            } else {
                SDL_SetRenderDrawColor(renderer, 120, 80, 200, borderA);
            }
            SDL_RenderDrawRect(renderer, &card);
        }

        if (isSpecial) {
            // Reset Defaults / Back - centered label
            const char* label = isResetItem(i) ? "Reset Defaults" : "Back";
            SDL_Color labelColor = selected ? SDL_Color{220, 200, 255, 255} : SDL_Color{140, 130, 170, 255};
            SDL_Surface* ls = TTF_RenderText_Blended(font, label, labelColor);
            if (ls) {
                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                if (lt) {
                    SDL_Rect lr = {SCREEN_WIDTH / 2 - ls->w / 2, y + (itemH - 6) / 2 - ls->h / 2, ls->w, ls->h};
                    SDL_RenderCopy(renderer, lt, nullptr, &lr);
                    SDL_DestroyTexture(lt);
                }
                SDL_FreeSurface(ls);
            }
        } else {
            // Action name (left)
            SDL_Color labelColor = selected ? SDL_Color{220, 200, 255, 255} : SDL_Color{140, 130, 170, 255};
            SDL_Surface* ls = TTF_RenderText_Blended(font, m_items[i].name.c_str(), labelColor);
            if (ls) {
                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                if (lt) {
                    SDL_Rect lr = {cardX + 20, y + (itemH - 6) / 2 - ls->h / 2, ls->w, ls->h};
                    SDL_RenderCopy(renderer, lt, nullptr, &lr);
                    SDL_DestroyTexture(lt);
                }
                SDL_FreeSurface(ls);
            }

            // Key name (right)
            std::string keyText;
            if (m_listening && selected) {
                // Blink "..." while listening
                bool show = static_cast<int>(m_blinkTimer * 3.0f) % 2 == 0;
                keyText = show ? "[...]" : "[   ]";
            } else {
                const char* name = SDL_GetScancodeName(m_items[i].key);
                keyText = std::string("[") + (name && name[0] ? name : "???") + "]";
            }

            SDL_Color keyColor;
            if (m_listening && selected) {
                keyColor = {255, 180, 100, 255};
            } else if (selected) {
                keyColor = {200, 180, 255, 255};
            } else {
                keyColor = {120, 110, 150, 255};
            }

            SDL_Surface* ks = TTF_RenderText_Blended(font, keyText.c_str(), keyColor);
            if (ks) {
                SDL_Texture* kt = SDL_CreateTextureFromSurface(renderer, ks);
                if (kt) {
                    SDL_Rect kr = {cardX + cardW - 20 - ks->w, y + (itemH - 6) / 2 - ks->h / 2, ks->w, ks->h};
                    SDL_RenderCopy(renderer, kt, nullptr, &kr);
                    SDL_DestroyTexture(kt);
                }
                SDL_FreeSurface(ks);
            }
        }
    }

    // Navigation hint
    {
        const char* hint = m_listening
            ? "Press a key to bind  |  ESC Cancel"
            : "W/S Navigate  |  ENTER Rebind  |  ESC Back";
        SDL_Color nc = {60, 55, 85, 140};
        SDL_Surface* ns = TTF_RenderText_Blended(font, hint, nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {SCREEN_WIDTH / 2 - ns->w / 2, 680, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }
}
