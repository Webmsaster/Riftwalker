#include "PauseState.h"
#include "PlayState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "UI/UITextures.h"
#include "Game/WeaponSystem.h"
#include "Game/RelicSystem.h"
#include "Game/ClassSystem.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Layout constants (scaled for logical resolution)
static constexpr int PANEL_X      = 120;
static constexpr int PANEL_Y      = 360;
static constexpr int PANEL_W      = 2320;
static constexpr int PANEL_H      = 920;
static constexpr int BOTTOM_BAR_Y = GameState::SCREEN_HEIGHT - 3;

void PauseState::clearConfirmsExcept(int keepIdx) {
    if (keepIdx != 1 && m_confirmRestart) {
        m_confirmRestart = false;
        m_buttons[1].setText(LOC("pause.restart"));
    }
    if (keepIdx != 4 && m_confirmAbandon) {
        m_confirmAbandon = false;
        m_buttons[4].setText(LOC("pause.abandon"));
    }
}

void PauseState::enter() {
    int cx = SCREEN_WIDTH / 2;
    int startY = 600;
    int btnW = 480;
    int btnH = 88;
    int gap = 20;

    m_buttons.clear();
    m_buttons.emplace_back(cx - btnW / 2, startY, btnW, btnH, LOC("pause.resume"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 1, btnW, btnH, LOC("pause.restart"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 2, btnW, btnH, LOC("pause.daily_leaderboard"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 3, btnW, btnH, LOC("pause.options"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 4, btnW, btnH, LOC("pause.abandon"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 5, btnW, btnH, LOC("pause.quit_menu"));

    m_buttons[0].onClick = [this]() { clearConfirmsExcept(-1); game->popState(); };
    m_buttons[1].onClick = [this]() {
        clearConfirmsExcept(1); // cancel any pending Abandon confirm
        if (!m_confirmRestart) {
            m_confirmRestart = true;
            m_buttons[1].setText(LOC("pause.restart_confirm"));
            return;
        }
        if (auto* playState = dynamic_cast<PlayState*>(game->getState(StateID::Play))) {
            playState->requestRestart();
            return;
        }
        game->changeState(StateID::Menu);
    };
    m_buttons[2].onClick = [this]() { clearConfirmsExcept(-1); game->pushState(StateID::DailyLeaderboard); };
    m_buttons[3].onClick = [this]() { clearConfirmsExcept(-1); game->pushState(StateID::Options); };
    m_buttons[4].onClick = [this]() {
        clearConfirmsExcept(4); // cancel any pending Restart confirm
        if (!m_confirmAbandon) {
            m_confirmAbandon = true;
            m_buttons[4].setText(LOC("pause.abandon_confirm"));
            return;
        }
        if (auto* playState = dynamic_cast<PlayState*>(game->getState(StateID::Play))) {
            playState->abandonRun();
            return;
        }
        game->changeState(StateID::Menu);
    };
    m_buttons[5].onClick = [this]() { clearConfirmsExcept(-1); game->changeState(StateID::Menu); };

    m_selectedButton = 0;
    m_buttons[0].setSelected(true);
    m_confirmRestart = false;
    m_slideInTimer = 0;
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
                clearConfirmsExcept(-1);
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_buttons[m_selectedButton].setSelected(false);
                m_selectedButton = (m_selectedButton + 1) % static_cast<int>(m_buttons.size());
                m_buttons[m_selectedButton].setSelected(true);
                AudioManager::instance().play(SFX::MenuSelect);
                clearConfirmsExcept(-1);
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                AudioManager::instance().play(SFX::MenuConfirm);
                if (m_buttons[m_selectedButton].onClick)
                    m_buttons[m_selectedButton].onClick();
                break;
            default: break;
        }
    }

    if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x, my = event.motion.y;
        for (int i = 0; i < static_cast<int>(m_buttons.size()); i++) {
            if (m_buttons[i].isHovered(mx, my)) {
                if (i != m_selectedButton) {
                    m_buttons[m_selectedButton].setSelected(false);
                    m_selectedButton = i;
                    m_buttons[m_selectedButton].setSelected(true);
                    AudioManager::instance().play(SFX::MenuSelect);
                    if (m_confirmAbandon) { m_confirmAbandon = false; m_buttons[4].setText(LOC("pause.abandon")); }
                    if (m_confirmRestart) { m_confirmRestart = false; m_buttons[1].setText(LOC("pause.restart")); }
                }
                break;
            }
        }
    }

    // Mouse wheel scrolling through pause menu items
    if (event.type == SDL_MOUSEWHEEL) {
        int btnCount = static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(false);
        if (event.wheel.y > 0)
            m_selectedButton = (m_selectedButton - 1 + btnCount) % btnCount;
        else if (event.wheel.y < 0)
            m_selectedButton = (m_selectedButton + 1) % btnCount;
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
        clearConfirmsExcept(-1);
    }

    // Right-click to resume (same as ESC)
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->popState();
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mx = event.button.x, my = event.button.y;
        for (int i = 0; i < static_cast<int>(m_buttons.size()); i++) {
            if (m_buttons[i].isHovered(mx, my)) {
                m_buttons[m_selectedButton].setSelected(false);
                m_selectedButton = i;
                m_buttons[m_selectedButton].setSelected(true);
                AudioManager::instance().play(SFX::MenuConfirm);
                if (m_buttons[m_selectedButton].onClick)
                    m_buttons[m_selectedButton].onClick();
                break;
            }
        }
    }
}

void PauseState::update(float dt) {
    m_time += dt;
    m_slideInTimer = std::min(1.0f, m_slideInTimer + dt / 0.18f);
    for (auto& btn : m_buttons) btn.update(dt);

    // Gamepad navigation — only active when a gamepad is connected to avoid
    // doubling up with the keyboard handling already in handleEvent()
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;
    if (input.isActionPressed(Action::MenuUp)) {
        m_buttons[m_selectedButton].setSelected(false);
        m_selectedButton = (m_selectedButton - 1 + static_cast<int>(m_buttons.size())) % static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
        clearConfirmsExcept(-1);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_buttons[m_selectedButton].setSelected(false);
        m_selectedButton = (m_selectedButton + 1) % static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
        clearConfirmsExcept(-1);
    }
    if (input.isActionPressed(Action::Confirm)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        if (m_buttons[m_selectedButton].onClick)
            m_buttons[m_selectedButton].onClick();
    }
    if (input.isActionPressed(Action::Cancel)) {
        game->popState();
    }
}

void PauseState::render(SDL_Renderer* renderer) {
    // Entry animation: overlay darkens from 0 to 180 over ~180ms (ease-out)
    float slideT = std::min(1.0f, m_slideInTimer);
    float easeT = 1.0f - (1.0f - slideT) * (1.0f - slideT); // ease-out quadratic
    Uint8 overlayAlpha = static_cast<Uint8>(180 * easeT);

    // Dark overlay with vignette
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, overlayAlpha);
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
    SDL_Rect botBar = {0, BOTTOM_BAR_Y, SCREEN_WIDTH, 3};
    SDL_RenderFillRect(renderer, &topBar);
    SDL_RenderFillRect(renderer, &botBar);

    // Central panel background (texture-based with 9-slice)
    SDL_Rect panelBg = {PANEL_X, PANEL_Y, PANEL_W, PANEL_H};
    if (!renderPanelBg(renderer, panelBg)) {
        SDL_SetRenderDrawColor(renderer, 10, 10, 20, 200);
        SDL_RenderFillRect(renderer, &panelBg);
        SDL_SetRenderDrawColor(renderer, 80, 60, 140, 100);
        SDL_RenderDrawRect(renderer, &panelBg);
    }

    // Corner accents (overlay on texture for extra flair)
    int cornerSize = 12;
    int panelBot   = PANEL_Y + PANEL_H;
    int panelRight = PANEL_X + PANEL_W;
    SDL_SetRenderDrawColor(renderer, 150, 100, 255, static_cast<Uint8>(100 + 50 * pulse));
    SDL_RenderDrawLine(renderer, PANEL_X, PANEL_Y, PANEL_X + cornerSize, PANEL_Y);
    SDL_RenderDrawLine(renderer, PANEL_X, PANEL_Y, PANEL_X, PANEL_Y + cornerSize);
    SDL_RenderDrawLine(renderer, panelRight, PANEL_Y, panelRight - cornerSize, PANEL_Y);
    SDL_RenderDrawLine(renderer, panelRight, PANEL_Y, panelRight, PANEL_Y + cornerSize);
    SDL_RenderDrawLine(renderer, PANEL_X, panelBot, PANEL_X + cornerSize, panelBot);
    SDL_RenderDrawLine(renderer, PANEL_X, panelBot, PANEL_X, panelBot - cornerSize);
    SDL_RenderDrawLine(renderer, panelRight, panelBot, panelRight - cornerSize, panelBot);
    SDL_RenderDrawLine(renderer, panelRight, panelBot, panelRight, panelBot - cornerSize);

    // Title with glow (above panel so it doesn't overlap Run Stats / Equipment columns)
    TTF_Font* font = game->getFont();
    const int titleY = 240;
    if (font) {
        // Glow layer
        SDL_Color glowC = {120, 80, 200, static_cast<Uint8>(60 + 40 * pulse)};
        SDL_Surface* gs = TTF_RenderUTF8_Blended(font, LOC("pause.title"), glowC);
        if (gs) {
            SDL_Texture* gt = SDL_CreateTextureFromSurface(renderer, gs);
            if (gt) {
                SDL_SetTextureBlendMode(gt, SDL_BLENDMODE_ADD);
                SDL_Rect gr = {SCREEN_WIDTH / 2 - gs->w - 2, titleY - 4, gs->w * 2 + 4, gs->h * 2 + 4};
                SDL_RenderCopy(renderer, gt, nullptr, &gr);
                SDL_DestroyTexture(gt);
            }
            SDL_FreeSurface(gs);
        }

        // Main title
        SDL_Color c = {200, 200, 230, 255};
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, LOC("pause.title"), c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {SCREEN_WIDTH / 2 - s->w, titleY, s->w * 2, s->h * 2};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }

        // Separator line under title
        SDL_SetRenderDrawColor(renderer, 100, 70, 180, 80);
        SDL_RenderDrawLine(renderer, 800, titleY + 100, 1760, titleY + 100);
    }

    // Buttons
    for (int i = 0; i < static_cast<int>(m_buttons.size()); i++) {
        // Highlight Restart Run button in orange while confirming
        if (i == 1 && m_confirmRestart) {
            SDL_Color savedNormal = m_buttons[i].normalColor;
            SDL_Color savedSelected = m_buttons[i].selectedColor;
            SDL_Color savedText = m_buttons[i].textColor;
            m_buttons[i].normalColor   = {120, 80, 10, 255};
            m_buttons[i].selectedColor = {200, 130, 20, 255};
            m_buttons[i].textColor     = {255, 220, 150, 255};
            m_buttons[i].render(renderer, font);
            m_buttons[i].normalColor   = savedNormal;
            m_buttons[i].selectedColor = savedSelected;
            m_buttons[i].textColor     = savedText;
        }
        // Highlight Abandon Run button in red while confirming
        else if (i == 4 && m_confirmAbandon) {
            SDL_Color savedNormal = m_buttons[i].normalColor;
            SDL_Color savedSelected = m_buttons[i].selectedColor;
            SDL_Color savedText = m_buttons[i].textColor;
            m_buttons[i].normalColor   = {120, 20, 20, 255};
            m_buttons[i].selectedColor = {200, 40, 40, 255};
            m_buttons[i].textColor     = {255, 180, 180, 255};
            m_buttons[i].render(renderer, font);
            m_buttons[i].normalColor   = savedNormal;
            m_buttons[i].selectedColor = savedSelected;
            m_buttons[i].textColor     = savedText;
        } else {
            m_buttons[i].render(renderer, font);
        }
    }

    // Run stats panels
    renderRunStats(renderer, font);

    // Controls hint
    if (font) {
        SDL_Color hintC = {120, 120, 140, 180};
        SDL_Surface* hs = TTF_RenderUTF8_Blended(font, LOC("pause.nav_hint"), hintC);
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

static void renderStatText(SDL_Renderer* renderer, TTF_Font* font,
                            const char* text, int x, int y, SDL_Color color) {
    if (!font || !text) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect rect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void PauseState::renderRunStats(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    auto* playState = dynamic_cast<PlayState*>(game->getState(StateID::Play));
    if (!playState) return;

    char buf[128];
    SDL_Color labelCol = {140, 130, 180, 220};
    SDL_Color valCol = {220, 220, 240, 255};
    SDL_Color accentCol = {180, 140, 255, 255};

    // === Left panel: Run Overview ===
    int lx = 170;
    int ly = 400;

    renderStatText(renderer, font, LOC("pause.run_stats"), lx, ly, accentCol);
    ly += 44;
    SDL_SetRenderDrawColor(renderer, 100, 80, 160, 80);
    SDL_RenderDrawLine(renderer, lx, ly, lx + 400, ly);
    ly += 16;

    // Floor / Zone
    {
        int floor = playState->roomsCleared + 1;
        int zone  = getZone(floor) + 1;
        std::snprintf(buf, sizeof(buf), LOC("pause.floor"), floor, zone);
        renderStatText(renderer, font, buf, lx, ly, valCol);
        ly += 36;
    }

    // Difficulty
    const char* diffNames[] = {LOC("pause.diff_easy"), LOC("pause.diff_normal"), LOC("pause.diff_hard")};
    int diff = playState->getCurrentDifficulty();
    std::snprintf(buf, sizeof(buf), LOC("pause.difficulty"), (diff >= 0 && diff <= 2) ? diffNames[diff] : "??");
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 36;

    // Boss level indicator
    if (playState->isBossLevel()) {
        renderStatText(renderer, font, LOC("pause.boss_floor"), lx, ly, {255, 80, 80, 255});
        ly += 36;
    }

    // Run time
    int mins = static_cast<int>(playState->getRunTime()) / 60;
    int secs = static_cast<int>(playState->getRunTime()) % 60;
    std::snprintf(buf, sizeof(buf), LOC("pause.time"), mins, secs);
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 36;

    // Kills
    std::snprintf(buf, sizeof(buf), LOC("pause.kills"), playState->enemiesKilled);
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 36;

    // Rifts
    std::snprintf(buf, sizeof(buf), LOC("pause.rifts_repaired"), playState->riftsRepaired);
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 36;

    // Shards
    std::snprintf(buf, sizeof(buf), LOC("pause.shards"), playState->shardsCollected);
    renderStatText(renderer, font, buf, lx, ly, {255, 215, 80, 255});
    ly += 36;

    // Best combo
    std::snprintf(buf, sizeof(buf), LOC("pause.best_combo"), playState->getBestCombo());
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 36;

    // NG+ level (run tier, not unlocked tier — matches enemy scaling)
    int runTier = playState->getNGPlusTier();
    if (runTier > 0) {
        std::snprintf(buf, sizeof(buf), LOC("pause.ngplus"), runTier);
        renderStatText(renderer, font, buf, lx, ly, {200, 120, 255, 255});
        ly += 36;
    }

    // === Middle panel: Equipment ===
    Player* player = playState->getPlayer();
    if (player && player->getEntity()) {
        Entity& pe = *player->getEntity();

        int mx = 680;
        int my = 400;
        renderStatText(renderer, font, LOC("pause.equipment"), mx, my, accentCol);
        my += 44;
        SDL_SetRenderDrawColor(renderer, 100, 80, 160, 80);
        SDL_RenderDrawLine(renderer, mx, my, mx + 400, my);
        my += 16;

        // Class
        PlayerClass pc = player->playerClass;
        if (pc >= PlayerClass::Voidwalker && pc < PlayerClass::COUNT) {
            std::snprintf(buf, sizeof(buf), LOC("pause.class"), ClassSystem::getData(pc).name);
            renderStatText(renderer, font, buf, mx, my, valCol);
            my += 36;
        }

        // Weapons
        if (pe.hasComponent<CombatComponent>()) {
            auto& combat = pe.getComponent<CombatComponent>();
            const char* mName = WeaponSystem::getWeaponName(combat.currentMelee);
            const char* rName = WeaponSystem::getWeaponName(combat.currentRanged);
            std::snprintf(buf, sizeof(buf), LOC("pause.melee"), mName);
            renderStatText(renderer, font, buf, mx, my, {255, 180, 80, 255});
            my += 36;
            std::snprintf(buf, sizeof(buf), LOC("pause.ranged"), rName);
            renderStatText(renderer, font, buf, mx, my, {80, 200, 255, 255});
            my += 36;

            // Weapon mastery
            auto& cs = playState->getCombatSystem();
            int mIdx = static_cast<int>(combat.currentMelee);
            int rIdx = static_cast<int>(combat.currentRanged);
            int mKills = (mIdx >= 0 && mIdx < static_cast<int>(WeaponID::COUNT)) ? cs.weaponKills[mIdx] : 0;
            int rKills = (rIdx >= 0 && rIdx < static_cast<int>(WeaponID::COUNT)) ? cs.weaponKills[rIdx] : 0;
            MasteryTier mTier = WeaponSystem::getMasteryTier(mKills);
            MasteryTier rTier = WeaponSystem::getMasteryTier(rKills);
            if (mTier != MasteryTier::None) {
                std::snprintf(buf, sizeof(buf), LOC("pause.mastery"), WeaponSystem::getMasteryTierName(mTier), mKills);
                renderStatText(renderer, font, buf, mx, my, {255, 200, 100, 200});
                my += 32;
            }
            if (rTier != MasteryTier::None) {
                std::snprintf(buf, sizeof(buf), LOC("pause.mastery"), WeaponSystem::getMasteryTierName(rTier), rKills);
                renderStatText(renderer, font, buf, mx, my, {100, 200, 255, 200});
                my += 32;
            }
        }

        // Player level / XP
        std::snprintf(buf, sizeof(buf), LOC("pause.level"), player->level, player->xp, player->xpToNextLevel);
        renderStatText(renderer, font, buf, mx, my, {180, 220, 255, 255});
        my += 36;

        // HP
        if (pe.hasComponent<HealthComponent>()) {
            auto& hp = pe.getComponent<HealthComponent>();
            std::snprintf(buf, sizeof(buf), LOC("pause.hp"), hp.currentHP, hp.maxHP);
            SDL_Color hpCol = (hp.getPercent() < 0.3f) ? SDL_Color{255, 80, 80, 255} : SDL_Color{80, 255, 80, 255};
            renderStatText(renderer, font, buf, mx, my, hpCol);
            my += 40;
        }

        // Relics
        if (pe.hasComponent<RelicComponent>()) {
            auto& relics = pe.getComponent<RelicComponent>();
            if (!relics.relics.empty()) {
                my += 8;
                renderStatText(renderer, font, LOC("pause.relics"), mx, my, accentCol);
                my += 44;
                SDL_SetRenderDrawColor(renderer, 100, 80, 160, 80);
                SDL_RenderDrawLine(renderer, mx, my, mx + 400, my);
                my += 12;

                for (auto& r : relics.relics) {
                    if (r.consumed) continue;
                    const auto& data = RelicSystem::getRelicData(r.id);
                    SDL_Color rCol = valCol;
                    if (data.tier == RelicTier::Rare) rCol = {100, 180, 255, 255};
                    else if (data.tier == RelicTier::Legendary) rCol = {255, 200, 50, 255};
                    // Localized relic name (fallback to data.name for EN)
                    char rlk[48];
                    std::snprintf(rlk, sizeof(rlk), "relic.%d.name", static_cast<int>(data.id));
                    const char* rloc = LOC(rlk);
                    renderStatText(renderer, font, (std::strcmp(rloc, rlk) == 0) ? data.name : rloc, mx, my, rCol);
                    my += 32;
                    if (my > SCREEN_HEIGHT - 200) break; // Don't overflow panel
                }
            }
        }
    }
}
