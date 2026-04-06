#include "MenuState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "UI/UITextures.h"
#include "Game/Bestiary.h"
#include "Game/DailyRun.h"
#include "Game/ChallengeMode.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

void MenuState::enter() {
    // Load persistent data
    AscensionSystem::load("ascension_save.dat");
    Bestiary::load("bestiary_save.dat");
    ChallengeMode::load("riftwalker_challenges.dat");

    // Menu music
    AudioManager::instance().stopAmbient();
    AudioManager::instance().stopMusicLayers();
    AudioManager::instance().playMusic("assets/music/menu_theme.ogg");

    int cx = SCREEN_WIDTH / 2;
    int btnW = 520;
    int btnH = 60;
    int gap = 4;
    int totalMenuH = 12 * (btnH + gap) - gap;
    int startY = SCREEN_HEIGHT - totalMenuH - 32;

    m_buttons.clear();
    m_buttons.emplace_back(cx - btnW / 2, startY, btnW, btnH, LOC("menu.new_run"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap), btnW, btnH, LOC("menu.daily_run"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 2, btnW, btnH, LOC("menu.daily_leaderboard"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 3, btnW, btnH, LOC("menu.challenges"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 4, btnW, btnH, LOC("menu.upgrades"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 5, btnW, btnH, LOC("menu.bestiary"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 6, btnW, btnH, LOC("menu.achievements"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 7, btnW, btnH, LOC("menu.lore"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 8, btnW, btnH, LOC("menu.run_history"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 9, btnW, btnH, LOC("menu.credits"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 10, btnW, btnH, LOC("menu.options"));
    m_buttons.emplace_back(cx - btnW / 2, startY + (btnH + gap) * 11, btnW, btnH, LOC("menu.quit"));

    m_buttons[0].onClick = [this]() { game->changeState(StateID::ClassSelect); };
    m_buttons[1].onClick = [this]() { g_dailyRunActive = true; game->changeState(StateID::ClassSelect); };
    m_buttons[2].onClick = [this]() { game->pushState(StateID::DailyLeaderboard); };
    m_buttons[3].onClick = [this]() { game->changeState(StateID::ChallengeSelect); };
    m_buttons[4].onClick = [this]() { game->changeState(StateID::Upgrade); };
    m_buttons[5].onClick = [this]() { game->changeState(StateID::Bestiary); };
    m_buttons[6].onClick = [this]() { game->pushState(StateID::Achievements); };
    m_buttons[7].onClick = [this]() { game->changeState(StateID::Lore); };
    m_buttons[8].onClick = [this]() { game->pushState(StateID::RunHistory); };
    m_buttons[9].onClick = [this]() { game->changeState(StateID::Credits); };
    m_buttons[10].onClick = [this]() { game->changeState(StateID::Options); };
    m_buttons[11].onClick = [this]() { game->quit(); };

    m_selectedButton = 0;
    m_buttons[0].setSelected(true);
    m_time = 0;
    m_fadeIn = 0;
    m_bgParticles.clear();

    // Staggered entrance animation for buttons
    for (int i = 0; i < static_cast<int>(m_buttons.size()); i++) {
        m_buttons[i].baseX = cx - btnW / 2;
        m_buttons[i].entranceDelay = 0.15f + i * 0.04f; // staggered 40ms each
        m_buttons[i].entranceProgress = 0.0f;
    }
}

void MenuState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_buttons[m_selectedButton].setSelected(false);
                m_selectedButton = (m_selectedButton - 1 + static_cast<int>(m_buttons.size())) % static_cast<int>(m_buttons.size());
                m_buttons[m_selectedButton].setSelected(true);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_buttons[m_selectedButton].setSelected(false);
                m_selectedButton = (m_selectedButton + 1) % static_cast<int>(m_buttons.size());
                m_buttons[m_selectedButton].setSelected(true);
                AudioManager::instance().play(SFX::MenuSelect);
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
                }
                break;
            }
        }
    }

    // Mouse wheel scrolling through menu items
    if (event.type == SDL_MOUSEWHEEL) {
        int btnCount = static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(false);
        if (event.wheel.y > 0)
            m_selectedButton = (m_selectedButton - 1 + btnCount) % btnCount;
        else if (event.wheel.y < 0)
            m_selectedButton = (m_selectedButton + 1) % btnCount;
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
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

void MenuState::update(float dt) {
    m_time += dt;
    if (m_fadeIn < 1.0f) {
        m_fadeIn += dt * 0.8f;
        if (m_fadeIn > 1.0f) m_fadeIn = 1.0f;
    }
    // Update button hover + entrance animations
    for (auto& btn : m_buttons) {
        btn.update(dt);
        btn.updateEntrance(dt);
    }

    // Gamepad navigation — only active when a gamepad is connected to avoid
    // doubling up with the keyboard handling already in handleEvent()
    auto& input = game->getInput();
    if (input.hasGamepad()) {
    if (input.isActionPressed(Action::MenuUp)) {
        m_buttons[m_selectedButton].setSelected(false);
        m_selectedButton = (m_selectedButton - 1 + static_cast<int>(m_buttons.size())) % static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_buttons[m_selectedButton].setSelected(false);
        m_selectedButton = (m_selectedButton + 1) % static_cast<int>(m_buttons.size());
        m_buttons[m_selectedButton].setSelected(true);
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Confirm)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        if (m_buttons[m_selectedButton].onClick)
            m_buttons[m_selectedButton].onClick();
    }
    } // end hasGamepad()

    // Spawn particles from portal area
    if (m_bgParticles.size() < 80) {
        float angle = static_cast<float>(std::rand()) / RAND_MAX * 6.283185f;
        float speed = 30.0f + static_cast<float>(std::rand()) / RAND_MAX * 60.0f;
        float dist = 35.0f + static_cast<float>(std::rand()) / RAND_MAX * 25.0f;
        bool dimA = std::rand() % 2 == 0;

        Particle p;
        p.x = static_cast<float>(SCREEN_WIDTH / 2) + std::cos(angle) * dist;
        p.y = 240.0f + std::sin(angle) * dist * 0.7f;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed - 15.0f;
        p.life = 0;
        p.maxLife = 2.0f + static_cast<float>(std::rand()) / RAND_MAX * 2.5f;
        p.size = 2.0f + static_cast<float>(std::rand()) / RAND_MAX * 3.0f;
        if (dimA) { p.r = 90; p.g = 50; p.b = 190; }
        else      { p.r = 190; p.g = 60; p.b = 90; }
        m_bgParticles.push_back(p);
    }

    // Update particles
    for (auto it = m_bgParticles.begin(); it != m_bgParticles.end(); ) {
        it->life += dt;
        it->x += it->vx * dt;
        it->y += it->vy * dt;
        it->vy += 8.0f * dt;
        if (it->life >= it->maxLife) {
            it = m_bgParticles.erase(it);
        } else {
            ++it;
        }
    }
}

void MenuState::renderPortal(SDL_Renderer* renderer) {
    int cx = SCREEN_WIDTH / 2, cy = 480;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Outer glow rings
    for (int ring = 0; ring < 6; ring++) {
        float baseR = 170.0f + ring * 44.0f;
        float pulse = std::sin(m_time * 2.0f + ring * 0.7f) * 8.0f;
        float radius = baseR + pulse;
        Uint8 alpha = static_cast<Uint8>(55 - ring * 8);
        bool altColor = ring % 2 == 0;

        int segments = 40;
        for (int i = 0; i < segments; i++) {
            float a1 = static_cast<float>(i) / segments * 6.283185f;
            float a2 = static_cast<float>(i + 1) / segments * 6.283185f;
            int x1 = cx + static_cast<int>(std::cos(a1) * radius);
            int y1 = cy + static_cast<int>(std::sin(a1) * radius * 0.7f);
            int x2 = cx + static_cast<int>(std::cos(a2) * radius);
            int y2 = cy + static_cast<int>(std::sin(a2) * radius * 0.7f);

            if (altColor)
                SDL_SetRenderDrawColor(renderer, 110, 55, 210, alpha);
            else
                SDL_SetRenderDrawColor(renderer, 210, 65, 110, alpha);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }

    // Inner void (filled ellipse)
    for (int dy = -100; dy <= 100; dy++) {
        int halfW = static_cast<int>(std::sqrt(std::max(0, 10000 - dy * dy)));
        float yRatio = static_cast<float>(std::abs(dy)) / 100.0f;
        float pulse = std::sin(m_time * 3.0f + yRatio * 3.0f);
        Uint8 rb = static_cast<Uint8>(std::clamp(12.0f + pulse * 12.0f, 0.0f, 255.0f));
        Uint8 gb = static_cast<Uint8>(std::clamp(4.0f + pulse * 6.0f, 0.0f, 255.0f));
        Uint8 bb = static_cast<Uint8>(std::clamp(28.0f + pulse * 22.0f, 0.0f, 255.0f));
        Uint8 ab = static_cast<Uint8>(std::clamp(100.0f + (1.0f - yRatio) * 100.0f, 0.0f, 255.0f));

        SDL_SetRenderDrawColor(renderer, rb, gb, bb, ab);
        int screenY = cy + static_cast<int>(dy * 0.7f);
        SDL_RenderDrawLine(renderer, cx - halfW, screenY, cx + halfW, screenY);
    }

    // Energy tendrils rotating outward
    for (int i = 0; i < 8; i++) {
        float angle = m_time * 0.8f + i * (6.283185f / 8);
        float len = 110.0f + 50.0f * std::sin(m_time * 2.5f + i * 1.3f);
        int x1 = cx + static_cast<int>(std::cos(angle) * 36);
        int y1 = cy + static_cast<int>(std::sin(angle) * 24);
        int x2 = cx + static_cast<int>(std::cos(angle) * len);
        int y2 = cy + static_cast<int>(std::sin(angle) * len * 0.7f);

        Uint8 ta = static_cast<Uint8>(std::clamp(90.0f + 50.0f * std::sin(m_time * 3.0f + i), 0.0f, 255.0f));
        SDL_SetRenderDrawColor(renderer, 140, 80, 230, ta);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + 1, x2, y2 + 1);
    }

    // Rotating sparkles
    for (int i = 0; i < 14; i++) {
        float angle = m_time * 1.2f + i * (6.283185f / 14);
        float dist = 100.0f + 36.0f * std::sin(m_time * 2.0f + i * 2.1f);
        int px = cx + static_cast<int>(std::cos(angle) * dist);
        int py = cy + static_cast<int>(std::sin(angle) * dist * 0.7f);
        float bright = 0.5f + 0.5f * std::sin(m_time * 4.0f + i * 1.5f);
        Uint8 sa = static_cast<Uint8>(180.0f * bright);

        if (i % 2 == 0)
            SDL_SetRenderDrawColor(renderer, 180, 140, 255, sa);
        else
            SDL_SetRenderDrawColor(renderer, 255, 120, 170, sa);

        SDL_Rect spark = {px - 2, py - 2, 5, 5};
        SDL_RenderFillRect(renderer, &spark);
    }
}

void MenuState::renderTitle(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Title color oscillates between dimension colors
    float dimShift = 0.5f + 0.5f * std::sin(m_time * 0.5f);
    Uint8 tr = static_cast<Uint8>(80 + 120 * dimShift);
    Uint8 tg = static_cast<Uint8>(40 + 20 * (1.0f - dimShift));
    Uint8 tb = static_cast<Uint8>(200 - 80 * dimShift);

    SDL_Surface* surface = TTF_RenderText_Blended(font, "R I F T W A L K E R", {tr, tg, tb, 255});
    if (!surface) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!tex) { SDL_FreeSurface(surface); return; }

    int tw = surface->w * 2;
    int th = surface->h * 2;
    int tx = SCREEN_WIDTH / 2 - tw / 2;
    int ty = 200;

    // Glow layers (additive blending for bloom effect)
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
    for (int g = 3; g >= 1; g--) {
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(25 + g * 12));
        SDL_Rect gr = {tx - g * 2, ty - g, tw + g * 4, th + g * 2};
        SDL_RenderCopy(renderer, tex, nullptr, &gr);
    }

    // Main title
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(tex, 255);
    SDL_Rect dst = {tx, ty, tw, th};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surface);

    // Subtitle
    Uint8 subAlpha = static_cast<Uint8>(200.0f * m_fadeIn);
    SDL_Color subColor = {120, 100, 165, subAlpha};
    SDL_Surface* sub = TTF_RenderText_Blended(font, LOC("menu.subtitle"), subColor);
    if (sub) {
        SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, sub);
        if (st) {
            SDL_SetTextureAlphaMod(st, subAlpha);
            SDL_Rect sr = {SCREEN_WIDTH / 2 - sub->w / 2, 340, sub->w, sub->h};
            SDL_RenderCopy(renderer, st, nullptr, &sr);
            SDL_DestroyTexture(st);
        }
        SDL_FreeSurface(sub);
    }
}

void MenuState::render(SDL_Renderer* renderer) {
    // Dark background
    SDL_SetRenderDrawColor(renderer, 8, 5, 15, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Subtle grid pattern
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 25);
    for (int x = 0; x < SCREEN_WIDTH; x += 160) SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 160) SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);

    // Background particles
    for (auto& p : m_bgParticles) {
        float alpha = 1.0f - (p.life / std::max(0.01f, p.maxLife));
        Uint8 pa = static_cast<Uint8>(alpha * 140);
        int size = static_cast<int>(p.size * alpha);
        if (size < 1) size = 1;
        SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, pa);
        SDL_Rect pr = {static_cast<int>(p.x) - size / 2,
                       static_cast<int>(p.y) - size / 2, size, size};
        SDL_RenderFillRect(renderer, &pr);
    }

    // Rift portal
    renderPortal(renderer);

    // Title
    TTF_Font* font = game->getFont();
    renderTitle(renderer, font);

    // Rift shards display with diamond icon
    if (font) {
        char shardText[64];
        int shards = game->getUpgradeSystem().getRiftShards();
        std::snprintf(shardText, sizeof(shardText), LOC("menu.rift_shards"), shards);
        Uint8 shardAlpha = static_cast<Uint8>(200.0f * m_fadeIn);
        SDL_Color shardColor = {180, 140, 255, shardAlpha};
        SDL_Surface* ss = TTF_RenderText_Blended(font, shardText, shardColor);
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_SetTextureAlphaMod(st, shardAlpha);

                // Diamond icon
                int iconX = SCREEN_WIDTH / 2 - ss->w / 2 - 20;
                int iconY = 388;
                SDL_SetRenderDrawColor(renderer, 180, 140, 255, shardAlpha);
                SDL_RenderDrawLine(renderer, iconX, iconY, iconX + 6, iconY - 8);
                SDL_RenderDrawLine(renderer, iconX + 6, iconY - 8, iconX + 12, iconY);
                SDL_RenderDrawLine(renderer, iconX + 12, iconY, iconX + 6, iconY + 8);
                SDL_RenderDrawLine(renderer, iconX + 6, iconY + 8, iconX, iconY);

                SDL_Rect sr = {SCREEN_WIDTH / 2 - ss->w / 2, 380, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &sr);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }
    }

    // Ascension level display (above buttons)
    if (font && AscensionSystem::currentLevel > 0) {
        char ascText[64];
        std::snprintf(ascText, sizeof(ascText), LOC("menu.ascension"), AscensionSystem::currentLevel);
        float pulse = 0.7f + 0.3f * std::sin(m_time * 2.0f);
        Uint8 aa = static_cast<Uint8>(220 * pulse * m_fadeIn);
        SDL_Color ascColor = {255, 180, 60, aa};
        int ascTextH = 16; // fallback
        SDL_Surface* as = TTF_RenderText_Blended(font, ascText, ascColor);
        if (as) {
            ascTextH = as->h;
            SDL_Texture* at = SDL_CreateTextureFromSurface(renderer, as);
            if (at) {
                SDL_SetTextureAlphaMod(at, aa);
                SDL_Rect ar = {SCREEN_WIDTH / 2 - as->w / 2, 400, as->w, as->h};
                SDL_RenderCopy(renderer, at, nullptr, &ar);
                SDL_DestroyTexture(at);
            }
            SDL_FreeSurface(as);
        }

        // Rift Cores display
        char coreText[64];
        std::snprintf(coreText, sizeof(coreText), LOC("menu.rift_cores"), AscensionSystem::riftCores);
        SDL_Surface* cs = TTF_RenderText_Blended(font, coreText, SDL_Color{200, 150, 255, static_cast<Uint8>(180 * m_fadeIn)});
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_Rect cr = {SCREEN_WIDTH / 2 - cs->w / 2, 405 + ascTextH, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }

    // Career stats panel
    if (font) {
        renderCareerStats(renderer, font);
    }

    // Buttons
    for (auto& btn : m_buttons) {
        btn.render(renderer, font);
    }

    // Daily Run info panel (shown when "Daily Run" button is highlighted)
    if (font && m_selectedButton == 1) {
        renderDailyInfo(renderer, font);
    }

    // Keyboard shortcut hint
    if (font) {
        SDL_Color hintC = {120, 120, 140, 180};
        SDL_Surface* hs = TTF_RenderText_Blended(font, LOC("menu.nav_hint"), hintC);
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

    // Version at bottom
    if (font) {
        SDL_Color c = {55, 50, 75, 100};
        SDL_Surface* s = TTF_RenderText_Blended(font, LOC("menu.version"), c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {SCREEN_WIDTH / 2 - s->w / 2, SCREEN_HEIGHT - 14, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Fade-in overlay
    if (m_fadeIn < 1.0f) {
        Uint8 fa = static_cast<Uint8>((1.0f - m_fadeIn) * 255);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, fa);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);
    }
}

void MenuState::renderDailyInfo(SDL_Renderer* renderer, TTF_Font* font) {
    // Gather daily data
    auto todayDate = DailyRun::getTodayDate();
    int seed = DailyRun::getTodaySeed();
    MutatorID mut = DailyRun::getDailyMutator();
    const auto& mutData = ChallengeMode::getMutatorData(mut);

    DailyRun dailyRun;
    dailyRun.load("riftwalker_daily.dat");
    int bestScore = dailyRun.getTodayBest();

    // Panel position: right of the menu buttons, aligned with button 1 (Daily Run)
    int btnW = 520;
    int btnH = 60;
    int gap = 4;
    int totalMenuH = 12 * (btnH + gap) - gap;
    int startY = SCREEN_HEIGHT - totalMenuH - 32;
    int panelX = SCREEN_WIDTH / 2 + btnW / 2 + 16;
    int panelY = startY + (btnH + gap); // same Y as button 1

    int panelW = 460;
    int lineH = 36;
    int panelH = lineH * 4 + 32; // 4 lines + padding (scaled for 2K)
    int textX = panelX + 20;
    int textY = panelY + 16;

    // Fade based on menu fade
    Uint8 panelAlpha = static_cast<Uint8>(180 * m_fadeIn);

    // Panel background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 18, 14, 32, panelAlpha);
    SDL_Rect panelRect = {panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer, &panelRect);

    // Panel border
    Uint8 borderAlpha = static_cast<Uint8>(120 * m_fadeIn);
    SDL_SetRenderDrawColor(renderer, 100, 60, 180, borderAlpha);
    SDL_RenderDrawRect(renderer, &panelRect);

    // Helper lambda to render a line of text
    auto drawLine = [&](const char* text, SDL_Color color, int y) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(255 * m_fadeIn));
            SDL_Rect dst = {textX, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    };

    // Line 1: Date
    char dateBuf[48];
    std::snprintf(dateBuf, sizeof(dateBuf), LOC("menu.daily_date"), todayDate.c_str());
    drawLine(dateBuf, {160, 150, 190, 255}, textY);

    // Line 2: Seed
    char seedBuf[48];
    std::snprintf(seedBuf, sizeof(seedBuf), LOC("menu.daily_seed"), seed);
    drawLine(seedBuf, {140, 130, 170, 255}, textY + lineH);

    // Line 3: Mutator
    char mutBuf[64];
    std::snprintf(mutBuf, sizeof(mutBuf), LOC("menu.daily_mutator"), mutData.name);
    drawLine(mutBuf, {200, 160, 255, 255}, textY + lineH * 2);

    // Line 4: Best score
    char scoreBuf[48];
    if (bestScore > 0)
        std::snprintf(scoreBuf, sizeof(scoreBuf), LOC("menu.daily_best"), bestScore);
    else
        std::snprintf(scoreBuf, sizeof(scoreBuf), "%s", LOC("menu.daily_no_best"));
    drawLine(scoreBuf, {180, 180, 60, 255}, textY + lineH * 3);
}

void MenuState::renderCareerStats(SDL_Renderer* renderer, TTF_Font* font) {
    auto& ups = game->getUpgradeSystem();
    if (ups.totalRuns == 0) return; // Nothing to show yet

    int lineH = 32;
    int panelW = 440;
    int panelH = lineH * 4 + 28;
    int panelX = 24;
    int panelY = SCREEN_HEIGHT - panelH - 40;

    Uint8 alpha = static_cast<Uint8>(120 * m_fadeIn);

    // Panel background (texture-based)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    if (!renderPanelBg(renderer, bg, alpha)) {
        SDL_SetRenderDrawColor(renderer, 12, 8, 24, alpha);
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 60, 45, 100, static_cast<Uint8>(80 * m_fadeIn));
        SDL_RenderDrawRect(renderer, &bg);
    }

    int tx = panelX + 16;
    int ty = panelY + 12;
    Uint8 ta = static_cast<Uint8>(180 * m_fadeIn);

    auto drawStat = [&](const char* text, SDL_Color color, int y) {
        SDL_Surface* s = TTF_RenderText_Blended(font, text, color);
        if (!s) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        if (t) {
            SDL_SetTextureAlphaMod(t, ta);
            SDL_Rect dst = {tx, y, s->w, s->h};
            SDL_RenderCopy(renderer, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
    };

    // Header
    drawStat(LOC("menu.career_stats"), {100, 85, 140, 255}, ty);

    // Line 1: Runs | Best Floor
    char buf[96];
    std::snprintf(buf, sizeof(buf), LOC("menu.stats_runs"),
                  ups.totalRuns, ups.bestRoomReached);
    drawStat(buf, {80, 75, 120, 255}, ty + lineH);

    // Line 2: Kills | Rifts
    std::snprintf(buf, sizeof(buf), LOC("menu.stats_kills"),
                  ups.totalEnemiesKilled, ups.totalRiftsRepaired);
    drawStat(buf, {80, 75, 120, 255}, ty + lineH * 2);

    // Line 3: Shards | Victories
    {
        int victories = 0;
        for (const auto& r : ups.getRunHistory())
            if (r.deathCause == 5) victories++;
        if (victories > 0) {
            std::snprintf(buf, sizeof(buf), LOC("menu.stats_victories"), victories);
            drawStat(buf, {80, 200, 80, 255}, ty + lineH * 3);
        }
        std::snprintf(buf, sizeof(buf), LOC("menu.stats_shards"), ups.getRiftShards());
        drawStat(buf, {80, 75, 120, 255}, ty + lineH * (victories > 0 ? 4 : 3));
    }

    // Total playtime
    if (ups.totalPlaytime > 0) {
        int totalMins = static_cast<int>(ups.totalPlaytime) / 60;
        int hours = totalMins / 60;
        int mins = totalMins % 60;
        std::snprintf(buf, sizeof(buf), LOC("menu.stats_playtime"), hours, mins);
        int ptLine = 4;
        for (const auto& r : ups.getRunHistory()) if (r.deathCause == 5) { ptLine = 5; break; }
        drawStat(buf, {80, 75, 120, 255}, ty + lineH * ptLine);
    }
}
