#include "States/LoreState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "UI/UITextures.h"
#include <algorithm>
#include <cmath>

void LoreState::enter() {
    m_fontTitle = TTF_OpenFont("assets/fonts/default.ttf", 56);
    m_fontBody = TTF_OpenFont("assets/fonts/default.ttf", 32);
    m_fontSmall = TTF_OpenFont("assets/fonts/default.ttf", 28);
    m_selected = 0;
    m_time = 0.0f;
    m_lore = game->getLoreSystem();
}

void LoreState::exit() {
    if (m_fontTitle) { TTF_CloseFont(m_fontTitle); m_fontTitle = nullptr; }
    if (m_fontBody) { TTF_CloseFont(m_fontBody); m_fontBody = nullptr; }
    if (m_fontSmall) { TTF_CloseFont(m_fontSmall); m_fontSmall = nullptr; }
}

void LoreState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad() || !m_lore) return;

    int total = static_cast<int>(m_lore->getFragments().size());
    if (total == 0) return;

    if (input.isActionPressed(Action::MenuUp)) {
        m_selected = (m_selected - 1 + total) % total;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_selected = (m_selected + 1) % total;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        if (game) game->changeState(StateID::Menu);
    }
}

void LoreState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 8, 4, 16, 255);
    SDL_RenderClear(renderer);

    if (!m_lore) return;
    auto& fragments = m_lore->getFragments();

    // Title
    if (m_fontTitle) {
        SDL_Color white = {220, 200, 255, 255};
        SDL_Surface* surf = TTF_RenderText_Blended(m_fontTitle, LOC("lore.title"), white);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 60, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Progress
    if (m_fontSmall) {
        char buf[64];
        snprintf(buf, sizeof(buf), LOC("lore.fragments"), m_lore->discoveredCount(), m_lore->totalCount());
        SDL_Color gray = {150, 130, 170, 255};
        SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, buf, gray);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 170, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // List on left
    int listX = 60, listY = 200;
    for (int i = 0; i < static_cast<int>(fragments.size()); i++) {
        bool discovered = fragments[i].discovered;
        bool selected = (i == m_selected);

        // Background
        if (selected) {
            SDL_Rect bg = {listX - 5, listY + i * 64 - 2, 500, 56};
            if (!renderPanelBg(renderer, bg, 200, "assets/textures/ui/panel_light.png")) {
                SDL_SetRenderDrawColor(renderer, 60, 30, 80, 200);
                SDL_RenderFillRect(renderer, &bg);
            }
        }

        if (m_fontSmall) {
            const char* title = discovered ? fragments[i].title.c_str() : "???";
            SDL_Color col = selected ? SDL_Color{255, 200, 255, 255} :
                            discovered ? SDL_Color{180, 160, 200, 255} :
                                         SDL_Color{80, 60, 100, 255};
            SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, title, col);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    SDL_Rect dst = {listX, listY + i * 64, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }
    }

    // Detail panel on right
    int panelX = 620, panelY = 200, panelW = 920, panelH = 880;
    SDL_Rect panel = {panelX, panelY, panelW, panelH};
    if (!renderPanelBg(renderer, panel, 220)) {
        SDL_SetRenderDrawColor(renderer, 20, 10, 30, 220);
        SDL_RenderFillRect(renderer, &panel);
    }
    SDL_SetRenderDrawColor(renderer, 80, 40, 120, 255);
    SDL_RenderDrawRect(renderer, &panel);

    if (m_selected >= 0 && m_selected < static_cast<int>(fragments.size())) {
        auto& frag = fragments[m_selected];
        if (frag.discovered) {
            // Title
            if (m_fontTitle) {
                SDL_Color purple = {200, 150, 255, 255};
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontTitle, frag.title.c_str(), purple);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {panelX + 40, panelY + 30, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }

            // Text (word-wrapped manually)
            if (m_fontBody) {
                SDL_Color textCol = {180, 170, 200, 255};
                // Simple line splitting at ~55 chars
                std::string text = frag.text;
                int lineY = panelY + 120;
                int maxLineW = panelW - 80;
                while (!text.empty() && lineY < panelY + panelH - 40) {
                    std::string line;
                    if (m_fontBody) {
                        // Find how many chars fit
                        size_t end = text.size();
                        for (size_t len = end; len > 0; len--) {
                            int tw = 0;
                            TTF_SizeText(m_fontBody, text.substr(0, len).c_str(), &tw, nullptr);
                            if (tw <= maxLineW) {
                                // Find last space before this point
                                size_t space = text.rfind(' ', len);
                                if (space != std::string::npos && space > 0 && len < text.size()) {
                                    line = text.substr(0, space);
                                    text = text.substr(space + 1);
                                } else {
                                    line = text.substr(0, len);
                                    text = text.substr(len);
                                }
                                break;
                            }
                        }
                    }
                    if (line.empty()) break;

                    SDL_Surface* surf = TTF_RenderText_Blended(m_fontBody, line.c_str(), textCol);
                    if (surf) {
                        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                        if (tex) {
                            SDL_Rect dst = {panelX + 40, lineY, surf->w, surf->h};
                            SDL_RenderCopy(renderer, tex, nullptr, &dst);
                            SDL_DestroyTexture(tex);
                        }
                        SDL_FreeSurface(surf);
                    }
                    lineY += 44;
                }
            }
        } else {
            // Undiscovered
            if (m_fontBody) {
                SDL_Color dim = {80, 60, 100, 255};
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontBody, LOC("lore.undiscovered"), dim);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {panelX + panelW / 2 - surf->w / 2, panelY + panelH / 2 - 20, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
        }
    }

    // Controls hint
    if (m_fontSmall) {
        SDL_Color hint = {100, 80, 120, 255};
        SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, LOC("lore.nav_hint"), hint);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, SCREEN_HEIGHT - 60, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }
}

void LoreState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (!m_lore) return;
        int total = static_cast<int>(m_lore->getFragments().size());
        if (total == 0) return;
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                if (game) game->changeState(StateID::Menu);
                break;
            case SDL_SCANCODE_W:
            case SDL_SCANCODE_UP:
                m_selected = (m_selected - 1 + total) % total;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_S:
            case SDL_SCANCODE_DOWN:
                m_selected = (m_selected + 1) % total;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            default: break;
        }
    }

    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        if (game) game->changeState(StateID::Menu);
    }

    // Mouse hover: highlight lore fragment in list
    if (event.type == SDL_MOUSEMOTION && m_lore) {
        int mx = event.motion.x, my = event.motion.y;
        int listX = 60, listY = 200;
        int total = static_cast<int>(m_lore->getFragments().size());
        for (int i = 0; i < total; i++) {
            int ey = listY + i * 64;
            if (mx >= listX - 5 && mx < listX + 495 && my >= ey - 2 && my < ey + 56) {
                if (i != m_selected) m_selected = i;
                break;
            }
        }
    }

    // Mouse wheel scrolling
    if (event.type == SDL_MOUSEWHEEL && m_lore) {
        int total = static_cast<int>(m_lore->getFragments().size());
        if (total > 0) {
            if (event.wheel.y > 0) {
                m_selected = (m_selected - 1 + total) % total;
                AudioManager::instance().play(SFX::MenuSelect);
            } else if (event.wheel.y < 0) {
                m_selected = (m_selected + 1) % total;
                AudioManager::instance().play(SFX::MenuSelect);
            }
        }
    }
}
