#include "PauseState.h"
#include "PlayState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Game/WeaponSystem.h"
#include "Game/RelicSystem.h"
#include "Game/ClassSystem.h"
#include "Components/RelicComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include <cmath>
#include <cstdio>

void PauseState::enter() {
    int cx = 640;
    int startY = 320;
    int btnW = 240;
    int btnH = 48;
    int gap = 12;

    m_buttons.clear();
    m_buttons.emplace_back(cx - btnW / 2, startY, btnW, btnH, "Resume");
    m_buttons.emplace_back(cx - btnW / 2, startY + btnH + gap, btnW, btnH, "Daily Leaderboard");
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 2, btnW, btnH, "Abandon Run");
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 3, btnW, btnH, "Quit to Menu");

    m_buttons[0].onClick = [this]() { game->popState(); };
    m_buttons[1].onClick = [this]() { game->pushState(StateID::DailyLeaderboard); };
    m_buttons[2].onClick = [this]() {
        if (!m_confirmAbandon) {
            m_confirmAbandon = true;
            m_buttons[2].setText("Confirm Abandon?");
            return;
        }
        if (auto* playState = dynamic_cast<PlayState*>(game->getState(StateID::Play))) {
            playState->abandonRun();
            return;
        }
        game->changeState(StateID::Menu);
    };
    m_buttons[3].onClick = [this]() { game->changeState(StateID::Menu); };

    m_selectedButton = 0;
    m_buttons[0].setSelected(true);
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
                if (m_confirmAbandon) { m_confirmAbandon = false; m_buttons[2].setText("Abandon Run"); }
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_buttons[m_selectedButton].setSelected(false);
                m_selectedButton = (m_selectedButton + 1) % static_cast<int>(m_buttons.size());
                m_buttons[m_selectedButton].setSelected(true);
                AudioManager::instance().play(SFX::MenuSelect);
                if (m_confirmAbandon) { m_confirmAbandon = false; m_buttons[2].setText("Abandon Run"); }
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                AudioManager::instance().play(SFX::MenuConfirm);
                if (m_buttons[m_selectedButton].onClick)
                    m_buttons[m_selectedButton].onClick();
                break;
            default: break;
        }
    }
}

void PauseState::update(float dt) {
    m_time += dt;

    // Gamepad navigation — only active when a gamepad is connected to avoid
    // doubling up with the keyboard handling already in handleEvent()
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;
    if (input.isActionPressed(Action::MenuUp)) {
        m_buttons[m_selectedButton].setSelected(false);
        m_selectedButton = (m_selectedButton - 1 + static_cast<int>(m_buttons.size())) % static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
        if (m_confirmAbandon) { m_confirmAbandon = false; m_buttons[2].setText("Abandon Run"); }
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_buttons[m_selectedButton].setSelected(false);
        m_selectedButton = (m_selectedButton + 1) % static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
        if (m_confirmAbandon) { m_confirmAbandon = false; m_buttons[2].setText("Abandon Run"); }
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
    // Dark overlay with vignette
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
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
    SDL_Rect botBar = {0, 717, SCREEN_WIDTH, 3};
    SDL_RenderFillRect(renderer, &topBar);
    SDL_RenderFillRect(renderer, &botBar);

    // Central panel background (wider to include stats)
    SDL_Rect panelBg = {60, 180, 1160, 460};
    SDL_SetRenderDrawColor(renderer, 10, 10, 20, 200);
    SDL_RenderFillRect(renderer, &panelBg);
    SDL_SetRenderDrawColor(renderer, 80, 60, 140, 100);
    SDL_RenderDrawRect(renderer, &panelBg);

    // Corner accents
    int cornerSize = 12;
    int panelBot = 180 + 460;   // panel startY + panel height
    int panelRight = 60 + 1160; // panel startX + panel width
    SDL_SetRenderDrawColor(renderer, 150, 100, 255, static_cast<Uint8>(100 + 50 * pulse));
    // Top-left
    SDL_RenderDrawLine(renderer, 60, 180, 60 + cornerSize, 180);
    SDL_RenderDrawLine(renderer, 60, 180, 60, 180 + cornerSize);
    // Top-right
    SDL_RenderDrawLine(renderer, panelRight, 180, panelRight - cornerSize, 180);
    SDL_RenderDrawLine(renderer, panelRight, 180, panelRight, 180 + cornerSize);
    // Bottom-left
    SDL_RenderDrawLine(renderer, 60, panelBot, 60 + cornerSize, panelBot);
    SDL_RenderDrawLine(renderer, 60, panelBot, 60, panelBot - cornerSize);
    // Bottom-right
    SDL_RenderDrawLine(renderer, panelRight, panelBot, panelRight - cornerSize, panelBot);
    SDL_RenderDrawLine(renderer, panelRight, panelBot, panelRight, panelBot - cornerSize);

    // Title with glow
    TTF_Font* font = game->getFont();
    if (font) {
        // Glow layer
        SDL_Color glowC = {120, 80, 200, static_cast<Uint8>(60 + 40 * pulse)};
        SDL_Surface* gs = TTF_RenderText_Blended(font, "P A U S E D", glowC);
        if (gs) {
            SDL_Texture* gt = SDL_CreateTextureFromSurface(renderer, gs);
            if (gt) {
                SDL_SetTextureBlendMode(gt, SDL_BLENDMODE_ADD);
                SDL_Rect gr = {640 - gs->w - 2, 218, gs->w * 2 + 4, gs->h * 2 + 4};
                SDL_RenderCopy(renderer, gt, nullptr, &gr);
                SDL_DestroyTexture(gt);
            }
            SDL_FreeSurface(gs);
        }

        // Main title
        SDL_Color c = {200, 200, 230, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "P A U S E D", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {640 - s->w, 220, s->w * 2, s->h * 2};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }

        // Separator line under title
        SDL_SetRenderDrawColor(renderer, 100, 70, 180, 80);
        SDL_RenderDrawLine(renderer, 400, 290, 880, 290);
    }

    // Buttons
    for (int i = 0; i < static_cast<int>(m_buttons.size()); i++) {
        // Highlight Abandon Run button in red while confirming
        if (i == 2 && m_confirmAbandon) {
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
        SDL_Color hintC = {100, 100, 130, 120};
        SDL_Surface* hs = TTF_RenderText_Blended(font, "ESC to resume", hintC);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            if (ht) {
                SDL_Rect hr = {640 - hs->w / 2, 650, hs->w, hs->h};
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
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
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
    int lx = 85;
    int ly = 200;

    renderStatText(renderer, font, "RUN STATS", lx, ly, accentCol);
    ly += 22;
    SDL_SetRenderDrawColor(renderer, 100, 80, 160, 80);
    SDL_RenderDrawLine(renderer, lx, ly, lx + 200, ly);
    ly += 8;

    // Floor
    std::snprintf(buf, sizeof(buf), "Floor: %d", playState->roomsCleared + 1);
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 18;

    // Difficulty
    const char* diffNames[] = {"Easy", "Normal", "Hard"};
    int diff = playState->getCurrentDifficulty();
    std::snprintf(buf, sizeof(buf), "Difficulty: %s", (diff >= 0 && diff <= 2) ? diffNames[diff] : "??");
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 18;

    // Boss level indicator
    if (playState->isBossLevel()) {
        renderStatText(renderer, font, "** BOSS FLOOR **", lx, ly, {255, 80, 80, 255});
        ly += 18;
    }

    // Run time
    int mins = static_cast<int>(playState->getRunTime()) / 60;
    int secs = static_cast<int>(playState->getRunTime()) % 60;
    std::snprintf(buf, sizeof(buf), "Time: %d:%02d", mins, secs);
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 18;

    // Kills
    std::snprintf(buf, sizeof(buf), "Kills: %d", playState->enemiesKilled);
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 18;

    // Rifts
    std::snprintf(buf, sizeof(buf), "Rifts Repaired: %d", playState->riftsRepaired);
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 18;

    // Shards
    std::snprintf(buf, sizeof(buf), "Shards: %d", playState->shardsCollected);
    renderStatText(renderer, font, buf, lx, ly, {255, 215, 80, 255});
    ly += 18;

    // Best combo
    std::snprintf(buf, sizeof(buf), "Best Combo: %d", playState->getBestCombo());
    renderStatText(renderer, font, buf, lx, ly, valCol);
    ly += 18;

    // NG+ level
    if (g_newGamePlusLevel > 0) {
        std::snprintf(buf, sizeof(buf), "New Game+ %d", g_newGamePlusLevel);
        renderStatText(renderer, font, buf, lx, ly, {200, 120, 255, 255});
        ly += 18;
    }

    // === Middle panel: Equipment ===
    Player* player = playState->getPlayer();
    if (player && player->getEntity()) {
        Entity& pe = *player->getEntity();

        int mx = 340;
        int my = 200;
        renderStatText(renderer, font, "EQUIPMENT", mx, my, accentCol);
        my += 22;
        SDL_SetRenderDrawColor(renderer, 100, 80, 160, 80);
        SDL_RenderDrawLine(renderer, mx, my, mx + 200, my);
        my += 8;

        // Class
        PlayerClass pc = player->playerClass;
        if (pc >= PlayerClass::Voidwalker && pc < PlayerClass::COUNT) {
            std::snprintf(buf, sizeof(buf), "Class: %s", ClassSystem::getData(pc).name);
            renderStatText(renderer, font, buf, mx, my, valCol);
            my += 18;
        }

        // Weapons
        if (pe.hasComponent<CombatComponent>()) {
            auto& combat = pe.getComponent<CombatComponent>();
            const char* mName = WeaponSystem::getWeaponName(combat.currentMelee);
            const char* rName = WeaponSystem::getWeaponName(combat.currentRanged);
            std::snprintf(buf, sizeof(buf), "Melee: %s", mName);
            renderStatText(renderer, font, buf, mx, my, {255, 180, 80, 255});
            my += 18;
            std::snprintf(buf, sizeof(buf), "Ranged: %s", rName);
            renderStatText(renderer, font, buf, mx, my, {80, 200, 255, 255});
            my += 18;

            // Weapon mastery
            auto& cs = playState->getCombatSystem();
            int mKills = cs.weaponKills[static_cast<int>(combat.currentMelee)];
            int rKills = cs.weaponKills[static_cast<int>(combat.currentRanged)];
            MasteryTier mTier = WeaponSystem::getMasteryTier(mKills);
            MasteryTier rTier = WeaponSystem::getMasteryTier(rKills);
            if (mTier != MasteryTier::None) {
                std::snprintf(buf, sizeof(buf), "  [%s] %d kills", WeaponSystem::getMasteryTierName(mTier), mKills);
                renderStatText(renderer, font, buf, mx, my, {255, 200, 100, 200});
                my += 16;
            }
            if (rTier != MasteryTier::None) {
                std::snprintf(buf, sizeof(buf), "  [%s] %d kills", WeaponSystem::getMasteryTierName(rTier), rKills);
                renderStatText(renderer, font, buf, mx, my, {100, 200, 255, 200});
                my += 16;
            }
        }

        // HP
        if (pe.hasComponent<HealthComponent>()) {
            auto& hp = pe.getComponent<HealthComponent>();
            std::snprintf(buf, sizeof(buf), "HP: %.0f / %.0f", hp.currentHP, hp.maxHP);
            SDL_Color hpCol = (hp.getPercent() < 0.3f) ? SDL_Color{255, 80, 80, 255} : SDL_Color{80, 255, 80, 255};
            renderStatText(renderer, font, buf, mx, my, hpCol);
            my += 20;
        }

        // Relics
        if (pe.hasComponent<RelicComponent>()) {
            auto& relics = pe.getComponent<RelicComponent>();
            if (!relics.relics.empty()) {
                my += 4;
                renderStatText(renderer, font, "RELICS", mx, my, accentCol);
                my += 22;
                SDL_SetRenderDrawColor(renderer, 100, 80, 160, 80);
                SDL_RenderDrawLine(renderer, mx, my, mx + 200, my);
                my += 6;

                for (auto& r : relics.relics) {
                    if (r.consumed) continue;
                    const auto& data = RelicSystem::getRelicData(r.id);
                    SDL_Color rCol = valCol;
                    if (data.tier == RelicTier::Rare) rCol = {100, 180, 255, 255};
                    else if (data.tier == RelicTier::Legendary) rCol = {255, 200, 50, 255};
                    renderStatText(renderer, font, data.name, mx, my, rCol);
                    my += 16;
                    if (my > 620) break; // Don't overflow panel
                }
            }
        }
    }
}
