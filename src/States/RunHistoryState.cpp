#include "RunHistoryState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Game/UpgradeSystem.h"
#include "Game/ClassSystem.h"
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
                if (m_scrollOffset > 0) m_scrollOffset--;
                break;
            case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S:
                m_scrollOffset++;
                {
                    const auto& hist = game->getUpgradeSystem().getRunHistory();
                    int maxVis = (Game::SCREEN_HEIGHT - 160) / 24; // match render maxVisible calc
                    int maxScroll = std::max(0, static_cast<int>(hist.size()) - maxVis);
                    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
                }
                break;
            default: break;
        }
    }
}

void RunHistoryState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    if (input.isActionPressed(Action::MenuUp)) {
        if (m_scrollOffset > 0) m_scrollOffset--;
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_scrollOffset++;
        const auto& hist = game->getUpgradeSystem().getRunHistory();
        int maxVis = (Game::SCREEN_HEIGHT - 160) / 24;
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
    renderText(renderer, font, "RUN HISTORY", screenW / 2 - 60, 25, {200, 180, 255, 255});

    // Lifetime stats panel
    int panelX = 40;
    int panelY = 65;
    SDL_SetRenderDrawColor(renderer, 30, 25, 50, 200);
    SDL_Rect statsBg = {panelX - 10, panelY - 5, 500, 55};
    SDL_RenderFillRect(renderer, &statsBg);
    SDL_SetRenderDrawColor(renderer, 100, 80, 160, 120);
    SDL_RenderDrawRect(renderer, &statsBg);

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Total Runs: %d    Enemies Killed: %d    Best Floor: %d    Rifts: %d",
                  upgrades.totalRuns, upgrades.totalEnemiesKilled,
                  upgrades.bestRoomReached, upgrades.totalRiftsRepaired);
    renderText(renderer, font, buf, panelX, panelY + 2, {180, 180, 200, 220});

    // Averages
    if (upgrades.totalRuns > 0) {
        float avgKills = static_cast<float>(upgrades.totalEnemiesKilled) / upgrades.totalRuns;
        std::snprintf(buf, sizeof(buf), "Avg Kills/Run: %.1f", avgKills);
        renderText(renderer, font, buf, panelX, panelY + 25, {150, 160, 180, 200});
    }

    // Table header
    int tableY = 135;
    SDL_SetRenderDrawColor(renderer, 40, 35, 60, 220);
    SDL_Rect headerBg = {panelX - 10, tableY - 3, screenW - 80, 22};
    SDL_RenderFillRect(renderer, &headerBg);

    SDL_Color headerCol = {160, 150, 200, 255};
    renderText(renderer, font, "#",    panelX,       tableY, headerCol);
    renderText(renderer, font, "Class", panelX + 30,  tableY, headerCol);
    renderText(renderer, font, "Floor", panelX + 130, tableY, headerCol);
    renderText(renderer, font, "Kills", panelX + 195, tableY, headerCol);
    renderText(renderer, font, "Rifts", panelX + 260, tableY, headerCol);
    renderText(renderer, font, "Combo", panelX + 325, tableY, headerCol);
    renderText(renderer, font, "Time",  panelX + 400, tableY, headerCol);
    renderText(renderer, font, "Shards", panelX + 480, tableY, headerCol);
    renderText(renderer, font, "Diff",  panelX + 560, tableY, headerCol);
    renderText(renderer, font, "Result", panelX + 620, tableY, headerCol);

    // Separator
    SDL_SetRenderDrawColor(renderer, 80, 70, 120, 150);
    SDL_RenderDrawLine(renderer, panelX - 10, tableY + 20, screenW - 40, tableY + 20);

    // Run rows
    int rowH = 24;
    int startY = tableY + 25;
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
            SDL_Rect rowBg = {panelX - 10, ry - 2, screenW - 80, rowH};
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
        renderText(renderer, font, className, panelX + 30, ry, rowCol);

        // Floor
        std::snprintf(buf, sizeof(buf), "%d", run.rooms);
        renderText(renderer, font, buf, panelX + 130, ry, rowCol);

        // Kills
        std::snprintf(buf, sizeof(buf), "%d", run.enemies);
        renderText(renderer, font, buf, panelX + 195, ry, rowCol);

        // Rifts
        std::snprintf(buf, sizeof(buf), "%d", run.rifts);
        renderText(renderer, font, buf, panelX + 260, ry, rowCol);

        // Best combo
        std::snprintf(buf, sizeof(buf), "%d", run.bestCombo);
        renderText(renderer, font, buf, panelX + 325, ry, rowCol);

        // Time (mm:ss)
        int mins = static_cast<int>(run.runTime) / 60;
        int secs = static_cast<int>(run.runTime) % 60;
        std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        renderText(renderer, font, buf, panelX + 400, ry, rowCol);

        // Shards
        std::snprintf(buf, sizeof(buf), "%d", run.shards);
        renderText(renderer, font, buf, panelX + 480, ry, rowCol);

        // Difficulty
        std::snprintf(buf, sizeof(buf), "%d", run.difficulty);
        renderText(renderer, font, buf, panelX + 560, ry, rowCol);

        // Death cause
        const char* result = UpgradeSystem::getDeathCauseName(run.deathCause);
        SDL_Color resultCol = (run.deathCause == 5) ? SDL_Color{80, 255, 80, 255} : SDL_Color{255, 80, 80, 220};
        renderText(renderer, font, result, panelX + 620, ry, resultCol);
    }

    // Scroll indicator
    if (static_cast<int>(history.size()) > maxVisible) {
        std::snprintf(buf, sizeof(buf), "[W/S to scroll - %d/%d]",
                      clampedScroll + 1, static_cast<int>(history.size()));
        renderText(renderer, font, buf, screenW / 2 - 80, screenH - 30, {120, 120, 150, 180});
    }

    // Empty state
    if (history.empty()) {
        renderText(renderer, font, "No runs recorded yet. Play some runs!",
                   screenW / 2 - 130, screenH / 2, {120, 120, 150, 200});
    }

    // Back hint
    renderText(renderer, font, "[ESC] Back", 20, screenH - 30, {100, 100, 120, 180});
}
