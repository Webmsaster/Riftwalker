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
    "N G + 1",  "N G + 2",  "N G + 3",  "N G + 4",  "N G + 5",
    "N G + 6",  "N G + 7",  "N G + 8",  "N G + 9",  "N G + 10"
};

static const char* s_tierDescs[] = {
    "Standard difficulty",
    "+10% enemy HP, slower entropy decay",
    "+15% enemy DMG, +1 relic choice at boss",
    "+20% elite spawn, keep weapon upgrades",
    "Boss +20% HP, extra shop slot",
    "Harder patterns, start with common relic",
    "+15% shard gain, traps +30%",
    "Abilities: 50% CD start, boss extra phase",
    "Start with 2 relics, elites get 2 modifiers",
    "Shop -20% prices, enemies +30% speed",
    "Start with legendary relic — CHAOS MODE"
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

// Reward line shown per tier
static const char* s_tierRewards[] = {
    "",
    "Reward: +20% shard bonus",
    "Reward: +40% shard bonus",
    "Reward: +60% shard bonus",
    "Reward: +80% shard bonus",
    "Reward: +100% shard bonus",
    "Reward: +120% shard bonus",
    "Reward: +150% shard bonus",
    "Reward: +180% shard bonus",
    "Reward: +200% shard bonus",
    "Reward: +250% shard bonus  |  CHAOS MASTER title"
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
        SDL_Surface* ts = TTF_RenderText_Blended(font, "N E W  G A M E +", tc);
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
        SDL_Surface* ss = TTF_RenderText_Blended(font,
            "Select challenge tier  (higher = harder + more shards)", sc);
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
        SDL_SetRenderDrawColor(renderer, 20, 16, 36, bgA);
        SDL_Rect card = {cardX, y, cardW, cardH};
        SDL_RenderFillRect(renderer, &card);

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
            SDL_Surface* bs = TTF_RenderText_Blended(font, "[NEXT CHALLENGE]", bc);
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
        SDL_Surface* ns = TTF_RenderText_Blended(font, s_tierNames[i], nameCol);
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
        SDL_Surface* ds = TTF_RenderText_Blended(font, s_tierDescs[i], descCol);
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
        if (selected && i > 0 && s_tierRewards[i][0] != '\0') {
            SDL_Color rc = {255, 215, 60, 200};
            SDL_Surface* rs = TTF_RenderText_Blended(font, s_tierRewards[i], rc);
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
        SDL_Surface* ns = TTF_RenderText_Blended(font,
            "W/S Navigate  |  ENTER Confirm  |  ESC Back", nc);
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
