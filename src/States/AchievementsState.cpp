#include "AchievementsState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include <cmath>

void AchievementsState::enter() {
    m_scrollOffset = 0;
    m_animTimer = 0;

    auto& achievements = game->getAchievements().getAll();
    int itemH = 60;
    int totalH = static_cast<int>(achievements.size()) * itemH;
    int visibleH = 520; // area between title and bottom
    m_maxScroll = std::max(0, totalH - visibleH);
}

void AchievementsState::exit() {}

void AchievementsState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_BACKSPACE:
                AudioManager::instance().play(SFX::MenuSelect);
                game->popState();
                break;
            case SDL_SCANCODE_W:
            case SDL_SCANCODE_UP:
                if (m_scrollOffset > 0) {
                    m_scrollOffset -= 60;
                    if (m_scrollOffset < 0) m_scrollOffset = 0;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                break;
            case SDL_SCANCODE_S:
            case SDL_SCANCODE_DOWN:
                if (m_scrollOffset < m_maxScroll) {
                    m_scrollOffset += 60;
                    if (m_scrollOffset > m_maxScroll) m_scrollOffset = m_maxScroll;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                break;
            default: break;
        }
    }
}

void AchievementsState::update(float dt) {
    m_animTimer += dt;
}

void AchievementsState::render(SDL_Renderer* renderer) {
    TTF_Font* font = game->getFont();
    if (!font) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Background
    SDL_SetRenderDrawColor(renderer, 12, 8, 20, 255);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Subtle grid pattern
    SDL_SetRenderDrawColor(renderer, 25, 18, 40, 255);
    for (int y = 0; y < SCREEN_HEIGHT; y += 40) {
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
    }
    for (int x = 0; x < SCREEN_WIDTH; x += 40) {
        SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    }

    // Title
    {
        SDL_Color tc = {220, 200, 255, 255};
        SDL_Surface* ts = TTF_RenderText_Blended(font, "A C H I E V E M E N T S", tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {640 - ts->w / 2, 30, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }

    // Counter
    auto& achSys = game->getAchievements();
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d / %d unlocked", achSys.getUnlockedCount(), achSys.getTotalCount());
        SDL_Color cc = {180, 160, 200, 200};
        SDL_Surface* cs = TTF_RenderText_Blended(font, buf, cc);
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_Rect cr = {640 - cs->w / 2, 60, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }

    // Clip area for scrollable list
    SDL_Rect clipRect = {140, 90, 1000, 540};
    SDL_RenderSetClipRect(renderer, &clipRect);

    auto& achievements = achSys.getAll();
    int itemH = 60;
    int startY = 100 - m_scrollOffset;

    for (int i = 0; i < static_cast<int>(achievements.size()); i++) {
        auto& a = achievements[i];
        int y = startY + i * itemH;

        // Skip if off-screen
        if (y + itemH < 90 || y > 630) continue;

        int cardX = 190;
        int cardW = 900;
        int cardH = 52;

        // Card background
        if (a.unlocked) {
            float pulse = 0.03f * std::sin(m_animTimer * 2.0f + i * 0.5f);
            SDL_SetRenderDrawColor(renderer, 30, 50, 40, static_cast<Uint8>(200 + 30 * pulse));
        } else {
            SDL_SetRenderDrawColor(renderer, 25, 20, 30, 180);
        }
        SDL_Rect card = {cardX, y, cardW, cardH};
        SDL_RenderFillRect(renderer, &card);

        // Border
        if (a.unlocked) {
            SDL_SetRenderDrawColor(renderer, 100, 220, 120, 200);
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 50, 70, 120);
        }
        SDL_RenderDrawRect(renderer, &card);

        // Icon placeholder (checkmark or lock)
        int iconX = cardX + 12;
        int iconY = y + 10;
        if (a.unlocked) {
            // Green checkmark
            SDL_SetRenderDrawColor(renderer, 80, 220, 100, 255);
            SDL_Rect check = {iconX, iconY, 24, 24};
            SDL_RenderFillRect(renderer, &check);
            SDL_SetRenderDrawColor(renderer, 20, 60, 30, 255);
            // V shape
            SDL_RenderDrawLine(renderer, iconX + 5, iconY + 12, iconX + 10, iconY + 18);
            SDL_RenderDrawLine(renderer, iconX + 10, iconY + 18, iconX + 19, iconY + 7);
        } else {
            // Gray lock
            SDL_SetRenderDrawColor(renderer, 80, 70, 90, 200);
            SDL_Rect lock = {iconX, iconY, 24, 24};
            SDL_RenderFillRect(renderer, &lock);
            SDL_SetRenderDrawColor(renderer, 50, 40, 60, 255);
            SDL_Rect lockInner = {iconX + 6, iconY + 10, 12, 10};
            SDL_RenderFillRect(renderer, &lockInner);
        }

        // Name
        SDL_Color nameColor = a.unlocked ? SDL_Color{200, 255, 210, 255} : SDL_Color{100, 90, 110, 180};
        SDL_Surface* ns = TTF_RenderText_Blended(font, a.name.c_str(), nameColor);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {cardX + 48, y + 8, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Description
        SDL_Color descColor = a.unlocked ? SDL_Color{160, 200, 170, 200} : SDL_Color{80, 70, 90, 140};
        SDL_Surface* ds = TTF_RenderText_Blended(font, a.description.c_str(), descColor);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {cardX + 48, y + 28, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }

        // Reward text (right-aligned, golden if unlocked, gray if locked)
        if (!a.rewardDesc.empty()) {
            SDL_Color rwColor = a.unlocked ? SDL_Color{255, 220, 80, 230} : SDL_Color{120, 100, 60, 120};
            SDL_Surface* rws = TTF_RenderText_Blended(font, a.rewardDesc.c_str(), rwColor);
            if (rws) {
                SDL_Texture* rwt = SDL_CreateTextureFromSurface(renderer, rws);
                if (rwt) {
                    SDL_Rect rwr = {cardX + cardW - rws->w - 12, y + (cardH - rws->h) / 2, rws->w, rws->h};
                    SDL_RenderCopy(renderer, rwt, nullptr, &rwr);
                    SDL_DestroyTexture(rwt);
                }
                SDL_FreeSurface(rws);
            }
        }
    }

    SDL_RenderSetClipRect(renderer, nullptr);

    // Scroll indicators
    if (m_scrollOffset > 0) {
        SDL_SetRenderDrawColor(renderer, 180, 160, 220, 150);
        int cx = 640;
        SDL_RenderDrawLine(renderer, cx - 10, 92, cx, 85);
        SDL_RenderDrawLine(renderer, cx + 10, 92, cx, 85);
    }
    if (m_scrollOffset < m_maxScroll) {
        SDL_SetRenderDrawColor(renderer, 180, 160, 220, 150);
        int cx = 640;
        SDL_RenderDrawLine(renderer, cx - 10, 628, cx, 635);
        SDL_RenderDrawLine(renderer, cx + 10, 628, cx, 635);
    }

    // Navigation hint
    {
        SDL_Color hc = {140, 120, 180, 160};
        SDL_Surface* hs = TTF_RenderText_Blended(font, "[W/S] Scroll   [ESC] Back", hc);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            if (ht) {
                SDL_Rect hr = {640 - hs->w / 2, 670, hs->w, hs->h};
                SDL_RenderCopy(renderer, ht, nullptr, &hr);
                SDL_DestroyTexture(ht);
            }
            SDL_FreeSurface(hs);
        }
    }
}
