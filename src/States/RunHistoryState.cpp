#include "RunHistoryState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/UpgradeSystem.h"
#include "Game/ClassSystem.h"
#include "UI/UITextures.h"
#include <cstdio>
#include <cmath>

static void renderText(SDL_Renderer* renderer, TTF_Font* font,
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

void RunHistoryState::enter() {
    m_scrollOffset = 0;
    m_time = 0;
}

void RunHistoryState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_BACKSPACE:
                AudioManager::instance().play(SFX::MenuSelect);
                game->popState();
                break;
            case SDL_SCANCODE_UP: case SDL_SCANCODE_W:
                if (m_scrollOffset > 0) { m_scrollOffset--; AudioManager::instance().play(SFX::MenuSelect); }
                break;
            case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S:
                m_scrollOffset++;
                AudioManager::instance().play(SFX::MenuSelect);
                {
                    const auto& hist = game->getUpgradeSystem().getRunHistory();
                    int maxVis = (Game::SCREEN_HEIGHT - 320) / 48; // match render maxVisible calc
                    int maxScroll = std::max(0, static_cast<int>(hist.size()) - maxVis);
                    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
                }
                break;
            default: break;
        }
    }

    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuSelect);
        game->popState();
    }

    // Mouse wheel scrolling
    if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0 && m_scrollOffset > 0) {
            m_scrollOffset--;
        } else if (event.wheel.y < 0) {
            m_scrollOffset++;
            const auto& hist = game->getUpgradeSystem().getRunHistory();
            int maxVis = (Game::SCREEN_HEIGHT - 320) / 48;
            int maxScroll = std::max(0, static_cast<int>(hist.size()) - maxVis);
            if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
        }
    }
}

void RunHistoryState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    if (input.isActionPressed(Action::MenuUp)) {
        if (m_scrollOffset > 0) { m_scrollOffset--; AudioManager::instance().play(SFX::MenuSelect); }
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_scrollOffset++;
        AudioManager::instance().play(SFX::MenuSelect);
        const auto& hist = game->getUpgradeSystem().getRunHistory();
        int maxVis = (Game::SCREEN_HEIGHT - 320) / 48;
        int maxScroll = std::max(0, static_cast<int>(hist.size()) - maxVis);
        if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuSelect);
        game->popState();
    }
}

void RunHistoryState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 15, 12, 25, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    TTF_Font* font = game->getFont();
    auto& upgrades = game->getUpgradeSystem();
    const auto& history = upgrades.getRunHistory();

    int screenW = Game::SCREEN_WIDTH;
    int screenH = Game::SCREEN_HEIGHT;

    // Title
    renderText(renderer, font, LOC("history.title"), screenW / 2 - 120, 50, {200, 180, 255, 255});

    // Lifetime stats panel
    int panelX = 80;
    int panelY = 130;
    SDL_Rect statsBg = {panelX - 10, panelY - 5, 1000, 110};
    if (!renderPanelBg(renderer, statsBg, 200)) {
        SDL_SetRenderDrawColor(renderer, 30, 25, 50, 200);
        SDL_RenderFillRect(renderer, &statsBg);
    }
    SDL_SetRenderDrawColor(renderer, 100, 80, 160, 120);
    SDL_RenderDrawRect(renderer, &statsBg);

    char buf[128];
    std::snprintf(buf, sizeof(buf), LOC("history.stats"),
                  upgrades.totalRuns, upgrades.totalEnemiesKilled,
                  upgrades.bestRoomReached, upgrades.totalRiftsRepaired);
    renderText(renderer, font, buf, panelX, panelY + 4, {180, 180, 200, 220});

    // Averages
    if (upgrades.totalRuns > 0) {
        float avgKills = static_cast<float>(upgrades.totalEnemiesKilled) / upgrades.totalRuns;
        std::snprintf(buf, sizeof(buf), LOC("history.avg_kills"), avgKills);
        renderText(renderer, font, buf, panelX, panelY + 50, {150, 160, 180, 200});
    }

    // Table header
    int tableY = 270;
    SDL_Rect headerBg = {panelX - 10, tableY - 3, screenW - 160, 44};
    if (!renderPanelBg(renderer, headerBg, 220)) {
        SDL_SetRenderDrawColor(renderer, 40, 35, 60, 220);
        SDL_RenderFillRect(renderer, &headerBg);
    }

    SDL_Color headerCol = {160, 150, 200, 255};
    renderText(renderer, font, "#",                      panelX,        tableY, headerCol);
    renderText(renderer, font, LOC("history.class"),   panelX + 60,  tableY, headerCol);
    renderText(renderer, font, LOC("history.floor"),   panelX + 260, tableY, headerCol);
    renderText(renderer, font, LOC("history.kills"),   panelX + 390, tableY, headerCol);
    renderText(renderer, font, LOC("history.rifts"),   panelX + 520, tableY, headerCol);
    renderText(renderer, font, LOC("history.combo"),   panelX + 650, tableY, headerCol);
    renderText(renderer, font, LOC("history.time"),    panelX + 800, tableY, headerCol);
    renderText(renderer, font, LOC("history.shards"),  panelX + 960, tableY, headerCol);
    renderText(renderer, font, LOC("history.diff"),    panelX + 1120, tableY, headerCol);
    renderText(renderer, font, LOC("history.result"),  panelX + 1240, tableY, headerCol);

    // Separator
    SDL_SetRenderDrawColor(renderer, 80, 70, 120, 150);
    SDL_RenderDrawLine(renderer, panelX - 10, tableY + 40, screenW - 80, tableY + 40);

    // Run rows
    int rowH = 48;
    int startY = tableY + 50;
    int maxVisible = (screenH - startY - 40) / rowH;
    int clampedScroll = std::min(m_scrollOffset,
        std::max(0, static_cast<int>(history.size()) - maxVisible));
    m_scrollOffset = clampedScroll;

    for (int i = 0; i < maxVisible && (i + clampedScroll) < static_cast<int>(history.size()); i++) {
        int idx = i + clampedScroll;
        const auto& run = history[idx];
        int ry = startY + i * rowH;

        // Alternating row background
        if (idx % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 25, 22, 40, 100);
            SDL_Rect rowBg = {panelX - 10, ry - 2, screenW - 160, rowH};
            SDL_RenderFillRect(renderer, &rowBg);
        }

        // Highlight best run (index 0)
        SDL_Color rowCol = {180, 180, 190, 220};
        if (idx == 0) rowCol = {255, 215, 80, 255}; // Gold for best

        // Run number
        std::snprintf(buf, sizeof(buf), "%d", idx + 1);
        renderText(renderer, font, buf, panelX, ry, rowCol);

        // Class name
        const char* className = "Unknown";
        if (run.playerClass >= 0 && run.playerClass < static_cast<int>(PlayerClass::COUNT))
            className = ClassSystem::getData(static_cast<PlayerClass>(run.playerClass)).name;
        renderText(renderer, font, className, panelX + 60, ry, rowCol);

        // Floor
        std::snprintf(buf, sizeof(buf), "%d", run.rooms);
        renderText(renderer, font, buf, panelX + 260, ry, rowCol);

        // Kills
        std::snprintf(buf, sizeof(buf), "%d", run.enemies);
        renderText(renderer, font, buf, panelX + 390, ry, rowCol);

        // Rifts
        std::snprintf(buf, sizeof(buf), "%d", run.rifts);
        renderText(renderer, font, buf, panelX + 520, ry, rowCol);

        // Best combo
        std::snprintf(buf, sizeof(buf), "%d", run.bestCombo);
        renderText(renderer, font, buf, panelX + 650, ry, rowCol);

        // Time (mm:ss)
        int mins = static_cast<int>(run.runTime) / 60;
        int secs = static_cast<int>(run.runTime) % 60;
        std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        renderText(renderer, font, buf, panelX + 800, ry, rowCol);

        // Shards
        std::snprintf(buf, sizeof(buf), "%d", run.shards);
        renderText(renderer, font, buf, panelX + 960, ry, rowCol);

        // Difficulty
        std::snprintf(buf, sizeof(buf), "%d", run.difficulty);
        renderText(renderer, font, buf, panelX + 1120, ry, rowCol);

        // Death cause
        const char* result = UpgradeSystem::getDeathCauseName(run.deathCause);
        SDL_Color resultCol = (run.deathCause == 5) ? SDL_Color{80, 255, 80, 255} : SDL_Color{255, 80, 80, 220};
        renderText(renderer, font, result, panelX + 1240, ry, resultCol);
    }

    // Scroll indicator
    if (static_cast<int>(history.size()) > maxVisible) {
        std::snprintf(buf, sizeof(buf), LOC("history.scroll"),
                      clampedScroll + 1, static_cast<int>(history.size()));
        renderText(renderer, font, buf, screenW / 2 - 160, screenH - 60, {120, 120, 150, 180});
    }

    // Empty state
    if (history.empty()) {
        renderText(renderer, font, LOC("history.empty"),
                   screenW / 2 - 260, screenH / 2, {120, 120, 150, 200});
    }

    // Back hint
    renderText(renderer, font, LOC("history.back"), 40, screenH - 60, {100, 100, 120, 180});
}
