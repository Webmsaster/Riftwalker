#include "ShopState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/AscensionSystem.h"
#include "UI/UITextures.h"
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <cmath>

// Layout constants (scaled for 2560x1440)
static constexpr int SHOP_CARD_W   = 440;
static constexpr int SHOP_CARD_H   = 600;
static constexpr int SHOP_CARD_GAP = 60;
static constexpr int SHOP_CARD_Y   = 260;
static constexpr int SHOP_SKIP_W   = 320;
static constexpr int SHOP_SKIP_H   = 100;

extern bool g_autoSmokeTest;
extern bool g_autoPlaytest;

void ShopState::enter() {
    // Auto-skip shop during automated test modes
    if (g_autoSmokeTest || g_autoPlaytest) {
        game->popState();
        return;
    }

    auto& buffs = game->getRunBuffSystem();
    // Difficulty from PlayState (stored in Game)
    m_offerings = buffs.generateShopOffering(game->getShopDifficulty());
    // Ascension shop discount
    if (AscensionSystem::currentLevel > 0) {
        float disc = AscensionSystem::getLevel(AscensionSystem::currentLevel).shopDiscountMult;
        for (auto& o : m_offerings) o.cost = static_cast<int>(o.cost * disc);
    }
    m_selectedIndex = 0;
    m_animTimer = 0;
    m_purchasedThisVisit = false;
}

void ShopState::exit() {
}

void ShopState::confirmSelection() {
    if (m_selectedIndex >= static_cast<int>(m_offerings.size())) {
        // Skip - continue to next level
        AudioManager::instance().play(SFX::MenuConfirm);
        game->popState();
    } else {
        // Try to purchase
        auto& buffs = game->getRunBuffSystem();
        int shards = game->getUpgradeSystem().getRiftShards();
        auto& offering = m_offerings[m_selectedIndex];
        if (shards >= offering.cost) {
            if (buffs.purchase(offering.id, shards)) {
                game->getUpgradeSystem().setRiftShards(shards);
                m_purchasedThisVisit = true;
                m_purchaseFlashTimer = 0.4f; // Visual confirmation pulse
                AudioManager::instance().play(SFX::Pickup);
                AudioManager::instance().play(SFX::LevelComplete); // celebration layer
                // Remove from offerings
                m_offerings.erase(m_offerings.begin() + m_selectedIndex);
                if (m_selectedIndex >= static_cast<int>(m_offerings.size())) {
                    m_selectedIndex = static_cast<int>(m_offerings.size());
                }
            }
        } else {
            AudioManager::instance().play(SFX::RiftFail);
        }
    }
}

void ShopState::handleEvent(const SDL_Event& event) {
    int totalOptions = static_cast<int>(m_offerings.size()) + 1; // +1 for Skip

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:
                m_selectedIndex = (m_selectedIndex - 1 + totalOptions) % totalOptions;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT:
                m_selectedIndex = (m_selectedIndex + 1) % totalOptions;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                confirmSelection();
                break;
            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->popState();
                break;
            default: break;
        }
    }

    // Mouse hover: highlight card or skip button
    if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x, my = event.motion.y;
        int totalCards = static_cast<int>(m_offerings.size());
        int cardW = SHOP_CARD_W, cardH = SHOP_CARD_H, cardGap = SHOP_CARD_GAP;
        int totalWidth = totalCards * cardW + (totalCards - 1) * cardGap;
        int startX = (Game::SCREEN_WIDTH - totalWidth) / 2;
        int cardY = SHOP_CARD_Y;

        int newSel = -1;
        for (int i = 0; i < totalCards; i++) {
            int cx = startX + i * (cardW + cardGap);
            if (mx >= cx && mx < cx + cardW && my >= cardY && my < cardY + cardH) {
                newSel = i;
                break;
            }
        }
        if (newSel == -1) {
            // Check skip button
            int skipW = SHOP_SKIP_W, skipH = SHOP_SKIP_H;
            int skipX = SCREEN_WIDTH / 2 - skipW / 2;
            int skipY = cardY + cardH + 100;
            if (mx >= skipX && mx < skipX + skipW && my >= skipY && my < skipY + skipH)
                newSel = totalCards; // skip index
        }
        if (newSel >= 0 && newSel != m_selectedIndex) {
            m_selectedIndex = newSel;
            AudioManager::instance().play(SFX::MenuSelect);
        }
    }

    // Mouse wheel scrolling through shop items
    if (event.type == SDL_MOUSEWHEEL) {
        int totalOptions = static_cast<int>(m_offerings.size()) + 1;
        if (event.wheel.y > 0)
            m_selectedIndex = (m_selectedIndex - 1 + totalOptions) % totalOptions;
        else if (event.wheel.y < 0)
            m_selectedIndex = (m_selectedIndex + 1) % totalOptions;
        AudioManager::instance().play(SFX::MenuSelect);
    }

    // Right-click to skip
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->popState();
        return;
    }

    // Mouse click: select + confirm
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        int totalCards = static_cast<int>(m_offerings.size());
        int cardW = SHOP_CARD_W, cardH = SHOP_CARD_H, cardGap = SHOP_CARD_GAP;
        int totalWidth = totalCards * cardW + (totalCards - 1) * cardGap;
        int startX = (Game::SCREEN_WIDTH - totalWidth) / 2;
        int cardY = SHOP_CARD_Y;

        int clicked = -1;
        for (int i = 0; i < totalCards; i++) {
            int cx = startX + i * (cardW + cardGap);
            if (mx >= cx && mx < cx + cardW && my >= cardY && my < cardY + cardH) {
                clicked = i;
                break;
            }
        }
        if (clicked == -1) {
            int skipW = SHOP_SKIP_W, skipH = SHOP_SKIP_H;
            int skipX = SCREEN_WIDTH / 2 - skipW / 2;
            int skipY = cardY + cardH + 100;
            if (mx >= skipX && mx < skipX + skipW && my >= skipY && my < skipY + skipH)
                clicked = totalCards;
        }
        if (clicked >= 0) {
            m_selectedIndex = clicked;
            confirmSelection();
        }
    }
}

void ShopState::update(float dt) {
    m_animTimer += dt;
    if (m_purchaseFlashTimer > 0) m_purchaseFlashTimer -= dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    int totalOptions = static_cast<int>(m_offerings.size()) + 1;
    if (input.isActionPressed(Action::MenuLeft)) {
        m_selectedIndex = (m_selectedIndex - 1 + totalOptions) % totalOptions;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuRight)) {
        m_selectedIndex = (m_selectedIndex + 1) % totalOptions;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Confirm)) {
        confirmSelection();
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->popState();
    }
}

void ShopState::render(SDL_Renderer* renderer) {
    TTF_Font* font = game->getFont();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Purchase flash: additive golden overlay pulse on successful buy
    if (m_purchaseFlashTimer > 0) {
        float flashT = m_purchaseFlashTimer / 0.4f;
        Uint8 flashA = static_cast<Uint8>(60 * flashT);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(renderer, 255, 200, 80, flashA);
        SDL_Rect fullscreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fullscreen);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

    // Dark background with rift pattern
    SDL_SetRenderDrawColor(renderer, 12, 8, 24, 255);
    SDL_Rect fullScreen = {0, 0, Game::SCREEN_WIDTH, Game::SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &fullScreen);

    // Animated background particles
    Uint32 ticks = SDL_GetTicks();
    for (int i = 0; i < 30; i++) {
        float speed = 0.3f + (i % 5) * 0.15f;
        int bx = ((i * 4513 + 23143 + static_cast<int>(m_animTimer * 20 * speed)) % SCREEN_WIDTH);
        int by = ((i * 3251 + 51749 + static_cast<int>(m_animTimer * 10 * speed)) % SCREEN_HEIGHT);
        float pulse = 0.3f + 0.3f * std::sin(ticks * 0.002f + i * 1.3f);
        Uint8 pa = static_cast<Uint8>(30 * pulse);
        SDL_SetRenderDrawColor(renderer, 120, 80, 200, pa);
        SDL_Rect p = {bx, by, 2 + i % 3, 2 + i % 3};
        SDL_RenderFillRect(renderer, &p);
    }

    // Title
    if (font) {
        SDL_Color titleColor = {200, 150, 255, 255};
        SDL_Surface* titleSurf = TTF_RenderUTF8_Blended(font, LOC("shop.title"), titleColor);
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            if (titleTex) {
                SDL_Rect tr = {SCREEN_WIDTH / 2 - titleSurf->w / 2, 80, titleSurf->w, titleSurf->h};
                SDL_RenderCopy(renderer, titleTex, nullptr, &tr);
                SDL_DestroyTexture(titleTex);
            }
            SDL_FreeSurface(titleSurf);
        }

        // Shards display
        char shardText[64];
        std::snprintf(shardText, sizeof(shardText), LOC("menu.rift_shards"),
                     game->getUpgradeSystem().getRiftShards());
        SDL_Color shardColor = {200, 170, 255, 255};
        SDL_Surface* ss = TTF_RenderUTF8_Blended(font, shardText, shardColor);
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_Rect sr = {SCREEN_WIDTH / 2 - ss->w / 2, 150, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &sr);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }
    }

    // Cards layout
    int totalCards = static_cast<int>(m_offerings.size());
    int cardW = SHOP_CARD_W, cardH = SHOP_CARD_H;
    int cardGap = SHOP_CARD_GAP;
    int totalWidth = totalCards * cardW + (totalCards - 1) * cardGap;
    int startX = (Game::SCREEN_WIDTH - totalWidth) / 2;
    int cardY = SHOP_CARD_Y;
    int shards = game->getUpgradeSystem().getRiftShards();

    for (int i = 0; i < totalCards; i++) {
        int cx = startX + i * (cardW + cardGap);
        bool selected = (m_selectedIndex == i);
        bool affordable = (shards >= m_offerings[i].cost);
        renderCard(renderer, m_offerings[i], cx, cardY, cardW, cardH, selected, affordable);
    }

    // Skip button
    {
        bool skipSelected = (m_selectedIndex >= totalCards);
        int skipW = SHOP_SKIP_W, skipH = SHOP_SKIP_H;
        int skipX = SCREEN_WIDTH / 2 - skipW / 2;
        int skipY = cardY + cardH + 100;

        SDL_SetRenderDrawColor(renderer, skipSelected ? 60 : 30, skipSelected ? 50 : 25,
                               skipSelected ? 80 : 40, 220);
        SDL_Rect skipBg = {skipX, skipY, skipW, skipH};
        SDL_RenderFillRect(renderer, &skipBg);

        SDL_SetRenderDrawColor(renderer, skipSelected ? 200 : 100, skipSelected ? 180 : 80,
                               skipSelected ? 255 : 150, 255);
        SDL_RenderDrawRect(renderer, &skipBg);

        if (skipSelected) {
            float pulse = 0.5f + 0.5f * std::sin(ticks * 0.006f);
            SDL_Rect glow = {skipX - 2, skipY - 2, skipW + 4, skipH + 4};
            SDL_SetRenderDrawColor(renderer, 180, 150, 255, static_cast<Uint8>(80 * pulse));
            SDL_RenderDrawRect(renderer, &glow);
        }

        if (font) {
            SDL_Color sc = skipSelected ? SDL_Color{255, 255, 255, 255} : SDL_Color{150, 150, 170, 200};
            SDL_Surface* ss = TTF_RenderUTF8_Blended(font, LOC("shop.skip"), sc);
            if (ss) {
                SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
                if (st) {
                    SDL_Rect sr = {skipX + skipW / 2 - ss->w / 2, skipY + skipH / 2 - ss->h / 2,
                                   ss->w, ss->h};
                    SDL_RenderCopy(renderer, st, nullptr, &sr);
                    SDL_DestroyTexture(st);
                }
                SDL_FreeSurface(ss);
            }
        }
    }

    // First-run tutorial hint: explain the shop on the very first visit
    if (font && game->getUpgradeSystem().totalRuns == 0 && !m_purchasedThisVisit) {
        float pulse = 0.6f + 0.4f * std::sin(ticks * 0.004f);
        Uint8 ta = static_cast<Uint8>(220 * pulse);
        // Semi-transparent bar above cards
        SDL_SetRenderDrawColor(renderer, 10, 5, 30, 160);
        SDL_Rect tutBg = {400, 200, 1760, 56};
        SDL_RenderFillRect(renderer, &tutBg);
        SDL_SetRenderDrawColor(renderer, 140, 100, 220, ta);
        SDL_RenderDrawRect(renderer, &tutBg);

        SDL_Color tutColor = {180, 220, 255, ta};
        SDL_Surface* ts = TTF_RenderUTF8_Blended(font,
            LOC("shop.tutorial"), tutColor);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_SetTextureAlphaMod(tt, ta);
                SDL_Rect tr2 = {SCREEN_WIDTH / 2 - ts->w / 2, 208, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr2);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }

    // Instructions at bottom
    if (font) {
        SDL_Color hintColor = {100, 100, 120, 150};
        SDL_Surface* hs = TTF_RenderUTF8_Blended(font, LOC("shop.nav_hint"), hintColor);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            if (ht) {
                SDL_Rect hr = {SCREEN_WIDTH / 2 - hs->w / 2, SCREEN_HEIGHT - 60, hs->w, hs->h};
                SDL_RenderCopy(renderer, ht, nullptr, &hr);
                SDL_DestroyTexture(ht);
            }
            SDL_FreeSurface(hs);
        }
    }
}

void ShopState::renderCard(SDL_Renderer* renderer, const RunBuff& buff, int x, int y,
                            int w, int h, bool selected, bool affordable) {
    TTF_Font* font = game->getFont();
    if (!font) return;
    Uint32 ticks = SDL_GetTicks();

    // Tier colors
    SDL_Color tierColor;
    const char* tierLabel;
    switch (buff.tier) {
        case BuffTier::Common:    tierColor = {80, 140, 255, 255}; tierLabel = LOC("shop.common"); break;
        case BuffTier::Rare:      tierColor = {180, 80, 255, 255}; tierLabel = LOC("shop.rare"); break;
        case BuffTier::Legendary: tierColor = {255, 200, 50, 255}; tierLabel = LOC("shop.legendary"); break;
        default: tierColor = {180, 180, 180, 255}; tierLabel = "???"; break;
    }

    // Selected bob animation — apply before drawing so frame and content move together
    if (selected) {
        y -= static_cast<int>(std::sin(SDL_GetTicks() * 0.005f) * 4);
    }

    // Card background (texture-based)
    SDL_Rect bg = {x, y, w, h};
    if (!renderPanelBg(renderer, bg, 240, "assets/textures/ui/panel_light.png")) {
        SDL_SetRenderDrawColor(renderer, 20, 15, 35, 240);
        SDL_RenderFillRect(renderer, &bg);
    }

    // Tier-colored border
    Uint8 borderA = selected ? 255 : 120;
    SDL_SetRenderDrawColor(renderer, tierColor.r, tierColor.g, tierColor.b, borderA);
    SDL_RenderDrawRect(renderer, &bg);
    SDL_Rect inner = {x + 1, y + 1, w - 2, h - 2};
    SDL_RenderDrawRect(renderer, &inner);

    // Selected glow
    if (selected) {
        float pulse = 0.5f + 0.5f * std::sin(ticks * 0.006f);
        Uint8 ga = static_cast<Uint8>(60 + 40 * pulse);
        SDL_SetRenderDrawColor(renderer, tierColor.r, tierColor.g, tierColor.b, ga);
        SDL_Rect glow = {x - 3, y - 3, w + 6, h + 6};
        SDL_RenderDrawRect(renderer, &glow);
        SDL_Rect glow2 = {x - 5, y - 5, w + 10, h + 10};
        SDL_SetRenderDrawColor(renderer, tierColor.r, tierColor.g, tierColor.b, static_cast<Uint8>(ga / 2));
        SDL_RenderDrawRect(renderer, &glow2);
    }

    // Tier header bar
    SDL_Rect headerBar = {x + 4, y + 4, w - 8, 48};
    SDL_SetRenderDrawColor(renderer, tierColor.r / 3, tierColor.g / 3, tierColor.b / 3, 200);
    SDL_RenderFillRect(renderer, &headerBar);

    if (font) {
        // Tier label
        SDL_Color tc = {tierColor.r, tierColor.g, tierColor.b, 220};
        SDL_Surface* ts = TTF_RenderUTF8_Blended(font, tierLabel, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {x + w / 2 - ts->w / 2, y + 10, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }

        // Buff icon area (procedural)
        int iconCX = x + w / 2;
        int iconCY = y + 160;
        int iconR = 50;

        // Icon circle background
        SDL_SetRenderDrawColor(renderer, tierColor.r / 4, tierColor.g / 4, tierColor.b / 4, 180);
        SDL_Rect iconBg = {iconCX - iconR, iconCY - iconR, iconR * 2, iconR * 2};
        SDL_RenderFillRect(renderer, &iconBg);
        SDL_SetRenderDrawColor(renderer, tierColor.r, tierColor.g, tierColor.b, 100);
        SDL_RenderDrawRect(renderer, &iconBg);

        // Simple icon inside
        SDL_SetRenderDrawColor(renderer, tierColor.r, tierColor.g, tierColor.b, 200);
        switch (buff.id) {
            case RunBuffID::MaxHPBoost:
                // Heart shape (two rects + triangle)
                SDL_RenderDrawLine(renderer, iconCX, iconCY - 16, iconCX - 16, iconCY);
                SDL_RenderDrawLine(renderer, iconCX - 16, iconCY, iconCX, iconCY + 20);
                SDL_RenderDrawLine(renderer, iconCX, iconCY + 20, iconCX + 16, iconCY);
                SDL_RenderDrawLine(renderer, iconCX + 16, iconCY, iconCX, iconCY - 16);
                break;
            case RunBuffID::FireWeapon:
                // Flame
                SDL_SetRenderDrawColor(renderer, 255, 120, 30, 200);
                SDL_RenderDrawLine(renderer, iconCX, iconCY - 20, iconCX - 10, iconCY + 10);
                SDL_RenderDrawLine(renderer, iconCX - 10, iconCY + 10, iconCX, iconCY + 4);
                SDL_RenderDrawLine(renderer, iconCX, iconCY + 4, iconCX + 10, iconCY + 10);
                SDL_RenderDrawLine(renderer, iconCX + 10, iconCY + 10, iconCX, iconCY - 20);
                break;
            default:
                // Generic diamond
                SDL_RenderDrawLine(renderer, iconCX, iconCY - 20, iconCX + 20, iconCY);
                SDL_RenderDrawLine(renderer, iconCX + 20, iconCY, iconCX, iconCY + 20);
                SDL_RenderDrawLine(renderer, iconCX, iconCY + 20, iconCX - 20, iconCY);
                SDL_RenderDrawLine(renderer, iconCX - 20, iconCY, iconCX, iconCY - 20);
                break;
        }

        // Buff name (localized)
        char buffNameKey[32], buffDescKey[32];
        std::snprintf(buffNameKey, sizeof(buffNameKey), "buff.%d.name", static_cast<int>(buff.id));
        std::snprintf(buffDescKey, sizeof(buffDescKey), "buff.%d.desc", static_cast<int>(buff.id));
        SDL_Color nc = {255, 255, 255, 240};
        SDL_Surface* ns = TTF_RenderUTF8_Blended(font, LOC(buffNameKey), nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {x + w / 2 - ns->w / 2, y + 240, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Description (localized)
        SDL_Color dc = {180, 180, 200, 200};
        SDL_Surface* ds = TTF_RenderUTF8_Blended(font, LOC(buffDescKey), dc);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {x + w / 2 - ds->w / 2, y + 300, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }

        // Divider line
        SDL_SetRenderDrawColor(renderer, tierColor.r / 2, tierColor.g / 2, tierColor.b / 2, 100);
        SDL_RenderDrawLine(renderer, x + 40, y + 400, x + w - 40, y + 400);

        // Cost
        char costText[32];
        std::snprintf(costText, sizeof(costText), LOC("shop.cost"), buff.cost);
        SDL_Color cc = affordable ? SDL_Color{200, 170, 255, 255} : SDL_Color{200, 60, 60, 255};
        SDL_Surface* cs = TTF_RenderUTF8_Blended(font, costText, cc);
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_Rect cr = {x + w / 2 - cs->w / 2, y + 440, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }

        // Buy hint for selected
        if (selected && affordable) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color bc = {180, 255, 180, static_cast<Uint8>(180 * blink)};
            SDL_Surface* bs = TTF_RenderUTF8_Blended(font, LOC("shop.buy"), bc);
            if (bs) {
                SDL_Texture* bt = SDL_CreateTextureFromSurface(renderer, bs);
                if (bt) {
                    SDL_Rect br = {x + w / 2 - bs->w / 2, y + h - 80, bs->w, bs->h};
                    SDL_RenderCopy(renderer, bt, nullptr, &br);
                    SDL_DestroyTexture(bt);
                }
                SDL_FreeSurface(bs);
            }
        } else if (selected && !affordable) {
            SDL_Color errColor = {200, 80, 80, 180};
            SDL_Surface* errSurf = TTF_RenderUTF8_Blended(font, LOC("shop.no_shards"), errColor);
            if (errSurf) {
                SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, errSurf);
                if (nt) {
                    SDL_Rect nr = {x + w / 2 - errSurf->w / 2, y + h - 80, errSurf->w, errSurf->h};
                    SDL_RenderCopy(renderer, nt, nullptr, &nr);
                    SDL_DestroyTexture(nt);
                }
                SDL_FreeSurface(errSurf);
            }
        }
    }
}
