#include "UpgradeState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/UpgradeSystem.h"
#include "UI/UITextures.h"
#include <cstdio>
#include <cmath>

// Layout constants (scaled for 2560x1440)
static constexpr int UPGRADE_MARGIN = 200;
static constexpr int UPGRADE_ITEM_H = 130;
static constexpr int UPGRADE_START_Y = 210;

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
            case SDL_SCANCODE_PAGEUP:
                m_selectedUpgrade = std::max(0, m_selectedUpgrade - VISIBLE_ITEMS);
                if (m_selectedUpgrade < m_scrollOffset) m_scrollOffset = m_selectedUpgrade;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_PAGEDOWN:
                m_selectedUpgrade = std::min(total - 1, m_selectedUpgrade + VISIBLE_ITEMS);
                if (m_selectedUpgrade >= m_scrollOffset + VISIBLE_ITEMS)
                    m_scrollOffset = m_selectedUpgrade - VISIBLE_ITEMS + 1;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_HOME:
                m_selectedUpgrade = 0;
                m_scrollOffset = 0;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_END:
                m_selectedUpgrade = total - 1;
                m_scrollOffset = std::max(0, total - VISIBLE_ITEMS);
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            default: break;
        }
    }

    // Mouse hover: update selection
    if (event.type == SDL_MOUSEMOTION && total > 0) {
        int mx = event.motion.x, my = event.motion.y;
        const int margin = UPGRADE_MARGIN, itemH = UPGRADE_ITEM_H, startY = UPGRADE_START_Y;
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

    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
    }

    // Mouse wheel scrolling
    if (event.type == SDL_MOUSEWHEEL && total > 0) {
        if (event.wheel.y > 0) {
            m_selectedUpgrade = (m_selectedUpgrade - 1 + total) % total;
            if (m_selectedUpgrade < m_scrollOffset) m_scrollOffset = m_selectedUpgrade;
            if (m_selectedUpgrade >= m_scrollOffset + VISIBLE_ITEMS)
                m_scrollOffset = m_selectedUpgrade - VISIBLE_ITEMS + 1;
            AudioManager::instance().play(SFX::MenuSelect);
        } else if (event.wheel.y < 0) {
            m_selectedUpgrade = (m_selectedUpgrade + 1) % total;
            if (m_selectedUpgrade >= m_scrollOffset + VISIBLE_ITEMS)
                m_scrollOffset = m_selectedUpgrade - VISIBLE_ITEMS + 1;
            if (m_selectedUpgrade < m_scrollOffset) m_scrollOffset = m_selectedUpgrade;
            AudioManager::instance().play(SFX::MenuSelect);
        }
    }

    // Mouse click: select + purchase
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && total > 0) {
        int mx = event.button.x, my = event.button.y;
        const int margin = UPGRADE_MARGIN, itemH = UPGRADE_ITEM_H, startY = UPGRADE_START_Y;
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

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    auto& upgrades = game->getUpgradeSystem();
    int total = static_cast<int>(upgrades.getUpgrades().size());
    if (total == 0) return;

    if (input.isActionPressed(Action::MenuUp)) {
        m_selectedUpgrade = (m_selectedUpgrade - 1 + total) % total;
        if (m_selectedUpgrade < m_scrollOffset) m_scrollOffset = m_selectedUpgrade;
        if (m_selectedUpgrade >= m_scrollOffset + VISIBLE_ITEMS)
            m_scrollOffset = m_selectedUpgrade - VISIBLE_ITEMS + 1;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_selectedUpgrade = (m_selectedUpgrade + 1) % total;
        if (m_selectedUpgrade >= m_scrollOffset + VISIBLE_ITEMS)
            m_scrollOffset = m_selectedUpgrade - VISIBLE_ITEMS + 1;
        if (m_selectedUpgrade < m_scrollOffset) m_scrollOffset = m_selectedUpgrade;
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::Confirm)) {
        purchaseSelected();
    }
    if (input.isActionPressed(Action::Cancel)) {
        game->changeState(StateID::Menu);
    }
}

void UpgradeState::renderText(SDL_Renderer* renderer, TTF_Font* font,
                               const char* text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text, color);
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
    SDL_Surface* titleSurf = TTF_RenderUTF8_Blended(font, LOC("upgrades.title"), {160, 110, 240, 255});
    if (titleSurf) {
        SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
        if (titleTex) {
            int tw = titleSurf->w;
            int th = titleSurf->h;
            // Glow
            SDL_SetTextureBlendMode(titleTex, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(titleTex, 40);
            SDL_Rect glowR = {SCREEN_WIDTH / 2 - tw / 2 - 2, 44, tw + 4, th + 4};
            SDL_RenderCopy(renderer, titleTex, nullptr, &glowR);
            // Main
            SDL_SetTextureBlendMode(titleTex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(titleTex, 255);
            SDL_Rect titleR = {SCREEN_WIDTH / 2 - tw / 2, 50, tw, th};
            SDL_RenderCopy(renderer, titleTex, nullptr, &titleR);
            SDL_DestroyTexture(titleTex);
        }
        SDL_FreeSurface(titleSurf);
    }

    // Shard count with diamond icon (centered under title)
    char shardText[64];
    std::snprintf(shardText, sizeof(shardText), LOC("menu.rift_shards"), upgrades.getRiftShards());
    SDL_Surface* shSurf = TTF_RenderUTF8_Blended(font, shardText, {200, 160, 255, 255});
    if (shSurf) {
        SDL_Texture* shTex = SDL_CreateTextureFromSurface(renderer, shSurf);
        if (shTex) {
            // Diamond icon
            int ix = SCREEN_WIDTH / 2 - shSurf->w / 2 - 18;
            int iy = 65;
            SDL_SetRenderDrawColor(renderer, 200, 160, 255, 230);
            SDL_RenderDrawLine(renderer, ix, iy, ix + 5, iy - 7);
            SDL_RenderDrawLine(renderer, ix + 5, iy - 7, ix + 10, iy);
            SDL_RenderDrawLine(renderer, ix + 10, iy, ix + 5, iy + 7);
            SDL_RenderDrawLine(renderer, ix + 5, iy + 7, ix, iy);

            SDL_Rect shR = {SCREEN_WIDTH / 2 - shSurf->w / 2, 114, shSurf->w, shSurf->h};
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
            if (!renderPanelBg(renderer, card, 230, "assets/textures/ui/panel_light.png")) {
                // Top half
                SDL_SetRenderDrawColor(renderer, 65, 40, 110, 230);
                SDL_Rect topH = {card.x, card.y, card.w, card.h / 2};
                SDL_RenderFillRect(renderer, &topH);
                // Bottom half
                SDL_SetRenderDrawColor(renderer, 50, 30, 90, 230);
                SDL_Rect botH = {card.x, card.y + card.h / 2, card.w, card.h - card.h / 2};
                SDL_RenderFillRect(renderer, &botH);
            }
        } else {
            if (!renderPanelBg(renderer, card, 180)) {
                SDL_SetRenderDrawColor(renderer, 25, 20, 40, 180);
                SDL_RenderFillRect(renderer, &card);
            }
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
        int iconX = margin + 24;
        int iconY = y + 24;
        int iconSize = 56;
        Uint8 iconAlpha = selected ? static_cast<Uint8>(220) : static_cast<Uint8>(140);
        SDL_SetRenderDrawColor(renderer, 140, 100, 220, iconAlpha);
        SDL_Rect iconBg = {iconX, iconY, iconSize, iconSize};
        SDL_RenderDrawRect(renderer, &iconBg);
        // Inner pattern varies by upgrade
        int pattern = i % 4;
        if (pattern == 0) { // Cross
            SDL_RenderDrawLine(renderer, iconX + 12, iconY + 28, iconX + 44, iconY + 28);
            SDL_RenderDrawLine(renderer, iconX + 28, iconY + 12, iconX + 28, iconY + 44);
        } else if (pattern == 1) { // Diamond
            int cx = iconX + 28, cy = iconY + 28;
            SDL_RenderDrawLine(renderer, cx, cy - 16, cx + 16, cy);
            SDL_RenderDrawLine(renderer, cx + 16, cy, cx, cy + 16);
            SDL_RenderDrawLine(renderer, cx, cy + 16, cx - 16, cy);
            SDL_RenderDrawLine(renderer, cx - 16, cy, cx, cy - 16);
        } else if (pattern == 2) { // Arrow up
            int cx = iconX + 28;
            SDL_RenderDrawLine(renderer, cx, iconY + 12, cx + 14, iconY + 32);
            SDL_RenderDrawLine(renderer, cx, iconY + 12, cx - 14, iconY + 32);
            SDL_RenderDrawLine(renderer, cx, iconY + 12, cx, iconY + 44);
        } else { // Circle (octagon)
            int cx = iconX + 28, cy = iconY + 28;
            for (int s = 0; s < 8; s++) {
                float a1 = s * 0.785398f;
                float a2 = (s + 1) * 0.785398f;
                SDL_RenderDrawLine(renderer,
                    cx + static_cast<int>(std::cos(a1) * 18),
                    cy + static_cast<int>(std::sin(a1) * 18),
                    cx + static_cast<int>(std::cos(a2) * 18),
                    cy + static_cast<int>(std::sin(a2) * 18));
            }
        }

        // Name (localized via upgrade.N.name key)
        int textX = margin + 100;
        SDL_Color nameColor = selected ? SDL_Color{235, 230, 250, 255} : SDL_Color{200, 195, 220, 255};
        char nameKey[32], descKey[32];
        std::snprintf(nameKey, sizeof(nameKey), "upgrade.%d.name", static_cast<int>(u.id));
        std::snprintf(descKey, sizeof(descKey), "upgrade.%d.desc", static_cast<int>(u.id));
        renderText(renderer, font, LOC(nameKey), textX, y + 12, nameColor);

        // Description (localized)
        SDL_Color descColor = {140, 135, 165, 180};
        renderText(renderer, font, LOC(descKey), textX, y + 56, descColor);

        // Progress bar for level
        int barX = SCREEN_WIDTH - margin - 440;
        int barY = y + 20;
        int barW = 200;
        int barH = 20;
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
        std::snprintf(levelText, sizeof(levelText), LOC("upgrades.level"), u.currentLevel, u.maxLevel);
        SDL_Color lvColor = maxed ? SDL_Color{100, 240, 100, 255} : SDL_Color{190, 185, 210, 255};
        renderText(renderer, font, levelText, barX, barY + 28, lvColor);

        // Cost
        if (!maxed) {
            char costText[32];
            std::snprintf(costText, sizeof(costText), LOC("upgrades.cost"), u.getNextCost());
            bool canAfford = u.canPurchase(upgrades.getRiftShards());
            SDL_Color costColor = canAfford ?
                SDL_Color{180, 140, 255, 255} : SDL_Color{140, 70, 70, 200};
            renderText(renderer, font, costText, barX, barY + 68, costColor);
        } else {
            renderText(renderer, font, LOC("upgrades.maxed"), barX + 40, barY + 68, {80, 200, 80, 200});
        }
    }

    // Scroll indicators
    if (m_scrollOffset > 0) {
        SDL_SetRenderDrawColor(renderer, 140, 100, 220, 150);
        int cx = SCREEN_WIDTH / 2;
        SDL_RenderDrawLine(renderer, cx - 16, startY - 16, cx, startY - 28);
        SDL_RenderDrawLine(renderer, cx, startY - 28, cx + 16, startY - 16);
    }
    if (endIdx < static_cast<int>(upgradeList.size())) {
        SDL_SetRenderDrawColor(renderer, 140, 100, 220, 150);
        int cx = SCREEN_WIDTH / 2;
        int botY = startY + VISIBLE_ITEMS * itemH;
        SDL_RenderDrawLine(renderer, cx - 16, botY + 4, cx, botY + 16);
        SDL_RenderDrawLine(renderer, cx, botY + 16, cx + 16, botY + 4);
    }

    // Purchase flash overlay
    if (m_flashTimer > 0) {
        Uint8 fa = static_cast<Uint8>(m_flashTimer * 80);
        SDL_SetRenderDrawColor(renderer, 140, 100, 255, fa);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);
    }

    // Instructions
    SDL_Rect instrBg = {0, SCREEN_HEIGHT - 50, SCREEN_WIDTH, 50};
    if (!renderPanelBg(renderer, instrBg, 80)) {
        SDL_SetRenderDrawColor(renderer, 60, 40, 90, 80);
        SDL_RenderFillRect(renderer, &instrBg);
    }
    renderText(renderer, font, LOC("upgrades.nav_hint"),
               SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT - 74, {120, 110, 150, 180});
}
