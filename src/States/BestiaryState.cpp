#include "BestiaryState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/Bestiary.h"
#include "Components/AIComponent.h"
#include "UI/UITextures.h"
#include <cmath>
#include <cstdio>
#include <string>

// ---- layout constants (scaled for 2560x1440) ----
static constexpr int LIST_X     = 40;
static constexpr int LIST_Y     = 190;
static constexpr int LIST_W     = 640;
static constexpr int ROW_H      = 88;
static constexpr int VISIBLE    = 12;

static constexpr int DETAIL_X   = 712;
static constexpr int DETAIL_Y   = 180;
static constexpr int DETAIL_W   = 1808;
static constexpr int DETAIL_H   = 1180;

static constexpr int PREVIEW_CX = DETAIL_X + 260;
static constexpr int PREVIEW_CY = DETAIL_Y + 260;

// ---- helpers ----
static void drawText(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                     int x, int y, SDL_Color color, float scale = 1.0f) {
    if (!text || text[0] == '\0') return;
    SDL_Surface* s = TTF_RenderText_Blended(font, text, color);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    if (t) {
        SDL_Rect r = {x, y, static_cast<int>(s->w * scale), static_cast<int>(s->h * scale)};
        SDL_RenderCopy(renderer, t, nullptr, &r);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

static void drawTextCentered(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                              int cx, int y, SDL_Color color, float scale = 1.0f) {
    if (!text || text[0] == '\0') return;
    SDL_Surface* s = TTF_RenderText_Blended(font, text, color);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    if (t) {
        int w = static_cast<int>(s->w * scale);
        int h = static_cast<int>(s->h * scale);
        SDL_Rect r = {cx - w / 2, y, w, h};
        SDL_RenderCopy(renderer, t, nullptr, &r);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

// Draw a filled bar: background then filled portion
static void drawBar(SDL_Renderer* renderer, int x, int y, int w, int h,
                    float fraction, SDL_Color barColor, SDL_Color bgColor) {
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_Rect bg = {x, y, w, h};
    SDL_RenderFillRect(renderer, &bg);
    if (fraction > 0.0f) {
        int fillW = static_cast<int>(w * fraction);
        SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, barColor.a);
        SDL_Rect fill = {x, y, fillW, h};
        SDL_RenderFillRect(renderer, &fill);
    }
    SDL_SetRenderDrawColor(renderer, 60, 55, 80, 120);
    SDL_RenderDrawRect(renderer, &bg);
}

// Returns color for each EnemyElement
static SDL_Color elementColor(EnemyElement el) {
    switch (el) {
        case EnemyElement::Fire:     return {255, 120,  40, 255};
        case EnemyElement::Ice:      return { 80, 180, 255, 255};
        case EnemyElement::Electric: return {255, 230,  60, 255};
        default:                     return {160, 150, 180, 200};
    }
}
static const char* elementName(EnemyElement el) {
    switch (el) {
        case EnemyElement::Fire:     return LOC("element.fire");
        case EnemyElement::Ice:      return LOC("element.ice");
        case EnemyElement::Electric: return LOC("element.electric");
        default:                     return LOC("element.none");
    }
}

// Draw word-wrapped text within a given pixel width.
// Returns the total height used (in pixels) so callers can offset subsequent content.
static int drawTextWrapped(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                            int x, int y, int maxWidth, SDL_Color color, int lineSpacing = 4) {
    if (!text || text[0] == '\0' || !font) return 0;

    std::string src(text);
    int curY    = y;
    int charH   = TTF_FontHeight(font);
    size_t pos  = 0;

    while (pos < src.size()) {
        // Find the longest word-boundary substring that fits in maxWidth
        size_t lineEnd   = pos;
        size_t lastSpace = pos;
        bool   first     = true;

        while (lineEnd < src.size()) {
            size_t next = src.find(' ', lineEnd + (first ? 0 : 1));
            if (next == std::string::npos) next = src.size();
            else next++; // include the space in the candidate

            std::string candidate = src.substr(pos, next - pos);
            int w = 0, h = 0;
            TTF_SizeText(font, candidate.c_str(), &w, &h);
            if (w > maxWidth && !first) break;
            lastSpace = next;
            lineEnd   = next;
            first     = false;
            if (next >= src.size()) break;
        }

        if (lineEnd == pos) lineEnd = pos + 1; // safety: advance at least one character

        std::string line = src.substr(pos, lastSpace - pos);
        // Strip trailing space
        if (!line.empty() && line.back() == ' ') line.pop_back();

        if (!line.empty()) {
            SDL_Surface* s = TTF_RenderText_Blended(font, line.c_str(), color);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {x, curY, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
        curY += charH + lineSpacing;
        pos   = lastSpace;
    }
    return curY - y;
}

// ---- Enter ----
void BestiaryState::enter() {
    m_selected    = 0;
    m_time        = 0;
    m_scrollOffset = 0;
    // Regular enemies (EnemyType::COUNT - 1, exclude Boss sentinel) + 6 bosses
    m_totalEntries = static_cast<int>(EnemyType::COUNT) - 1 + 6;
}

// ---- Event ----
void BestiaryState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_selected = (m_selected - 1 + m_totalEntries) % m_totalEntries;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_selected = (m_selected + 1) % m_totalEntries;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_PAGEUP:
                m_selected = std::max(0, m_selected - 8);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_PAGEDOWN:
                m_selected = std::min(m_totalEntries - 1, m_selected + 8);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_HOME:
                m_selected = 0;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_END:
                m_selected = m_totalEntries - 1;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::Menu);
                break;
            default: break;
        }
        // Scroll to keep selected in view
        if (m_selected < m_scrollOffset) m_scrollOffset = m_selected;
        if (m_selected >= m_scrollOffset + VISIBLE) m_scrollOffset = m_selected - VISIBLE + 1;
    }

    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
    }

    // Mouse hover: highlight entry in list
    if (event.type == SDL_MOUSEMOTION) {
        int mx = event.motion.x, my = event.motion.y;
        for (int vi = 0; vi < VISIBLE && (vi + m_scrollOffset) < m_totalEntries; vi++) {
            int idx = vi + m_scrollOffset;
            int ey = LIST_Y + vi * (ROW_H + 8);
            if (mx >= LIST_X && mx < LIST_X + LIST_W && my >= ey && my < ey + ROW_H) {
                if (idx != m_selected) {
                    m_selected = idx;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                break;
            }
        }
    }

    // Mouse wheel scrolling
    if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0) {
            m_selected = (m_selected - 1 + m_totalEntries) % m_totalEntries;
            AudioManager::instance().play(SFX::MenuSelect);
        } else if (event.wheel.y < 0) {
            m_selected = (m_selected + 1) % m_totalEntries;
            AudioManager::instance().play(SFX::MenuSelect);
        }
        if (m_selected < m_scrollOffset) m_scrollOffset = m_selected;
        if (m_selected >= m_scrollOffset + VISIBLE) m_scrollOffset = m_selected - VISIBLE + 1;
    }
}

// ---- Update ----
void BestiaryState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    if (input.isActionPressed(Action::MenuUp)) {
        m_selected = (m_selected - 1 + m_totalEntries) % m_totalEntries;
        AudioManager::instance().play(SFX::MenuSelect);
        if (m_selected < m_scrollOffset) m_scrollOffset = m_selected;
        if (m_selected >= m_scrollOffset + VISIBLE) m_scrollOffset = m_selected - VISIBLE + 1;
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_selected = (m_selected + 1) % m_totalEntries;
        AudioManager::instance().play(SFX::MenuSelect);
        if (m_selected < m_scrollOffset) m_scrollOffset = m_selected;
        if (m_selected >= m_scrollOffset + VISIBLE) m_scrollOffset = m_selected - VISIBLE + 1;
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
    }
}

// ---- Render ----
void BestiaryState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 10, 8, 20, 255);
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

    // ---- Title ----
    drawTextCentered(renderer, font, LOC("bestiary.title"), SCREEN_WIDTH / 2, 36, {180, 100, 100, 255}, 1.5f);

    // Discovery counter
    {
        char countText[48];
        std::snprintf(countText, sizeof(countText), LOC("bestiary.discovered"),
                      Bestiary::getDiscoveredCount(), Bestiary::getTotalCount());
        drawTextCentered(renderer, font, countText, SCREEN_WIDTH / 2, 124, {120, 110, 150, 200});
    }

    // ---- Left: list panel ----
    {
        SDL_Rect panelBg = {LIST_X - 4, LIST_Y - 4, LIST_W + 8, VISIBLE * (ROW_H + 4) + 8};
        if (!renderPanelBg(renderer, panelBg, 200)) {
            SDL_SetRenderDrawColor(renderer, 15, 12, 28, 120);
            SDL_RenderFillRect(renderer, &panelBg);
            SDL_SetRenderDrawColor(renderer, 60, 50, 90, 60);
            SDL_RenderDrawRect(renderer, &panelBg);
        }
    }

    int regularCount = static_cast<int>(EnemyType::COUNT) - 1; // exclude Boss sentinel

    for (int vi = 0; vi < VISIBLE && (vi + m_scrollOffset) < m_totalEntries; vi++) {
        int idx      = vi + m_scrollOffset;
        bool isBoss  = (idx >= regularCount);
        int typeIdx  = isBoss ? (idx - regularCount) : idx;

        const BestiaryEntry& entry = isBoss ?
            Bestiary::getBossEntry(typeIdx) :
            Bestiary::getEntry(static_cast<EnemyType>(typeIdx));

        bool selected = (idx == m_selected);
        int ey = LIST_Y + vi * (ROW_H + 8);

        // Row background
        Uint8 bgA = selected ? 90 : 30;
        SDL_SetRenderDrawColor(renderer, 25, 20, 42, bgA);
        SDL_Rect rowBg = {LIST_X, ey, LIST_W, ROW_H};
        SDL_RenderFillRect(renderer, &rowBg);

        if (selected) {
            float pulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
            Uint8 borderA = static_cast<Uint8>(180 * pulse);
            SDL_Color bc = isBoss ? SDL_Color{255, 160, 40, borderA} : SDL_Color{180, 110, 200, borderA};
            SDL_SetRenderDrawColor(renderer, bc.r, bc.g, bc.b, borderA);
            SDL_RenderDrawRect(renderer, &rowBg);
        }

        // Small color swatch matching enemy color
        if (entry.discovered) {
            SDL_Color swatchColor = {200, 60, 60, 200};
            if (isBoss) {
                switch (typeIdx) {
                    case 0: swatchColor = {200, 40, 180, 220}; break;
                    case 1: swatchColor = {40, 180, 120, 220}; break;
                    case 2: swatchColor = {120, 80, 200, 220}; break;
                    case 3: swatchColor = {180, 160, 80, 220}; break;
                    case 4: swatchColor = {100, 40, 220, 220}; break;
                    case 5: swatchColor = {60, 200, 60, 220};  break; // Entropy Incarnate
                }
            } else {
                switch (typeIdx) {
                    case 0: swatchColor = {200, 60,  60, 220}; break;   // Walker
                    case 1: swatchColor = {180, 80, 200, 220}; break;   // Flyer
                    case 2: swatchColor = {200, 200, 60, 220}; break;   // Turret
                    case 3: swatchColor = {220, 120, 40, 220}; break;   // Charger
                    case 4: swatchColor = {100,  50, 200, 220}; break;  // Phaser
                    case 5: swatchColor = {255,  80,  30, 220}; break;  // Exploder
                    case 6: swatchColor = { 80, 180, 220, 220}; break;  // Shielder
                    case 7: swatchColor = { 60, 160,  80, 220}; break;  // Crawler
                    case 8: swatchColor = {180,  50, 220, 220}; break;  // Summoner
                    case 9: swatchColor = {200, 180,  40, 220}; break;   // Sniper
                    case 10: swatchColor = { 90,  40, 140, 220}; break;  // Teleporter
                    case 11: swatchColor = {140, 160, 190, 220}; break;  // Reflector
                    case 12: swatchColor = { 50, 100,  40, 220}; break;  // Leech
                    case 13: swatchColor = {255, 180,  50, 220}; break;  // Swarmer
                    case 14: swatchColor = {100,  50, 180, 220}; break;  // GravityWell
                    case 15: swatchColor = {140,  95,  45, 220}; break;  // Mimic
                }
            }
            SDL_SetRenderDrawColor(renderer, swatchColor.r, swatchColor.g, swatchColor.b, swatchColor.a);
            SDL_Rect swatch = {LIST_X + 12, ey + 12, 16, ROW_H - 24};
            SDL_RenderFillRect(renderer, &swatch);
        }

        // Name text
        int textX = LIST_X + 40;
        // Localized enemy name (fallback to struct data for EN)
        char bNameKey[32];
        std::snprintf(bNameKey, sizeof(bNameKey), "enemy.%d.name", static_cast<int>(entry.type));
        const char* bLocName = LOC(bNameKey);
        const char* locEnemyName = (std::strcmp(bLocName, bNameKey) == 0) ? entry.name : bLocName;
        const char* displayName = entry.discovered ? locEnemyName : (isBoss ? LOC("bestiary.boss_label") : "???");
        SDL_Color nameColor = entry.discovered ?
            (isBoss ? SDL_Color{255, 190, 90, 255} : SDL_Color{210, 195, 220, 255}) :
            SDL_Color{75, 70, 90, 140};
        if (selected) {
            nameColor.r = static_cast<Uint8>(std::min(255, nameColor.r + 25));
            nameColor.a = 255;
        }
        drawText(renderer, font, displayName, textX, ey + 10, nameColor);

        // Kill count sub-line
        if (entry.discovered) {
            char killText[32];
            std::snprintf(killText, sizeof(killText), LOC("bestiary.kills"), entry.killCount);
            drawText(renderer, font, killText, textX, ey + 48, {100, 95, 120, 140});
        } else {
            drawText(renderer, font, LOC("bestiary.unencountered"), textX, ey + 48, {65, 60, 80, 100});
        }

        // Boss badge
        if (isBoss) {
            float p = 0.5f + 0.5f * std::sin(m_time * 2.5f + typeIdx * 0.8f);
            Uint8 ba = static_cast<Uint8>(80 + 80 * p);
            SDL_SetRenderDrawColor(renderer, 255, 160, 40, ba);
            SDL_Rect badgeDot = {LIST_X + LIST_W - 24, ey + ROW_H / 2 - 8, 16, 16};
            SDL_RenderFillRect(renderer, &badgeDot);
        }
    }

    // Scroll indicator dots
    if (m_totalEntries > VISIBLE) {
        int dotsX = LIST_X + LIST_W + 8;
        int dotsAreaH = VISIBLE * (ROW_H + 4);
        for (int i = 0; i < m_totalEntries; i++) {
            int dotY = LIST_Y + static_cast<int>(dotsAreaH * i / static_cast<float>(m_totalEntries));
            bool isCur = (i == m_selected);
            SDL_SetRenderDrawColor(renderer, isCur ? 200 : 70, isCur ? 160 : 65, isCur ? 220 : 90, isCur ? 255 : 120);
            SDL_Rect dot = {dotsX, dotY, isCur ? 8 : 4, isCur ? 16 : 8};
            SDL_RenderFillRect(renderer, &dot);
        }
    }

    // ---- Right: detail panel ----
    SDL_SetRenderDrawColor(renderer, 16, 12, 28, 100);
    SDL_Rect detailBg = {DETAIL_X, DETAIL_Y, DETAIL_W, DETAIL_H};
    SDL_RenderFillRect(renderer, &detailBg);
    SDL_SetRenderDrawColor(renderer, 70, 60, 100, 70);
    SDL_RenderDrawRect(renderer, &detailBg);

    bool selIsBoss = (m_selected >= regularCount);
    int selTypeIdx = selIsBoss ? (m_selected - regularCount) : m_selected;
    const BestiaryEntry& entry = selIsBoss ?
        Bestiary::getBossEntry(selTypeIdx) :
        Bestiary::getEntry(static_cast<EnemyType>(selTypeIdx));

    if (entry.discovered) {
        renderDiscoveredDetail(renderer, font, entry, selTypeIdx, selIsBoss);
    } else {
        renderUndiscoveredDetail(renderer, font, selTypeIdx, selIsBoss);
    }

    // ---- Navigation hint ----
    drawTextCentered(renderer, font, LOC("bestiary.nav_hint"),
                     SCREEN_WIDTH / 2, SCREEN_HEIGHT - 50, {60, 55, 85, 140});
}

// ---- Discovered detail panel ----
void BestiaryState::renderDiscoveredDetail(SDL_Renderer* renderer, TTF_Font* font,
                                           const BestiaryEntry& entry, int typeIdx, bool isBoss) {
    // Preview on the left sub-panel
    {
        SDL_SetRenderDrawColor(renderer, 10, 8, 22, 100);
        SDL_Rect previewPanel = {DETAIL_X + 16, DETAIL_Y + 16, 480, DETAIL_H - 32};
        SDL_RenderFillRect(renderer, &previewPanel);
        SDL_SetRenderDrawColor(renderer, 50, 45, 75, 80);
        SDL_RenderDrawRect(renderer, &previewPanel);
    }

    renderEnemyPreview(renderer, PREVIEW_CX, PREVIEW_CY, typeIdx, isBoss);

    // Kill count below preview
    {
        char killText[32];
        std::snprintf(killText, sizeof(killText), LOC("bestiary.kills"), entry.killCount);
        SDL_Color kc = entry.killCount > 0 ? SDL_Color{180, 140, 255, 220} : SDL_Color{100, 95, 120, 160};
        drawTextCentered(renderer, font, killText, PREVIEW_CX, PREVIEW_CY + 110, kc);
    }

    // Boss badge text
    if (isBoss) {
        float p = 0.5f + 0.5f * std::sin(m_time * 2.0f);
        Uint8 a = static_cast<Uint8>(180 + 75 * p);
        drawTextCentered(renderer, font, LOC("bestiary.boss_label"), PREVIEW_CX, PREVIEW_CY + 130, {255, 180, 60, a});
    }

    // ---- Right: stats + info ----
    int infoX  = DETAIL_X + 520;
    int infoY  = DETAIL_Y + 28;
    int infoW  = DETAIL_W - 536;

    // Name header
    SDL_Color nameColor = isBoss ? SDL_Color{255, 195, 90, 255} : SDL_Color{225, 210, 235, 255};
    // Localized enemy name for detail view
    char detNameKey[32];
    std::snprintf(detNameKey, sizeof(detNameKey), "enemy.%d.name", static_cast<int>(entry.type));
    const char* detLocN = LOC(detNameKey);
    const char* detEnemyName = (std::strcmp(detLocN, detNameKey) == 0) ? entry.name : detLocN;
    drawText(renderer, font, detEnemyName, infoX, infoY, nameColor, 1.4f);

    // Divider line
    int divY = infoY + 72;
    SDL_SetRenderDrawColor(renderer, 90, 75, 120, 100);
    SDL_RenderDrawLine(renderer, infoX, divY, DETAIL_X + DETAIL_W - 32, divY);

    // Lore section header
    int loreY = divY + 16;
    drawText(renderer, font, LOC("bestiary.lore"), infoX, loreY, {120, 100, 160, 200});
    loreY += 40;

    // Word-wrapped lore body
    int loreMaxW = infoW - 20;
    int loreUsedH = drawTextWrapped(renderer, font, entry.lore,
                                    infoX, loreY, loreMaxW,
                                    {140, 130, 160, 175}, 3);

    // ---- Stat bars ----
    // Add a small gap after lore, minimum to keep stats visible
    int statsY = loreY + loreUsedH + 16;
    const int barW  = infoW - 20;
    const int barH  = 24;
    const int rowSp = 60;

    // HP bar
    {
        char hpLabel[32];
        std::snprintf(hpLabel, sizeof(hpLabel), LOC("bestiary.stat_hp"), entry.baseHP);
        drawText(renderer, font, hpLabel, infoX, statsY, {160, 220, 160, 220});
        float fraction = std::min(entry.baseHP / 400.0f, 1.0f);
        drawBar(renderer, infoX + 220, statsY + 3, barW - 220, barH, fraction,
                {80, 200, 90, 200}, {20, 30, 20, 120});
    }

    // DMG bar
    {
        char dmgLabel[32];
        std::snprintf(dmgLabel, sizeof(dmgLabel), LOC("bestiary.stat_dmg"), entry.baseDMG);
        drawText(renderer, font, dmgLabel, infoX, statsY + rowSp, {220, 140, 140, 220});
        float fraction = std::min(entry.baseDMG / 30.0f, 1.0f);
        drawBar(renderer, infoX + 220, statsY + rowSp + 3, barW - 220, barH, fraction,
                {220, 80, 80, 200}, {30, 20, 20, 120});
    }

    // Speed bar
    {
        char spLabel[32];
        if (entry.baseSpeed > 0)
            std::snprintf(spLabel, sizeof(spLabel), LOC("bestiary.stat_spd"), entry.baseSpeed);
        else
            std::snprintf(spLabel, sizeof(spLabel), "%s", LOC("bestiary.stat_spd_na"));
        drawText(renderer, font, spLabel, infoX, statsY + rowSp * 2, {140, 180, 230, 220});
        float fraction = std::min(entry.baseSpeed / 200.0f, 1.0f);
        drawBar(renderer, infoX + 220, statsY + rowSp * 2 + 3, barW - 220, barH, fraction,
                {80, 140, 220, 200}, {20, 25, 35, 120});
    }

    // Element
    {
        SDL_Color elColor = elementColor(entry.element);
        char elLabel[32];
        std::snprintf(elLabel, sizeof(elLabel), LOC("bestiary.stat_elem"), elementName(entry.element));
        drawText(renderer, font, elLabel, infoX, statsY + rowSp * 3, elColor);

        // Small element icon squares
        if (entry.element != EnemyElement::None) {
            float pulse = 0.5f + 0.5f * std::sin(m_time * 3.5f);
            Uint8 ia = static_cast<Uint8>(150 + 100 * pulse);
            SDL_SetRenderDrawColor(renderer, elColor.r, elColor.g, elColor.b, ia);
            for (int k = 0; k < 3; k++) {
                SDL_Rect sq = {infoX + 220 + k * 32, statsY + rowSp * 3 + 4, 20, 20};
                SDL_RenderFillRect(renderer, &sq);
            }
        }
    }

    // Divider
    int div2Y = statsY + rowSp * 4 + 8;
    SDL_SetRenderDrawColor(renderer, 70, 60, 100, 80);
    SDL_RenderDrawLine(renderer, infoX, div2Y, DETAIL_X + DETAIL_W - 32, div2Y);

    // Abilities
    int infoBlockY = div2Y + 16;

    drawText(renderer, font, LOC("bestiary.abilities"), infoX, infoBlockY, {160, 130, 200, 200});
    {
        char abilKey[32];
        std::snprintf(abilKey, sizeof(abilKey), "enemy.%d.abil", static_cast<int>(entry.type));
        const char* abilLoc = LOC(abilKey);
        const char* abilText = (std::strcmp(abilLoc, abilKey) == 0) ? entry.abilities : abilLoc;
        SDL_Surface* as = TTF_RenderText_Blended(font, abilText, {180, 165, 195, 190});
        if (as) {
            SDL_Texture* at = SDL_CreateTextureFromSurface(renderer, as);
            if (at) {
                int maxW = infoW - 20;
                float scale = (as->w > maxW) ? static_cast<float>(maxW) / as->w : 1.0f;
                SDL_Rect ar = {infoX, infoBlockY + 40, static_cast<int>(as->w * scale), static_cast<int>(as->h * scale)};
                SDL_RenderCopy(renderer, at, nullptr, &ar);
                SDL_DestroyTexture(at);
            }
            SDL_FreeSurface(as);
        }
    }

    // Weakness
    int weakY = infoBlockY + 100;
    drawText(renderer, font, LOC("bestiary.weakness"), infoX, weakY, {255, 200, 80, 210});
    {
        char weakKey[32];
        std::snprintf(weakKey, sizeof(weakKey), "enemy.%d.weak", static_cast<int>(entry.type));
        const char* weakLoc = LOC(weakKey);
        const char* weakText = (std::strcmp(weakLoc, weakKey) == 0) ? entry.weakness : weakLoc;
        SDL_Surface* ws = TTF_RenderText_Blended(font, weakText, {230, 210, 160, 190});
        if (ws) {
            SDL_Texture* wt = SDL_CreateTextureFromSurface(renderer, ws);
            if (wt) {
                int maxW = infoW - 20;
                float scale = (ws->w > maxW) ? static_cast<float>(maxW) / ws->w : 1.0f;
                SDL_Rect wr = {infoX, weakY + 40, static_cast<int>(ws->w * scale), static_cast<int>(ws->h * scale)};
                SDL_RenderCopy(renderer, wt, nullptr, &wr);
                SDL_DestroyTexture(wt);
            }
            SDL_FreeSurface(ws);
        }
    }

    // Effective weapons
    int effY = weakY + 100;
    drawText(renderer, font, LOC("bestiary.effective"), infoX, effY, {100, 220, 160, 210});
    {
        char effKey[32];
        std::snprintf(effKey, sizeof(effKey), "enemy.%d.eff", static_cast<int>(entry.type));
        const char* effLoc = LOC(effKey);
        const char* effText = (std::strcmp(effLoc, effKey) == 0) ? entry.effectiveWeapons : effLoc;
        SDL_Surface* es = TTF_RenderText_Blended(font, effText, {160, 215, 185, 185});
        if (es) {
            SDL_Texture* et = SDL_CreateTextureFromSurface(renderer, es);
            if (et) {
                int maxW = infoW - 20;
                float scale = (es->w > maxW) ? static_cast<float>(maxW) / es->w : 1.0f;
                SDL_Rect er = {infoX, effY + 40, static_cast<int>(es->w * scale), static_cast<int>(es->h * scale)};
                SDL_RenderCopy(renderer, et, nullptr, &er);
                SDL_DestroyTexture(et);
            }
            SDL_FreeSurface(es);
        }
    }
}

// ---- Undiscovered detail panel ----
void BestiaryState::renderUndiscoveredDetail(SDL_Renderer* renderer, TTF_Font* font,
                                              int typeIdx, bool isBoss) {
    // Silhouette preview in the left sub-panel
    {
        SDL_SetRenderDrawColor(renderer, 8, 6, 18, 100);
        SDL_Rect previewPanel = {DETAIL_X + 16, DETAIL_Y + 16, 480, DETAIL_H - 32};
        SDL_RenderFillRect(renderer, &previewPanel);
        SDL_SetRenderDrawColor(renderer, 40, 35, 60, 80);
        SDL_RenderDrawRect(renderer, &previewPanel);
    }

    renderSilhouette(renderer, PREVIEW_CX, PREVIEW_CY, typeIdx, isBoss);

    // "???" overlay on silhouette
    float qPulse = 0.4f + 0.6f * std::abs(std::sin(m_time * 1.5f));
    Uint8 qa = static_cast<Uint8>(180 * qPulse);
    drawTextCentered(renderer, font, "?", PREVIEW_CX, PREVIEW_CY - 10, {100, 90, 120, qa}, 2.5f);

    // Right: mystery text
    int infoX = DETAIL_X + 520;
    int infoY = DETAIL_Y + 28;

    // Unknown name with boss hint
    const char* unknownName = isBoss ? LOC("bestiary.unknown_boss") : LOC("bestiary.unknown_enemy");
    drawText(renderer, font, unknownName, infoX, infoY, {90, 80, 110, 180}, 1.4f);

    int divY = infoY + 72;
    SDL_SetRenderDrawColor(renderer, 60, 50, 85, 80);
    SDL_RenderDrawLine(renderer, infoX, divY, DETAIL_X + DETAIL_W - 32, divY);

    drawText(renderer, font, LOC("bestiary.encounter"),
             infoX, divY + 28, {80, 75, 100, 160});
    drawText(renderer, font, LOC("bestiary.defeat"),
             infoX, divY + 68, {75, 70, 95, 140});

    // Hidden stat bars (blacked out with question marks)
    int statsY = divY + 140;
    const int barW  = DETAIL_W - 540;
    const int barH  = 24;
    const int rowSp = 60;
    const char* labels[] = {LOC("bestiary.hp_locked"), LOC("bestiary.dmg_locked"), LOC("bestiary.spd_locked"), LOC("bestiary.elem_locked")};
    SDL_Color labelColor = {65, 60, 80, 120};
    for (int i = 0; i < 4; i++) {
        drawText(renderer, font, labels[i], infoX, statsY + i * rowSp, labelColor);
        drawBar(renderer, infoX + 220, statsY + i * rowSp + 3, barW - 220, barH, 0.0f,
                {0, 0, 0, 0}, {20, 18, 28, 100});
        // Draw diagonal lines over bar to indicate locked
        SDL_SetRenderDrawColor(renderer, 40, 35, 55, 80);
        int bx = infoX + 110;
        int by = statsY + i * rowSp + 3;
        int bw = barW - 110;
        for (int dx = 0; dx < bw; dx += 8) {
            SDL_RenderDrawLine(renderer, bx + dx, by, bx + dx + barH, by + barH);
        }
    }

    // Hint about how to unlock
    int hintY = statsY + rowSp * 4 + 32;
    SDL_SetRenderDrawColor(renderer, 50, 45, 70, 80);
    SDL_RenderDrawLine(renderer, infoX, hintY, DETAIL_X + DETAIL_W - 32, hintY);
    drawText(renderer, font, LOC("bestiary.abilities_locked"), infoX, hintY + 16, {60, 55, 80, 120});
    drawText(renderer, font, LOC("bestiary.weakness_locked"), infoX, hintY + 76, {60, 55, 80, 120});
}

// ---- Enemy preview (discovered) ----
void BestiaryState::renderEnemyPreview(SDL_Renderer* renderer, int cx, int cy, int type, bool isBoss) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Animate hover for flying types
    float hover = std::sin(m_time * 2.2f) * 4.0f;

    if (isBoss) {
        renderBossPreview(renderer, cx, cy, type);
        return;
    }

    // Regular enemies
    switch (type) {
        case 0: renderWalkerPreview(renderer, cx, cy, hover); break;
        case 1: renderFlyerPreview(renderer, cx, cy + static_cast<int>(hover), hover); break;
        case 2: renderTurretPreview(renderer, cx, cy, hover); break;
        case 3: renderChargerPreview(renderer, cx, cy, hover); break;
        case 4: renderPhaserPreview(renderer, cx, cy, hover); break;
        case 5: renderExploderPreview(renderer, cx, cy, hover); break;
        case 6: renderShielderPreview(renderer, cx, cy, hover); break;
        case 7: renderCrawlerPreview(renderer, cx, cy, hover); break;
        case 8: renderSummonerPreview(renderer, cx, cy, hover); break;
        case 9: renderSniperPreview(renderer, cx, cy, hover); break;
        case 10: renderTeleporterPreview(renderer, cx, cy, hover); break;
        case 11: renderReflectorPreview(renderer, cx, cy, hover); break;
        case 12: renderLeechPreview(renderer, cx, cy, hover); break;
        case 13: renderSwarmerPreview(renderer, cx, cy + static_cast<int>(hover), hover); break;
        case 14: renderGravityWellPreview(renderer, cx, cy, hover); break;
        case 15: renderMimicPreview(renderer, cx, cy, hover); break;
        default: break;
    }
}

// ---- Silhouette (undiscovered) ----
void BestiaryState::renderSilhouette(SDL_Renderer* renderer, int cx, int cy, int type, bool isBoss) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    float pulse = 0.3f + 0.2f * std::sin(m_time * 1.8f);
    Uint8 alpha = static_cast<Uint8>(60 + 40 * pulse);
    SDL_SetRenderDrawColor(renderer, 50, 45, 70, alpha);

    int w = isBoss ? 64 : 30;
    int h = isBoss ? 64 : 36;

    if (!isBoss) {
        // Give rough shape hints based on type index
        switch (type) {
            case 1: h = 24; break;          // Flyer: shorter
            case 2: w = 28; h = 28; break;  // Turret: square
            case 3: w = 36; h = 28; break;  // Charger: wide
            case 4: w = 26; h = 40; break;  // Phaser: tall
            case 5: w = 22; h = 22; break;  // Exploder: small
            case 6: w = 32; h = 36; break;  // Shielder: tall
            case 7: w = 26; h = 18; break;  // Crawler: flat
            case 8: w = 30; h = 38; break;  // Summoner: tall
            case 9: w = 24; h = 34; break;   // Sniper: thin
            case 10: w = 28; h = 38; break;  // Teleporter: robed
            case 11: w = 34; h = 32; break;  // Reflector: broad + shield
            case 12: w = 30; h = 20; break;  // Leech: wide blob
            case 13: w = 16; h = 16; break;  // Swarmer: tiny
            case 14: w = 32; h = 32; break;  // GravityWell: orb
            case 15: w = 28; h = 28; break;  // Mimic: crate-shaped
            default: break;
        }
    }

    SDL_Rect body = {cx - w / 2, cy - h / 2, w, h};
    SDL_RenderFillRect(renderer, &body);

    // Eyes as dark spots (visible through silhouette)
    SDL_SetRenderDrawColor(renderer, 18, 15, 25, 200);
    int eyeSz = std::max(4, w / 8);
    SDL_Rect eyeL = {cx - w / 4, cy - h / 6, eyeSz, eyeSz};
    SDL_Rect eyeR = {cx + w / 4 - eyeSz, cy - h / 6, eyeSz, eyeSz};
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);
}

// ---- Per-type preview helpers ----
static void drawEyes(SDL_Renderer* renderer, int cx, int cy, int bodyH, int bodyW, Uint8 r=255, Uint8 g=40, Uint8 b=40) {
    int eyeSize = std::max(3, bodyW / 8);
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_Rect eyeL = {cx - bodyW / 4, cy - bodyH / 6, eyeSize, eyeSize};
    SDL_Rect eyeR = {cx + bodyW / 4 - eyeSize, cy - bodyH / 6, eyeSize, eyeSize};
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);
}

void BestiaryState::renderWalkerPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    // Body: dark red rectangle, simple humanoid
    int bw = 28, bh = 32;
    SDL_SetRenderDrawColor(renderer, 200, 60, 60, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Legs
    SDL_SetRenderDrawColor(renderer, 160, 40, 40, 200);
    SDL_Rect legL = {cx - bw/2, cy + bh/2, 10, 10};
    SDL_Rect legR = {cx + bw/2 - 10, cy + bh/2, 10, 10};
    SDL_RenderFillRect(renderer, &legL);
    SDL_RenderFillRect(renderer, &legR);
    drawEyes(renderer, cx, cy, bh, bw);
    // Border
    SDL_SetRenderDrawColor(renderer, 240, 80, 80, 100);
    SDL_RenderDrawRect(renderer, &body);
}

void BestiaryState::renderFlyerPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 24, bh = 24;
    // Wings (triangular via lines)
    float wingFlap = std::sin(m_time * 5.0f) * 6.0f;
    SDL_SetRenderDrawColor(renderer, 150, 60, 180, 160);
    SDL_RenderDrawLine(renderer, cx, cy, cx - 30, cy - 8 + static_cast<int>(wingFlap));
    SDL_RenderDrawLine(renderer, cx - 30, cy - 8 + static_cast<int>(wingFlap), cx - 15, cy + 4);
    SDL_RenderDrawLine(renderer, cx, cy, cx + 30, cy - 8 + static_cast<int>(wingFlap));
    SDL_RenderDrawLine(renderer, cx + 30, cy - 8 + static_cast<int>(wingFlap), cx + 15, cy + 4);
    // Body
    SDL_SetRenderDrawColor(renderer, 180, 80, 200, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    drawEyes(renderer, cx, cy, bh, bw, 255, 80, 255);
}

void BestiaryState::renderTurretPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 28, bh = 28;
    // Base plate
    SDL_SetRenderDrawColor(renderer, 140, 140, 40, 180);
    SDL_Rect base = {cx - bw/2 - 4, cy + bh/2, bw + 8, 8};
    SDL_RenderFillRect(renderer, &base);
    // Body
    SDL_SetRenderDrawColor(renderer, 200, 200, 60, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Barrel
    float barrelAngle = std::sin(m_time * 0.8f) * 0.2f;
    int barrelLen = 18;
    int bx2 = cx + static_cast<int>(std::cos(barrelAngle) * barrelLen);
    int by2 = cy - static_cast<int>(std::sin(barrelAngle) * barrelLen);
    SDL_SetRenderDrawColor(renderer, 240, 240, 80, 255);
    SDL_RenderDrawLine(renderer, cx, cy, bx2, by2);
    SDL_RenderDrawLine(renderer, cx, cy + 2, bx2, by2 + 2);
    SDL_RenderDrawLine(renderer, cx, cy - 2, bx2, by2 - 2);
    drawEyes(renderer, cx, cy, bh, bw, 255, 220, 0);
}

void BestiaryState::renderChargerPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 36, bh = 28;
    // Wide low body
    SDL_SetRenderDrawColor(renderer, 220, 120, 40, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Horn/tusk
    SDL_SetRenderDrawColor(renderer, 255, 200, 80, 240);
    SDL_RenderDrawLine(renderer, cx + bw/2, cy - 4, cx + bw/2 + 12, cy - 10);
    SDL_RenderDrawLine(renderer, cx + bw/2, cy - 2, cx + bw/2 + 12, cy - 8);
    // Legs
    SDL_SetRenderDrawColor(renderer, 180, 90, 30, 200);
    for (int i = 0; i < 3; i++) {
        int lx = cx - bw/2 + i * 14;
        SDL_Rect leg = {lx, cy + bh/2, 8, 10};
        SDL_RenderFillRect(renderer, &leg);
    }
    drawEyes(renderer, cx, cy, bh, bw, 255, 80, 20);
}

void BestiaryState::renderPhaserPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 26, bh = 40;
    // Phase shimmer effect
    float phase = std::sin(m_time * 3.0f);
    Uint8 alpha = static_cast<Uint8>(140 + 80 * phase);
    // Ghost copy offset
    SDL_SetRenderDrawColor(renderer, 80, 40, 160, static_cast<Uint8>(alpha / 3));
    SDL_Rect ghost = {cx - bw/2 + 8, cy - bh/2 - 4, bw, bh};
    SDL_RenderFillRect(renderer, &ghost);
    // Body
    SDL_SetRenderDrawColor(renderer, 100, 50, 200, alpha);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Dim-shift aura lines
    SDL_SetRenderDrawColor(renderer, 140, 80, 255, static_cast<Uint8>(80 * std::abs(phase)));
    SDL_RenderDrawRect(renderer, &body);
    SDL_Rect aura = {cx - bw/2 - 3, cy - bh/2 - 3, bw + 6, bh + 6};
    SDL_RenderDrawRect(renderer, &aura);
    drawEyes(renderer, cx, cy, bh, bw, 180, 80, 255);
}

void BestiaryState::renderExploderPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 22, bh = 22;
    // Pulsing danger glow
    float pulse = 0.5f + 0.5f * std::sin(m_time * 4.5f);
    Uint8 glowA = static_cast<Uint8>(80 * pulse);
    SDL_SetRenderDrawColor(renderer, 255, 80, 30, glowA);
    SDL_Rect glow = {cx - bw/2 - 10, cy - bh/2 - 10, bw + 20, bh + 20};
    SDL_RenderFillRect(renderer, &glow);
    // Body (round-ish via offset rects)
    SDL_SetRenderDrawColor(renderer, 255, 80, 30, 230);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Fuse lines
    SDL_SetRenderDrawColor(renderer, 255, 220, 40, 200);
    SDL_RenderDrawLine(renderer, cx, cy - bh/2, cx + 4, cy - bh/2 - 10);
    SDL_RenderDrawLine(renderer, cx + 4, cy - bh/2 - 10, cx + 8, cy - bh/2 - 6);
    drawEyes(renderer, cx, cy, bh, bw, 255, 200, 40);
}

void BestiaryState::renderShielderPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 32, bh = 36;
    // Body
    SDL_SetRenderDrawColor(renderer, 80, 180, 220, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Shield on right side
    SDL_SetRenderDrawColor(renderer, 140, 210, 255, 200);
    SDL_Rect shieldFill = {cx + bw/2, cy - bh/2 + 4, 12, bh - 8};
    SDL_RenderFillRect(renderer, &shieldFill);
    SDL_SetRenderDrawColor(renderer, 200, 240, 255, 255);
    SDL_RenderDrawRect(renderer, &shieldFill);
    // Shield highlight
    SDL_RenderDrawLine(renderer, cx + bw/2 + 2, cy - bh/2 + 6, cx + bw/2 + 2, cy + bh/2 - 6);
    drawEyes(renderer, cx, cy, bh, bw, 40, 180, 255);
}

void BestiaryState::renderCrawlerPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    // Crawler is on the ceiling - render upside-down at top of preview area
    int acY = cy - 40;
    int bw = 26, bh = 18;
    // Legs spread on ceiling
    SDL_SetRenderDrawColor(renderer, 40, 120, 60, 180);
    for (int i = -2; i <= 2; i++) {
        SDL_RenderDrawLine(renderer, cx + i * 5, acY, cx + i * 12, acY - 12 + std::abs(i) * 2);
    }
    // Body
    SDL_SetRenderDrawColor(renderer, 60, 160, 80, 220);
    SDL_Rect body = {cx - bw/2, acY, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    drawEyes(renderer, cx, acY + bh/2, bh, bw, 40, 220, 80);
    // Drop line hint
    SDL_SetRenderDrawColor(renderer, 60, 160, 80, 60);
    SDL_RenderDrawLine(renderer, cx, acY + bh, cx, cy + 20);
}

void BestiaryState::renderSummonerPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 30, bh = 38;
    // Aura glow
    float auraP = 0.5f + 0.5f * std::sin(m_time * 2.0f);
    SDL_SetRenderDrawColor(renderer, 160, 40, 200, static_cast<Uint8>(40 * auraP));
    SDL_Rect aura = {cx - bw/2 - 12, cy - bh/2 - 8, bw + 24, bh + 16};
    SDL_RenderFillRect(renderer, &aura);
    // Body
    SDL_SetRenderDrawColor(renderer, 180, 50, 220, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    drawEyes(renderer, cx, cy, bh, bw, 220, 40, 255);
    // Small orbiting minion dots
    for (int m = 0; m < 3; m++) {
        float angle = m_time * 2.5f + m * (2.0f * 3.14159f / 3.0f);
        int mx = cx + static_cast<int>(std::cos(angle) * 40);
        int my = cy + static_cast<int>(std::sin(angle) * 20);
        SDL_SetRenderDrawColor(renderer, 200, 100, 255, 180);
        SDL_Rect minion = {mx - 4, my - 4, 8, 8};
        SDL_RenderFillRect(renderer, &minion);
    }
}

void BestiaryState::renderSniperPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 24, bh = 34;
    // Body - tall and thin
    SDL_SetRenderDrawColor(renderer, 200, 180, 40, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Long rifle (animated aim)
    float aimAngle = -0.15f + std::sin(m_time * 0.6f) * 0.05f;
    int rifleLen = 30;
    SDL_SetRenderDrawColor(renderer, 255, 220, 60, 240);
    SDL_RenderDrawLine(renderer, cx, cy,
        cx + static_cast<int>(std::cos(aimAngle) * rifleLen),
        cy - static_cast<int>(std::sin(aimAngle) * rifleLen));
    // Telegraph laser dot
    float tP = 0.5f + 0.5f * std::sin(m_time * 6.0f);
    SDL_SetRenderDrawColor(renderer, 255, 40, 40, static_cast<Uint8>(200 * tP));
    int laserX = cx + static_cast<int>(std::cos(aimAngle) * rifleLen);
    int laserY = cy - static_cast<int>(std::sin(aimAngle) * rifleLen);
    SDL_Rect dot = {laserX - 2, laserY - 2, 4, 4};
    SDL_RenderFillRect(renderer, &dot);
    drawEyes(renderer, cx, cy, bh, bw, 255, 200, 40);
}

void BestiaryState::renderTeleporterPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 28, bh = 38;
    // Hooded robe body (trapezoid)
    SDL_SetRenderDrawColor(renderer, 90, 40, 140, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2 + 8, bw, bh - 8};
    SDL_RenderFillRect(renderer, &body);
    // Hood
    SDL_SetRenderDrawColor(renderer, 60, 30, 100, 240);
    SDL_Rect hood = {cx - bw/3, cy - bh/2, bw*2/3, bh/3};
    SDL_RenderFillRect(renderer, &hood);
    // Pulsing cyan-purple eyes
    float eyePulse = 0.5f + 0.5f * std::sin(m_time * 3.0f);
    Uint8 eyeG = static_cast<Uint8>(180 * eyePulse);
    Uint8 eyeB = static_cast<Uint8>(200 + 55 * eyePulse);
    SDL_SetRenderDrawColor(renderer, 80, eyeG, eyeB, 255);
    SDL_Rect eyeL = {cx - 5, cy - bh/2 + bh/6 + 2, 3, 3};
    SDL_Rect eyeR = {cx + 2, cy - bh/2 + bh/6 + 2, 3, 3};
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);
    // Dimensional crack lines
    for (int i = 0; i < 4; i++) {
        float angle = m_time * 1.5f + i * 1.5708f;
        int lx = cx + static_cast<int>(std::cos(angle) * (bw/2 + 3));
        int ly = cy + static_cast<int>(std::sin(angle) * (bh/2 + 3));
        int lx2 = lx + static_cast<int>(std::cos(angle + 0.5f) * 8);
        int ly2 = ly + static_cast<int>(std::sin(angle + 0.5f) * 8);
        SDL_SetRenderDrawColor(renderer, 160, 80, 220, static_cast<Uint8>(120 * eyePulse));
        SDL_RenderDrawLine(renderer, lx, ly, lx2, ly2);
    }
}

void BestiaryState::renderReflectorPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 32, bh = 32;
    // Broad silver-blue body
    SDL_SetRenderDrawColor(renderer, 140, 160, 190, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Head
    SDL_SetRenderDrawColor(renderer, 160, 170, 200, 240);
    SDL_Rect head = {cx - bw/3, cy - bh/2 - 6, bw*2/3, 8};
    SDL_RenderFillRect(renderer, &head);
    // Eye slit
    SDL_SetRenderDrawColor(renderer, 200, 220, 255, 255);
    SDL_RenderDrawLine(renderer, cx - bw/4, cy - bh/2 - 2, cx + bw/4, cy - bh/2 - 2);
    // Rotating mirror-shield diamond
    float shieldRot = m_time * 2.0f;
    int shieldX = cx + bw/2 + 10;
    int shieldSize = 10;
    float shieldPulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
    Uint8 shieldA = static_cast<Uint8>(200 * shieldPulse);
    SDL_SetRenderDrawColor(renderer, 200, 220, 255, shieldA);
    int dx1 = static_cast<int>(std::cos(shieldRot) * shieldSize);
    int dy1 = static_cast<int>(std::sin(shieldRot) * shieldSize);
    int dx2 = static_cast<int>(std::cos(shieldRot + 1.5708f) * shieldSize);
    int dy2 = static_cast<int>(std::sin(shieldRot + 1.5708f) * shieldSize);
    SDL_RenderDrawLine(renderer, shieldX+dx1, cy+dy1, shieldX+dx2, cy+dy2);
    SDL_RenderDrawLine(renderer, shieldX+dx2, cy+dy2, shieldX-dx1, cy-dy1);
    SDL_RenderDrawLine(renderer, shieldX-dx1, cy-dy1, shieldX-dx2, cy-dy2);
    SDL_RenderDrawLine(renderer, shieldX-dx2, cy-dy2, shieldX+dx1, cy+dy1);
    // Legs
    SDL_SetRenderDrawColor(renderer, 120, 140, 170, 200);
    SDL_Rect legL = {cx - bw/3, cy + bh/2, 4, 8};
    SDL_Rect legR = {cx + bw/3 - 4, cy + bh/2, 4, 8};
    SDL_RenderFillRect(renderer, &legL);
    SDL_RenderFillRect(renderer, &legR);
}

void BestiaryState::renderLeechPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 30, bh = 20;
    // Wide green blob body
    SDL_SetRenderDrawColor(renderer, 50, 100, 40, 220);
    SDL_Rect body = {cx - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &body);
    // Lighter rounded top
    SDL_SetRenderDrawColor(renderer, 60, 120, 50, 240);
    SDL_Rect top = {cx - bw/2 + 2, cy - bh/2, bw - 4, bh/2};
    SDL_RenderFillRect(renderer, &top);
    // Pulsing green core
    float corePulse = 0.4f + 0.6f * std::sin(m_time * 2.5f);
    Uint8 coreG = static_cast<Uint8>(180 + 75 * corePulse);
    SDL_SetRenderDrawColor(renderer, 30, coreG, 40, 240);
    int coreSize = bw/4;
    SDL_Rect core = {cx - coreSize/2, cy - coreSize/2, coreSize, coreSize};
    SDL_RenderFillRect(renderer, &core);
    // Dripping green dots
    for (int i = 0; i < 3; i++) {
        float dripPhase = std::fmod(m_time * 1.5f + i * 1.3f, 2.0f);
        int dripX = cx - bw/4 + i * (bw/4);
        int dripY = cy + bh/2 + static_cast<int>(dripPhase * 8.0f);
        Uint8 dripA = static_cast<Uint8>(220 * (1.0f - dripPhase / 2.0f));
        SDL_SetRenderDrawColor(renderer, 80, 200, 60, dripA);
        SDL_Rect drip = {dripX, dripY, 2, 2};
        SDL_RenderFillRect(renderer, &drip);
    }
    // Mouth
    SDL_SetRenderDrawColor(renderer, 200, 40, 40, 200);
    SDL_Rect mouth = {cx + bw/2 - 6, cy - 1, 5, 2};
    SDL_RenderFillRect(renderer, &mouth);
}

void BestiaryState::renderSwarmerPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    // Draw a small cluster of 3 swarmers to show the swarm nature
    for (int s = 0; s < 3; s++) {
        float offset = s * 2.094f; // 2*PI/3
        int sx = cx + static_cast<int>(std::cos(m_time * 2.0f + offset) * 16);
        int sy = cy + static_cast<int>(std::sin(m_time * 2.0f + offset) * 8);
        int bodySize = 8;
        // Amber body
        SDL_SetRenderDrawColor(renderer, 255, 180, 50, 220);
        SDL_Rect body = {sx - bodySize/2, sy - bodySize/2, bodySize, bodySize};
        SDL_RenderFillRect(renderer, &body);
        // Core
        SDL_SetRenderDrawColor(renderer, 255, 220, 100, 240);
        SDL_Rect core = {sx - 2, sy - 2, 4, 4};
        SDL_RenderFillRect(renderer, &core);
        // Wings (flutter)
        float flutter = std::sin(m_time * 8.0f + s) * 2.0f;
        int wingW = bodySize/2 + static_cast<int>(flutter);
        SDL_SetRenderDrawColor(renderer, 255, 160, 30, 180);
        SDL_Rect wingL = {sx - wingW - bodySize/2, sy - 1, wingW, 3};
        SDL_Rect wingR = {sx + bodySize/2, sy - 1, wingW, 3};
        SDL_RenderFillRect(renderer, &wingL);
        SDL_RenderFillRect(renderer, &wingR);
        // Eye
        SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
        SDL_Rect eye = {sx + 2, sy - 2, 2, 2};
        SDL_RenderFillRect(renderer, &eye);
    }
}

void BestiaryState::renderGravityWellPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 32;
    // Outer gravity ring (pulsing dots)
    float pulse = std::sin(m_time * 2.0f) * 0.3f + 0.7f;
    Uint8 ringA = static_cast<Uint8>(80 * pulse);
    int ringR = bw/2 + 6;
    SDL_SetRenderDrawColor(renderer, 150, 80, 220, ringA);
    for (int i = 0; i < 12; i++) {
        float rad = i * 3.14159f / 6.0f + m_time;
        int px = cx + static_cast<int>(std::cos(rad) * ringR);
        int py = cy + static_cast<int>(std::sin(rad) * ringR);
        SDL_Rect dot = {px - 1, py - 1, 3, 3};
        SDL_RenderFillRect(renderer, &dot);
    }
    // Dark purple orb body
    SDL_SetRenderDrawColor(renderer, 100, 50, 180, 220);
    SDL_Rect body = {cx - bw/2 + 2, cy - bw/2 + 2, bw - 4, bw - 4};
    SDL_RenderFillRect(renderer, &body);
    // Bright core
    int coreSize = bw/4;
    Uint8 coreA = static_cast<Uint8>(180 + 75 * pulse);
    SDL_SetRenderDrawColor(renderer, 180, 100, 255, coreA);
    SDL_Rect coreRect = {cx - coreSize, cy - coreSize, coreSize*2, coreSize*2};
    SDL_RenderFillRect(renderer, &coreRect);
    // Inner white hot
    SDL_SetRenderDrawColor(renderer, 220, 200, 255, coreA);
    SDL_Rect inner = {cx - coreSize/2, cy - coreSize/2, coreSize, coreSize};
    SDL_RenderFillRect(renderer, &inner);
    // Orbiting particles
    for (int i = 0; i < 4; i++) {
        float angle = m_time * 3.0f + i * 1.57f;
        float dist = bw * 0.35f + std::sin(m_time * 2.0f + i) * 3.0f;
        int px = cx + static_cast<int>(std::cos(angle) * dist);
        int py = cy + static_cast<int>(std::sin(angle) * dist);
        SDL_SetRenderDrawColor(renderer, 200, 140, 255, 220);
        SDL_Rect part = {px - 1, py - 1, 2, 2};
        SDL_RenderFillRect(renderer, &part);
    }
}

void BestiaryState::renderMimicPreview(SDL_Renderer* renderer, int cx, int cy, float /*hover*/) {
    int bw = 28, bh = 28;
    // Crate disguise on left half
    int crateX = cx - 24;
    SDL_SetRenderDrawColor(renderer, 140, 95, 45, 220);
    SDL_Rect crate = {crateX - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &crate);
    // Plank lines
    SDL_SetRenderDrawColor(renderer, 100, 65, 25, 200);
    SDL_RenderDrawLine(renderer, crateX - bw/2 + 2, cy - bh/6, crateX + bw/2 - 2, cy - bh/6);
    SDL_RenderDrawLine(renderer, crateX - bw/2 + 2, cy + bh/6, crateX + bw/2 - 2, cy + bh/6);
    SDL_RenderDrawLine(renderer, crateX, cy - bh/2 + 2, crateX, cy + bh/2 - 2);

    // Arrow showing transformation
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 120);
    SDL_RenderDrawLine(renderer, cx - 4, cy, cx + 4, cy);
    SDL_RenderDrawLine(renderer, cx + 2, cy - 3, cx + 5, cy);
    SDL_RenderDrawLine(renderer, cx + 2, cy + 3, cx + 5, cy);

    // Revealed form on right half (angry red creature)
    int revX = cx + 24;
    SDL_SetRenderDrawColor(renderer, 220, 60, 50, 220);
    SDL_Rect revBody = {revX - bw/2, cy - bh/2, bw, bh};
    SDL_RenderFillRect(renderer, &revBody);
    // Jagged teeth
    SDL_SetRenderDrawColor(renderer, 255, 255, 220, 240);
    for (int i = 0; i < bw; i += 4) {
        int toothH = 3 + (i % 8 == 0 ? 2 : 0);
        SDL_Rect tooth = {revX - bw/2 + i, cy - bh/2, 2, toothH};
        SDL_RenderFillRect(renderer, &tooth);
    }
    // Angry eyes
    SDL_SetRenderDrawColor(renderer, 255, 255, 60, 255);
    SDL_Rect eyeL = {revX - 6, cy - 4, 4, 3};
    SDL_Rect eyeR = {revX + 2, cy - 4, 4, 3};
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);
    // Pulsing red rage aura
    float rage = std::sin(m_time * 4.0f) * 0.3f + 0.5f;
    SDL_SetRenderDrawColor(renderer, 255, 40, 40, static_cast<Uint8>(40 * rage));
    SDL_Rect aura = {revX - bw/2 - 2, cy - bh/2 - 2, bw + 4, bh + 4};
    SDL_RenderFillRect(renderer, &aura);
}

void BestiaryState::renderBossPreview(SDL_Renderer* renderer, int cx, int cy, int bossType) {
    int size = 60;
    SDL_Color colors[] = {
        {200,  40, 180, 255},  // 0: Rift Guardian - magenta
        { 40, 180, 120, 255},  // 1: Void Wyrm - teal
        {120,  80, 200, 255},  // 2: Dimensional Architect - violet
        {180, 160,  80, 255},  // 3: Temporal Weaver - gold
        {100,  40, 220, 255},  // 4: Void Sovereign - deep purple
        { 60, 200,  60, 255},  // 5: Entropy Incarnate - sickly green
    };
    SDL_Color col = (bossType >= 0 && bossType < 6) ? colors[bossType] : colors[0];

    // Animated outer ring
    float pulse = 0.5f + 0.5f * std::sin(m_time * 2.5f);
    for (int ring = 0; ring < 3; ring++) {
        int expand = 6 + ring * 8;
        Uint8 ra = static_cast<Uint8>(100 * pulse * (3 - ring) / 3.0f);
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ra);
        SDL_Rect aura = {cx - size/2 - expand, cy - size/2 - expand,
                         size + expand*2, size + expand*2};
        SDL_RenderDrawRect(renderer, &aura);
    }

    // Main body
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 240);
    SDL_Rect body = {cx - size/2, cy - size/2, size, size};
    SDL_RenderFillRect(renderer, &body);

    // Boss-type specific details
    switch (bossType) {
        case 0: { // Rift Guardian: crown spikes
            SDL_SetRenderDrawColor(renderer, 255, 180, 80, 220);
            SDL_RenderDrawLine(renderer, cx - 16, cy - size/2, cx - 8, cy - size/2 - 14);
            SDL_RenderDrawLine(renderer, cx,      cy - size/2, cx,     cy - size/2 - 18);
            SDL_RenderDrawLine(renderer, cx + 16, cy - size/2, cx + 8, cy - size/2 - 14);
            break;
        }
        case 1: { // Void Wyrm: serpent coil
            float coilAngle = m_time * 1.5f;
            SDL_SetRenderDrawColor(renderer, 40, 220, 140, 180);
            SDL_RenderDrawLine(renderer, cx,
                cy + size/2,
                cx + static_cast<int>(std::cos(coilAngle) * 20),
                cy + size/2 + 20 + static_cast<int>(std::sin(coilAngle) * 8));
            break;
        }
        case 2: { // Dimensional Architect: rift portals
            SDL_SetRenderDrawColor(renderer, 180, 120, 255, 160);
            SDL_Rect portalL = {cx - size/2 - 20, cy - 10, 16, 20};
            SDL_Rect portalR = {cx + size/2 + 4,  cy - 10, 16, 20};
            SDL_RenderDrawRect(renderer, &portalL);
            SDL_RenderDrawRect(renderer, &portalR);
            break;
        }
        case 3: { // Temporal Weaver: clock hands
            float sweep = m_time * 0.8f;
            SDL_SetRenderDrawColor(renderer, 255, 230, 100, 200);
            SDL_RenderDrawLine(renderer, cx, cy,
                cx + static_cast<int>(std::cos(sweep) * 25),
                cy - static_cast<int>(std::sin(sweep) * 25));
            SDL_SetRenderDrawColor(renderer, 255, 180, 60, 200);
            SDL_RenderDrawLine(renderer, cx, cy,
                cx + static_cast<int>(std::cos(sweep * 12.0f) * 15),
                cy - static_cast<int>(std::sin(sweep * 12.0f) * 15));
            break;
        }
        case 4: { // Void Sovereign: void core
            float vPulse = 0.5f + 0.5f * std::sin(m_time * 4.0f);
            SDL_SetRenderDrawColor(renderer, 50, 20, 100, static_cast<Uint8>(200 * vPulse));
            SDL_Rect core = {cx - 10, cy - 10, 20, 20};
            SDL_RenderFillRect(renderer, &core);
            SDL_SetRenderDrawColor(renderer, 200, 100, 255, static_cast<Uint8>(180 * vPulse));
            SDL_RenderDrawRect(renderer, &core);
            break;
        }
        case 5: { // Entropy Incarnate: radiating decay lines
            float ePulse = 0.4f + 0.6f * std::abs(std::sin(m_time * 3.5f));
            Uint8 ea = static_cast<Uint8>(160 * ePulse);
            SDL_SetRenderDrawColor(renderer, 60, 200, 60, ea);
            // Entropy decay rays emanating from center
            for (int ray = 0; ray < 8; ray++) {
                float angle = ray * (3.14159f / 4.0f) + m_time * 0.6f;
                int inner = size / 2 + 2;
                int outer = size / 2 + 18;
                SDL_RenderDrawLine(renderer,
                    cx + static_cast<int>(std::cos(angle) * inner),
                    cy + static_cast<int>(std::sin(angle) * inner),
                    cx + static_cast<int>(std::cos(angle) * outer),
                    cy + static_cast<int>(std::sin(angle) * outer));
            }
            // Dark eroding core
            SDL_SetRenderDrawColor(renderer, 10, 30, 10, static_cast<Uint8>(200 * ePulse));
            SDL_Rect ecore = {cx - 8, cy - 8, 16, 16};
            SDL_RenderFillRect(renderer, &ecore);
            break;
        }
        default: break;
    }

    // Boss eyes (larger, more menacing)
    int eyeSize = size / 7;
    SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
    SDL_Rect eyeL = {cx - size/4, cy - size/6, eyeSize, eyeSize};
    SDL_Rect eyeR = {cx + size/4 - eyeSize, cy - size/6, eyeSize, eyeSize};
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);
    // Glowing inner pupil
    float eyePulse = 0.4f + 0.6f * std::abs(std::sin(m_time * 3.0f));
    SDL_SetRenderDrawColor(renderer, 255, 200, 200, static_cast<Uint8>(200 * eyePulse));
    SDL_Rect pupL = {cx - size/4 + eyeSize/4, cy - size/6 + eyeSize/4, eyeSize/2, eyeSize/2};
    SDL_Rect pupR = {cx + size/4 - eyeSize/2 - eyeSize/4, cy - size/6 + eyeSize/4, eyeSize/2, eyeSize/2};
    SDL_RenderFillRect(renderer, &pupL);
    SDL_RenderFillRect(renderer, &pupR);
}
