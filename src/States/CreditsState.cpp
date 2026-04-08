#include "States/CreditsState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include <cmath>
#include <cstdlib>

void CreditsState::enter() {
    m_fontTitle = TTF_OpenFont("assets/fonts/default.ttf", 56);
    m_fontBody = TTF_OpenFont("assets/fonts/default.ttf", 32);
    m_scrollY = 0.0f;
    m_time = 0.0f;
    m_particles.clear();

    // Credits music (same as victory, or menu theme from menu context)
    AudioManager::instance().playMusic("assets/music/victory.ogg");

    // Credits text — empty strings create blank lines for spacing
    m_lines = {
        "R I F T W A L K E R",
        "",
        LOC("credits.subtitle"),
        "",
        "",
        LOC("credits.section.design"),
        LOC("credits.design_line"),
        "",
        "",
        LOC("credits.section.engine"),
        "SDL2  |  SDL2_image  |  SDL2_mixer  |  SDL2_ttf",
        "C++17  |  CMake  |  vcpkg",
        "",
        "",
        LOC("credits.section.arch"),
        LOC("credits.arch.ecs"),
        LOC("credits.arch.procgen"),
        LOC("credits.arch.dualdim"),
        "",
        "",
        LOC("credits.section.gameplay"),
        LOC("credits.gameplay.classes"),
        LOC("credits.gameplay.weapons"),
        LOC("credits.gameplay.relics"),
        LOC("credits.gameplay.bosses"),
        LOC("credits.gameplay.enemies"),
        LOC("credits.gameplay.entropy"),
        LOC("credits.gameplay.ngplus"),
        "",
        "",
        LOC("credits.section.combat"),
        LOC("credits.combat.melee"),
        LOC("credits.combat.charged"),
        LOC("credits.combat.abilities"),
        LOC("credits.combat.elements"),
        "",
        "",
        LOC("credits.section.world"),
        LOC("credits.world.floors"),
        LOC("credits.world.puzzles"),
        LOC("credits.world.npcs"),
        LOC("credits.world.lore"),
        LOC("credits.world.events"),
        "",
        "",
        LOC("credits.section.progression"),
        LOC("credits.prog.achievements"),
        LOC("credits.prog.upgrades"),
        LOC("credits.prog.daily"),
        LOC("credits.prog.challenges"),
        LOC("credits.prog.bestiary"),
        "",
        "",
        LOC("credits.section.audio"),
        LOC("credits.audio.sfx"),
        LOC("credits.audio.music"),
        "",
        "",
        LOC("credits.section.visual"),
        LOC("credits.visual.render"),
        LOC("credits.visual.parallax"),
        LOC("credits.visual.particles"),
        LOC("credits.visual.indicators"),
        "",
        "",
        LOC("credits.section.testing"),
        LOC("credits.testing.bot"),
        LOC("credits.testing.visual"),
        LOC("credits.testing.balance"),
        "",
        "",
        LOC("credits.section.thanks"),
        LOC("credits.thanks.void"),
        LOC("credits.thanks.coffee"),
        "",
        "",
        "",
        LOC("credits.thanks"),
        "",
        "",
        "",
        LOC("credits.return")
    };
}

void CreditsState::exit() {
    if (m_fontTitle) { TTF_CloseFont(m_fontTitle); m_fontTitle = nullptr; }
    if (m_fontBody) { TTF_CloseFont(m_fontBody); m_fontBody = nullptr; }
    m_particles.clear();
}

void CreditsState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            AudioManager::instance().play(SFX::MenuConfirm);
            if (game) game->changeState(StateID::Menu);
        }
    }

    // Mouse wheel scrolling (speed up/rewind credits)
    if (event.type == SDL_MOUSEWHEEL) {
        m_scrollY -= event.wheel.y * 80.0f;
        if (m_scrollY < 0) m_scrollY = 0;
        AudioManager::instance().play(SFX::MenuSelect);
    }

    // Right-click or left-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONDOWN) {
        AudioManager::instance().play(SFX::MenuConfirm);
        if (game) game->changeState(StateID::Menu);
    }
}

void CreditsState::update(float dt) {
    m_time += dt;
    m_scrollY += dt * 60.0f; // Scroll speed: 60px/s (scaled for 2K)

    // Gamepad back
    auto& input = game->getInput();
    if (input.hasGamepad() && input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        if (game) game->changeState(StateID::Menu);
    }

    // Spawn ambient particles
    if (m_particles.size() < 70) {
        Particle p;
        p.x = static_cast<float>(std::rand() % SCREEN_WIDTH);
        p.y = static_cast<float>(SCREEN_HEIGHT + 5);
        p.vy = -(20.0f + static_cast<float>(std::rand() % 40));
        p.life = 0.0f;
        p.maxLife = 3.0f + static_cast<float>(std::rand() % 30) / 10.0f;
        p.size = 1.0f + static_cast<float>(std::rand() % 3);
        // Dimensional color palette (purple, cyan, magenta, blue)
        int colorChoice = std::rand() % 4;
        if (colorChoice == 0) { p.r = 100; p.g = 60; p.b = 200; }      // Purple
        else if (colorChoice == 1) { p.r = 60; p.g = 140; p.b = 200; }  // Cyan
        else if (colorChoice == 2) { p.r = 160; p.g = 50; p.b = 180; }  // Magenta
        else { p.r = 40; p.g = 100; p.b = 220; }                        // Blue
        m_particles.push_back(p);
    }

    // Update particles
    for (auto it = m_particles.begin(); it != m_particles.end(); ) {
        it->life += dt;
        it->y += it->vy * dt;
        if (it->life >= it->maxLife || it->y < -10.0f) {
            it = m_particles.erase(it);
        } else {
            ++it;
        }
    }
}

void CreditsState::render(SDL_Renderer* renderer) {
    // Dark background
    SDL_SetRenderDrawColor(renderer, 8, 4, 16, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Render particles behind text
    for (auto& p : m_particles) {
        float alpha = 1.0f - (p.life / std::max(0.01f, p.maxLife));
        Uint8 pa = static_cast<Uint8>(alpha * 100.0f);
        int size = static_cast<int>(p.size * alpha);
        if (size < 1) size = 1;
        SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, pa);
        SDL_Rect pr = {static_cast<int>(p.x) - size / 2,
                       static_cast<int>(p.y) - size / 2, size, size};
        SDL_RenderFillRect(renderer, &pr);
    }

    // Render scrolling credits
    // Text starts below the screen and scrolls up
    float baseY = static_cast<float>(SCREEN_HEIGHT) - m_scrollY;
    int lineSpacing = 56;

    for (int i = 0; i < static_cast<int>(m_lines.size()); i++) {
        float lineY = baseY + i * lineSpacing;

        // Skip lines that are off-screen
        if (lineY < -40.0f || lineY > SCREEN_HEIGHT + 40.0f) continue;

        // Empty lines are spacers
        if (m_lines[i].empty()) continue;

        // Determine if this is a title/heading line
        bool isTitle = (i == 0); // "R I F T W A L K E R"
        bool isHeading = (m_lines[i].size() > 3 &&
                          m_lines[i][0] == '-' && m_lines[i][1] == '-' && m_lines[i][2] == '-');

        // Fade at edges of screen
        float edgeFade = 1.0f;
        if (lineY < 80.0f) edgeFade = std::max(0.0f, lineY / 80.0f);
        if (lineY > SCREEN_HEIGHT - 80.0f) edgeFade = std::max(0.0f, (SCREEN_HEIGHT - lineY) / 80.0f);

        TTF_Font* font = (isTitle && m_fontTitle) ? m_fontTitle : m_fontBody;
        if (!font) font = game->getFont();
        if (!font) continue;

        SDL_Color col;
        if (isTitle) {
            // Dimension-shifting color for main title
            float shift = 0.5f + 0.5f * std::sin(m_time * 0.8f);
            col = {
                static_cast<Uint8>((80 + 120 * shift) * edgeFade),
                static_cast<Uint8>((40 + 20 * (1.0f - shift)) * edgeFade),
                static_cast<Uint8>((200 - 80 * shift) * edgeFade),
                255
            };
        } else if (isHeading) {
            col = {
                static_cast<Uint8>(180 * edgeFade),
                static_cast<Uint8>(140 * edgeFade),
                static_cast<Uint8>(255 * edgeFade),
                255
            };
        } else {
            col = {
                static_cast<Uint8>(190 * edgeFade),
                static_cast<Uint8>(180 * edgeFade),
                static_cast<Uint8>(210 * edgeFade),
                255
            };
        }

        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, m_lines[i].c_str(), col);
        if (!surf) continue;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            int tw = surf->w;
            int th = surf->h;
            // Scale up title
            if (isTitle) { tw *= 2; th *= 2; }
            int tx = SCREEN_WIDTH / 2 - tw / 2;
            SDL_Rect dst = {tx, static_cast<int>(lineY), tw, th};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }

    // Top and bottom gradient overlays for smooth fade
    for (int i = 0; i < 60; i++) {
        Uint8 a = static_cast<Uint8>(255 - i * 4);
        SDL_SetRenderDrawColor(renderer, 8, 4, 16, a);
        SDL_RenderDrawLine(renderer, 0, i, SCREEN_WIDTH, i);
        SDL_RenderDrawLine(renderer, 0, SCREEN_HEIGHT - 1 - i, SCREEN_WIDTH, SCREEN_HEIGHT - 1 - i);
    }

    // Controls hint at bottom
    TTF_Font* hintFont = m_fontBody ? m_fontBody : game->getFont();
    if (hintFont) {
        SDL_Color hint = {80, 60, 100, 180};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(hintFont, LOC("credits.back"), hint);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, 180);
                SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, SCREEN_HEIGHT - 60,
                                surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }
}
