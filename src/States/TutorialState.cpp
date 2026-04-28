#include "TutorialState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
#include "Core/AudioManager.h"
#include "UI/UITextures.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

bool TutorialState::s_openedFromNewRun = false;

void TutorialState::enter() {
    m_page = 0;
    m_fadeIn = 0;
    m_time = 0;
    m_pageTransition = 0;
    m_transitionDir = 0;
    m_particles.clear();
    AudioManager::instance().play(SFX::MenuConfirm);
}

void TutorialState::handleEvent(const SDL_Event& event) {
    if (m_pageTransition > 0) return; // Block input during transition

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT:
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                nextPage();
                break;
            case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:
                prevPage();
                break;
            case SDL_SCANCODE_ESCAPE:
                finish();
                break;
            default: break;
        }
    }

    // Mouse click: left = next, right = back
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            AudioManager::instance().play(SFX::MenuSelect);
            nextPage();
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            AudioManager::instance().play(SFX::MenuSelect);
            prevPage();
        }
    }

    // Mouse wheel: forward/back
    if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y < 0) nextPage();
        else if (event.wheel.y > 0) prevPage();
    }

    // Gamepad
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
            event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            nextPage();
        else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
                 event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
            prevPage();
        else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START)
            finish();
    }
}

void TutorialState::nextPage() {
    if (m_page >= PAGE_COUNT - 1) {
        finish();
    } else {
        m_page++;
        m_pageTransition = 0.3f;
        m_transitionDir = 1;
        AudioManager::instance().play(SFX::MenuSelect);
    }
}

void TutorialState::prevPage() {
    if (m_page > 0) {
        m_page--;
        m_pageTransition = 0.3f;
        m_transitionDir = -1;
        AudioManager::instance().play(SFX::MenuSelect);
    }
}

void TutorialState::finish() {
    AudioManager::instance().play(SFX::MenuConfirm);
    if (s_openedFromNewRun) {
        s_openedFromNewRun = false; // consume the flag
        game->changeState(StateID::ClassSelect);
    } else {
        game->changeState(StateID::Menu);
    }
}

void TutorialState::update(float dt) {
    m_time += dt;
    if (m_fadeIn < 1.0f) m_fadeIn = std::min(1.0f, m_fadeIn + dt * 3.0f);
    if (m_pageTransition > 0) m_pageTransition = std::max(0.0f, m_pageTransition - dt * 4.0f);

    // Background particles
    if (m_particles.size() < 40) {
        Particle p;
        p.x = static_cast<float>(rand() % SCREEN_WIDTH);
        p.y = static_cast<float>(SCREEN_HEIGHT + 10);
        p.vx = (rand() % 40 - 20) * 0.5f;
        p.vy = -(30.0f + rand() % 40);
        p.life = 0;
        p.maxLife = 3.0f + (rand() % 30) * 0.1f;
        p.size = 2.0f + (rand() % 4);
        p.r = static_cast<Uint8>(100 + rand() % 60);
        p.g = static_cast<Uint8>(60 + rand() % 40);
        p.b = static_cast<Uint8>(180 + rand() % 75);
        m_particles.push_back(p);
    }
    for (int i = static_cast<int>(m_particles.size()) - 1; i >= 0; i--) {
        auto& p = m_particles[i];
        p.life += dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (p.life >= p.maxLife) {
            m_particles.erase(m_particles.begin() + i);
        }
    }
}

void TutorialState::render(SDL_Renderer* renderer) {
    // Dark background with gradient
    SDL_SetRenderDrawColor(renderer, 8, 6, 18, 255);
    SDL_RenderClear(renderer);

    // Gradient overlay (darker at top, slightly lighter at bottom)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
        float t = static_cast<float>(y) / SCREEN_HEIGHT;
        Uint8 a = static_cast<Uint8>(10 + 15 * t);
        SDL_SetRenderDrawColor(renderer, 20, 15, 40, a);
        SDL_Rect band = {0, y, SCREEN_WIDTH, 4};
        SDL_RenderFillRect(renderer, &band);
    }

    // Particles
    for (auto& p : m_particles) {
        float t = p.life / p.maxLife;
        Uint8 a = static_cast<Uint8>(180 * (1.0f - t) * m_fadeIn);
        SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, a);
        int sz = static_cast<int>(p.size * (1.0f - t * 0.5f));
        SDL_Rect r = {static_cast<int>(p.x) - sz / 2, static_cast<int>(p.y) - sz / 2, sz, sz};
        SDL_RenderFillRect(renderer, &r);
    }

    TTF_Font* font = game->getFont();
    if (font) {
        renderPage(renderer, font);
        renderPageIndicator(renderer);
    }

    // Navigation hint at bottom
    if (font) {
        Uint8 hintA = static_cast<Uint8>(120 * m_fadeIn);
        const char* hint = LOC("tut.nav_hint");
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, hint, {150, 150, 170, hintA});
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {SCREEN_WIDTH / 2 - s->w / 2, SCREEN_HEIGHT - 60, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }
}

void TutorialState::renderKeyIcon(SDL_Renderer* renderer, int x, int y, int w, int h,
                                   const char* label, TTF_Font* font) {
    // Key cap background
    SDL_SetRenderDrawColor(renderer, 40, 35, 60, 230);
    SDL_Rect bg = {x, y, w, h};
    SDL_RenderFillRect(renderer, &bg);
    // Border
    SDL_SetRenderDrawColor(renderer, 120, 100, 180, 200);
    SDL_RenderDrawRect(renderer, &bg);
    // Highlight top
    SDL_SetRenderDrawColor(renderer, 160, 140, 220, 80);
    SDL_Rect top = {x + 1, y + 1, w - 2, 2};
    SDL_RenderFillRect(renderer, &top);
    // Label
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, label, {220, 210, 255, 255});
    if (s) {
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        if (t) {
            SDL_Rect r = {x + w / 2 - s->w / 2, y + h / 2 - s->h / 2, s->w, s->h};
            SDL_RenderCopy(renderer, t, nullptr, &r);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
    }
}

void TutorialState::renderPage(SDL_Renderer* renderer, TTF_Font* font) {
    Uint8 alpha = static_cast<Uint8>(255 * m_fadeIn);

    // Page content area
    int panelX = SCREEN_WIDTH / 2 - 700;
    int panelY = 120;
    int panelW = 1400;
    int panelH = 1000;

    // Panel background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 15, 12, 30, static_cast<Uint8>(200 * m_fadeIn));
    SDL_Rect panel = {panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 100, 70, 180, static_cast<Uint8>(120 * m_fadeIn));
    SDL_RenderDrawRect(renderer, &panel);

    // Title and content depend on page
    int cx = SCREEN_WIDTH / 2;
    int titleY = panelY + 40;
    int contentY = panelY + 140;

    auto renderTitle = [&](const char* key) {
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, LOC(key), {200, 180, 255, alpha});
        if (s) {
            int tw = static_cast<int>(s->w * 1.5f);
            int th = static_cast<int>(s->h * 1.5f);
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {cx - tw / 2, titleY, tw, th};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    };

    auto renderLine = [&](const char* text, int y, SDL_Color color = {200, 200, 210, 255}) {
        color.a = alpha;
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, text, color);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {cx - s->w / 2, y, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    };

    int keyW = 70, keyH = 50;
    int lineH = 60;

    switch (m_page) {
        case 0: { // Welcome
            renderTitle("tut.page.welcome.title");
            int y = contentY;
            renderLine(LOC("tut.page.welcome.1"), y); y += lineH;
            renderLine(LOC("tut.page.welcome.2"), y); y += lineH;
            renderLine(LOC("tut.page.welcome.3"), y); y += lineH;
            y += 30;
            renderLine(LOC("tut.page.welcome.4"), y, {160, 140, 200, 255}); y += lineH;
            renderLine(LOC("tut.page.welcome.5"), y, {160, 140, 200, 255});
            // Decorative rift portal visualization — concentric horizontal
            // slices to simulate an elliptical glow (SDL has no native oval)
            float pulse = 0.5f + 0.5f * std::sin(m_time * 2.0f);
            int riftCX = cx, riftCY = contentY + 550;
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for (int ring = 4; ring >= 0; ring--) {
                int rw = 120 + ring * 40 + static_cast<int>(pulse * 16);
                int rh = 40 + ring * 14 + static_cast<int>(pulse * 6);
                Uint8 ra = static_cast<Uint8>((60 - ring * 10) * m_fadeIn);
                SDL_SetRenderDrawColor(renderer, 140, 80, 255, ra);
                // Sample an ellipse using horizontal scanlines for a smooth shape
                for (int y2 = -rh; y2 <= rh; y2++) {
                    float ny = static_cast<float>(y2) / rh;
                    float w = rw * std::sqrt(std::max(0.0f, 1.0f - ny * ny));
                    SDL_RenderDrawLine(renderer,
                        riftCX - static_cast<int>(w), riftCY + y2,
                        riftCX + static_cast<int>(w), riftCY + y2);
                }
            }
            // Bright inner core
            SDL_SetRenderDrawColor(renderer, 220, 180, 255,
                static_cast<Uint8>(180 * m_fadeIn));
            for (int y2 = -12; y2 <= 12; y2++) {
                float ny = static_cast<float>(y2) / 12.0f;
                float w = 36.0f * std::sqrt(std::max(0.0f, 1.0f - ny * ny));
                SDL_RenderDrawLine(renderer,
                    riftCX - static_cast<int>(w), riftCY + y2,
                    riftCX + static_cast<int>(w), riftCY + y2);
            }
            break;
        }

        case 1: { // Controls
            renderTitle("tut.page.controls.title");
            int y = contentY;
            int keyStartX = cx - 200;

            renderLine(LOC("tut.page.controls.move"), y);
            y += lineH;
            // WASD keys visual
            renderKeyIcon(renderer, keyStartX + keyW + 10, y, keyW, keyH, "W", font);
            renderKeyIcon(renderer, keyStartX, y + keyH + 5, keyW, keyH, "A", font);
            renderKeyIcon(renderer, keyStartX + keyW + 10, y + keyH + 5, keyW, keyH, "S", font);
            renderKeyIcon(renderer, keyStartX + (keyW + 10) * 2, y + keyH + 5, keyW, keyH, "D", font);
            y += keyH * 2 + 40;

            renderLine(LOC("tut.page.controls.jump"), y);
            y += lineH;
            renderKeyIcon(renderer, cx - 100, y, 200, keyH, "SPACE", font);
            y += keyH + 30;

            renderLine(LOC("tut.page.controls.dash"), y);
            y += lineH;
            renderKeyIcon(renderer, cx - 70, y, 140, keyH, "SHIFT", font);
            y += keyH + 30;

            renderLine(LOC("tut.page.controls.wall"), y, {160, 160, 180, 255});
            break;
        }

        case 2: { // Combat
            renderTitle("tut.page.combat.title");
            int y = contentY;

            renderLine(LOC("tut.page.combat.melee"), y); y += lineH;
            renderKeyIcon(renderer, cx - 35, y, keyW, keyH, "J", font);
            y += keyH + 30;

            renderLine(LOC("tut.page.combat.ranged"), y); y += lineH;
            renderKeyIcon(renderer, cx - 35, y, keyW, keyH, "K", font);
            y += keyH + 30;

            renderLine(LOC("tut.page.combat.parry"), y, {255, 200, 100, 255}); y += lineH;
            renderLine(LOC("tut.page.combat.combo"), y); y += lineH;
            renderLine(LOC("tut.page.combat.weapons"), y, {160, 160, 180, 255}); y += lineH;
            // Weapon switch keys
            renderKeyIcon(renderer, cx - 80, y, keyW, keyH, "Q", font);
            renderKeyIcon(renderer, cx + 10, y, keyW, keyH, "R", font);
            break;
        }

        case 3: { // Dimensions
            renderTitle("tut.page.dim.title");
            int y = contentY;

            renderLine(LOC("tut.page.dim.core"), y); y += lineH;
            renderKeyIcon(renderer, cx - 35, y, keyW, keyH, "E", font);
            y += keyH + 30;

            // Dimension A vs B visualization
            int boxW = 500, boxH = 180;
            // Dim A box (blue)
            SDL_SetRenderDrawColor(renderer, 30, 50, 120, static_cast<Uint8>(160 * m_fadeIn));
            SDL_Rect dimA = {cx - boxW - 30, y, boxW, boxH};
            SDL_RenderFillRect(renderer, &dimA);
            SDL_SetRenderDrawColor(renderer, 80, 120, 255, static_cast<Uint8>(180 * m_fadeIn));
            SDL_RenderDrawRect(renderer, &dimA);
            renderLine(LOC("tut.page.dim.a_label"), y + 20, {100, 160, 255, 255});
            renderLine(LOC("tut.page.dim.a_desc"), y + 80, {150, 170, 200, 255});
            renderLine(LOC("tut.page.dim.a_desc2"), y + 130, {130, 150, 180, 255});

            // Dim B box (red)
            SDL_SetRenderDrawColor(renderer, 120, 30, 30, static_cast<Uint8>(160 * m_fadeIn));
            SDL_Rect dimB = {cx + 30, y, boxW, boxH};
            SDL_RenderFillRect(renderer, &dimB);
            SDL_SetRenderDrawColor(renderer, 255, 100, 100, static_cast<Uint8>(180 * m_fadeIn));
            SDL_RenderDrawRect(renderer, &dimB);
            renderLine(LOC("tut.page.dim.b_label"), y + 20, {255, 120, 100, 255});
            renderLine(LOC("tut.page.dim.b_desc"), y + 80, {200, 150, 150, 255});
            renderLine(LOC("tut.page.dim.b_desc2"), y + 130, {180, 130, 130, 255});

            y += boxH + 40;
            renderLine(LOC("tut.page.dim.entropy"), y, {255, 180, 80, 255}); y += lineH;
            renderLine(LOC("tut.page.dim.rifts"), y); y += lineH;
            renderKeyIcon(renderer, cx - 35, y, keyW, keyH, "F", font);
            break;
        }

        case 4: { // Progression
            renderTitle("tut.page.prog.title");
            int y = contentY;

            renderLine(LOC("tut.page.prog.roguelike"), y, {255, 200, 100, 255}); y += lineH;
            renderLine(LOC("tut.page.prog.death"), y); y += lineH;
            renderLine(LOC("tut.page.prog.upgrades"), y); y += lineH;
            y += 20;
            renderLine(LOC("tut.page.prog.classes"), y, {180, 140, 255, 255}); y += lineH;
            renderLine(LOC("tut.page.prog.relics"), y); y += lineH;
            renderLine(LOC("tut.page.prog.floors"), y); y += lineH;
            renderLine(LOC("tut.page.prog.bosses"), y, {255, 100, 80, 255});
            break;
        }

        case 5: { // Parry — defensive timing
            renderTitle("tut.page.parry.title");
            int y = contentY;

            renderLine(LOC("tut.page.parry.what"), y, {255, 220, 100, 255}); y += lineH;
            renderLine(LOC("tut.page.parry.timing"), y); y += lineH;
            renderLine(LOC("tut.page.parry.window"), y, {200, 180, 200, 255}); y += lineH;
            y += 16;
            renderLine(LOC("tut.page.parry.reward"), y, {180, 255, 180, 255}); y += lineH;
            renderLine(LOC("tut.page.parry.counter"), y); y += lineH;
            y += 16;
            renderLine(LOC("tut.page.parry.tip"), y, {200, 180, 100, 255});
            break;
        }

        case 6: { // Combo Finisher
            renderTitle("tut.page.finisher.title");
            int y = contentY;

            renderLine(LOC("tut.page.finisher.build"), y); y += lineH;
            renderKeyIcon(renderer, cx - 35, y, keyW, keyH, "F", font);
            y += keyH + 30;
            renderLine(LOC("tut.page.finisher.cue"), y, {255, 200, 100, 255}); y += lineH;
            renderLine(LOC("tut.page.finisher.execute"), y); y += lineH;
            y += 16;
            renderLine(LOC("tut.page.finisher.classes"), y, {180, 140, 255, 255}); y += lineH;
            renderLine(LOC("tut.page.finisher.tip"), y, {200, 180, 100, 255});
            break;
        }

        case 7: { // Synergy — relic + weapon combos
            renderTitle("tut.page.synergy.title");
            int y = contentY;

            renderLine(LOC("tut.page.synergy.intro"), y); y += lineH;
            y += 16;
            renderLine(LOC("tut.page.synergy.relic"), y, {255, 200, 100, 255}); y += lineH;
            renderLine(LOC("tut.page.synergy.relic_ex"), y, {200, 180, 220, 255}); y += lineH;
            y += 16;
            renderLine(LOC("tut.page.synergy.weapon"), y, {120, 220, 255, 255}); y += lineH;
            renderLine(LOC("tut.page.synergy.weapon_ex"), y, {200, 220, 240, 255}); y += lineH;
            y += 16;
            renderLine(LOC("tut.page.synergy.discover"), y, {180, 255, 180, 255}); y += lineH;
            renderLine(LOC("tut.page.synergy.tip"), y, {200, 180, 100, 255});
            break;
        }

        case 8: { // Ready!
            renderTitle("tut.page.ready.title");
            int y = contentY + 40;

            renderLine(LOC("tut.page.ready.1"), y, {180, 255, 180, 255}); y += lineH;
            renderLine(LOC("tut.page.ready.2"), y); y += lineH;
            renderLine(LOC("tut.page.ready.3"), y); y += lineH;
            y += 40;
            renderLine(LOC("tut.page.ready.tip1"), y, {200, 180, 100, 255}); y += lineH;
            renderLine(LOC("tut.page.ready.tip2"), y, {200, 180, 100, 255}); y += lineH;
            renderLine(LOC("tut.page.ready.tip3"), y, {200, 180, 100, 255}); y += lineH;
            y += 60;

            // Pulsing "Press ENTER to begin" prompt
            float pulse = 0.5f + 0.5f * std::sin(m_time * 3.0f);
            Uint8 pa = static_cast<Uint8>((150 + 105 * pulse) * m_fadeIn);
            renderLine(LOC("tut.page.ready.start"), y, {160, 255, 160, pa});
            break;
        }
    }
}

void TutorialState::renderPageIndicator(SDL_Renderer* renderer) {
    int dotSize = 12;
    int gap = 20;
    int totalW = PAGE_COUNT * dotSize + (PAGE_COUNT - 1) * gap;
    int startX = SCREEN_WIDTH / 2 - totalW / 2;
    int y = SCREEN_HEIGHT - 110;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < PAGE_COUNT; i++) {
        int x = startX + i * (dotSize + gap);
        if (i == m_page) {
            // Active dot: bright purple, larger
            SDL_SetRenderDrawColor(renderer, 160, 100, 255, static_cast<Uint8>(240 * m_fadeIn));
            SDL_Rect r = {x - 2, y - 2, dotSize + 4, dotSize + 4};
            SDL_RenderFillRect(renderer, &r);
        } else {
            // Inactive dot: dim
            SDL_SetRenderDrawColor(renderer, 80, 60, 120, static_cast<Uint8>(140 * m_fadeIn));
            SDL_Rect r = {x, y, dotSize, dotSize};
            SDL_RenderFillRect(renderer, &r);
        }
    }
}
