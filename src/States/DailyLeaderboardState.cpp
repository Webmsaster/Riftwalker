#include "DailyLeaderboardState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/ClassSystem.h"
#include "Game/DailyRun.h"
#include "UI/UITextures.h"
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

// --- Static render helpers (same pattern as RunHistoryState) ---

static void dlRenderText(SDL_Renderer* renderer, TTF_Font* font,
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

static void dlRenderTextCentered(SDL_Renderer* renderer, TTF_Font* font,
                                  const char* text, int cx, int y, SDL_Color color) {
    if (!font || !text) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect rect = {cx - surface->w / 2, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static const char* getDLClassName(int cls) {
    if (cls >= 0 && cls < static_cast<int>(PlayerClass::COUNT))
        return ClassSystem::getData(static_cast<PlayerClass>(cls)).name;
    return "Unknown";
}

// ---------------------------------------------------------------

void DailyLeaderboardState::enter() {
    m_time         = 0.f;
    m_fadeIn       = 0.f;
    m_newBestPulse = 0.f;
    m_scrollOffset = 0;

    m_todayDate = DailyRun::getTodayDate();
    m_todaySeed = DailyRun::getTodaySeed();

    // Load leaderboard data from disk
    DailyRun dailyRun;
    dailyRun.load("riftwalker_daily.dat");

    m_daySummaries = dailyRun.getDaySummaries();

    // Collect only today's entries, sorted by score descending
    m_todayEntries.clear();
    for (const auto& e : dailyRun.getEntries()) {
        if (std::string(e.date) == m_todayDate)
            m_todayEntries.push_back(e);
    }
    std::sort(m_todayEntries.begin(), m_todayEntries.end(),
              [](const DailyLeaderboardEntry& a, const DailyLeaderboardEntry& b) {
                  return a.score > b.score;
              });
}

void DailyLeaderboardState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_BACKSPACE:
                AudioManager::instance().play(SFX::MenuSelect);
                game->popState();
                break;
            case SDL_SCANCODE_UP:
            case SDL_SCANCODE_W:
                if (m_scrollOffset > 0) { m_scrollOffset--; AudioManager::instance().play(SFX::MenuSelect); }
                break;
            case SDL_SCANCODE_DOWN:
            case SDL_SCANCODE_S:
                m_scrollOffset++;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_PAGEUP:
                m_scrollOffset = std::max(0, m_scrollOffset - 10);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_PAGEDOWN:
                m_scrollOffset += 10;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_HOME:
                m_scrollOffset = 0;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_END:
                m_scrollOffset = static_cast<int>(m_todayEntries.size());
                AudioManager::instance().play(SFX::MenuSelect);
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
        }
    }
}

void DailyLeaderboardState::update(float dt) {
    m_time += dt;
    if (m_fadeIn < 1.f) {
        m_fadeIn += dt * 1.2f;
        if (m_fadeIn > 1.f) m_fadeIn = 1.f;
    }
    m_newBestPulse += dt * 5.0f;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    if (input.isActionPressed(Action::MenuUp)) {
        if (m_scrollOffset > 0) { m_scrollOffset--; AudioManager::instance().play(SFX::MenuSelect); }
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_scrollOffset++;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuSelect);
        game->popState();
    }
}

void DailyLeaderboardState::render(SDL_Renderer* renderer) {
    // Background
    SDL_SetRenderDrawColor(renderer, 6, 4, 14, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Subtle star field
    for (int i = 0; i < 50; i++) {
        float x = static_cast<float>((i * 157 + 31) % SCREEN_WIDTH);
        float y = static_cast<float>((i * 113 + 17) % SCREEN_HEIGHT);
        float tw = 0.5f + 0.5f * std::sin(m_time * 1.5f + i * 0.8f);
        Uint8 sa = static_cast<Uint8>(15 + 20 * tw);
        SDL_SetRenderDrawColor(renderer, 120, 100, 200, sa);
        SDL_Rect star = {static_cast<int>(x), static_cast<int>(y), 2, 2};
        SDL_RenderFillRect(renderer, &star);
    }

    TTF_Font* font = game->getFont();
    Uint8 alpha = static_cast<Uint8>(255 * m_fadeIn);

    // --- Title ---
    {
        SDL_Color titleColor = {220, 190, 80, alpha};
        dlRenderTextCentered(renderer, font, LOC("daily.title"), SCREEN_WIDTH / 2, 44, titleColor);
        SDL_SetRenderDrawColor(renderer, 220, 190, 80, static_cast<Uint8>(alpha * 0.5f));
        SDL_RenderDrawLine(renderer, 600, 92, 1960, 92);
    }

    // Today's date and seed
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), LOC("daily.today_seed"),
                      m_todayDate.c_str(), m_todaySeed);
        SDL_Color infoColor = {140, 130, 180, alpha};
        dlRenderTextCentered(renderer, font, buf, SCREEN_WIDTH / 2, 104, infoColor);
    }

    // Today's personal best (only show when there ARE entries — empty state shown
    // once below in the table area, not duplicated in the header)
    if (!m_todayEntries.empty()) {
        int todayBest = m_todayEntries[0].score;
        char buf[64];
        std::snprintf(buf, sizeof(buf), LOC("daily.best_today"), todayBest);
        SDL_Color bestColor = {255, 220, 60, alpha};
        dlRenderTextCentered(renderer, font, buf, SCREEN_WIDTH / 2, 140, bestColor);
    }

    // --- Today's Top 10 Table ---
    int tableX  = 100;
    int tableY  = 200;
    int tableW  = 1640;
    int rowH    = 52;

    // Table header background
    SDL_Rect headerBg = {tableX - 5, tableY - 3, tableW + 10, 44};
    if (!renderPanelBg(renderer, headerBg, 200)) {
        SDL_SetRenderDrawColor(renderer, 25, 20, 45, 200);
        SDL_RenderFillRect(renderer, &headerBg);
    }
    SDL_SetRenderDrawColor(renderer, 220, 190, 80, static_cast<Uint8>(alpha * 0.6f));
    SDL_RenderDrawRect(renderer, &headerBg);

    SDL_Color headerCol = {255, 230, 150, alpha};
    dlRenderText(renderer, font, "#",                    tableX,         tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.score"),   tableX + 70,   tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.class"),   tableX + 310,  tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.floors"),  tableX + 540,  tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.kills"),   tableX + 680,  tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.rifts"),   tableX + 820,  tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.combo"),   tableX + 960,  tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.time"),    tableX + 1110, tableY, headerCol);
    dlRenderText(renderer, font, LOC("daily.result"),  tableX + 1300, tableY, headerCol);

    SDL_SetRenderDrawColor(renderer, 180, 160, 70, static_cast<Uint8>(alpha * 0.5f));
    SDL_RenderDrawLine(renderer, tableX - 5, tableY + 40, tableX + tableW + 5, tableY + 40);

    int startRow   = tableY + 48;
    int maxVisible = 10;
    int maxScroll  = std::max(0, static_cast<int>(m_todayEntries.size()) - maxVisible);
    m_scrollOffset = std::min(m_scrollOffset, maxScroll);
    m_scrollOffset = std::max(m_scrollOffset, 0);

    if (m_todayEntries.empty()) {
        dlRenderTextCentered(renderer, font, LOC("daily.no_runs_recorded"),
                             tableX + tableW / 2, startRow + 40,
                             {100, 100, 130, alpha});
    }

    for (int i = 0; i < maxVisible && (i + m_scrollOffset) < static_cast<int>(m_todayEntries.size()); i++) {
        int idx  = i + m_scrollOffset;
        const auto& e = m_todayEntries[idx];
        int ry   = startRow + i * rowH;
        int rank = idx + 1;

        // Row background
        Uint8 rowA = static_cast<Uint8>(80 * m_fadeIn);
        if (rank == 1)
            SDL_SetRenderDrawColor(renderer, 40, 30, 10, rowA);
        else if (rank == 2)
            SDL_SetRenderDrawColor(renderer, 25, 25, 30, rowA);
        else if (rank == 3)
            SDL_SetRenderDrawColor(renderer, 30, 18, 10, rowA);
        else
            SDL_SetRenderDrawColor(renderer, (idx % 2 == 0) ? 18 : 12, 15, 28, rowA);

        SDL_Rect rowRect = {tableX - 5, ry - 2, tableW + 10, rowH - 1};
        SDL_RenderFillRect(renderer, &rowRect);

        // Rank color: gold/silver/bronze for top 3
        SDL_Color rankColor;
        if (rank == 1)      rankColor = {255, 215, 50, alpha};
        else if (rank == 2) rankColor = {200, 200, 210, alpha};
        else if (rank == 3) rankColor = {210, 140, 60, alpha};
        else                rankColor = {170, 165, 190, alpha};

        // Highlight the run that was just submitted
        bool isHighlighted = (highlightScore >= 0 && e.score == highlightScore);
        if (isHighlighted) {
            float pulse = 0.7f + 0.3f * std::sin(m_newBestPulse);
            Uint8 pa = static_cast<Uint8>(alpha * pulse);
            SDL_SetRenderDrawColor(renderer, 80, 200, 100, static_cast<Uint8>(pa * 0.25f));
            SDL_RenderFillRect(renderer, &rowRect);
            rankColor.r = static_cast<Uint8>(std::min(255, rankColor.r + 40));
            rankColor.g = static_cast<Uint8>(std::min(255, rankColor.g + 60));
        }

        char buf[64];

        // Rank number
        std::snprintf(buf, sizeof(buf), "%d", rank);
        dlRenderText(renderer, font, buf, tableX, ry, rankColor);

        // Score (with medal prefix for top 3)
        if      (rank == 1) std::snprintf(buf, sizeof(buf), "[1] %d", e.score);
        else if (rank == 2) std::snprintf(buf, sizeof(buf), "[2] %d", e.score);
        else if (rank == 3) std::snprintf(buf, sizeof(buf), "[3] %d", e.score);
        else                std::snprintf(buf, sizeof(buf), "%d", e.score);
        dlRenderText(renderer, font, buf, tableX + 70, ry, rankColor);

        // Class
        dlRenderText(renderer, font, getDLClassName(e.playerClass), tableX + 310, ry, rankColor);

        // Floors
        std::snprintf(buf, sizeof(buf), "%d", e.floors);
        dlRenderText(renderer, font, buf, tableX + 540, ry, rankColor);

        // Kills
        std::snprintf(buf, sizeof(buf), "%d", e.kills);
        dlRenderText(renderer, font, buf, tableX + 680, ry, rankColor);

        // Rifts
        std::snprintf(buf, sizeof(buf), "%d", e.rifts);
        dlRenderText(renderer, font, buf, tableX + 820, ry, rankColor);

        // Best combo
        std::snprintf(buf, sizeof(buf), "%d", e.bestCombo);
        dlRenderText(renderer, font, buf, tableX + 960, ry, rankColor);

        // Time mm:ss
        int mins = static_cast<int>(e.runTime) / 60;
        int secs = static_cast<int>(e.runTime) % 60;
        std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        dlRenderText(renderer, font, buf, tableX + 1110, ry, rankColor);

        // Result
        const char* resultStr = (e.deathCause == 5) ? LOC("daily.victory") : LOC("daily.fallen");
        SDL_Color resultColor = (e.deathCause == 5) ? SDL_Color{80, 255, 80, alpha}
                                                     : SDL_Color{255, 80, 80, alpha};
        dlRenderText(renderer, font, resultStr, tableX + 1300, ry, resultColor);
    }

    // Scroll indicator
    if (static_cast<int>(m_todayEntries.size()) > maxVisible) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "[W/S] %d/%d",
                      m_scrollOffset + 1, static_cast<int>(m_todayEntries.size()));
        dlRenderText(renderer, font, buf,
                     tableX + tableW / 2 - 60,
                     startRow + maxVisible * rowH + 8,
                     {100, 100, 130, alpha});
    }

    // --- "NEW BEST!" pulsing banner (top-right corner) ---
    if (isNewBest && m_fadeIn >= 0.8f) {
        float pulse = 0.7f + 0.3f * std::sin(m_newBestPulse);
        Uint8 ba = static_cast<Uint8>(230 * pulse * m_fadeIn);

        SDL_Rect banner = {SCREEN_WIDTH - 460, 40, 420, 72};
        if (!renderPanelBg(renderer, banner, static_cast<Uint8>(ba * 0.15f), "assets/textures/ui/panel_light.png")) {
            SDL_SetRenderDrawColor(renderer, 255, 200, 50, static_cast<Uint8>(ba * 0.15f));
            SDL_RenderFillRect(renderer, &banner);
        }
        SDL_SetRenderDrawColor(renderer, 255, 220, 80, ba);
        SDL_RenderDrawRect(renderer, &banner);

        SDL_Color newBestColor = {255, 220, 50, ba};
        dlRenderTextCentered(renderer, font, LOC("summary.new_best"), SCREEN_WIDTH - 250, 62, newBestColor);

        // Orbiting sparkles
        for (int i = 0; i < 6; i++) {
            float angle = m_newBestPulse * 0.8f + i * (6.283185f / 6);
            int sx = SCREEN_WIDTH - 250 + static_cast<int>(std::cos(angle) * 180);
            int sy = 76 + static_cast<int>(std::sin(angle) * 24);
            Uint8 spa = static_cast<Uint8>(ba * (0.5f + 0.5f * std::sin(m_newBestPulse + i * 1.2f)));
            SDL_SetRenderDrawColor(renderer, 255, 230, 100, spa);
            SDL_Rect spark = {sx - 2, sy - 2, 5, 5};
            SDL_RenderFillRect(renderer, &spark);
        }
    }

    // --- Previous days' best scores (compact list on the right) ---
    {
        int prevX = tableX + tableW + 60;
        int prevY = 200;

        SDL_Color sectionHeader = {160, 140, 200, alpha};
        dlRenderText(renderer, font, LOC("daily.previous_days"), prevX, prevY, sectionHeader);
        prevY += 40;

        SDL_SetRenderDrawColor(renderer, 100, 80, 160, static_cast<Uint8>(alpha * 0.4f));
        SDL_RenderDrawLine(renderer, prevX, prevY, prevX + 540, prevY);
        prevY += 16;

        bool anyPrev = false;
        int shown = 0;
        for (const auto& ds : m_daySummaries) {
            if (ds.date == m_todayDate) continue;  // skip today
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s  %d", ds.date.c_str(), ds.bestScore);
            dlRenderText(renderer, font, buf, prevX, prevY, {160, 155, 185, alpha});
            prevY += 36;
            anyPrev = true;
            if (++shown >= 6) break;
        }
        if (!anyPrev) {
            dlRenderText(renderer, font, LOC("daily.no_history"), prevX, prevY, {100, 100, 130, alpha});
        }
    }

    // Back hint
    dlRenderText(renderer, font, LOC("daily.back_hint"), 40, SCREEN_HEIGHT - 56, {100, 100, 130, alpha});

    // Fade-in overlay
    if (m_fadeIn < 1.f) {
        Uint8 fa = static_cast<Uint8>((1.f - m_fadeIn) * 255);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, fa);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);
    }
}
