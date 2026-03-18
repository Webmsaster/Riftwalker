#include "UpgradeState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Game/UpgradeSystem.h"
#include <cstdio>
#include <cmath>

// Layout constants
static constexpr int UPGRADE_MARGIN = 100;
static constexpr int UPGRADE_ITEM_H = 65;
static constexpr int UPGRADE_START_Y = 105;

void UpgradeState::enter() {
    m_selectedUpgrade = 0;
    m_scrollOffset = 0;
    m_time = 0;
    m_flashTimer = 0;
}

void UpgradeState::purchaseSelected() {
    auto& upgrades = game->getUpgradeSystem();
    auto& upgradeList = upgrades.getUpgrades();
    if (m_selectedUpgrade < static_cast<int>(upgradeList.size())) {
        if (upgrades.purchaseUpgrade(upgradeList[m_selectedUpgrade].id)) {
            m_flashTimer = 0.5f;
            AudioManager::instance().play(SFX::Pickup);
        }
    }
}

void UpgradeState::handleEvent(const SDL_Event& event) {
    auto& upgrades = game->getUpgradeSystem();
    int total = static_cast<int>(upgrades.getUpgrades().size());

    if (event.type == SDL_KEYDOWN) {
        if (total == 0) return;

        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
                game->changeState(StateID::Menu);
                break;
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_selectedUpgrade = (m_selectedUpgrade - 1 + total) % total;
                if (m_selectedUpgrade < m_scrollOffset) m_scrollOffset = m_selectedUpgrade;
                if (m_selectedUpgrade >= m_scrollOffset + VISIBLE_ITEMS)
                    m_scrollOffset = m_selectedUpgrade - VISIBLE_ITEMS + 1;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_selectedUpgrade = (m_selectedUpgrade + 1) % total;
                if (m_selectedUpgrade >= m_scrollOffset + VISIBLE_ITEMS)
                    m_scrollOffset = m_selectedUpgrade - VISIBLE_ITEMS + 1;
                if (m_selectedUpgrade < m_scrollOffset) m_scrollOffset = m_selectedUpgrade;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                purchaseSelected();
                break;
            default: break;
        }
    }

    // Mouse hover: update selection
    if (event.type == SDL_MOUSEMOTION && total > 0) {
        int mx = event.motion.x, my = event.motion.y;
        const int margin = 100, itemH = 65, startY = 105;
        int endIdx = std::min(m_scrollOffset + VISIBLE_ITEMS, total);
        for (int i = m_scrollOffset; i < endIdx; i++) {
            int y = startY + (i - m_scrollOffset) * itemH;
            SDL_Rect card = {margin, y, SCREEN_WIDTH - margin * 2, itemH - 4};
            if (mx >= card.x && mx < card.x + card.w && my >= card.y && my < card.y + card.h) {
                if (i != m_selectedUpgrade) {
                    m_selectedUpgrade = i;
                    AudioManager::instance().play(SFX::MenuSelect);
                }
                break;
            }
        }
    }

    // Mouse click: select + purchase
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && total > 0) {
        int mx = event.button.x, my = event.button.y;
        const int margin = 100, itemH = 65, startY = 105;
        int endIdx = std::min(m_scrollOffset + VISIBLE_ITEMS, total);
        for (int i = m_scrollOffset; i < endIdx; i++) {
            int y = startY + (i - m_scrollOffset) * itemH;
            SDL_Rect card = {margin, y, SCREEN_WIDTH - margin * 2, itemH - 4};
            if (mx >= card.x && mx < card.x + card.w && my >= card.y && my < card.y + card.h) {
                m_selectedUpgrade = i;
                purchaseSelected();
                break;
            }
        }
    }
}

void UpgradeState::update(float dt) {
    m_time += dt;
    if (m_flashTimer > 0) m_flashTimer -= dt;
}

void UpgradeState::renderText(SDL_Renderer* renderer, TTF_Font* font,
                               const char* text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* s = TTF_RenderText_Blended(font, text, color);
    if (s) {
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        if (t) {
            SDL_Rect r = {x, y, s->w, s->h};
            SDL_RenderCopy(renderer, t, nullptr, &r);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
    }
}

void UpgradeState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 8, 5, 15, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Subtle background pattern
    for (int i = 0; i < 30; i++) {
        float y = static_cast<float>((i * 73 + static_cast<int>(m_time * 10)) % SCREEN_HEIGHT);
        Uint8 a = static_cast<Uint8>(15 + (i * 3) % 15);
        SDL_SetRenderDrawColor(renderer, 40, 25, 70, a);
        SDL_RenderDrawLine(renderer, 0, static_cast<int>(y), SCREEN_WIDTH, static_cast<int>(y));
    }

    TTF_Font* font = game->getFont();
    auto& upgrades = game->getUpgradeSystem();
    auto& upgradeList = upgrades.getUpgrades();

    int margin = UPGRADE_MARGIN;
    int itemH  = UPGRADE_ITEM_H;
    int startY = UPGRADE_START_Y;

    if (!font) return;

    // Title with glow
    SDL_Surface* titleSurf = TTF_RenderText_Blended(font, "RIFTSUIT UPGRADES", {160, 110, 240, 255});
    if (titleSurf) {
        SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
        if (titleTex) {
            int tw = titleSurf->w;
            int th = titleSurf->h;
            // Glow
            SDL_SetTextureBlendMode(titleTex, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(titleTex, 40);
            SDL_Rect glowR = {640 - tw / 2 - 2, 22, tw + 4, th + 4};
            SDL_RenderCopy(renderer, titleTex, nullptr, &glowR);
            // Main
            SDL_SetTextureBlendMode(titleTex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(titleTex, 255);
            SDL_Rect titleR = {640 - tw / 2, 25, tw, th};
            SDL_RenderCopy(renderer, titleTex, nullptr, &titleR);
            SDL_DestroyTexture(titleTex);
        }
        SDL_FreeSurface(titleSurf);
    }

    // Shard count with diamond icon
    char shardText[64];
    std::snprintf(shardText, sizeof(shardText), "Rift Shards: %d", upgrades.getRiftShards());
    renderText(renderer, font, shardText, 0, 0, {200, 160, 255, 255});

    // Re-render centered (need width)
    SDL_Surface* shSurf = TTF_RenderText_Blended(font, shardText, {200, 160, 255, 255});
    if (shSurf) {
        SDL_Texture* shTex = SDL_CreateTextureFromSurface(renderer, shSurf);
        if (shTex) {
            // Diamond icon
            int ix = 640 - shSurf->w / 2 - 18;
            int iy = 65;
            SDL_SetRenderDrawColor(renderer, 200, 160, 255, 230);
            SDL_RenderDrawLine(renderer, ix, iy, ix + 5, iy - 7);
            SDL_RenderDrawLine(renderer, ix + 5, iy - 7, ix + 10, iy);
            SDL_RenderDrawLine(renderer, ix + 10, iy, ix + 5, iy + 7);
            SDL_RenderDrawLine(renderer, ix + 5, iy + 7, ix, iy);

            SDL_Rect shR = {640 - shSurf->w / 2, 57, shSurf->w, shSurf->h};
            SDL_RenderCopy(renderer, shTex, nullptr, &shR);
            SDL_DestroyTexture(shTex);
        }
        SDL_FreeSurface(shSurf);
    }

    // Separator line
    SDL_SetRenderDrawColor(renderer, 80, 50, 140, 100);
    SDL_RenderDrawLine(renderer, margin, 90, SCREEN_WIDTH - margin, 90);

    // Upgrade list
    int endIdx = std::min(m_scrollOffset + VISIBLE_ITEMS, static_cast<int>(upgradeList.size()));
    for (int i = m_scrollOffset; i < endIdx; i++) {
        auto& u = upgradeList[i];
        int y = startY + (i - m_scrollOffset) * itemH;
        bool selected = (i == m_selectedUpgrade);
        bool maxed = u.currentLevel >= u.maxLevel;

        // Card background with gradient
        SDL_Rect card = {margin, y, SCREEN_WIDTH - margin * 2, itemH - 4};
        if (selected) {
            // Top half
            SDL_SetRenderDrawColor(renderer, 65, 40, 110, 230);
            SDL_Rect topH = {card.x, card.y, card.w, card.h / 2};
            SDL_RenderFillRect(renderer, &topH);
            // Bottom half
            SDL_SetRenderDrawColor(renderer, 50, 30, 90, 230);
            SDL_Rect botH = {card.x, card.y + card.h / 2, card.w, card.h - card.h / 2};
            SDL_RenderFillRect(renderer, &botH);
        } else {
            SDL_SetRenderDrawColor(renderer, 25, 20, 40, 180);
            SDL_RenderFillRect(renderer, &card);
        }

        // Card border
        if (selected) {
            SDL_SetRenderDrawColor(renderer, 140, 90, 220, 200);
            SDL_RenderDrawRect(renderer, &card);
            // Top highlight
            SDL_SetRenderDrawColor(renderer, 160, 120, 240, 80);
            SDL_RenderDrawLine(renderer, card.x + 1, card.y, card.x + card.w - 1, card.y);
        } else {
            SDL_SetRenderDrawColor(renderer, 50, 40, 70, 120);
            SDL_RenderDrawRect(renderer, &card);
        }

        // Upgrade icon (procedural shape based on ID)
        int iconX = margin + 12;
        int iconY = y + 12;
        int iconSize = 28;
        Uint8 iconAlpha = selected ? static_cast<Uint8>(220) : static_cast<Uint8>(140);
        SDL_SetRenderDrawColor(renderer, 140, 100, 220, iconAlpha);
        SDL_Rect iconBg = {iconX, iconY, iconSize, iconSize};
        SDL_RenderDrawRect(renderer, &iconBg);
        // Inner pattern varies by upgrade
        int pattern = i % 4;
        if (pattern == 0) { // Cross
            SDL_RenderDrawLine(renderer, iconX + 6, iconY + 14, iconX + 22, iconY + 14);
            SDL_RenderDrawLine(renderer, iconX + 14, iconY + 6, iconX + 14, iconY + 22);
        } else if (pattern == 1) { // Diamond
            int cx = iconX + 14, cy = iconY + 14;
            SDL_RenderDrawLine(renderer, cx, cy - 8, cx + 8, cy);
            SDL_RenderDrawLine(renderer, cx + 8, cy, cx, cy + 8);
            SDL_RenderDrawLine(renderer, cx, cy + 8, cx - 8, cy);
            SDL_RenderDrawLine(renderer, cx - 8, cy, cx, cy - 8);
        } else if (pattern == 2) { // Arrow up
            int cx = iconX + 14;
            SDL_RenderDrawLine(renderer, cx, iconY + 6, cx + 7, iconY + 16);
            SDL_RenderDrawLine(renderer, cx, iconY + 6, cx - 7, iconY + 16);
            SDL_RenderDrawLine(renderer, cx, iconY + 6, cx, iconY + 22);
        } else { // Circle (octagon)
            int cx = iconX + 14, cy = iconY + 14;
            for (int s = 0; s < 8; s++) {
                float a1 = s * 0.785398f;
                float a2 = (s + 1) * 0.785398f;
                SDL_RenderDrawLine(renderer,
                    cx + static_cast<int>(std::cos(a1) * 9),
                    cy + static_cast<int>(std::sin(a1) * 9),
                    cx + static_cast<int>(std::cos(a2) * 9),
                    cy + static_cast<int>(std::sin(a2) * 9));
            }
        }

        // Name
        int textX = margin + 50;
        SDL_Color nameColor = selected ? SDL_Color{235, 230, 250, 255} : SDL_Color{200, 195, 220, 255};
        renderText(renderer, font, u.name.c_str(), textX, y + 6, nameColor);

        // Description
        SDL_Color descColor = {140, 135, 165, 180};
        renderText(renderer, font, u.description.c_str(), textX, y + 28, descColor);

        // Progress bar for level
        int barX = SCREEN_WIDTH - margin - 220;
        int barY = y + 10;
        int barW = 100;
        int barH = 10;
        float progress = u.maxLevel > 0 ? static_cast<float>(u.currentLevel) / u.maxLevel : 0.0f;

        // Bar background
        SDL_SetRenderDrawColor(renderer, 20, 15, 35, 200);
        SDL_Rect barBg = {barX, barY, barW, barH};
        SDL_RenderFillRect(renderer, &barBg);

        // Bar fill
        int fillW = static_cast<int>(barW * progress);
        if (fillW > 0) {
            SDL_Color barColor = maxed ? SDL_Color{80, 220, 80, 255} : SDL_Color{130, 80, 220, 255};
            SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, barColor.a);
            SDL_Rect barFill = {barX, barY, fillW, barH};
            SDL_RenderFillRect(renderer, &barFill);
        }

        // Bar border
        SDL_SetRenderDrawColor(renderer, 80, 60, 120, 150);
        SDL_RenderDrawRect(renderer, &barBg);

        // Level text
        char levelText[32];
        std::snprintf(levelText, sizeof(levelText), "Lv %d/%d", u.currentLevel, u.maxLevel);
        SDL_Color lvColor = maxed ? SDL_Color{100, 240, 100, 255} : SDL_Color{190, 185, 210, 255};
        renderText(renderer, font, levelText, barX, barY + 14, lvColor);

        // Cost
        if (!maxed) {
            char costText[32];
            std::snprintf(costText, sizeof(costText), "Cost: %d", u.getNextCost());
            bool canAfford = u.canPurchase(upgrades.getRiftShards());
            SDL_Color costColor = canAfford ?
                SDL_Color{180, 140, 255, 255} : SDL_Color{140, 70, 70, 200};
            renderText(renderer, font, costText, barX, barY + 34, costColor);
        } else {
            renderText(renderer, font, "MAXED", barX + 20, barY + 34, {80, 200, 80, 200});
        }
    }

    // Scroll indicators
    if (m_scrollOffset > 0) {
        SDL_SetRenderDrawColor(renderer, 140, 100, 220, 150);
        int cx = 640;
        SDL_RenderDrawLine(renderer, cx - 8, startY - 8, cx, startY - 14);
        SDL_RenderDrawLine(renderer, cx, startY - 14, cx + 8, startY - 8);
    }
    if (endIdx < static_cast<int>(upgradeList.size())) {
        SDL_SetRenderDrawColor(renderer, 140, 100, 220, 150);
        int cx = 640;
        int botY = startY + VISIBLE_ITEMS * itemH;
        SDL_RenderDrawLine(renderer, cx - 8, botY + 2, cx, botY + 8);
        SDL_RenderDrawLine(renderer, cx, botY + 8, cx + 8, botY + 2);
    }

    // Purchase flash overlay
    if (m_flashTimer > 0) {
        Uint8 fa = static_cast<Uint8>(m_flashTimer * 80);
        SDL_SetRenderDrawColor(renderer, 140, 100, 255, fa);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);
    }

    // Instructions
    SDL_SetRenderDrawColor(renderer, 60, 40, 90, 80);
    SDL_Rect instrBg = {0, 670, SCREEN_WIDTH, 50};
    SDL_RenderFillRect(renderer, &instrBg);
    renderText(renderer, font, "W/S: Navigate    ENTER: Purchase    ESC: Back",
               640 - 200, 683, {120, 110, 150, 180});
}
