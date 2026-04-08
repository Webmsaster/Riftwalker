#include "NGPlusSelectState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/UpgradeSystem.h"
#include "UI/UITextures.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// Tier name/desc/reward keys — looked up via LOC() at render time
static const char* s_tierNameKeys[] = {
    "ngplus.tier.0", "ngplus.tier.1", "ngplus.tier.2", "ngplus.tier.3",
    "ngplus.tier.4", "ngplus.tier.5", "ngplus.tier.6", "ngplus.tier.7",
    "ngplus.tier.8", "ngplus.tier.9", "ngplus.tier.10"
};

static const char* s_tierDescKeys[] = {
    "ngplus.desc.0", "ngplus.desc.1", "ngplus.desc.2", "ngplus.desc.3",
    "ngplus.desc.4", "ngplus.desc.5", "ngplus.desc.6", "ngplus.desc.7",
    "ngplus.desc.8", "ngplus.desc.9", "ngplus.desc.10"
};

static SDL_Color s_tierColors[] = {
    {180, 180, 180, 255},
    {140, 220, 100, 255},
    {80,  200, 220, 255},
    {200, 140,  80, 255},
    {220,  80,  80, 255},
    {200,  60, 255, 255},
    {60,  200,  60, 255},
    {200, 200,  60, 255},
    {255, 120,  60, 255},
    {255,  60,  60, 255},
    {255,  40, 200, 255}
};

// Reward keys (tier 0 has no reward)
static const char* s_tierRewardKeys[] = {
    nullptr,
    "ngplus.reward.1", "ngplus.reward.2", "ngplus.reward.3",
    "ngplus.reward.4", "ngplus.reward.5", "ngplus.reward.6",
    "ngplus.reward.7", "ngplus.reward.8", "ngplus.reward.9",
    "ngplus.reward.10"
};

void NGPlusSelectState::enter() {
    m_maxTier = game->getUpgradeSystem().getMaxUnlockedNGPlus();
    // Clamp selection to the next unlockable tier (maxTier + 1, capped at 10)
    m_selected = std::min(m_maxTier + 1, 10); // default to next challenge
    if (m_selected > m_maxTier + 1) m_selected = m_maxTier + 1;
    m_time = 0;
}

void NGPlusSelectState::handleConfirm() {
    if (m_selected > m_maxTier) {
        AudioManager::instance().play(SFX::RiftFail);
        return;
    }
    g_selectedNGPlus = m_selected;
    AudioManager::instance().play(SFX::MenuConfirm);
    game->changeState(StateID::Play);
}

void NGPlusSelectState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_selected = (m_selected - 1 + (m_maxTier + 2)) % (m_maxTier + 2);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_selected = (m_selected + 1) % (m_maxTier + 2);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                handleConfirm();
                break;
            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::DifficultySelect);
                break;
            default: break;
        }
    }

    // Card layout mirrors render() calculations exactly
    int totalOptions = std::min(m_maxTier + 2, 11);
    int cardW        = 1400;
    int cardH        = (totalOptions > 6) ? 100 : 156;
    int cardX        = SCREEN_WIDTH / 2 - cardW / 2;
    int startY       = 310;

    if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x, my = event.motion.y;
        for (int i = 0; i < totalOptions; i++) {
            int y = startY + i * (cardH + 16);
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
            m_selected = (m_selected - 1 + totalOptions) % totalOptions;
        else if (event.wheel.y < 0)
            m_selected = (m_selected + 1) % totalOptions;
        AudioManager::instance().play(SFX::MenuSelect);
    }

    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::DifficultySelect);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        for (int i = 0; i < totalOptions; i++) {
            int y = startY + i * (cardH + 16);
            if (mx >= cardX && mx < cardX + cardW && my >= y && my < y + cardH) {
                m_selected = i;
                handleConfirm();
                break;
            }
        }
    }
}

void NGPlusSelectState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    if (input.isActionPressed(Action::MenuUp)) {
        m_selected = (m_selected - 1 + (m_maxTier + 2)) % (m_maxTier + 2);
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_selected = (m_selected + 1) % (m_maxTier + 2);
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Confirm)) {
        handleConfirm();
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::DifficultySelect);
    }
}

void NGPlusSelectState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 8, 5, 18, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Subtle grid background
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 18);
    for (int x = 0; x < SCREEN_WIDTH; x += 160)
        SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 160)
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Gold animated title
    {
        float pulse = 0.85f + 0.15f * std::sin(m_time * 3.0f);
        SDL_Color tc = {
            static_cast<Uint8>(255 * pulse),
            static_cast<Uint8>(200 * pulse),
            static_cast<Uint8>(50 * pulse),
            255
        };
        SDL_Surface* ts = TTF_RenderUTF8_Blended(font, LOC("ngplus.title"), tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                int tw = static_cast<int>(ts->w * 1.8f);
                int th = static_cast<int>(ts->h * 1.8f);
                SDL_Rect r = {SCREEN_WIDTH / 2 - tw / 2, 120, tw, th};
                SDL_RenderCopy(renderer, tt, nullptr, &r);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }

    // Subtitle
    {
        SDL_Color sc = {120, 100, 160, 180};
        SDL_Surface* ss = TTF_RenderUTF8_Blended(font,
            LOC("ngplus.subtitle"), sc);
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_Rect r = {SCREEN_WIDTH / 2 - ss->w / 2, 118, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &r);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }
    }

    // Option cards — Normal + unlocked NG+ tiers + one locked next tier
    int cardW = 1400;
    int cardH = 156;
    int cardX = SCREEN_WIDTH / 2 - cardW / 2;
    int startY = 310;
    int totalOptions = m_maxTier + 2; // Normal + all unlocked + one locked (next)
    if (totalOptions > 11) totalOptions = 11;

    for (int i = 0; i < totalOptions; i++) {
        bool unlocked = (i <= m_maxTier + 1) && (i <= 10);
        bool isNextChallenge = (i == m_maxTier + 1) && (i <= 10);
        bool selected = (i == m_selected);
        int y = startY + i * (cardH + 8);

        SDL_Color col = s_tierColors[i];

        // Card background
        Uint8 bgA = selected ? 90 : (unlocked ? 28 : 15);
        SDL_Rect card = {cardX, y, cardW, cardH};
        if (!renderPanelBg(renderer, card, bgA, selected ? "assets/textures/ui/panel_light.png" : "assets/textures/ui/panel_dark.png")) {
            SDL_SetRenderDrawColor(renderer, 20, 16, 36, bgA);
            SDL_RenderFillRect(renderer, &card);
        }

        // Left accent bar
        Uint8 accentA = selected ? static_cast<Uint8>(240) : (unlocked ? static_cast<Uint8>(70) : static_cast<Uint8>(25));
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, accentA);
        SDL_Rect accent = {cardX, y, 10, cardH};
        SDL_RenderFillRect(renderer, &accent);

        // Selection pulsing border
        if (selected) {
            float p = 0.6f + 0.4f * std::sin(m_time * 4.5f);
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(200 * p));
            SDL_RenderDrawRect(renderer, &card);
        }

        // "NEXT CHALLENGE" badge for the next unlockable tier
        if (isNextChallenge && !unlocked) {
            SDL_Color bc = {255, 200, 50, 180};
            SDL_Surface* bs = TTF_RenderUTF8_Blended(font, LOC("ngplus.next_challenge"), bc);
            if (bs) {
                SDL_Texture* bt = SDL_CreateTextureFromSurface(renderer, bs);
                if (bt) {
                    SDL_Rect br = {cardX + cardW - bs->w - 24, y + 12, bs->w, bs->h};
                    SDL_RenderCopy(renderer, bt, nullptr, &br);
                    SDL_DestroyTexture(bt);
                }
                SDL_FreeSurface(bs);
            }
        }

        // Tier name
        Uint8 nameAlpha = unlocked ? 255 : static_cast<Uint8>(90);
        SDL_Color nameCol = {col.r, col.g, col.b, nameAlpha};
        if (!selected) {
            nameCol.r = static_cast<Uint8>(nameCol.r * 0.6f);
            nameCol.g = static_cast<Uint8>(nameCol.g * 0.6f);
            nameCol.b = static_cast<Uint8>(nameCol.b * 0.6f);
        }
        SDL_Surface* ns = TTF_RenderUTF8_Blended(font, LOC(s_tierNameKeys[i]), nameCol);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                int nw = static_cast<int>(ns->w * 1.25f);
                int nh = static_cast<int>(ns->h * 1.25f);
                SDL_Rect nr = {cardX + 32, y + 20, nw, nh};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Description
        SDL_Color descCol = selected ? SDL_Color{175, 170, 195, 220} : SDL_Color{85, 80, 105, 130};
        SDL_Surface* ds = TTF_RenderUTF8_Blended(font, LOC(s_tierDescKeys[i]), descCol);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {cardX + 36, y + 88, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }

        // Reward line (on selected card only, right-aligned)
        if (selected && i > 0 && s_tierRewardKeys[i]) {
            SDL_Color rc = {255, 215, 60, 200};
            SDL_Surface* rs = TTF_RenderUTF8_Blended(font, LOC(s_tierRewardKeys[i]), rc);
            if (rs) {
                SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
                if (rt) {
                    SDL_Rect rr = {cardX + cardW - rs->w - 20, y + cardH - rs->h - 12, rs->w, rs->h};
                    SDL_RenderCopy(renderer, rt, nullptr, &rr);
                    SDL_DestroyTexture(rt);
                }
                SDL_FreeSurface(rs);
            }
        }
    }

    // Navigation hint
    {
        SDL_Color nc = {60, 55, 85, 130};
        SDL_Surface* ns = TTF_RenderUTF8_Blended(font,
            LOC("ngplus.nav_hint"), nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {SCREEN_WIDTH / 2 - ns->w / 2, SCREEN_HEIGHT - 50, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }
}
