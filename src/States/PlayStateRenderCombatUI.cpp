// PlayStateRenderCombatUI.cpp -- Split from PlayStateRenderOverlays.cpp
// Notifications (achievement, lore, unlock) + combat UI (kill streak, level up,
// damage indicators, offscreen enemy arrows)
#include "PlayState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
#include "Game/AchievementSystem.h"
#include "Game/LoreSystem.h"
#include "Components/TransformComponent.h"
#include "Components/AIComponent.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

// ============================================================
// Notification popups
// ============================================================

void PlayState::renderAchievementNotification(SDL_Renderer* renderer, TTF_Font* font) {
    auto* notif = game->getAchievements().getActiveNotification();
    if (!notif || !font) return;

    float t = notif->timer / notif->duration;
    float slideIn = std::min(1.0f, (1.0f - t) * 5.0f);
    float fadeOut = std::min(1.0f, t * 3.0f);
    float alpha = slideIn * fadeOut;
    Uint8 a = static_cast<Uint8>(alpha * 220);

    bool hasReward = !notif->rewardText.empty();
    int popW = 600, popH = hasReward ? 112 : 80;
    int popX = SCREEN_WIDTH / 2 - popW / 2;
    int popY = SCREEN_HEIGHT - 80 - popH - static_cast<int>(slideIn * 20);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 40, 25, a);
    SDL_Rect bg = {popX, popY, popW, popH};
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 100, 220, 120, a);
    SDL_RenderDrawRect(renderer, &bg);

    // Trophy icon
    SDL_SetRenderDrawColor(renderer, 255, 200, 50, a);
    SDL_Rect trophy = {popX + 20, popY + 16, 32, 32};
    SDL_RenderFillRect(renderer, &trophy);

    // Achievement name
    char achText[128];
    snprintf(achText, sizeof(achText), "Achievement: %s", notif->name.c_str());
    SDL_Color tc = {200, 255, 210, a};
    SDL_Surface* ts = TTF_RenderText_Blended(font, achText, tc);
    if (ts) {
        SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
        if (tt) {
            SDL_SetTextureAlphaMod(tt, a);
            SDL_Rect tr = {popX + 68, popY + 8, ts->w, ts->h};
            SDL_RenderCopy(renderer, tt, nullptr, &tr);
            SDL_DestroyTexture(tt);
        }
        SDL_FreeSurface(ts);
    }

    // Reward text line (golden)
    if (hasReward) {
        char rewardText[128];
        snprintf(rewardText, sizeof(rewardText), "Reward: %s", notif->rewardText.c_str());
        SDL_Color rc = {255, 220, 80, a};
        SDL_Surface* rs = TTF_RenderText_Blended(font, rewardText, rc);
        if (rs) {
            SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
            if (rt) {
                SDL_SetTextureAlphaMod(rt, a);
                SDL_Rect rr = {popX + 68, popY + 56, rs->w, rs->h};
                SDL_RenderCopy(renderer, rt, nullptr, &rr);
                SDL_DestroyTexture(rt);
            }
            SDL_FreeSurface(rs);
        }
    }
}

void PlayState::renderLoreNotification(SDL_Renderer* renderer, TTF_Font* font) {
    auto* lore = game->getLoreSystem();
    if (!lore) return;
    auto* loreNotif = lore->getActiveNotification();
    if (!loreNotif || !font) return;

    float t = loreNotif->timer / loreNotif->duration;
    float slideIn = std::min(1.0f, (1.0f - t) * 4.0f);
    float fadeOut = std::min(1.0f, t * 2.5f);
    float alpha = slideIn * fadeOut;
    Uint8 a = static_cast<Uint8>(alpha * 230);

    int popW = 640, popH = 88;
    int popX = SCREEN_WIDTH / 2 - popW / 2;
    int popY = 40 + static_cast<int>((1.0f - slideIn) * 60);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 25, 15, 40, a);
    SDL_Rect bg = {popX, popY, popW, popH};
    SDL_RenderFillRect(renderer, &bg);
    float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.006f);
    Uint8 borderA = static_cast<Uint8>(a * pulse);
    SDL_SetRenderDrawColor(renderer, 160, 100, 255, borderA);
    SDL_RenderDrawRect(renderer, &bg);
    SDL_Rect innerBorder = {popX + 1, popY + 1, popW - 2, popH - 2};
    SDL_SetRenderDrawColor(renderer, 120, 70, 200, static_cast<Uint8>(borderA * 0.5f));
    SDL_RenderDrawRect(renderer, &innerBorder);

    // Scroll icon
    SDL_SetRenderDrawColor(renderer, 180, 140, 255, a);
    SDL_Rect scrollIcon = {popX + 24, popY + 20, 24, 32};
    SDL_RenderFillRect(renderer, &scrollIcon);
    SDL_SetRenderDrawColor(renderer, 100, 60, 180, a);
    for (int line = 0; line < 3; line++) {
        SDL_RenderDrawLine(renderer, popX + 28, popY + 28 + line * 8,
                           popX + 44, popY + 28 + line * 8);
    }

    // "LORE DISCOVERED" label
    SDL_Color labelColor = {140, 110, 200, a};
    SDL_Surface* labelSurf = TTF_RenderText_Blended(font, LOC("hud.lore_discovered"), labelColor);
    if (labelSurf) {
        SDL_Texture* labelTex = SDL_CreateTextureFromSurface(renderer, labelSurf);
        if (labelTex) {
            SDL_SetTextureAlphaMod(labelTex, a);
            SDL_Rect lr = {popX + 64, popY + 8, labelSurf->w, labelSurf->h};
            SDL_RenderCopy(renderer, labelTex, nullptr, &lr);
            SDL_DestroyTexture(labelTex);
        }
        SDL_FreeSurface(labelSurf);
    }

    // Lore title
    SDL_Color titleColor = {220, 200, 255, a};
    SDL_Surface* titleSurf = TTF_RenderText_Blended(font, loreNotif->title.c_str(), titleColor);
    if (titleSurf) {
        SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
        if (titleTex) {
            SDL_SetTextureAlphaMod(titleTex, a);
            SDL_Rect tr = {popX + 64, popY + 44, titleSurf->w, titleSurf->h};
            SDL_RenderCopy(renderer, titleTex, nullptr, &tr);
            SDL_DestroyTexture(titleTex);
        }
        SDL_FreeSurface(titleSurf);
    }
}

void PlayState::renderUnlockNotifications(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;
    // Stack popups in top-center
    int baseY = 120;
    for (int i = 0; i < MAX_UNLOCK_NOTIFS; i++) {
        int idx = (m_unlockNotifHead + i) % MAX_UNLOCK_NOTIFS;
        auto& n = m_unlockNotifs[idx];
        if (n.timer <= 0) continue;

        float t = n.timer / n.maxTimer;
        float slideIn = std::min(1.0f, (1.0f - t) * 6.0f);
        float fadeOut = std::min(1.0f, t * 4.0f);
        float alpha = slideIn * fadeOut;
        Uint8 a = static_cast<Uint8>(alpha * 240);

        int popW = 560, popH = 64;
        int popX = SCREEN_WIDTH / 2 - popW / 2;
        int popY = baseY + static_cast<int>((1.0f - slideIn) * 40);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 30, 25, 10, a);
        SDL_Rect bg = {popX, popY, popW, popH};
        SDL_RenderFillRect(renderer, &bg);
        float gPulse = 0.7f + 0.3f * std::sin(SDL_GetTicks() * 0.008f);
        SDL_SetRenderDrawColor(renderer, 255, 200, 50, static_cast<Uint8>(a * gPulse));
        SDL_RenderDrawRect(renderer, &bg);

        SDL_Color tc = {255, 220, 80, a};
        SDL_Surface* ts = TTF_RenderText_Blended(font, n.text, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_SetTextureAlphaMod(tt, a);
                SDL_Rect tr = {popX + 20, popY + 12, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }

        baseY += popH + 8;
    }
}

// ============================================================
// Combat UI overlays
// ============================================================

void PlayState::renderKillStreak(SDL_Renderer* renderer, TTF_Font* font) {
    if (m_killStreakDisplayTimer <= 0 || m_killStreakText.empty() || !font) return;

    float alpha = 1.0f;
    if (m_killStreakDisplayTimer < 0.4f) alpha = m_killStreakDisplayTimer / 0.4f;
    Uint8 a = static_cast<Uint8>(alpha * 255);

    float pulse = 1.0f + 0.06f * std::sin(m_killStreakDisplayTimer * 10.0f);

    bool isGodlike = (m_killStreakCount >= 12);
    if (isGodlike) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint8 glowA = static_cast<Uint8>(alpha * 40);
        SDL_SetRenderDrawColor(renderer, 255, 30, 30, glowA);
        int glowW = 640, glowH = 120;
        SDL_Rect glow = {SCREEN_WIDTH / 2 - glowW / 2, SCREEN_HEIGHT / 4 - glowH / 2, glowW, glowH};
        SDL_RenderFillRect(renderer, &glow);
    }

    SDL_Color col = {m_killStreakColor.r, m_killStreakColor.g, m_killStreakColor.b, a};
    SDL_Surface* surf = TTF_RenderText_Blended(font, m_killStreakText.c_str(), col);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            float scale = 2.2f * pulse;
            int w = static_cast<int>(surf->w * scale);
            int h = static_cast<int>(surf->h * scale);
            SDL_Rect dst = {SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT / 4 - h / 2, w, h};
            SDL_SetTextureAlphaMod(tex, a);
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }

    if (a > 30) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        int lineW = static_cast<int>(320 * alpha * pulse);
        SDL_SetRenderDrawColor(renderer, m_killStreakColor.r, m_killStreakColor.g,
                               m_killStreakColor.b, static_cast<Uint8>(a * 0.6f));
        SDL_Rect line = {SCREEN_WIDTH / 2 - lineW / 2, SCREEN_HEIGHT / 4 + 36, lineW, 4};
        SDL_RenderFillRect(renderer, &line);
    }
}

void PlayState::renderLevelUp(SDL_Renderer* renderer, TTF_Font* font) {
    if (m_levelUpFlashTimer > 0) {
        float flashPct = m_levelUpFlashTimer / 0.3f;
        // Additive golden flash
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        Uint8 flashA = static_cast<Uint8>(flashPct * 50);
        SDL_SetRenderDrawColor(renderer, flashA, static_cast<Uint8>(flashA * 0.8f), 0, flashA);
        SDL_Rect fullScreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fullScreen);

        // Expanding golden ring from player
        if (m_player && m_player->getEntity()) {
            Vec2 cam = m_camera.getPosition();
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            int cx = static_cast<int>(pt.getCenter().x - cam.x);
            int cy = static_cast<int>(pt.getCenter().y - cam.y);
            float ringRadius = (1.0f - flashPct) * 280.0f;
            Uint8 ringA = static_cast<Uint8>(flashPct * 35);
            for (int r = 0; r < 3; r++) {
                int rr = static_cast<int>(ringRadius) + r * 4;
                SDL_SetRenderDrawColor(renderer, ringA, static_cast<Uint8>(ringA * 0.7f), 0, static_cast<Uint8>(ringA / (r + 1)));
                SDL_Rect ring = {cx - rr, cy - rr, rr * 2, rr * 2};
                SDL_RenderDrawRect(renderer, &ring);
            }
            // Radial speed lines
            for (int line = 0; line < 8; line++) {
                float angle = line * 0.785f;
                float inner = ringRadius * 0.5f;
                int x1 = cx + static_cast<int>(std::cos(angle) * inner);
                int y1 = cy + static_cast<int>(std::sin(angle) * inner);
                int x2 = cx + static_cast<int>(std::cos(angle) * ringRadius);
                int y2 = cy + static_cast<int>(std::sin(angle) * ringRadius);
                SDL_SetRenderDrawColor(renderer, ringA, static_cast<Uint8>(ringA * 0.8f), 0, ringA);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

    if (m_levelUpTimer <= 0 || !font) return;

    float alpha = 1.0f;
    if (m_levelUpTimer < 0.4f) alpha = m_levelUpTimer / 0.4f;
    float slideUp = (1.0f - std::min(1.0f, (1.5f - m_levelUpTimer) / 0.3f)) * 10.0f;
    Uint8 a = static_cast<Uint8>(alpha * 255);

    // Golden glow backdrop (additive for warm bright feel)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    Uint8 glowA = static_cast<Uint8>(alpha * 40);
    SDL_SetRenderDrawColor(renderer, glowA, static_cast<Uint8>(glowA * 0.8f), 0, glowA);
    int glowW = 800, glowH = 200;
    SDL_Rect glow = {SCREEN_WIDTH / 2 - glowW / 2,
                     SCREEN_HEIGHT / 3 - glowH / 2 + static_cast<int>(slideUp), glowW, glowH};
    SDL_RenderFillRect(renderer, &glow);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    float pulse = 1.0f + 0.05f * std::sin(m_levelUpTimer * 12.0f);
    SDL_Color gold = {255, 215, 0, a};
    SDL_Surface* surf = TTF_RenderText_Blended(font, LOC("hud.level_up"), gold);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            float scale = 2.5f * pulse;
            int w = static_cast<int>(surf->w * scale);
            int h = static_cast<int>(surf->h * scale);
            SDL_Rect dst = {SCREEN_WIDTH / 2 - w / 2,
                            SCREEN_HEIGHT / 3 - h / 2 + static_cast<int>(slideUp), w, h};
            SDL_SetTextureAlphaMod(tex, a);
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }

    char lvlText[32];
    std::snprintf(lvlText, sizeof(lvlText), "Lv.%d", m_levelUpDisplayLevel);
    SDL_Color lvlColor = {255, 240, 180, static_cast<Uint8>(a * 0.8f)};
    SDL_Surface* lvlSurf = TTF_RenderText_Blended(font, lvlText, lvlColor);
    if (lvlSurf) {
        SDL_Texture* lvlTex = SDL_CreateTextureFromSurface(renderer, lvlSurf);
        if (lvlTex) {
            float lvlScale = 1.6f;
            int lw = static_cast<int>(lvlSurf->w * lvlScale);
            int lh = static_cast<int>(lvlSurf->h * lvlScale);
            SDL_Rect dst = {SCREEN_WIDTH / 2 - lw / 2,
                            SCREEN_HEIGHT / 3 + 48 + static_cast<int>(slideUp), lw, lh};
            SDL_SetTextureAlphaMod(lvlTex, static_cast<Uint8>(a * 0.8f));
            SDL_RenderCopy(renderer, lvlTex, nullptr, &dst);
            SDL_DestroyTexture(lvlTex);
        }
        SDL_FreeSurface(lvlSurf);
    }

    if (a > 30) {
        int lineW = static_cast<int>(180 * alpha * pulse);
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, static_cast<Uint8>(a * 0.5f));
        SDL_Rect line = {SCREEN_WIDTH / 2 - lineW / 2,
                         SCREEN_HEIGHT / 3 + 46 + static_cast<int>(slideUp), lineW, 2};
        SDL_RenderFillRect(renderer, &line);
    }
}

// ============================================================
// Damage indicators & off-screen enemy arrows
// ============================================================

void PlayState::updateDamageIndicators(float dt) {
    for (auto it = m_damageIndicators.begin(); it != m_damageIndicators.end(); ) {
        it->timer -= dt;
        if (it->timer <= 0) {
            it = m_damageIndicators.erase(it);
        } else {
            ++it;
        }
    }
}

void PlayState::renderDamageIndicators(SDL_Renderer* renderer) {
    if (m_damageIndicators.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    constexpr int kEdgeThickness = 64;
    constexpr int kEdgeFadeInner = 40;

    for (auto& ind : m_damageIndicators) {
        float alpha = ind.timer / ind.maxTimer;
        float displayAlpha = (alpha > 0.8f) ? (1.0f - alpha) / 0.2f : alpha / 0.8f;
        displayAlpha = std::min(1.0f, displayAlpha);

        float ax = std::cos(ind.angle);
        float ay = std::sin(ind.angle);
        float rightW = std::max(0.0f, ax), leftW = std::max(0.0f, -ax);
        float downW = std::max(0.0f, ay), upW = std::max(0.0f, -ay);
        float maxW = std::max({rightW, leftW, downW, upW, 0.001f});
        rightW /= maxW; leftW /= maxW; downW /= maxW; upW /= maxW;

        auto drawEdge = [&](float weight, int x, int y, int w, int h,
                            int fadeX, int fadeY, int fadeW, int fadeH) {
            if (weight < 0.1f) return;
            Uint8 a = static_cast<Uint8>(std::min(255.0f, weight * displayAlpha * 180.0f));
            if (a < 2) return;
            SDL_SetRenderDrawColor(renderer, 220, 30, 20, a);
            SDL_Rect bar = {x, y, w, h};
            SDL_RenderFillRect(renderer, &bar);
            Uint8 fadeA = static_cast<Uint8>(a * 0.4f);
            SDL_SetRenderDrawColor(renderer, 200, 20, 15, fadeA);
            SDL_Rect fade = {fadeX, fadeY, fadeW, fadeH};
            SDL_RenderFillRect(renderer, &fade);
        };

        drawEdge(leftW,  0, 0, kEdgeThickness, SCREEN_HEIGHT,
                 kEdgeThickness, 0, kEdgeFadeInner, SCREEN_HEIGHT);
        drawEdge(rightW, SCREEN_WIDTH - kEdgeThickness, 0, kEdgeThickness, SCREEN_HEIGHT,
                 SCREEN_WIDTH - kEdgeThickness - kEdgeFadeInner, 0, kEdgeFadeInner, SCREEN_HEIGHT);
        drawEdge(upW,    0, 0, SCREEN_WIDTH, kEdgeThickness,
                 0, kEdgeThickness, SCREEN_WIDTH, kEdgeFadeInner);
        drawEdge(downW,  0, SCREEN_HEIGHT - kEdgeThickness, SCREEN_WIDTH, kEdgeThickness,
                 0, SCREEN_HEIGHT - kEdgeThickness - kEdgeFadeInner, SCREEN_WIDTH, kEdgeFadeInner);
    }
}

void PlayState::renderOffscreenEnemyIndicators(SDL_Renderer* renderer) {
    if (!m_player || !m_player->getEntity()) return;

    Vec2 camPos = m_camera.getPosition();
    float halfW = SCREEN_WIDTH / 2.0f;
    float halfH = SCREEN_HEIGHT / 2.0f;
    int currentDim = m_dimManager.getCurrentDimension();

    struct Indicator { int sx, sy; float dist; };
    Indicator indicators[5];
    int count = 0;

    m_entities.forEach([&](Entity& e) {
        if (count >= 5) return;
        if (e.getTag().find("enemy") == std::string::npos || !e.isAlive()) return;
        if (e.dimension != 0 && e.dimension != currentDim) return;
        if (!e.hasComponent<TransformComponent>()) return;
        auto& t = e.getComponent<TransformComponent>();
        Vec2 ePos = t.getCenter();

        float dx = ePos.x - camPos.x;
        float dy = ePos.y - camPos.y;
        if (std::abs(dx) <= halfW && std::abs(dy) <= halfH) return;

        float dist = std::sqrt(dx * dx + dy * dy);
        float maxDist = halfW * 3.0f;
        if (dist > maxDist) return;

        int sx = static_cast<int>(std::clamp(dx + halfW, 32.0f, static_cast<float>(SCREEN_WIDTH) - 32.0f));
        int sy = static_cast<int>(std::clamp(dy + halfH, 32.0f, static_cast<float>(SCREEN_HEIGHT) - 32.0f));

        int insertIdx = count;
        for (int i = 0; i < count; i++) {
            if (dist < indicators[i].dist) { insertIdx = i; break; }
        }
        for (int i = std::min(count, 4); i > insertIdx; i--) indicators[i] = indicators[i - 1];
        if (insertIdx < 5) {
            indicators[insertIdx] = {sx, sy, dist};
            if (count < 5) count++;
        }
    });

    if (count == 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    float maxDist = halfW * 3.0f;

    for (int i = 0; i < count; i++) {
        auto& ind = indicators[i];
        float t = 1.0f - (ind.dist / maxDist);
        Uint8 alpha = static_cast<Uint8>(80 + 150 * t);

        float adx = static_cast<float>(ind.sx) - halfW;
        float ady = static_cast<float>(ind.sy) - halfH;
        float len = std::sqrt(adx * adx + ady * ady);
        if (len < 1.0f) continue;
        float nx = adx / len;
        float ny = ady / len;

        int sz = static_cast<int>(10 + 4 * t);
        int tipX = ind.sx, tipY = ind.sy;
        int baseX1 = tipX - static_cast<int>(nx * sz + ny * sz * 0.6f);
        int baseY1 = tipY - static_cast<int>(ny * sz - nx * sz * 0.6f);
        int baseX2 = tipX - static_cast<int>(nx * sz - ny * sz * 0.6f);
        int baseY2 = tipY - static_cast<int>(ny * sz + nx * sz * 0.6f);

        SDL_SetRenderDrawColor(renderer, 220, 50, 40, alpha);
        for (int s = 0; s <= sz; s++) {
            float frac = static_cast<float>(s) / static_cast<float>(sz);
            int x1 = tipX + static_cast<int>((baseX1 - tipX) * frac);
            int y1 = tipY + static_cast<int>((baseY1 - tipY) * frac);
            int x2 = tipX + static_cast<int>((baseX2 - tipX) * frac);
            int y2 = tipY + static_cast<int>((baseY2 - tipY) * frac);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }
}
