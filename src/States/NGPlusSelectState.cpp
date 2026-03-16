#include "NGPlusSelectState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Game/UpgradeSystem.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// Per-tier descriptor strings
static const char* s_tierNames[] = {
    "N O R M A L",
    "N G + 1",
    "N G + 2",
    "N G + 3",
    "N G + 4",
    "N G + 5"
};

static const char* s_tierDescs[] = {
    "Standard difficulty",
    "+25% enemy HP, +15% DMG, slower entropy decay",
    "All above + double elite rate, boss phases faster",
    "All above + +50% total HP, hazard damage +50%",
    "All above + forced dim-switch every 30s, mini-bosses everywhere",
    "All above + +100% total HP, PERMADEATH (no revives)"
};

static SDL_Color s_tierColors[] = {
    {180, 180, 180, 255},
    {140, 220, 100, 255},
    {80,  200, 220, 255},
    {200, 140,  80, 255},
    {220,  80,  80, 255},
    {200,  60, 255, 255}
};

// Reward line shown per tier
static const char* s_tierRewards[] = {
    "",
    "Reward: +20% shard bonus",
    "Reward: +40% shard bonus",
    "Reward: +60% shard bonus",
    "Reward: +80% shard bonus",
    "Reward: +100% shard bonus  |  Exclusive NG+5 title"
};

void NGPlusSelectState::enter() {
    m_maxTier = game->getUpgradeSystem().getMaxUnlockedNGPlus();
    // Clamp selection to the next unlockable tier (maxTier + 1, capped at 5)
    m_selected = std::min(m_maxTier + 1, 5); // default to next challenge
    if (m_selected > m_maxTier + 1) m_selected = m_maxTier + 1;
    m_time = 0;
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
                g_selectedNGPlus = m_selected;
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::Play);
                break;
            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::DifficultySelect);
                break;
            default: break;
        }
    }
}

void NGPlusSelectState::update(float dt) {
    m_time += dt;
}

void NGPlusSelectState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 8, 5, 18, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Subtle grid background
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 18);
    for (int x = 0; x < SCREEN_WIDTH; x += 80)
        SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 80)
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
        SDL_Surface* ts = TTF_RenderText_Blended(font, "N E W  G A M E +", tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                int tw = static_cast<int>(ts->w * 1.8f);
                int th = static_cast<int>(ts->h * 1.8f);
                SDL_Rect r = {640 - tw / 2, 60, tw, th};
                SDL_RenderCopy(renderer, tt, nullptr, &r);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }

    // Subtitle
    {
        SDL_Color sc = {120, 100, 160, 180};
        SDL_Surface* ss = TTF_RenderText_Blended(font,
            "Select challenge tier  (higher = harder + more shards)", sc);
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_Rect r = {640 - ss->w / 2, 118, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &r);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }
    }

    // Option cards — Normal + unlocked NG+ tiers + one locked next tier
    int cardW = 700;
    int cardH = 78;
    int cardX = 640 - cardW / 2;
    int startY = 155;
    int totalOptions = m_maxTier + 2; // Normal + all unlocked + one locked (next)
    if (totalOptions > 6) totalOptions = 6;

    for (int i = 0; i < totalOptions; i++) {
        bool unlocked = (i <= m_maxTier + 1) && (i <= 5);
        bool isNextChallenge = (i == m_maxTier + 1) && (i <= 5);
        bool selected = (i == m_selected);
        int y = startY + i * (cardH + 8);

        SDL_Color col = s_tierColors[i];

        // Card background
        Uint8 bgA = selected ? 90 : (unlocked ? 28 : 15);
        SDL_SetRenderDrawColor(renderer, 20, 16, 36, bgA);
        SDL_Rect card = {cardX, y, cardW, cardH};
        SDL_RenderFillRect(renderer, &card);

        // Left accent bar
        Uint8 accentA = selected ? static_cast<Uint8>(240) : (unlocked ? static_cast<Uint8>(70) : static_cast<Uint8>(25));
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, accentA);
        SDL_Rect accent = {cardX, y, 5, cardH};
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
            SDL_Surface* bs = TTF_RenderText_Blended(font, "[NEXT CHALLENGE]", bc);
            if (bs) {
                SDL_Texture* bt = SDL_CreateTextureFromSurface(renderer, bs);
                if (bt) {
                    SDL_Rect br = {cardX + cardW - bs->w - 12, y + 6, bs->w, bs->h};
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
        SDL_Surface* ns = TTF_RenderText_Blended(font, s_tierNames[i], nameCol);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                int nw = static_cast<int>(ns->w * 1.25f);
                int nh = static_cast<int>(ns->h * 1.25f);
                SDL_Rect nr = {cardX + 16, y + 10, nw, nh};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Description
        SDL_Color descCol = selected ? SDL_Color{175, 170, 195, 220} : SDL_Color{85, 80, 105, 130};
        SDL_Surface* ds = TTF_RenderText_Blended(font, s_tierDescs[i], descCol);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {cardX + 18, y + 44, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }

        // Reward line (on selected card only, right-aligned)
        if (selected && i > 0 && s_tierRewards[i][0] != '\0') {
            SDL_Color rc = {255, 215, 60, 200};
            SDL_Surface* rs = TTF_RenderText_Blended(font, s_tierRewards[i], rc);
            if (rs) {
                SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
                if (rt) {
                    SDL_Rect rr = {cardX + cardW - rs->w - 10, y + cardH - rs->h - 6, rs->w, rs->h};
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
        SDL_Surface* ns = TTF_RenderText_Blended(font,
            "W/S Navigate  |  ENTER Confirm  |  ESC Back", nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {640 - ns->w / 2, 670, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }
}
