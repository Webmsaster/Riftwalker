// PlayStateRenderWorld.cpp -- Split from PlayStateRender.cpp (world entities, NPCs, events)
#include "PlayState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AbilityComponent.h"
#include "Core/AudioManager.h"
#include "Game/AchievementSystem.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/LoreSystem.h"
#include "Game/DailyRun.h"
#include "Game/Bestiary.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

void PlayState::renderLevelCompleteTransition(SDL_Renderer* renderer) {
    if (!m_levelComplete) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    float transitionDur = m_playtest ? 0.1f : 2.0f;
    float progress = m_levelCompleteTimer / transitionDur;
    if (progress > 1.0f) progress = 1.0f;

    // Iris center: player screen pos or screen center
    int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;
    if (m_player && m_player->getEntity()) {
        Vec2 wp = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        Vec2 sp = m_camera.worldToScreen(wp);
        cx = static_cast<int>(sp.x);
        cy = static_cast<int>(sp.y);
    }

    // Max radius = distance from center to farthest corner
    float dx = std::max((float)cx, (float)(SCREEN_WIDTH - cx));
    float dy = std::max((float)cy, (float)(SCREEN_HEIGHT - cy));
    float maxR = std::sqrt(dx * dx + dy * dy);

    // Iris shrinks from maxR to 0 as progress goes 0->1
    float irisR = maxR * (1.0f - progress);
    float glowWidth = 12.0f;
    Uint32 ticks = SDL_GetTicks();
    float pulse = 0.8f + 0.2f * std::sin(ticks * 0.008f);

    // Iris wipe: dark fill outside shrinking circle (2-pixel scanline steps for performance)
    constexpr int STEP = 2;
    SDL_SetRenderDrawColor(renderer, 10, 5, 20, 255);
    for (int y = 0; y < SCREEN_HEIGHT; y += STEP) {
        float fy = static_cast<float>(y + STEP / 2 - cy);
        float r2 = irisR * irisR - fy * fy;
        if (r2 <= 0.0f) {
            // Entire row is outside circle
            SDL_Rect row = {0, y, SCREEN_WIDTH, STEP};
            SDL_RenderFillRect(renderer, &row);
            continue;
        }
        float halfW = std::sqrt(r2);
        int left = cx - static_cast<int>(halfW);
        int right = cx + static_cast<int>(halfW);
        if (left > 0) { SDL_Rect r = {0, y, left, STEP}; SDL_RenderFillRect(renderer, &r); }
        if (right < SCREEN_WIDTH) { SDL_Rect r = {right, y, SCREEN_WIDTH - right, STEP}; SDL_RenderFillRect(renderer, &r); }
    }

    // Glowing purple rim at circle edge (ring of dots at the iris boundary)
    int rimPoints = std::max(60, static_cast<int>(irisR * 0.5f));
    int glowLayers = static_cast<int>(glowWidth * pulse);
    for (int layer = 0; layer < glowLayers; layer += 2) {
        float t = static_cast<float>(layer) / glowWidth;
        Uint8 a = static_cast<Uint8>((1.0f - t) * 200 * pulse);
        float lr = irisR - layer; // inner glow
        if (lr <= 0.0f) break;
        Uint8 red = static_cast<Uint8>(std::min(255.0f, 160.0f + 60.0f * t));
        SDL_SetRenderDrawColor(renderer, red, 80, 255, a);
        for (int i = 0; i < rimPoints; i++) {
            float ang = i * 6.283185f / rimPoints + ticks * 0.001f;
            int px = cx + static_cast<int>(std::cos(ang) * lr);
            int py = cy + static_cast<int>(std::sin(ang) * lr);
            if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT) continue;
            SDL_Rect dot = {px - 1, py - 1, 3, 3};
            SDL_RenderFillRect(renderer, &dot);
        }
    }

    // Text banner with gameplay tip (fades in quickly)
    if (progress < 0.85f) {
        float textAlpha = std::min(1.0f, progress * 3.0f);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(160 * textAlpha));
        SDL_Rect banner = {0, 640, SCREEN_WIDTH, 160};
        SDL_RenderFillRect(renderer, &banner);

        TTF_Font* font = game->getFont();
        if (font) {
            Uint8 ta = static_cast<Uint8>(textAlpha * 255);

            // Main transition text
            SDL_Color c = {140, 255, 180, ta};
            SDL_Surface* s = TTF_RenderText_Blended(font, LOC("hud.rift_stabilized"), c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {SCREEN_WIDTH / 2 - s->w / 2, 668, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }

            // Gameplay tip (seeded by current floor for consistency, localized)
            static const char* tipKeys[] = {
                "tip.0", "tip.1", "tip.2", "tip.3", "tip.4",
                "tip.5", "tip.6", "tip.7", "tip.8", "tip.9",
                "tip.10", "tip.11", "tip.12", "tip.13", "tip.14",
                "tip.15", "tip.16", "tip.17", "tip.18", "tip.19",
                "tip.20", "tip.21", "tip.22", "tip.23", "tip.24",
                "tip.25", "tip.26", "tip.27", "tip.28", "tip.29",
                "tip.30", "tip.31", "tip.32", "tip.33", "tip.34",
                "tip.35", "tip.36", "tip.37", "tip.38", "tip.39",
                "tip.40", "tip.41", "tip.42", "tip.43", "tip.44",
                "tip.45", "tip.46", "tip.47", "tip.48", "tip.49",
            };
            constexpr int NUM_TIPS = sizeof(tipKeys) / sizeof(tipKeys[0]);
            int tipIdx = (m_currentDifficulty + m_runSeed) % NUM_TIPS;

            Uint8 tipAlpha = static_cast<Uint8>(textAlpha * 160);
            SDL_Color tipColor = {160, 150, 180, tipAlpha};
            SDL_Surface* ts = TTF_RenderText_Blended(font, LOC(tipKeys[tipIdx]), tipColor);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    // Render at 75% scale for a subtler look
                    int tipW = ts->w * 3 / 4;
                    int tipH = ts->h * 3 / 4;
                    SDL_Rect tr = {SCREEN_WIDTH / 2 - tipW / 2, 738, tipW, tipH};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
        }
    }
}

void PlayState::renderRandomEvents(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& events = m_level->getRandomEvents();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        SDL_FRect worldRect = {event.position.x - 16, event.position.y - 32, 32, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        // Skip if off screen (generous margin for glow/aura)
        if (sr.x + sr.w < -40 || sr.x > SCREEN_WIDTH + 40 ||
            sr.y + sr.h < -40 || sr.y > SCREEN_HEIGHT + 40) continue;

        float bob = std::sin(ticks * 0.003f + i * 2.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);
        float t = ticks * 0.001f; // Time in seconds

        // Event color
        SDL_Color col;
        switch (event.type) {
            case RandomEventType::Merchant:           col = {80, 200, 80, 255}; break;
            case RandomEventType::Shrine:             col = event.getShrineColor(); break;
            case RandomEventType::DimensionalAnomaly: col = {200, 50, 200, 255}; break;
            case RandomEventType::RiftEcho:           col = {150, 150, 255, 255}; break;
            case RandomEventType::SuitRepairStation:  col = {100, 255, 150, 255}; break;
            case RandomEventType::GamblingRift:       col = {255, 200, 50, 255}; break;
        }

        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i);
        int cx = sr.x + sr.w / 2;
        int cy = sr.y + sr.h / 2;

        // ---- Ground shadow (soft, multi-layer) ----
        for (int s = 3; s >= 0; s--) {
            int expand = s * 3;
            Uint8 sa = static_cast<Uint8>(20 - s * 4);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, sa);
            SDL_Rect shadow = {sr.x - expand, sr.y + sr.h - 2 + s, sr.w + expand * 2, 4 + s};
            SDL_RenderFillRect(renderer, &shadow);
        }

        // ---- Pulsing glow aura (4 layers) ----
        for (int layer = 4; layer >= 1; layer--) {
            int expand = layer * 5 + static_cast<int>(glow * 4);
            Uint8 ga = static_cast<Uint8>(std::min(255.0f, (15.0f + 10.0f * glow) * (5 - layer)));
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ga);
            SDL_Rect glowR = {sr.x - expand, sr.y - expand, sr.w + expand * 2, sr.h + expand * 2};
            SDL_RenderFillRect(renderer, &glowR);
        }

        // ============================================================
        // Type-specific detailed rendering
        // ============================================================
        switch (event.type) {

        case RandomEventType::Merchant: {
            // --- Merchant stall with canopy ---
            int stallW = sr.w + 12;
            int stallH = sr.h;
            int stallX = cx - stallW / 2;
            int stallY = sr.y;

            // Canopy (top part, striped awning effect)
            int canopyH = stallH / 3;
            for (int stripe = 0; stripe < 5; stripe++) {
                int sw = stallW + 8;
                int sx = stallX - 4 + stripe * sw / 5;
                Uint8 r = (stripe % 2 == 0) ? 60 : 30;
                Uint8 g = (stripe % 2 == 0) ? 160 : 100;
                Uint8 b = (stripe % 2 == 0) ? 60 : 40;
                SDL_SetRenderDrawColor(renderer, r, g, b, 220);
                SDL_Rect canopyStripe = {sx, stallY - canopyH, sw / 5 + 1, canopyH};
                SDL_RenderFillRect(renderer, &canopyStripe);
            }
            // Canopy front edge (darker strip)
            SDL_SetRenderDrawColor(renderer, 40, 100, 40, 240);
            SDL_Rect canopyEdge = {stallX - 4, stallY - 2, stallW + 8, 3};
            SDL_RenderFillRect(renderer, &canopyEdge);

            // Support poles
            SDL_SetRenderDrawColor(renderer, 90, 70, 50, 220);
            SDL_Rect poleL = {stallX - 2, stallY - canopyH, 3, stallH + canopyH};
            SDL_Rect poleR = {stallX + stallW - 1, stallY - canopyH, 3, stallH + canopyH};
            SDL_RenderFillRect(renderer, &poleL);
            SDL_RenderFillRect(renderer, &poleR);

            // Counter/table
            SDL_SetRenderDrawColor(renderer, 100, 75, 45, 230);
            SDL_Rect counter = {stallX, stallY + stallH / 2, stallW, stallH / 2};
            SDL_RenderFillRect(renderer, &counter);
            // Counter top highlight
            SDL_SetRenderDrawColor(renderer, 130, 100, 60, 200);
            SDL_Rect counterTop = {stallX, stallY + stallH / 2, stallW, 3};
            SDL_RenderFillRect(renderer, &counterTop);

            // Goods on counter (3 small colored items)
            for (int item = 0; item < 3; item++) {
                int ix = stallX + 4 + item * (stallW / 3);
                int iy = stallY + stallH / 2 - 5;
                float itemBob = std::sin(t * 2.0f + item * 1.5f) * 1.5f;
                iy -= static_cast<int>(itemBob);
                Uint8 ir = (item == 0) ? 255 : (item == 1) ? 80 : 200;
                Uint8 ig = (item == 0) ? 220 : (item == 1) ? 220 : 100;
                Uint8 ib = (item == 0) ? 60 : (item == 1) ? 255 : 255;
                // Item glow
                SDL_SetRenderDrawColor(renderer, ir, ig, ib, static_cast<Uint8>(40 * glow));
                SDL_Rect itemGlow = {ix - 2, iy - 2, 8, 8};
                SDL_RenderFillRect(renderer, &itemGlow);
                // Item body
                SDL_SetRenderDrawColor(renderer, ir, ig, ib, 220);
                SDL_Rect itemR = {ix, iy, 5, 5};
                SDL_RenderFillRect(renderer, &itemR);
            }

            // Coin icon floating above
            float coinBob = std::sin(t * 3.0f) * 3.0f;
            int coinY = stallY - canopyH - 8 - static_cast<int>(coinBob);
            SDL_SetRenderDrawColor(renderer, 255, 215, 60, static_cast<Uint8>(180 * glow + 60));
            SDL_Rect coin = {cx - 4, coinY - 4, 8, 8};
            SDL_RenderFillRect(renderer, &coin);
            // Coin inner shine
            SDL_SetRenderDrawColor(renderer, 255, 255, 180, 160);
            SDL_Rect coinShine = {cx - 2, coinY - 2, 4, 4};
            SDL_RenderFillRect(renderer, &coinShine);
            // Dollar sign line
            SDL_SetRenderDrawColor(renderer, 180, 150, 30, 200);
            SDL_RenderDrawLine(renderer, cx, coinY - 5, cx, coinY + 5);
            break;
        }

        case RandomEventType::Shrine: {
            // --- Ornate pedestal with glowing crystal ---
            int pedW = sr.w + 4;
            int pedX = cx - pedW / 2;
            int baseY = sr.y + sr.h;

            // Pedestal base (wide, darker)
            SDL_SetRenderDrawColor(renderer, 60, 55, 70, 230);
            SDL_Rect base = {pedX - 4, baseY - 6, pedW + 8, 6};
            SDL_RenderFillRect(renderer, &base);
            // Pedestal column (narrower, gradient layers)
            for (int layer = 0; layer < 4; layer++) {
                int shrink = layer * 2;
                float brightness = 0.6f + layer * 0.1f;
                Uint8 r = static_cast<Uint8>(std::min(255.0f, 80 * brightness));
                Uint8 g = static_cast<Uint8>(std::min(255.0f, 75 * brightness));
                Uint8 b = static_cast<Uint8>(std::min(255.0f, 95 * brightness));
                SDL_SetRenderDrawColor(renderer, r, g, b, 220);
                int colH = sr.h / 2;
                SDL_Rect column = {pedX + shrink, baseY - 6 - colH + layer * 2, pedW - shrink * 2, colH - layer * 2};
                SDL_RenderFillRect(renderer, &column);
            }
            // Pedestal top plate
            SDL_SetRenderDrawColor(renderer, 100, 95, 115, 240);
            int plateY = baseY - 6 - sr.h / 2 - 2;
            SDL_Rect plate = {pedX - 2, plateY, pedW + 4, 4};
            SDL_RenderFillRect(renderer, &plate);

            // Crystal on top
            int crystalH = sr.h / 2 + 2;
            int crystalW = sr.w / 3;
            int crystalX = cx - crystalW / 2;
            int crystalY = plateY - crystalH;

            // Crystal glow (pulsing, colored by shrine type)
            for (int gLayer = 3; gLayer >= 0; gLayer--) {
                int expand = gLayer * 4 + static_cast<int>(glow * 3);
                Uint8 ga2 = static_cast<Uint8>(std::min(255.0f, (20.0f + 15.0f * glow) * (4 - gLayer)));
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ga2);
                SDL_Rect cGlow = {crystalX - expand, crystalY - expand, crystalW + expand * 2, crystalH + expand * 2};
                SDL_RenderFillRect(renderer, &cGlow);
            }

            // Crystal body (gradient: dark at base, bright at tip)
            for (int row = 0; row < crystalH; row++) {
                float frac = static_cast<float>(row) / crystalH;
                // Narrow towards top (diamond shape)
                int rowW = static_cast<int>(crystalW * (1.0f - frac * 0.6f));
                Uint8 cr = static_cast<Uint8>(std::min(255.0f, col.r * (0.5f + 0.5f * frac) + 40 * frac));
                Uint8 cg = static_cast<Uint8>(std::min(255.0f, col.g * (0.5f + 0.5f * frac) + 40 * frac));
                Uint8 cb = static_cast<Uint8>(std::min(255.0f, col.b * (0.5f + 0.5f * frac) + 40 * frac));
                SDL_SetRenderDrawColor(renderer, cr, cg, cb, static_cast<Uint8>(200 + 40 * frac));
                SDL_Rect crystalRow = {cx - rowW / 2, crystalY + crystalH - 1 - row, rowW, 1};
                SDL_RenderFillRect(renderer, &crystalRow);
            }
            // Crystal highlight (white streak)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(120 + 80 * glow));
            SDL_Rect highlight = {cx - 1, crystalY + 2, 2, crystalH / 2};
            SDL_RenderFillRect(renderer, &highlight);

            // Particle sparkles around crystal
            for (int p = 0; p < 4; p++) {
                float angle = t * 1.5f + p * 1.57f;
                float radius = 10.0f + 4.0f * std::sin(t * 2.0f + p);
                int px = cx + static_cast<int>(std::cos(angle) * radius);
                int py = crystalY + crystalH / 2 + static_cast<int>(std::sin(angle) * radius * 0.6f);
                Uint8 pa = static_cast<Uint8>(120 + 80 * std::sin(t * 4.0f + p * 2.0f));
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, pa);
                SDL_Rect spark = {px - 1, py - 1, 2, 2};
                SDL_RenderFillRect(renderer, &spark);
            }

            // Rune marks on pedestal sides
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(60 + 40 * glow));
            int runeY = plateY + 6;
            for (int r = 0; r < 3; r++) {
                int rx = pedX + 3 + r * (pedW / 3);
                SDL_Rect rune = {rx, runeY + r * 4, 3, 2};
                SDL_RenderFillRect(renderer, &rune);
            }
            break;
        }

        case RandomEventType::DimensionalAnomaly: {
            // --- Swirling vortex (concentric rotating rectangles) ---
            int vortexSize = std::max(sr.w, sr.h) + 8;

            // Background dark core
            SDL_SetRenderDrawColor(renderer, 10, 0, 20, 200);
            SDL_Rect core = {cx - vortexSize / 4, cy - vortexSize / 4, vortexSize / 2, vortexSize / 2};
            SDL_RenderFillRect(renderer, &core);

            // Concentric rotating layers (8 layers)
            for (int layer = 7; layer >= 0; layer--) {
                float layerT = t * (1.5f + layer * 0.3f) + layer * 0.8f;
                float scale = 0.15f + layer * 0.12f;
                int size = static_cast<int>(vortexSize * scale);

                // Rotation simulated by shifting x/y based on sin/cos
                int offsetX = static_cast<int>(std::cos(layerT) * layer * 2);
                int offsetY = static_cast<int>(std::sin(layerT) * layer * 2);

                float layerFrac = static_cast<float>(layer) / 7.0f;
                Uint8 r = static_cast<Uint8>(std::min(255.0f, 80 + 170 * layerFrac));
                Uint8 g = static_cast<Uint8>(std::min(255.0f, 20 + 30 * layerFrac));
                Uint8 b = static_cast<Uint8>(std::min(255.0f, 120 + 135 * layerFrac));
                Uint8 a = static_cast<Uint8>(std::min(255.0f, 180 - layer * 15.0f + 40.0f * glow));

                SDL_SetRenderDrawColor(renderer, r, g, b, a);
                SDL_Rect vRect = {cx - size / 2 + offsetX, cy - size / 2 + offsetY, size, size};
                SDL_RenderDrawRect(renderer, &vRect);

                // Fill inner layers more
                if (layer < 4) {
                    SDL_SetRenderDrawColor(renderer, r, g, b, static_cast<Uint8>(a / 3));
                    SDL_RenderFillRect(renderer, &vRect);
                }
            }

            // Bright center eye
            SDL_SetRenderDrawColor(renderer, 255, 200, 255, static_cast<Uint8>(200 + 55 * glow));
            SDL_Rect eye = {cx - 3, cy - 3, 6, 6};
            SDL_RenderFillRect(renderer, &eye);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(150 + 80 * glow));
            SDL_Rect eyeCore = {cx - 1, cy - 1, 2, 2};
            SDL_RenderFillRect(renderer, &eyeCore);

            // Lightning tendrils (4 zigzag lines radiating outward)
            for (int tendril = 0; tendril < 4; tendril++) {
                float angle = t * 0.8f + tendril * 1.57f;
                Uint8 ta2 = static_cast<Uint8>(100 + 80 * std::sin(t * 5.0f + tendril));
                SDL_SetRenderDrawColor(renderer, 220, 160, 255, ta2);
                int prevX = cx, prevY = cy;
                for (int seg = 1; seg <= 4; seg++) {
                    float dist = seg * (vortexSize * 0.12f);
                    int jitterX = static_cast<int>((std::sin(t * 8.0f + seg * 3.0f + tendril) * 4));
                    int jitterY = static_cast<int>((std::cos(t * 7.0f + seg * 2.5f + tendril) * 4));
                    int nx = cx + static_cast<int>(std::cos(angle) * dist) + jitterX;
                    int ny = cy + static_cast<int>(std::sin(angle) * dist) + jitterY;
                    SDL_RenderDrawLine(renderer, prevX, prevY, nx, ny);
                    prevX = nx;
                    prevY = ny;
                }
            }
            break;
        }

        case RandomEventType::RiftEcho: {
            // --- Ghostly translucent figure ---
            int figW = sr.w;
            int figH = sr.h + 6;
            int figX = cx - figW / 2;
            int figY = sr.y - 3;

            // Ghostly flicker
            float flicker = 0.6f + 0.4f * std::sin(t * 6.0f + i * 1.5f);
            float drift = std::sin(t * 1.2f + i) * 3.0f;
            figX += static_cast<int>(drift);

            // Ghostly body layers (translucent, stacked)
            for (int layer = 4; layer >= 0; layer--) {
                int expand = layer * 2;
                float alphaFrac = (5 - layer) / 5.0f;
                Uint8 ba = static_cast<Uint8>(std::min(255.0f, 60 * alphaFrac * flicker));
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ba);
                // Taper at bottom for ghostly trail
                int bottomExpand = layer * 3;
                // Upper body
                SDL_Rect upper = {figX - expand, figY - expand, figW + expand * 2, figH * 2 / 3};
                SDL_RenderFillRect(renderer, &upper);
                // Lower body (wider, more transparent - ghost tail)
                SDL_Rect lower = {figX - expand - bottomExpand, figY + figH * 2 / 3 - expand,
                                  figW + (expand + bottomExpand) * 2, figH / 3 + expand};
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(ba / 2));
                SDL_RenderFillRect(renderer, &lower);
            }

            // Core body (more opaque)
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(100 * flicker));
            SDL_Rect body = {figX + 2, figY + 2, figW - 4, figH * 2 / 3};
            SDL_RenderFillRect(renderer, &body);

            // Head
            int headSize = figW / 2;
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(120 * flicker));
            SDL_Rect head = {cx - headSize / 2, figY - headSize / 2, headSize, headSize};
            SDL_RenderFillRect(renderer, &head);

            // Eyes (hollow, haunting)
            Uint8 eyeAlpha = static_cast<Uint8>(200 * flicker);
            SDL_SetRenderDrawColor(renderer, 200, 220, 255, eyeAlpha);
            int eyeY2 = figY - headSize / 2 + headSize / 3;
            SDL_Rect lEye = {cx - headSize / 4 - 2, eyeY2, 3, 3};
            SDL_Rect rEye = {cx + headSize / 4 - 1, eyeY2, 3, 3};
            SDL_RenderFillRect(renderer, &lEye);
            SDL_RenderFillRect(renderer, &rEye);
            // Eye glow
            SDL_SetRenderDrawColor(renderer, 200, 220, 255, static_cast<Uint8>(60 * flicker));
            SDL_Rect lEyeGlow = {cx - headSize / 4 - 4, eyeY2 - 2, 7, 7};
            SDL_Rect rEyeGlow = {cx + headSize / 4 - 3, eyeY2 - 2, 7, 7};
            SDL_RenderFillRect(renderer, &lEyeGlow);
            SDL_RenderFillRect(renderer, &rEyeGlow);

            // Wispy particles drifting upward
            for (int p = 0; p < 5; p++) {
                float pTime = t * 1.5f + p * 1.3f;
                float pY = figY - std::fmod(pTime * 20.0f, 30.0f);
                float pX = cx + std::sin(pTime * 2.0f + p) * 8.0f;
                Uint8 pA = static_cast<Uint8>(std::max(0.0f, 80.0f * (1.0f - std::fmod(pTime * 0.5f, 1.0f)) * flicker));
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, pA);
                SDL_Rect wisp = {static_cast<int>(pX) - 1, static_cast<int>(pY) - 1, 2, 2};
                SDL_RenderFillRect(renderer, &wisp);
            }
            break;
        }

        case RandomEventType::SuitRepairStation: {
            // --- Mechanical workbench ---
            int benchW = sr.w + 16;
            int benchH = sr.h;
            int benchX = cx - benchW / 2;
            int benchY = sr.y;

            // Workbench legs
            SDL_SetRenderDrawColor(renderer, 70, 70, 80, 230);
            SDL_Rect legL = {benchX + 3, benchY + benchH * 2 / 3, 4, benchH / 3};
            SDL_Rect legR = {benchX + benchW - 7, benchY + benchH * 2 / 3, 4, benchH / 3};
            SDL_RenderFillRect(renderer, &legL);
            SDL_RenderFillRect(renderer, &legR);

            // Workbench surface
            SDL_SetRenderDrawColor(renderer, 80, 85, 95, 240);
            SDL_Rect surface = {benchX, benchY + benchH / 2, benchW, benchH / 5};
            SDL_RenderFillRect(renderer, &surface);
            // Surface highlight
            SDL_SetRenderDrawColor(renderer, 110, 120, 130, 200);
            SDL_Rect surfTop = {benchX, benchY + benchH / 2, benchW, 2};
            SDL_RenderFillRect(renderer, &surfTop);

            // Mechanical arm (animated)
            float armAngle = std::sin(t * 2.0f) * 0.4f;
            int armBaseX = cx;
            int armBaseY = benchY + benchH / 2 - 2;
            int armEndX = armBaseX + static_cast<int>(std::sin(armAngle) * 12);
            int armEndY = armBaseY - 14 + static_cast<int>(std::cos(armAngle) * 4);
            SDL_SetRenderDrawColor(renderer, 140, 150, 160, 220);
            SDL_RenderDrawLine(renderer, armBaseX, armBaseY, armEndX, armEndY);
            // Arm joint
            SDL_SetRenderDrawColor(renderer, 180, 190, 200, 230);
            SDL_Rect joint = {armEndX - 2, armEndY - 2, 4, 4};
            SDL_RenderFillRect(renderer, &joint);

            // Welding spark at arm tip
            float sparkPhase = std::fmod(t * 5.0f, 1.0f);
            if (sparkPhase > 0.5f) {
                Uint8 sparkA = static_cast<Uint8>(255 * (1.0f - sparkPhase) * 2);
                // Spark glow
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(sparkA / 3));
                SDL_Rect sparkGlow = {armEndX - 4, armEndY - 4, 8, 8};
                SDL_RenderFillRect(renderer, &sparkGlow);
                // Spark core
                SDL_SetRenderDrawColor(renderer, 255, 255, 200, sparkA);
                SDL_Rect sparkCore = {armEndX - 1, armEndY - 1, 3, 3};
                SDL_RenderFillRect(renderer, &sparkCore);
            }

            // Tool rack (back panel)
            SDL_SetRenderDrawColor(renderer, 55, 55, 65, 210);
            SDL_Rect backPanel = {benchX + 2, benchY, benchW - 4, benchH / 2};
            SDL_RenderFillRect(renderer, &backPanel);
            // Tools on rack (3 small bars at different angles)
            SDL_SetRenderDrawColor(renderer, 120, 130, 140, 200);
            SDL_RenderDrawLine(renderer, benchX + 6, benchY + 4, benchX + 6, benchY + benchH / 3);
            SDL_RenderDrawLine(renderer, benchX + benchW / 2, benchY + 6, benchX + benchW / 2, benchY + benchH / 3);
            SDL_RenderDrawLine(renderer, benchX + benchW - 8, benchY + 3, benchX + benchW - 8, benchY + benchH / 3);

            // Status light (pulsing green)
            Uint8 lightA = static_cast<Uint8>(120 + 135 * glow);
            SDL_SetRenderDrawColor(renderer, 80, 255, 140, static_cast<Uint8>(lightA / 3));
            SDL_Rect lightGlow = {benchX + benchW - 12, benchY + 2, 8, 8};
            SDL_RenderFillRect(renderer, &lightGlow);
            SDL_SetRenderDrawColor(renderer, 80, 255, 140, lightA);
            SDL_Rect light = {benchX + benchW - 10, benchY + 4, 4, 4};
            SDL_RenderFillRect(renderer, &light);

            // "+" repair symbol floating above
            float symbolBob = std::sin(t * 2.5f) * 3.0f;
            int symY = benchY - 12 - static_cast<int>(symbolBob);
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(180 + 60 * glow));
            SDL_RenderDrawLine(renderer, cx - 5, symY, cx + 5, symY);
            SDL_RenderDrawLine(renderer, cx, symY - 5, cx, symY + 5);
            // Symbol glow
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(40 * glow));
            SDL_Rect symGlow = {cx - 7, symY - 7, 14, 14};
            SDL_RenderFillRect(renderer, &symGlow);
            break;
        }

        case RandomEventType::GamblingRift: {
            // --- Slot machine / roulette shape ---
            int machW = sr.w + 8;
            int machH = sr.h + 4;
            int machX = cx - machW / 2;
            int machY = sr.y - 2;

            // Machine body (dark metallic)
            // Gradient simulation (5 horizontal bands)
            for (int band = 0; band < 5; band++) {
                float frac = static_cast<float>(band) / 4.0f;
                Uint8 r = static_cast<Uint8>(40 + 30 * frac);
                Uint8 g = static_cast<Uint8>(30 + 20 * frac);
                Uint8 b = static_cast<Uint8>(50 + 30 * frac);
                SDL_SetRenderDrawColor(renderer, r, g, b, 235);
                SDL_Rect bandR = {machX, machY + band * machH / 5, machW, machH / 5 + 1};
                SDL_RenderFillRect(renderer, &bandR);
            }

            // Gold trim border
            SDL_SetRenderDrawColor(renderer, 200, 170, 40, 200);
            SDL_Rect goldBorder1 = {machX, machY, machW, machH};
            SDL_RenderDrawRect(renderer, &goldBorder1);
            SDL_Rect goldBorder2 = {machX + 1, machY + 1, machW - 2, machH - 2};
            SDL_RenderDrawRect(renderer, &goldBorder2);

            // Display window (3 spinning slots)
            int slotW = machW / 3 - 4;
            int slotH = machH / 3;
            int slotY = machY + machH / 4;
            for (int slot = 0; slot < 3; slot++) {
                int slotX = machX + 3 + slot * (machW / 3);
                // Slot background
                SDL_SetRenderDrawColor(renderer, 15, 10, 25, 240);
                SDL_Rect slotBg = {slotX, slotY, slotW, slotH};
                SDL_RenderFillRect(renderer, &slotBg);

                // Spinning symbol (changes with time, different per slot)
                float spinT = std::fmod(t * 3.0f + slot * 0.7f, 3.0f);
                int symbolType = static_cast<int>(spinT) % 3;
                Uint8 symR = (symbolType == 0) ? 255 : (symbolType == 1) ? 80 : 255;
                Uint8 symG = (symbolType == 0) ? 215 : (symbolType == 1) ? 255 : 80;
                Uint8 symB = (symbolType == 0) ? 60  : (symbolType == 1) ? 80  : 200;
                SDL_SetRenderDrawColor(renderer, symR, symG, symB, 220);
                SDL_Rect sym = {slotX + slotW / 4, slotY + slotH / 4, slotW / 2, slotH / 2};
                SDL_RenderFillRect(renderer, &sym);
            }

            // Pull lever on right side
            int leverX = machX + machW + 2;
            int leverY = machY + machH / 3;
            float leverAngle = std::sin(t * 1.5f) * 0.3f;
            int leverEndY = leverY - 10 + static_cast<int>(std::sin(leverAngle) * 5);
            SDL_SetRenderDrawColor(renderer, 160, 160, 170, 220);
            SDL_RenderDrawLine(renderer, leverX, leverY, leverX, leverEndY);
            // Lever ball
            SDL_SetRenderDrawColor(renderer, 255, 60, 60, 230);
            SDL_Rect leverBall = {leverX - 2, leverEndY - 3, 5, 5};
            SDL_RenderFillRect(renderer, &leverBall);

            // Flashing lights along top
            for (int light = 0; light < 5; light++) {
                float lightPhase = std::sin(t * 4.0f + light * 1.2f);
                bool on = lightPhase > 0.0f;
                int lx = machX + 3 + light * (machW - 6) / 4;
                int ly = machY - 3;
                if (on) {
                    SDL_SetRenderDrawColor(renderer, 255, 200, 50, static_cast<Uint8>(100 + 100 * lightPhase));
                    SDL_Rect lightGlow = {lx - 2, ly - 1, 5, 4};
                    SDL_RenderFillRect(renderer, &lightGlow);
                }
                SDL_SetRenderDrawColor(renderer, on ? (Uint8)255 : (Uint8)80, on ? (Uint8)200 : (Uint8)60, on ? (Uint8)50 : (Uint8)30, 220);
                SDL_Rect lightBulb = {lx - 1, ly, 3, 2};
                SDL_RenderFillRect(renderer, &lightBulb);
            }

            // "?" symbol floating above
            float qBob = std::sin(t * 2.0f) * 3.0f;
            int qY = machY - 14 - static_cast<int>(qBob);
            // Question mark glow
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(50 * glow));
            SDL_Rect qGlow = {cx - 6, qY - 6, 12, 14};
            SDL_RenderFillRect(renderer, &qGlow);
            // Question mark body
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(180 + 60 * glow));
            SDL_RenderDrawLine(renderer, cx - 3, qY - 3, cx + 3, qY - 3);
            SDL_RenderDrawLine(renderer, cx + 3, qY - 3, cx + 3, qY);
            SDL_RenderDrawLine(renderer, cx + 3, qY, cx, qY);
            SDL_RenderDrawLine(renderer, cx, qY, cx, qY + 2);
            SDL_Rect qDot = {cx - 1, qY + 4, 2, 2};
            SDL_RenderFillRect(renderer, &qDot);
            break;
        }

        } // end switch

        // ---- Interaction hint when near ----
        if (i == m_nearEventIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", event.getName());
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            int nameH = 0;
            if (hs) {
                nameH = hs->h;
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    int offy = (event.type == RandomEventType::Shrine) ? -50 : -36;
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y + offy, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }

            // Shrine: show effect preview below the name
            if (event.type == RandomEventType::Shrine) {
                SDL_Color ec = event.getShrineColor();
                ec.a = static_cast<Uint8>(180 * blink);
                SDL_Surface* es = TTF_RenderText_Blended(font, event.getShrineEffect(), ec);
                if (es) {
                    SDL_Texture* et = SDL_CreateTextureFromSurface(renderer, es);
                    if (et) {
                        SDL_Rect er = {sr.x + sr.w / 2 - es->w / 2, sr.y - 50 + nameH + 2, es->w, es->h};
                        SDL_RenderCopy(renderer, et, nullptr, &er);
                        SDL_DestroyTexture(et);
                    }
                    SDL_FreeSurface(es);
                }
            }
        }
    }
}

void PlayState::renderNPCs(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& npcs = m_level->getNPCs();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        SDL_FRect worldRect = {npc.position.x - 12, npc.position.y - 32, 24, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        // Skip if off screen (generous margin for glow effects)
        if (sr.x + sr.w < -40 || sr.x > SCREEN_WIDTH + 40 ||
            sr.y + sr.h < -40 || sr.y > SCREEN_HEIGHT + 40) continue;

        float t = ticks * 0.001f; // Time in seconds
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i * 2.0f);

        // Breathing animation (subtle scale oscillation)
        float breathe = 1.0f + 0.02f * std::sin(ticks * 0.003f + i * 1.7f);
        int breatheOffsetW = static_cast<int>((sr.w * breathe - sr.w) / 2);
        int breatheOffsetH = static_cast<int>((sr.h * breathe - sr.h));
        sr.x -= breatheOffsetW;
        sr.w += breatheOffsetW * 2;
        sr.y -= breatheOffsetH;
        sr.h += breatheOffsetH;

        // Bob animation
        float bob = std::sin(ticks * 0.003f + i * 3.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC color by type
        SDL_Color col;
        switch (npc.type) {
            case NPCType::RiftScholar:   col = {100, 180, 255, 255}; break;
            case NPCType::DimRefugee:    col = {255, 180, 80, 255}; break;
            case NPCType::LostEngineer:  col = {180, 255, 120, 255}; break;
            case NPCType::EchoOfSelf:    col = {220, 120, 255, 255}; break;
            case NPCType::Blacksmith:    col = {255, 160, 50, 255}; break;
            case NPCType::FortuneTeller: col = {180, 100, 220, 255}; break;
            case NPCType::VoidMerchant:  col = {100, 220, 200, 255}; break;
            default:                     col = {200, 200, 200, 255}; break;
        }

        int cx = sr.x + sr.w / 2;
        int cy = sr.y + sr.h / 2;

        // ---- Ground shadow (multi-layer, soft) ----
        for (int s = 4; s >= 0; s--) {
            int expand = s * 3;
            Uint8 sa = static_cast<Uint8>(std::max(0, 25 - s * 5));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, sa);
            SDL_Rect shadow = {sr.x - expand + 2, sr.y + sr.h - 1 + s * 2, sr.w + expand * 2 - 4, 3 + s};
            SDL_RenderFillRect(renderer, &shadow);
        }

        // ---- Glow aura (4 concentric layers) ----
        for (int layer = 4; layer >= 1; layer--) {
            int expand = layer * 5 + static_cast<int>(glow * 4);
            Uint8 ga = static_cast<Uint8>(std::min(255.0f, (12.0f + 10.0f * glow) * (5 - layer)));
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ga);
            SDL_Rect glowR = {sr.x - expand, sr.y - expand, sr.w + expand * 2, sr.h + expand * 2};
            SDL_RenderFillRect(renderer, &glowR);
        }

        // ============================================================
        // Hooded figure silhouette (layered robe/cloak)
        // ============================================================

        // --- Robe bottom (wider, darker - cloak spread) ---
        int robeW = sr.w + 6;
        int robeH = sr.h * 2 / 5;
        int robeX = cx - robeW / 2;
        int robeY = sr.y + sr.h - robeH;
        // Robe gradient (4 horizontal bands, dark to mid)
        for (int band = 0; band < 4; band++) {
            float frac = static_cast<float>(band) / 3.0f;
            Uint8 r = static_cast<Uint8>(std::min(255.0f, col.r * (0.15f + 0.1f * frac)));
            Uint8 g = static_cast<Uint8>(std::min(255.0f, col.g * (0.15f + 0.1f * frac)));
            Uint8 b = static_cast<Uint8>(std::min(255.0f, col.b * (0.15f + 0.1f * frac)));
            // Slightly narrower towards top
            int bandShrink = (3 - band) * 1;
            SDL_SetRenderDrawColor(renderer, r, g, b, 220);
            SDL_Rect bandR = {robeX + bandShrink, robeY + band * robeH / 4,
                              robeW - bandShrink * 2, robeH / 4 + 1};
            SDL_RenderFillRect(renderer, &bandR);
        }

        // --- Torso (main body, slightly narrower) ---
        int torsoW = sr.w;
        int torsoH = sr.h * 2 / 5;
        int torsoX = cx - torsoW / 2;
        int torsoY = sr.y + sr.h / 5;
        // Torso gradient (5 bands, mid darkness)
        for (int band = 0; band < 5; band++) {
            float frac = static_cast<float>(band) / 4.0f;
            Uint8 r = static_cast<Uint8>(std::min(255.0f, col.r * (0.2f + 0.15f * frac)));
            Uint8 g = static_cast<Uint8>(std::min(255.0f, col.g * (0.2f + 0.15f * frac)));
            Uint8 b = static_cast<Uint8>(std::min(255.0f, col.b * (0.2f + 0.15f * frac)));
            SDL_SetRenderDrawColor(renderer, r, g, b, 230);
            SDL_Rect bandR = {torsoX, torsoY + band * torsoH / 5, torsoW, torsoH / 5 + 1};
            SDL_RenderFillRect(renderer, &bandR);
        }

        // --- Shoulders (wider band at top of torso) ---
        int shoulderW = sr.w + 4;
        SDL_SetRenderDrawColor(renderer,
            static_cast<Uint8>(std::min(255, col.r / 3 + 15)),
            static_cast<Uint8>(std::min(255, col.g / 3 + 15)),
            static_cast<Uint8>(std::min(255, col.b / 3 + 15)), 230);
        SDL_Rect shoulders = {cx - shoulderW / 2, torsoY - 2, shoulderW, 5};
        SDL_RenderFillRect(renderer, &shoulders);

        // --- Hood (cone-like shape built from stacked rects) ---
        int hoodBaseW = sr.w - 2;
        int hoodH = sr.h / 3 + 2;
        int hoodBaseY = sr.y;
        // Hood layers (narrow at top, wide at bottom)
        for (int row = 0; row < hoodH; row++) {
            float frac = static_cast<float>(row) / hoodH;
            int rowW = static_cast<int>(hoodBaseW * (0.3f + 0.7f * frac));
            // Color gradient: darker inside, lighter at edges
            float brightnessBase = 0.25f + 0.15f * frac;
            Uint8 r = static_cast<Uint8>(std::min(255.0f, col.r * brightnessBase));
            Uint8 g = static_cast<Uint8>(std::min(255.0f, col.g * brightnessBase));
            Uint8 b2 = static_cast<Uint8>(std::min(255.0f, col.b * brightnessBase));
            SDL_SetRenderDrawColor(renderer, r, g, b2, 235);
            SDL_Rect hoodRow = {cx - rowW / 2, hoodBaseY + row, rowW, 1};
            SDL_RenderFillRect(renderer, &hoodRow);
        }

        // Hood interior shadow (dark face area)
        int faceW = sr.w / 2 - 2;
        int faceH = hoodH / 2;
        int faceX = cx - faceW / 2;
        int faceY = hoodBaseY + hoodH / 3;
        SDL_SetRenderDrawColor(renderer, 8, 5, 15, 200);
        SDL_Rect face = {faceX, faceY, faceW, faceH};
        SDL_RenderFillRect(renderer, &face);

        // ---- Eyes (glowing dots with outer glow) ----
        int eyeY = faceY + faceH / 3;
        int eyeSpacing = faceW / 3;
        int lEyeX = cx - eyeSpacing / 2;
        int rEyeX = cx + eyeSpacing / 2;
        Uint8 eyeBright = static_cast<Uint8>(std::min(255.0f, 180.0f + 75.0f * glow));

        // Eye outer glow
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(40 + 30 * glow));
        SDL_Rect lEyeGlow = {lEyeX - 4, eyeY - 4, 8, 8};
        SDL_Rect rEyeGlow = {rEyeX - 4, eyeY - 4, 8, 8};
        SDL_RenderFillRect(renderer, &lEyeGlow);
        SDL_RenderFillRect(renderer, &rEyeGlow);
        // Eye mid glow
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(80 + 50 * glow));
        SDL_Rect lEyeMid = {lEyeX - 2, eyeY - 2, 5, 5};
        SDL_Rect rEyeMid = {rEyeX - 2, eyeY - 2, 5, 5};
        SDL_RenderFillRect(renderer, &lEyeMid);
        SDL_RenderFillRect(renderer, &rEyeMid);
        // Eye core (bright white/colored)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, eyeBright);
        SDL_Rect lEyeCore = {lEyeX - 1, eyeY - 1, 3, 3};
        SDL_Rect rEyeCore = {rEyeX - 1, eyeY - 1, 3, 3};
        SDL_RenderFillRect(renderer, &lEyeCore);
        SDL_RenderFillRect(renderer, &rEyeCore);
        // Eye pupil (tiny bright center)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect lPupil = {lEyeX, eyeY, 1, 1};
        SDL_Rect rPupil = {rEyeX, eyeY, 1, 1};
        SDL_RenderFillRect(renderer, &lPupil);
        SDL_RenderFillRect(renderer, &rPupil);

        // ---- Cloak hem detail (small decorative line at bottom) ----
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(60 + 30 * glow));
        SDL_RenderDrawLine(renderer, robeX + 2, sr.y + sr.h - 1, robeX + robeW - 2, sr.y + sr.h - 1);

        // ============================================================
        // Type-specific elaborate details
        // ============================================================
        switch (npc.type) {
            case NPCType::RiftScholar: {
                // --- Floating book with turning pages ---
                int bookX = cx + sr.w / 2 + 4;
                int bookY = sr.y + sr.h / 4;
                float bookBob = std::sin(t * 2.0f + i) * 2.0f;
                bookY -= static_cast<int>(bookBob);

                // Book glow
                SDL_SetRenderDrawColor(renderer, 100, 180, 255, static_cast<Uint8>(40 * glow));
                SDL_Rect bookGlow = {bookX - 4, bookY - 4, 18, 16};
                SDL_RenderFillRect(renderer, &bookGlow);

                // Book covers (two angled halves)
                SDL_SetRenderDrawColor(renderer, 60, 40, 20, 230);
                SDL_Rect coverL = {bookX, bookY, 5, 10};
                SDL_Rect coverR = {bookX + 6, bookY, 5, 10};
                SDL_RenderFillRect(renderer, &coverL);
                SDL_RenderFillRect(renderer, &coverR);
                // Spine
                SDL_SetRenderDrawColor(renderer, 80, 60, 30, 240);
                SDL_Rect spine = {bookX + 5, bookY, 1, 10};
                SDL_RenderFillRect(renderer, &spine);
                // Pages (white, animated turn)
                SDL_SetRenderDrawColor(renderer, 230, 225, 210, 220);
                SDL_Rect pageL = {bookX + 1, bookY + 1, 4, 8};
                SDL_RenderFillRect(renderer, &pageL);
                float pageTurn = std::fmod(t * 0.8f, 2.0f);
                if (pageTurn < 1.0f) {
                    // Page turning right
                    int pageOffset = static_cast<int>(pageTurn * 4);
                    SDL_SetRenderDrawColor(renderer, 240, 235, 220, 200);
                    SDL_Rect turning = {bookX + 5 + pageOffset, bookY + 1, 2, 8};
                    SDL_RenderFillRect(renderer, &turning);
                }
                SDL_Rect pageR = {bookX + 7, bookY + 1, 3, 8};
                SDL_RenderFillRect(renderer, &pageR);
                // Text lines on pages
                SDL_SetRenderDrawColor(renderer, 80, 80, 100, 120);
                for (int line = 0; line < 3; line++) {
                    SDL_Rect textLine = {bookX + 2, bookY + 2 + line * 3, 3, 1};
                    SDL_RenderFillRect(renderer, &textLine);
                }

                // Magical reading particles (floating up from book)
                for (int p = 0; p < 3; p++) {
                    float pTime = t * 1.2f + p * 1.0f;
                    float pY2 = bookY - std::fmod(pTime * 15.0f, 20.0f);
                    float pX = bookX + 5 + std::sin(pTime * 3.0f + p) * 6.0f;
                    Uint8 pA = static_cast<Uint8>(std::max(0.0f, 120.0f * (1.0f - std::fmod(pTime * 0.5f, 1.0f))));
                    SDL_SetRenderDrawColor(renderer, 140, 200, 255, pA);
                    SDL_Rect particle = {static_cast<int>(pX), static_cast<int>(pY2), 2, 2};
                    SDL_RenderFillRect(renderer, &particle);
                }
                break;
            }

            case NPCType::DimRefugee: {
                // --- Glowing dimensional shard / rift tear they carry ---
                int shardX = cx - sr.w / 2 - 8;
                int shardY = sr.y + sr.h / 3;
                float shardPulse = 0.6f + 0.4f * std::sin(t * 3.0f + i);

                // Shard glow
                SDL_SetRenderDrawColor(renderer, 255, 200, 60, static_cast<Uint8>(50 * shardPulse));
                SDL_Rect shardGlow = {shardX - 4, shardY - 6, 12, 16};
                SDL_RenderFillRect(renderer, &shardGlow);

                // Diamond shard shape (rows narrowing top and bottom)
                int shardH = 12;
                for (int row = 0; row < shardH; row++) {
                    float rowFrac = static_cast<float>(row) / (shardH - 1);
                    int rowW2 = static_cast<int>(4 * (1.0f - std::abs(rowFrac - 0.5f) * 2.0f));
                    if (rowW2 < 1) rowW2 = 1;
                    Uint8 sr2 = static_cast<Uint8>(std::min(255.0f, 200 + 55 * rowFrac));
                    Uint8 sg = static_cast<Uint8>(std::min(255.0f, 150 + 60 * (1.0f - rowFrac)));
                    SDL_SetRenderDrawColor(renderer, sr2, sg, 40, static_cast<Uint8>(200 * shardPulse));
                    SDL_Rect shardRow = {shardX + 2 - rowW2 / 2, shardY - shardH / 2 + row, rowW2, 1};
                    SDL_RenderFillRect(renderer, &shardRow);
                }
                // Shard highlight
                SDL_SetRenderDrawColor(renderer, 255, 255, 200, static_cast<Uint8>(140 * shardPulse));
                SDL_Rect shardHi = {shardX + 1, shardY - 2, 1, 3};
                SDL_RenderFillRect(renderer, &shardHi);

                // Tattered cloak edge effect (ragged hem)
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 80);
                for (int tatter = 0; tatter < 4; tatter++) {
                    int tx = sr.x + 3 + tatter * sr.w / 4;
                    int ty = sr.y + sr.h + static_cast<int>(std::sin(t + tatter) * 2);
                    SDL_Rect rag = {tx, ty, 3, 2 + tatter % 2};
                    SDL_RenderFillRect(renderer, &rag);
                }
                break;
            }

            case NPCType::LostEngineer: {
                // --- Gear/cog symbol + wrench + sparking circuitry ---
                int gearX = cx;
                int gearY = sr.y - 10;
                float gearRot = t * 1.5f;

                // Gear glow
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(40 * glow));
                SDL_Rect gearGlow = {gearX - 10, gearY - 10, 20, 20};
                SDL_RenderFillRect(renderer, &gearGlow);

                // Gear body (circle approximated with small rects)
                SDL_SetRenderDrawColor(renderer, 150, 170, 140, 220);
                SDL_Rect gearCenter = {gearX - 4, gearY - 4, 8, 8};
                SDL_RenderFillRect(renderer, &gearCenter);
                // Gear axle hole
                SDL_SetRenderDrawColor(renderer, 50, 55, 45, 230);
                SDL_Rect axle = {gearX - 1, gearY - 1, 3, 3};
                SDL_RenderFillRect(renderer, &axle);
                // Gear teeth (8 teeth rotating)
                SDL_SetRenderDrawColor(renderer, 150, 170, 140, 200);
                for (int tooth = 0; tooth < 8; tooth++) {
                    float angle = gearRot + tooth * 0.785f;
                    int tx = gearX + static_cast<int>(std::cos(angle) * 6);
                    int ty = gearY + static_cast<int>(std::sin(angle) * 6);
                    SDL_Rect toothR = {tx - 1, ty - 1, 3, 3};
                    SDL_RenderFillRect(renderer, &toothR);
                }

                // Wrench held in hand area
                int wrenchX = cx + sr.w / 2 + 2;
                int wrenchY = sr.y + sr.h / 2;
                SDL_SetRenderDrawColor(renderer, 130, 140, 150, 200);
                SDL_RenderDrawLine(renderer, wrenchX, wrenchY, wrenchX, wrenchY + 10);
                SDL_SetRenderDrawColor(renderer, 160, 170, 180, 220);
                SDL_Rect wrenchHead = {wrenchX - 2, wrenchY - 2, 5, 4};
                SDL_RenderFillRect(renderer, &wrenchHead);

                // Circuit lines on robe
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(60 + 40 * glow));
                int circY = sr.y + sr.h / 2;
                SDL_RenderDrawLine(renderer, sr.x + 3, circY, sr.x + 3, circY + 8);
                SDL_RenderDrawLine(renderer, sr.x + 3, circY + 8, sr.x + 8, circY + 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w - 4, circY + 2, sr.x + sr.w - 4, circY + 10);
                // Sparking node
                float sparkPhase = std::fmod(t * 4.0f, 1.0f);
                if (sparkPhase > 0.6f) {
                    SDL_SetRenderDrawColor(renderer, 200, 255, 180, static_cast<Uint8>(200 * (1.0f - sparkPhase) * 2.5f));
                    SDL_Rect sparkNode = {sr.x + 2, circY + 7, 3, 3};
                    SDL_RenderFillRect(renderer, &sparkNode);
                }
                break;
            }

            case NPCType::EchoOfSelf: {
                // --- Mirror/ghost duplicate effect ---
                float echoFlicker = 0.4f + 0.6f * std::sin(t * 4.0f + i * 2.0f);
                float echoShift = std::sin(t * 1.5f) * 4.0f;

                // Ghost duplicate offset (slightly shifted and more transparent)
                int ghostX = sr.x + static_cast<int>(echoShift);
                int ghostY = sr.y - 2;
                // Ghost body layers
                for (int layer = 2; layer >= 0; layer--) {
                    int expand = layer * 2;
                    Uint8 ga2 = static_cast<Uint8>(std::min(255.0f, 30.0f * (3 - layer) * echoFlicker));
                    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ga2);
                    SDL_Rect ghost = {ghostX - expand, ghostY - expand, sr.w + expand * 2, sr.h + expand * 2};
                    SDL_RenderFillRect(renderer, &ghost);
                }

                // Mirror frame around NPC
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(80 * glow));
                SDL_Rect mirrorOuter = {sr.x - 4, sr.y - 4, sr.w + 8, sr.h + 8};
                SDL_RenderDrawRect(renderer, &mirrorOuter);
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(120 * glow));
                SDL_Rect mirrorInner = {sr.x - 2, sr.y - 2, sr.w + 4, sr.h + 4};
                SDL_RenderDrawRect(renderer, &mirrorInner);

                // Reflection lines (diagonal shimmering)
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(40 * glow));
                float shimmer = std::fmod(t * 2.0f, 3.0f);
                int shimmerY = sr.y + static_cast<int>(shimmer * sr.h / 3);
                SDL_RenderDrawLine(renderer, sr.x, shimmerY, sr.x + sr.w, shimmerY - 4);

                // "?" identity particles
                for (int p = 0; p < 3; p++) {
                    float pAng = t * 2.0f + p * 2.09f;
                    int px = cx + static_cast<int>(std::cos(pAng) * (sr.w / 2 + 8));
                    int py = cy + static_cast<int>(std::sin(pAng) * (sr.h / 2 + 4));
                    Uint8 pA = static_cast<Uint8>(80 + 60 * std::sin(t * 3.0f + p));
                    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, pA);
                    SDL_Rect pRect = {px - 1, py - 1, 2, 2};
                    SDL_RenderFillRect(renderer, &pRect);
                }
                break;
            }

            case NPCType::Blacksmith: {
                // --- Glowing anvil with spark shower + hammer ---
                int anvilX = cx;
                int anvilY = sr.y - 6;

                // Anvil glow (warm forge light)
                SDL_SetRenderDrawColor(renderer, 255, 120, 30, static_cast<Uint8>(30 + 25 * glow));
                SDL_Rect anvilGlow = {anvilX - 12, anvilY - 10, 24, 18};
                SDL_RenderFillRect(renderer, &anvilGlow);

                // Anvil top (wide)
                SDL_SetRenderDrawColor(renderer, 100, 95, 90, 240);
                SDL_Rect anvilTop = {anvilX - 8, anvilY - 2, 16, 4};
                SDL_RenderFillRect(renderer, &anvilTop);
                // Anvil body (narrower)
                SDL_SetRenderDrawColor(renderer, 80, 75, 70, 235);
                SDL_Rect anvilBody = {anvilX - 5, anvilY + 2, 10, 5};
                SDL_RenderFillRect(renderer, &anvilBody);
                // Anvil base (wide again)
                SDL_SetRenderDrawColor(renderer, 90, 85, 80, 240);
                SDL_Rect anvilBase = {anvilX - 7, anvilY + 7, 14, 3};
                SDL_RenderFillRect(renderer, &anvilBase);
                // Anvil highlight
                SDL_SetRenderDrawColor(renderer, 140, 135, 130, 120);
                SDL_Rect anvilHi = {anvilX - 7, anvilY - 2, 14, 1};
                SDL_RenderFillRect(renderer, &anvilHi);

                // Hammer (animated striking motion)
                float hammerAngle = std::sin(t * 4.0f) * 0.5f;
                int hammerBaseX = anvilX + 10;
                int hammerBaseY = anvilY + 4;
                int hammerEndX = hammerBaseX + static_cast<int>(std::sin(hammerAngle) * 8);
                int hammerEndY = hammerBaseY - 10 + static_cast<int>(std::cos(hammerAngle) * 6);
                // Hammer handle
                SDL_SetRenderDrawColor(renderer, 110, 80, 50, 220);
                SDL_RenderDrawLine(renderer, hammerBaseX, hammerBaseY, hammerEndX, hammerEndY);
                // Hammer head
                SDL_SetRenderDrawColor(renderer, 140, 140, 150, 230);
                SDL_Rect hammerHead = {hammerEndX - 3, hammerEndY - 2, 6, 4};
                SDL_RenderFillRect(renderer, &hammerHead);

                // Spark shower (many particles bursting from anvil)
                float hitPhase = std::sin(t * 4.0f);
                if (hitPhase > 0.8f) {
                    for (int spark = 0; spark < 6; spark++) {
                        float sparkAge = std::fmod(t * 8.0f + spark * 0.4f, 1.0f);
                        float sparkDirX = (spark - 3) * 3.0f * (1.0f - sparkAge);
                        float sparkDirY = -sparkAge * 15.0f + sparkAge * sparkAge * 20.0f; // Gravity arc
                        int sx = anvilX + static_cast<int>(sparkDirX);
                        int sy = anvilY - 2 + static_cast<int>(sparkDirY);
                        Uint8 sparkA2 = static_cast<Uint8>(std::max(0.0f, 255.0f * (1.0f - sparkAge)));
                        Uint8 sparkG = static_cast<Uint8>(std::min(255.0f, 180.0f + 75.0f * (1.0f - sparkAge)));
                        SDL_SetRenderDrawColor(renderer, 255, sparkG, 40, sparkA2);
                        SDL_Rect sparkRect = {sx, sy, 2, 2};
                        SDL_RenderFillRect(renderer, &sparkRect);
                    }
                }

                // Forge glow on robe (warm light from below)
                SDL_SetRenderDrawColor(renderer, 255, 100, 30, static_cast<Uint8>(20 + 15 * glow));
                SDL_Rect forgeLight = {sr.x + 2, sr.y + sr.h / 2, sr.w - 4, sr.h / 3};
                SDL_RenderFillRect(renderer, &forgeLight);
                break;
            }

            case NPCType::FortuneTeller: {
                // --- Crystal ball + mystical eye symbol ---
                int ballX = cx;
                int ballY = sr.y - 8;
                float crystalPulse = 0.5f + 0.5f * std::sin(t * 2.5f + i);

                // Crystal ball glow
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(40 * crystalPulse));
                SDL_Rect ballGlow = {ballX - 8, ballY - 8, 16, 16};
                SDL_RenderFillRect(renderer, &ballGlow);
                // Crystal ball body
                SDL_SetRenderDrawColor(renderer, 120, 60, 180, static_cast<Uint8>(180 + 40 * crystalPulse));
                SDL_Rect ball = {ballX - 5, ballY - 5, 10, 10};
                SDL_RenderFillRect(renderer, &ball);
                // Inner swirl
                SDL_SetRenderDrawColor(renderer, 200, 160, 255, static_cast<Uint8>(120 * crystalPulse));
                int swirlX = ballX + static_cast<int>(std::cos(t * 2.0f) * 2);
                int swirlY = ballY + static_cast<int>(std::sin(t * 2.0f) * 2);
                SDL_Rect swirl = {swirlX - 2, swirlY - 2, 4, 4};
                SDL_RenderFillRect(renderer, &swirl);
                // Ball highlight
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(80 * crystalPulse));
                SDL_Rect ballHi = {ballX - 3, ballY - 4, 2, 2};
                SDL_RenderFillRect(renderer, &ballHi);
                // Ball stand
                SDL_SetRenderDrawColor(renderer, 90, 70, 110, 220);
                SDL_Rect stand = {ballX - 3, ballY + 5, 6, 3};
                SDL_RenderFillRect(renderer, &stand);

                // Mystical stars floating around
                for (int star = 0; star < 4; star++) {
                    float starAng = t * 1.2f + star * 1.57f;
                    float starR = 12.0f + 3.0f * std::sin(t * 2.0f + star);
                    int sx = ballX + static_cast<int>(std::cos(starAng) * starR);
                    int sy = ballY + static_cast<int>(std::sin(starAng) * starR * 0.7f);
                    Uint8 starA = static_cast<Uint8>(100 + 80 * std::sin(t * 3.0f + star * 1.5f));
                    SDL_SetRenderDrawColor(renderer, 220, 180, 255, starA);
                    SDL_Rect starR2 = {sx - 1, sy - 1, 2, 2};
                    SDL_RenderFillRect(renderer, &starR2);
                }

                // Robe mystical runes
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(50 + 30 * glow));
                for (int rune = 0; rune < 3; rune++) {
                    int rx = sr.x + 3 + rune * (sr.w / 3);
                    int ry = sr.y + sr.h * 2 / 3 + rune * 2;
                    SDL_Rect runeR = {rx, ry, 3, 2};
                    SDL_RenderFillRect(renderer, &runeR);
                }
                break;
            }

            case NPCType::VoidMerchant: {
                // --- Floating void merchandise + dimensional tear ---
                int tearX = cx;
                int tearY = sr.y - 4;

                // Small dimensional tear behind merchant
                for (int ring = 3; ring >= 0; ring--) {
                    float ringT = t * (1.0f + ring * 0.4f);
                    int offsetX = static_cast<int>(std::cos(ringT) * ring);
                    int offsetY = static_cast<int>(std::sin(ringT) * ring);
                    int size = 4 + ring * 3;
                    Uint8 ringA = static_cast<Uint8>(std::min(255.0f, (60.0f - ring * 12.0f) * glow));
                    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, ringA);
                    SDL_Rect ringR = {tearX - size / 2 + offsetX, tearY - size / 2 + offsetY, size, size};
                    SDL_RenderDrawRect(renderer, &ringR);
                }

                // Floating merchandise items (3 hovering relics)
                for (int item = 0; item < 3; item++) {
                    float itemBob = std::sin(t * 2.0f + item * 2.0f) * 3.0f;
                    int ix = sr.x - 6 + item * (sr.w / 2 + 3);
                    int iy = sr.y + sr.h / 3 - static_cast<int>(itemBob);
                    // Item glow
                    Uint8 igA = static_cast<Uint8>(30 + 20 * std::sin(t * 3.0f + item));
                    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, igA);
                    SDL_Rect itemGlow = {ix - 3, iy - 3, 8, 8};
                    SDL_RenderFillRect(renderer, &itemGlow);
                    // Item
                    Uint8 iR = (item == 0) ? 200 : (item == 1) ? 100 : 180;
                    Uint8 iG = (item == 0) ? 100 : (item == 1) ? 200 : 220;
                    Uint8 iB = (item == 0) ? 220 : (item == 1) ? 180 : 100;
                    SDL_SetRenderDrawColor(renderer, iR, iG, iB, 200);
                    SDL_Rect itemR = {ix - 1, iy - 1, 4, 4};
                    SDL_RenderFillRect(renderer, &itemR);
                }

                // Void wisps
                for (int wisp = 0; wisp < 3; wisp++) {
                    float wAng = t * 1.0f + wisp * 2.09f;
                    float wR = sr.w / 2 + 10.0f;
                    int wx = cx + static_cast<int>(std::cos(wAng) * wR);
                    int wy = cy + static_cast<int>(std::sin(wAng) * wR * 0.5f);
                    Uint8 wA = static_cast<Uint8>(60 + 40 * std::sin(t * 2.0f + wisp));
                    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, wA);
                    SDL_Rect wispR = {wx - 1, wy - 1, 3, 3};
                    SDL_RenderFillRect(renderer, &wispR);
                }
                break;
            }

            default: {
                // --- Generic NPC: simple glowing rune above head ---
                float runePulse = 0.5f + 0.5f * std::sin(t * 2.0f + i);
                int runeY = sr.y - 10;
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(40 * runePulse));
                SDL_Rect runeGlow = {cx - 5, runeY - 5, 10, 10};
                SDL_RenderFillRect(renderer, &runeGlow);
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(160 * runePulse));
                SDL_RenderDrawLine(renderer, cx, runeY - 4, cx + 4, runeY);
                SDL_RenderDrawLine(renderer, cx + 4, runeY, cx, runeY + 4);
                SDL_RenderDrawLine(renderer, cx, runeY + 4, cx - 4, runeY);
                SDL_RenderDrawLine(renderer, cx - 4, runeY, cx, runeY - 4);
                break;
            }
        }

        // ---- Interaction hint ----
        if (i == m_nearNPCIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", npc.name);
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y - 36, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }
}

void PlayState::renderNPCDialog(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_nearNPCIndex < 0 || !m_level) return;

    auto& npcs = m_level->getNPCs();
    if (m_nearNPCIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[m_nearNPCIndex];

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Dialog box (taller for returning NPCs with longer story text)
    int stage = getNPCStoryStage(npc.type);
    int boxW = 500, boxH = 260 + (stage > 0 ? 30 : 0);
    int boxX = SCREEN_WIDTH / 2 - boxW / 2, boxY = SCREEN_HEIGHT / 2 - boxH / 2;

    SDL_SetRenderDrawColor(renderer, 20, 15, 35, 240);
    SDL_Rect box = {boxX, boxY, boxW, boxH};
    SDL_RenderFillRect(renderer, &box);

    // Border
    SDL_SetRenderDrawColor(renderer, 120, 80, 200, 200);
    SDL_RenderDrawRect(renderer, &box);

    // NPC name
    SDL_Color nameColor = {180, 140, 255, 255};
    SDL_Surface* ns = TTF_RenderText_Blended(font, npc.name, nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            SDL_Rect nr = {boxX + 20, boxY + 15, ns->w, ns->h};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    // Story stage indicator (show returning NPC badge)
    if (stage > 0) {
        const char* stageLabel = (stage >= 2) ? "[Old Friend]" : "[Returning]";
        SDL_Color stageColor = (stage >= 2) ? SDL_Color{255, 215, 80, 200} : SDL_Color{140, 200, 140, 180};
        SDL_Surface* stS = TTF_RenderText_Blended(font, stageLabel, stageColor);
        if (stS) {
            SDL_Texture* stT = SDL_CreateTextureFromSurface(renderer, stS);
            if (stT) {
                SDL_Rect stR = {boxX + boxW - stS->w - 20, boxY + 15, stS->w, stS->h};
                SDL_RenderCopy(renderer, stT, nullptr, &stR);
                SDL_DestroyTexture(stT);
            }
            SDL_FreeSurface(stS);
        }
    }

    // Greeting text (stage-based)
    const char* greeting = NPCSystem::getGreeting(npc.type, stage);
    SDL_Color textColor = {200, 200, 220, 255};
    SDL_Surface* ds = TTF_RenderText_Blended(font, greeting, textColor);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            SDL_Rect dr = {boxX + 20, boxY + 45, ds->w, ds->h};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }

    // Story line (stage-based, may contain newlines -- render line by line)
    const char* storyText = NPCSystem::getStoryLine(npc.type, stage);
    int lineY = boxY + 70;
    {
        // Split on newline and render each line
        std::string story(storyText);
        size_t pos = 0;
        while (pos < story.size()) {
            size_t nl = story.find('\n', pos);
            std::string line = (nl != std::string::npos) ? story.substr(pos, nl - pos) : story.substr(pos);
            pos = (nl != std::string::npos) ? nl + 1 : story.size();
            if (line.empty()) { lineY += 18; continue; }
            SDL_Surface* ls = TTF_RenderText_Blended(font, line.c_str(), SDL_Color{160, 160, 180, 200});
            if (ls) {
                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                if (lt) {
                    SDL_Rect lr = {boxX + 20, lineY, ls->w, ls->h};
                    SDL_RenderCopy(renderer, lt, nullptr, &lr);
                    SDL_DestroyTexture(lt);
                }
                SDL_FreeSurface(ls);
            }
            lineY += 18;
        }
    }

    // Dialog options (stage-based) -- positioned below story text
    bool hasQuest = m_activeQuest.active || m_activeQuest.completed;
    auto options = NPCSystem::getDialogOptions(npc.type, stage, hasQuest);
    int optY = std::max(lineY + 8, boxY + 110);
    for (int i = 0; i < static_cast<int>(options.size()); i++) {
        bool selected = (i == m_npcDialogChoice);
        SDL_Color oc = selected ? SDL_Color{255, 220, 100, 255} : SDL_Color{180, 180, 200, 200};

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 80, 60, 120, 100);
            SDL_Rect selR = {boxX + 15, optY - 2, boxW - 30, 22};
            SDL_RenderFillRect(renderer, &selR);

            // Arrow
            SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
            SDL_RenderDrawLine(renderer, boxX + 20, optY + 8, boxX + 28, optY + 4);
            SDL_RenderDrawLine(renderer, boxX + 28, optY + 4, boxX + 20, optY);
        }

        SDL_Surface* os = TTF_RenderText_Blended(font, options[i], oc);
        if (os) {
            SDL_Texture* ot = SDL_CreateTextureFromSurface(renderer, os);
            if (ot) {
                SDL_Rect or_ = {boxX + 35, optY, os->w, os->h};
                SDL_RenderCopy(renderer, ot, nullptr, &or_);
                SDL_DestroyTexture(ot);
            }
            SDL_FreeSurface(os);
        }
        optY += 26;
    }

    // Controls hint
    SDL_Surface* hs = TTF_RenderText_Blended(font, LOC("npc.nav_hint"),
                                              SDL_Color{120, 120, 140, 150});
    if (hs) {
        SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
        if (ht) {
            SDL_Rect hr = {boxX + boxW / 2 - hs->w / 2, boxY + boxH - 25, hs->w, hs->h};
            SDL_RenderCopy(renderer, ht, nullptr, &hr);
            SDL_DestroyTexture(ht);
        }
        SDL_FreeSurface(hs);
    }
}
