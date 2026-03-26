// RenderSystemEnemies.cpp -- Split from RenderSystem.cpp (regular enemy rendering)
#include "RenderSystem.h"
#include "Components/SpriteComponent.h"
#include "Components/HealthComponent.h"
#include "Components/AIComponent.h"
#include <cmath>
#include <algorithm>

// Shared HP bar for all enemies: color gradient, delayed damage lag bar, elite diamond marker
void RenderSystem::renderEnemyHPBar(SDL_Renderer* renderer, int x, int y, int w,
                                     Entity& entity, float alpha, bool isBoss) {
    if (!entity.hasComponent<HealthComponent>()) return;
    auto& hp = entity.getComponent<HealthComponent>();
    float pct = hp.getPercent();
    if (pct >= 1.0f && !isBoss) return; // full HP: nothing to show (bosses always show)

    Uint8 a = static_cast<Uint8>(255 * alpha);
    int barW = isBoss ? w + 20 : w;
    int barH = isBoss ? 5 : 4;
    int barX = isBoss ? x - 10 : x;
    int barY = isBoss ? y - 14 : y - 8;

    // Background (dark)
    fillRect(renderer, barX - 1, barY - 1, barW + 2, barH + 2, 10, 10, 10, a);
    fillRect(renderer, barX, barY, barW, barH, 30, 30, 30, a);

    // Lag bar (delayed damage indicator — fading orange/white)
    float lagPct = hp.getLagPercent();
    if (lagPct > pct) {
        int lagW = static_cast<int>(barW * lagPct);
        fillRect(renderer, barX, barY, lagW, barH, 200, 120, 60, static_cast<Uint8>(a * 0.8f));
    }

    // Current HP fill with color gradient: green > yellow > red
    int hpW = static_cast<int>(barW * pct);
    Uint8 r, g, b;
    if (pct > 0.6f) {
        // Green to yellow (60%-100%)
        float t = (pct - 0.6f) / 0.4f; // 1 at full, 0 at 60%
        r = static_cast<Uint8>(220 - 180 * t);  // 220 -> 40
        g = static_cast<Uint8>(200 + 40 * t);   // 200 -> 240
        b = static_cast<Uint8>(40);
    } else if (pct > 0.3f) {
        // Yellow to orange (30%-60%)
        float t = (pct - 0.3f) / 0.3f; // 1 at 60%, 0 at 30%
        r = static_cast<Uint8>(240 - 20 * t);   // 240 -> 220
        g = static_cast<Uint8>(80 + 120 * t);   // 80 -> 200
        b = static_cast<Uint8>(30);
    } else {
        // Orange to red (0%-30%)
        float t = pct / 0.3f; // 1 at 30%, 0 at 0%
        r = static_cast<Uint8>(200 + 40 * t);   // 200 -> 240
        g = static_cast<Uint8>(30 + 50 * t);    // 30 -> 80
        b = static_cast<Uint8>(20);
    }
    fillRect(renderer, barX, barY, hpW, barH, r, g, b, a);

    // Subtle top highlight on HP fill (1px lighter strip)
    if (hpW > 2) {
        fillRect(renderer, barX, barY, hpW, 1,
                 static_cast<Uint8>(std::min(255, r + 40)),
                 static_cast<Uint8>(std::min(255, g + 30)),
                 static_cast<Uint8>(std::min(255, b + 20)),
                 static_cast<Uint8>(a * 0.5f));
    }

    // Boss phase markers
    if (isBoss) {
        fillRect(renderer, barX + barW * 2 / 3, barY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
        fillRect(renderer, barX + barW / 3, barY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
    }

    // Elite diamond marker
    if (entity.hasComponent<AIComponent>()) {
        auto& ai = entity.getComponent<AIComponent>();
        if (ai.isElite) {
            // Small diamond icon to the left of the HP bar
            int dx = barX - 6;
            int dy = barY + barH / 2;
            // Diamond shape: 4 pixels arranged as a diamond
            fillRect(renderer, dx, dy - 2, 3, 1, 255, 220, 80, a);
            fillRect(renderer, dx - 1, dy - 1, 5, 1, 255, 220, 80, a);
            fillRect(renderer, dx, dy, 3, 1, 255, 220, 80, a);
        }
    }
}

void RenderSystem::renderWalker(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;

    // Idle animation: gentle 2px vertical bob in Patrol/Idle
    if (entity.hasComponent<AIComponent>()) {
        auto aiSt = entity.getComponent<AIComponent>().state;
        if (aiSt == AIState::Patrol || aiSt == AIState::Idle) {
            float bobTime = SDL_GetTicks() * 0.003f;
            int bob = static_cast<int>(std::sin(bobTime) * 2.0f);
            y += bob;
        }
    }

    // Drop shadow
    fillRect(renderer, x + 3, y + h, w - 6, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Animated legs
    float time = SDL_GetTicks() * 0.006f;
    int legOff = static_cast<int>(std::sin(time) * 2);
    fillRect(renderer, x + 2, y + h - 8 + legOff, w / 3, 8, 150, 40, 40, a);
    fillRect(renderer, x + w - w / 3 - 2, y + h - 8 - legOff, w / 3, 8, 150, 40, 40, a);

    // Body outline
    fillRect(renderer, x - 1, y + 3, w + 2, h - 10, 100, 20, 20, a);
    // Bulky body
    fillRect(renderer, x, y + 4, w, h - 12, 200, 55, 55, a);
    // Body top highlight
    fillRect(renderer, x + 1, y + 5, w - 2, 2, 240, 90, 90, static_cast<Uint8>(a * 0.6f));
    // Body bottom darkening
    fillRect(renderer, x + 1, y + h - 10, w - 2, 2, 140, 30, 30, static_cast<Uint8>(a * 0.5f));
    fillRect(renderer, x + 3, y + h / 2, w - 6, h / 4, 160, 40, 40, a);

    // Head outline + fill
    fillRect(renderer, x + 1, y - 1, w - 2, h / 3 + 2, 120, 25, 25, a);
    fillRect(renderer, x + 2, y, w - 4, h / 3, 220, 60, 60, a);
    // Head top highlight
    fillRect(renderer, x + 3, y + 1, w - 6, 2, 255, 100, 100, static_cast<Uint8>(a * 0.5f));

    // Angry eyes
    int eyeY = y + h / 6;
    if (!flipped) {
        fillRect(renderer, x + w / 2, eyeY, 5, 4, 255, 200, 50, a);
        fillRect(renderer, x + w - 8, eyeY, 5, 4, 255, 200, 50, a);
    } else {
        fillRect(renderer, x + 3, eyeY, 5, 4, 255, 200, 50, a);
        fillRect(renderer, x + w / 2 - 5, eyeY, 5, 4, 255, 200, 50, a);
    }

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha);
}

void RenderSystem::renderFlyer(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;

    // Idle animation: figure-8 hover pattern in Patrol
    if (entity.hasComponent<AIComponent>()) {
        auto aiSt = entity.getComponent<AIComponent>().state;
        if (aiSt == AIState::Patrol || aiSt == AIState::Idle) {
            float fig8Time = SDL_GetTicks() * 0.003f;
            int fig8X = static_cast<int>(std::sin(fig8Time) * 3.0f);
            int fig8Y = static_cast<int>(std::sin(fig8Time * 2.0f) * 2.0f);
            x += fig8X;
            y += fig8Y;
        }
    }

    int cx = x + w / 2, cy = y + h / 2;

    float time = SDL_GetTicks() * 0.012f;
    int wingOff = static_cast<int>(std::sin(time) * 5);

    // Wings (animated flap)
    SDL_SetRenderDrawColor(renderer, 140, 60, 180, a);
    for (int i = 0; i < 3; i++) {
        SDL_RenderDrawLine(renderer, cx - 2, cy + i * 2, cx - w / 2 - 4, cy + wingOff - 4 + i);
        SDL_RenderDrawLine(renderer, cx + 2, cy + i * 2, cx + w / 2 + 4, cy - wingOff - 4 + i);
    }

    // Drop shadow (faint for flying entity)
    fillRect(renderer, cx - 4, cy + h / 2 + 6, 8, 2, 0, 0, 0, static_cast<Uint8>(a * 0.2f));

    // Diamond body outline
    fillRect(renderer, cx - 5, cy - 7, 10, 14, 100, 30, 130, a);
    fillRect(renderer, cx - 7, cy - 4, 14, 8, 100, 30, 130, a);
    // Diamond body fill
    fillRect(renderer, cx - 4, cy - 6, 8, 12, 180, 70, 210, a);
    fillRect(renderer, cx - 6, cy - 3, 12, 6, 180, 70, 210, a);
    // Top highlight
    fillRect(renderer, cx - 3, cy - 5, 6, 2, 220, 120, 240, static_cast<Uint8>(a * 0.6f));

    // Glowing eye
    fillRect(renderer, cx - 2, cy - 3, 4, 3, 255, 100, 255, a);

    // Tail
    drawLine(renderer, cx, cy + 6, cx + static_cast<int>(std::sin(time * 0.7f) * 3), cy + 14, 160, 50, 190, a);

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha);
}

void RenderSystem::renderTurret(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    bool flipped = entity.getComponent<SpriteComponent>().flipX;

    // Drop shadow
    fillRect(renderer, x + 1, y + h, w - 2, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Wide base
    fillRect(renderer, x - 2, y + h - 8, w + 4, 8, 140, 140, 40, a);
    fillRect(renderer, x, y + h - 12, w, 6, 160, 160, 50, a);

    // Box body outline
    fillRect(renderer, x + 1, y + 3, w - 2, h - 14, 100, 100, 20, a);
    // Box body
    fillRect(renderer, x + 2, y + 4, w - 4, h - 16, 200, 200, 55, a);
    // Top highlight
    fillRect(renderer, x + 3, y + 5, w - 6, 2, 240, 240, 90, static_cast<Uint8>(a * 0.5f));
    fillRect(renderer, x + 5, y + 8, w - 10, h - 24, 170, 170, 40, a);

    // Barrel
    int barrelY = y + h / 3;
    int barrelLen = w / 2 + 6;
    if (!flipped) {
        fillRect(renderer, x + w - 2, barrelY, barrelLen, 5, 180, 180, 50, a);
        fillRect(renderer, x + w + barrelLen - 4, barrelY - 2, 4, 9, 200, 200, 60, a);
        fillRect(renderer, x + w + barrelLen - 2, barrelY, 2, 5, 255, 230, 100, a);
    } else {
        fillRect(renderer, x - barrelLen + 2, barrelY, barrelLen, 5, 180, 180, 50, a);
        fillRect(renderer, x - barrelLen, barrelY - 2, 4, 9, 200, 200, 60, a);
        fillRect(renderer, x - barrelLen, barrelY, 2, 5, 255, 230, 100, a);
    }

    // Sensor eye
    fillRect(renderer, x + w / 2 - 3, y + 6, 6, 4, 255, 60, 60, a);

    // Idle animation: rotating scanner line
    if (entity.hasComponent<AIComponent>()) {
        auto aiSt = entity.getComponent<AIComponent>().state;
        if (aiSt == AIState::Idle || aiSt == AIState::Patrol) {
            float scanTime = SDL_GetTicks() * 0.003f;
            int scanLen = w / 2 + 6;
            int scanCX = x + w / 2;
            int scanCY = y + 6;
            int scanEndX = scanCX + static_cast<int>(std::cos(scanTime) * scanLen);
            int scanEndY = scanCY + static_cast<int>(std::sin(scanTime) * scanLen * 0.4f);
            drawLine(renderer, scanCX, scanCY, scanEndX, scanEndY, 255, 80, 80, static_cast<Uint8>(120 * alpha));
        }
    }

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha);
}

void RenderSystem::renderCharger(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;

    // Drop shadow
    fillRect(renderer, x + 2, y + h, w - 4, 3, 0, 0, 0, static_cast<Uint8>(a * 0.35f));

    // Thick legs
    fillRect(renderer, x + 2, y + h - 8, w / 3, 8, 180, 90, 30, a);
    fillRect(renderer, x + w - w / 3 - 2, y + h - 8, w / 3, 8, 180, 90, 30, a);

    // Body outline
    fillRect(renderer, x - 3, y + 5, w + 6, h - 12,
             static_cast<Uint8>(sprite.color.r * 0.35f),
             static_cast<Uint8>(sprite.color.g * 0.35f),
             static_cast<Uint8>(sprite.color.b * 0.35f), a);
    // Wide body
    fillRect(renderer, x - 2, y + 6, w + 4, h - 14, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Body top highlight
    fillRect(renderer, x - 1, y + 7, w + 2, 2,
             static_cast<Uint8>(std::min(255, sprite.color.r + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 40)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 30)),
             static_cast<Uint8>(a * 0.6f));
    // Armor plate
    fillRect(renderer, x, y + 8, w, h / 3,
             static_cast<Uint8>(std::min(255, sprite.color.r + 20)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 10)),
             sprite.color.b, a);
    // Body bottom darkening
    fillRect(renderer, x - 1, y + h - 10, w + 2, 2,
             static_cast<Uint8>(sprite.color.r * 0.5f),
             static_cast<Uint8>(sprite.color.g * 0.5f),
             static_cast<Uint8>(sprite.color.b * 0.5f), static_cast<Uint8>(a * 0.5f));

    // Head outline + fill
    fillRect(renderer, x + 3, y - 1, w - 6, h / 3 + 2,
             static_cast<Uint8>(sprite.color.r * 0.4f),
             static_cast<Uint8>(sprite.color.g * 0.4f),
             static_cast<Uint8>(sprite.color.b * 0.4f), a);
    fillRect(renderer, x + 4, y, w - 8, h / 3, sprite.color.r, sprite.color.g, sprite.color.b, a);

    // Horns
    if (!flipped) {
        drawLine(renderer, x + w - 4, y + 2, x + w + 6, y - 6, 240, 200, 80, a);
        drawLine(renderer, x + w - 4, y + 4, x + w + 5, y - 4, 240, 200, 80, a);
    } else {
        drawLine(renderer, x + 4, y + 2, x - 6, y - 6, 240, 200, 80, a);
        drawLine(renderer, x + 4, y + 4, x - 5, y - 4, 240, 200, 80, a);
    }

    // Eyes
    fillRect(renderer, x + w / 3, y + h / 6, 3, 3, 255, 100, 30, a);
    fillRect(renderer, x + 2 * w / 3 - 3, y + h / 6, 3, 3, 255, 100, 30, a);

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha);
}

void RenderSystem::renderPhaser(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(200 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();

    float time = SDL_GetTicks() * 0.005f;
    int shimmer = static_cast<int>(std::sin(time) * 3);

    // Drop shadow (faint for ghostly entity)
    fillRect(renderer, x + 4, y + h, w - 8, 2, 0, 0, 0, static_cast<Uint8>(a * 0.15f));

    // Ghostly wavy body
    for (int i = 0; i < h; i += 2) {
        int waveOff = static_cast<int>(std::sin(time + i * 0.15f) * 3);
        float yFrac = static_cast<float>(i) / h;
        int rowW = static_cast<int>(w * (0.5f + yFrac * 0.5f));
        int rowX = x + (w - rowW) / 2 + waveOff;
        Uint8 rowA = static_cast<Uint8>(a * (0.4f + yFrac * 0.6f));
        fillRect(renderer, rowX, y + i, rowW, 2, sprite.color.r, sprite.color.g, sprite.color.b, rowA);
    }

    // Face mask
    int faceY = y + h / 5;
    fillRect(renderer, x + w / 4 + shimmer, faceY, w / 2, h / 5, 220, 200, 255, a);

    // Dimension-shifting eyes
    Uint32 t = SDL_GetTicks();
    bool phase1 = (t / 200) % 2 == 0;
    Uint8 eyeR = phase1 ? 100 : 200;
    Uint8 eyeB = phase1 ? 200 : 100;
    fillRect(renderer, x + w / 3 + shimmer, faceY + 3, 3, 4, eyeR, 50, eyeB, 255);
    fillRect(renderer, x + 2 * w / 3 - 3 + shimmer, faceY + 3, 3, 4, eyeR, 50, eyeB, 255);

    // Trailing wisps
    for (int i = 0; i < 3; i++) {
        int px = x + w / 2 + static_cast<int>(std::sin(time * 1.5f + i * 2.1f) * w / 2);
        int py = y + h + i * 4 + static_cast<int>(std::sin(time + i) * 2);
        fillRect(renderer, px, py, 2, 2, sprite.color.r, sprite.color.g, sprite.color.b,
                 static_cast<Uint8>(a * 0.4f));
    }

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha);
}

void RenderSystem::renderExploder(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    float time = SDL_GetTicks() * 0.01f;

    // Pulsing orange-red body (gets faster at low HP)
    float hpPct = 1.0f;
    if (entity.hasComponent<HealthComponent>()) {
        hpPct = entity.getComponent<HealthComponent>().getPercent();
    }

    // Proximity grow: pulsing size increase when chasing, up to 15% bigger at low HP
    if (entity.hasComponent<AIComponent>()) {
        auto aiSt = entity.getComponent<AIComponent>().state;
        if (aiSt == AIState::Chase || aiSt == AIState::Attack) {
            float growFactor = (1.0f - hpPct) * 0.15f; // up to 15% bigger at 0 HP
            float growPulse = 1.0f + growFactor * (0.5f + 0.5f * std::sin(time * 3.0f));
            int growW = static_cast<int>(w * growPulse);
            int growH = static_cast<int>(h * growPulse);
            x -= (growW - w) / 2;
            y -= (growH - h) / 2;
            w = growW;
            h = growH;
        }
    }

    float pulseSpeed = 1.0f + (1.0f - hpPct) * 4.0f;
    float pulse = 0.7f + 0.3f * std::sin(time * pulseSpeed);

    // Drop shadow
    fillRect(renderer, x + 2, y + h - 2, w - 4, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    Uint8 bodyR = static_cast<Uint8>(255 * pulse);
    Uint8 bodyG = static_cast<Uint8>(60 * pulse);
    // Body outline (dark border)
    fillRect(renderer, x - 1, y + 1, w + 2, h - 2,
             static_cast<Uint8>(bodyR * 0.3f), static_cast<Uint8>(bodyG * 0.3f), 8, a);
    fillRect(renderer, x, y + 2, w, h - 4, bodyR, bodyG, 20, a);
    // Top highlight
    fillRect(renderer, x + 1, y + 3, w - 2, 2,
             static_cast<Uint8>(std::min(255, (int)bodyR + 40)),
             static_cast<Uint8>(std::min(255, (int)bodyG + 60)), 50,
             static_cast<Uint8>(a * 0.5f));

    // Inner glow (brighter as HP decreases)
    Uint8 glowA = static_cast<Uint8>(a * (0.3f + (1.0f - hpPct) * 0.5f));
    fillRect(renderer, x + w / 4, y + h / 4, w / 2, h / 2, 255, 200, 50, glowA);

    // Warning symbol (!)
    fillRect(renderer, x + w / 2 - 1, y + h / 4, 3, h / 3, 255, 255, 100, a);
    fillRect(renderer, x + w / 2 - 1, y + h * 2 / 3, 3, 3, 255, 255, 100, a);

    // Fuse sparks
    int sparkX = x + w / 2 + static_cast<int>(std::sin(time * 3) * 4);
    int sparkY = y - 2 + static_cast<int>(std::cos(time * 4) * 2);
    fillRect(renderer, sparkX, sparkY, 2, 2, 255, 255, 200, a);
    fillRect(renderer, sparkX + 2, sparkY - 1, 1, 1, 255, 200, 50, a);

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha);
}

void RenderSystem::renderShielder(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;

    // Drop shadow
    fillRect(renderer, x + 3, y + h, w - 6, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Legs
    fillRect(renderer, x + 3, y + h - 8, w / 3, 8, 50, 120, 160, a);
    fillRect(renderer, x + w - w / 3 - 3, y + h - 8, w / 3, 8, 50, 120, 160, a);

    // Body outline
    fillRect(renderer, x + 1, y + 5, w - 2, h - 12, 30, 80, 110, a);
    // Body (armored)
    fillRect(renderer, x + 2, y + 6, w - 4, h - 14, 70, 160, 200, a);
    // Top highlight
    fillRect(renderer, x + 3, y + 7, w - 6, 2, 110, 200, 240, static_cast<Uint8>(a * 0.6f));
    // Armor highlight (left edge)
    fillRect(renderer, x + 3, y + 9, (w - 6) / 2, h / 3, 100, 190, 230, static_cast<Uint8>(a * 0.6f));
    // Bottom darkening
    fillRect(renderer, x + 2, y + h - 10, w - 4, 2, 40, 100, 140, static_cast<Uint8>(a * 0.5f));

    // Head outline + fill
    fillRect(renderer, x + 3, y - 1, w - 6, h / 3 + 2, 30, 80, 110, a);
    fillRect(renderer, x + 4, y, w - 8, h / 3, 60, 140, 180, a);
    // Head top highlight
    fillRect(renderer, x + 5, y + 1, w - 10, 2, 90, 180, 220, static_cast<Uint8>(a * 0.5f));
    // Visor slit
    int visorY = y + h / 8;
    fillRect(renderer, x + 6, visorY, w - 12, 3, 200, 240, 255, a);

    // Shield (large, on the front side)
    float time = SDL_GetTicks() * 0.003f;
    Uint8 shieldAlpha = static_cast<Uint8>(a * (0.7f + 0.3f * std::sin(time)));
    int shieldW = 6;
    int shieldH = h - 4;
    if (!flipped) {
        fillRect(renderer, x + w, y + 2, shieldW, shieldH, 120, 200, 255, shieldAlpha);
        fillRect(renderer, x + w + 1, y + 4, shieldW - 2, shieldH - 4, 180, 230, 255, static_cast<Uint8>(shieldAlpha * 0.6f));
        // Shield edge glow
        drawLine(renderer, x + w + shieldW, y + 2, x + w + shieldW, y + 2 + shieldH, 200, 240, 255, static_cast<Uint8>(shieldAlpha * 0.4f));
        // Shimmer highlight band moving across shield
        int shimmerY = y + 2 + static_cast<int>(std::fmod(time * 8.0f, static_cast<float>(shieldH)));
        fillRect(renderer, x + w, shimmerY, shieldW, 2, 240, 250, 255, static_cast<Uint8>(shieldAlpha * 0.8f));
    } else {
        fillRect(renderer, x - shieldW, y + 2, shieldW, shieldH, 120, 200, 255, shieldAlpha);
        fillRect(renderer, x - shieldW + 1, y + 4, shieldW - 2, shieldH - 4, 180, 230, 255, static_cast<Uint8>(shieldAlpha * 0.6f));
        drawLine(renderer, x - shieldW, y + 2, x - shieldW, y + 2 + shieldH, 200, 240, 255, static_cast<Uint8>(shieldAlpha * 0.4f));
        // Shimmer highlight band
        int shimmerY = y + 2 + static_cast<int>(std::fmod(time * 8.0f, static_cast<float>(shieldH)));
        fillRect(renderer, x - shieldW, shimmerY, shieldW, 2, 240, 250, 255, static_cast<Uint8>(shieldAlpha * 0.8f));
    }

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha);
}

void RenderSystem::renderCrawler(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;
    float time = SDL_GetTicks() * 0.005f;

    bool onCeiling = false;
    if (entity.hasComponent<AIComponent>()) {
        onCeiling = entity.getComponent<AIComponent>().onCeiling;
    }

    // Drop shadow (underneath for ground crawlers, above for ceiling)
    if (!onCeiling) {
        fillRect(renderer, x + 3, y + h, w - 6, 2, 0, 0, 0, static_cast<Uint8>(a * 0.3f));
    }

    // Body outline
    fillRect(renderer, x + 1, y + 1, w - 2, h - 2,
             static_cast<Uint8>(sprite.color.r * 0.35f),
             static_cast<Uint8>(sprite.color.g * 0.35f),
             static_cast<Uint8>(sprite.color.b * 0.35f), a);
    // Flat, wide body (like a bug)
    fillRect(renderer, x + 2, y + 2, w - 4, h - 4, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Top highlight
    fillRect(renderer, x + 3, y + 3, w - 6, 2,
             static_cast<Uint8>(std::min(255, sprite.color.r + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 50)),
             static_cast<Uint8>(a * 0.5f));

    // Carapace stripe
    fillRect(renderer, x + 4, y + h / 3, w - 8, 2,
             static_cast<Uint8>(std::min(255, sprite.color.r + 40)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 40)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 40)), a);

    // 6 legs (3 per side, animated)
    for (int i = 0; i < 3; i++) {
        float legAnim = std::sin(time * 3.0f + i * 2.0f) * 3.0f;
        int legX = x + 4 + i * (w - 8) / 3;
        int legDir = onCeiling ? -1 : 1;
        int legY = onCeiling ? y : y + h;
        drawLine(renderer, legX, legY, legX - 3, legY + legDir * (6 + static_cast<int>(legAnim)),
                 80, 120, 60, a);
        drawLine(renderer, legX, legY, legX + 3, legY + legDir * (6 - static_cast<int>(legAnim)),
                 80, 120, 60, a);
    }

    // Eyes (2 small dots)
    int eyeY = onCeiling ? y + h - 4 : y + 3;
    if (!flipped) {
        fillRect(renderer, x + w - 6, eyeY, 2, 2, 255, 200, 50, a);
        fillRect(renderer, x + w - 10, eyeY, 2, 2, 255, 200, 50, a);
    } else {
        fillRect(renderer, x + 4, eyeY, 2, 2, 255, 200, 50, a);
        fillRect(renderer, x + 8, eyeY, 2, 2, 255, 200, 50, a);
    }
}

void RenderSystem::renderSummoner(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    float time = SDL_GetTicks() * 0.004f;

    // Drop shadow
    fillRect(renderer, x + 3, y + h, w - 6, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Robed body (trapezoid shape)
    int robeTop = w / 2;
    int robeBot = w;
    // Robe outline
    fillRect(renderer, x - 1, y + h / 2 - 1, robeBot + 2, h / 2 + 2,
             static_cast<Uint8>(sprite.color.r * 0.3f),
             static_cast<Uint8>(sprite.color.g * 0.3f),
             static_cast<Uint8>(sprite.color.b * 0.3f), a);
    fillRect(renderer, x + (w - robeTop) / 2, y + h / 4, robeTop, h / 4,
             sprite.color.r, sprite.color.g, sprite.color.b, a);
    fillRect(renderer, x, y + h / 2, robeBot, h / 2,
             static_cast<Uint8>(sprite.color.r * 0.7f),
             static_cast<Uint8>(sprite.color.g * 0.7f),
             static_cast<Uint8>(sprite.color.b * 0.7f), a);
    // Bottom highlight on robe
    fillRect(renderer, x + 1, y + h - 3, robeBot - 2, 2,
             static_cast<Uint8>(sprite.color.r * 0.5f),
             static_cast<Uint8>(sprite.color.g * 0.5f),
             static_cast<Uint8>(sprite.color.b * 0.5f), static_cast<Uint8>(a * 0.4f));

    // Hood/head
    int headW = w / 2;
    int headH = h / 4;
    fillRect(renderer, x + (w - headW) / 2, y, headW, headH,
             static_cast<Uint8>(sprite.color.r * 0.5f),
             static_cast<Uint8>(sprite.color.g * 0.5f),
             static_cast<Uint8>(sprite.color.b * 0.5f), a);

    // Glowing eyes under hood
    int eyeY = y + headH / 2;
    float eyePulse = 0.5f + 0.5f * std::sin(time * 2.0f);
    Uint8 eyeGlow = static_cast<Uint8>(150 + 105 * eyePulse);
    int eyeOff = headW / 4;
    fillRect(renderer, x + w / 2 - eyeOff - 1, eyeY, 3, 2, eyeGlow, 50, eyeGlow, a);
    fillRect(renderer, x + w / 2 + eyeOff - 1, eyeY, 3, 2, eyeGlow, 50, eyeGlow, a);

    // Floating orbs around hands (summoning energy)
    for (int i = 0; i < 3; i++) {
        float angle = time * 2.0f + i * 2.094f;
        int ox = x + w / 2 + static_cast<int>(std::cos(angle) * (w / 2 + 6));
        int oy = y + h / 3 + static_cast<int>(std::sin(angle) * 8);
        Uint8 oa = static_cast<Uint8>(a * (0.3f + 0.3f * std::sin(time * 3.0f + i)));
        fillRect(renderer, ox - 2, oy - 2, 4, 4, 200, 100, 255, oa);
    }
}

void RenderSystem::renderSniper(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;
    float time = SDL_GetTicks() * 0.004f;

    bool telegraphing = false;
    if (entity.hasComponent<AIComponent>()) {
        telegraphing = entity.getComponent<AIComponent>().isTelegraphing;
    }

    // Drop shadow
    fillRect(renderer, x + 3, y + h, w - 6, 2, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Body outline
    fillRect(renderer, x + 3, y + h / 5 - 1, w - 6, h * 3 / 5 + 2,
             static_cast<Uint8>(sprite.color.r * 0.35f),
             static_cast<Uint8>(sprite.color.g * 0.35f),
             static_cast<Uint8>(sprite.color.b * 0.35f), a);
    // Thin body
    fillRect(renderer, x + 4, y + h / 5, w - 8, h * 3 / 5,
             sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Top highlight
    fillRect(renderer, x + 5, y + h / 5 + 1, w - 10, 2,
             static_cast<Uint8>(std::min(255, sprite.color.r + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 50)),
             static_cast<Uint8>(a * 0.5f));

    // Head outline + fill
    int headW = w / 2;
    int headH = h / 5;
    fillRect(renderer, x + (w - headW) / 2 - 1, y - 1, headW + 2, headH + 2,
             static_cast<Uint8>(sprite.color.r * 0.4f),
             static_cast<Uint8>(sprite.color.g * 0.4f),
             static_cast<Uint8>(sprite.color.b * 0.4f), a);
    fillRect(renderer, x + (w - headW) / 2, y, headW, headH,
             sprite.color.r, sprite.color.g, sprite.color.b, a);

    // Scope eye (one glowing eye)
    int eyeX = flipped ? x + w / 4 : x + w * 3 / 4 - 3;
    int eyeY = y + headH / 3;
    Uint8 scopeGlow = telegraphing ? 255 : static_cast<Uint8>(180 + 75 * std::sin(time * 2.0f));
    fillRect(renderer, eyeX, eyeY, 3, 3, scopeGlow, 50, 50, a);

    // Long rifle
    int rifleLen = w + 8;
    int rifleY = y + h / 3;
    if (!flipped) {
        fillRect(renderer, x + w - 2, rifleY, rifleLen, 3, 150, 140, 50, a);
        fillRect(renderer, x + w + rifleLen - 4, rifleY - 1, 4, 5, 200, 180, 40, a);
    } else {
        fillRect(renderer, x - rifleLen + 2, rifleY, rifleLen, 3, 150, 140, 50, a);
        fillRect(renderer, x - rifleLen + 2, rifleY - 1, 4, 5, 200, 180, 40, a);
    }

    // Legs
    int legY = y + h * 4 / 5;
    fillRect(renderer, x + w / 4, legY, 3, h / 5, sprite.color.r, sprite.color.g, sprite.color.b, a);
    fillRect(renderer, x + w * 3 / 4 - 3, legY, 3, h / 5, sprite.color.r, sprite.color.g, sprite.color.b, a);

    // Telegraph laser line: dotted red line that gets denser as shot approaches
    if (telegraphing) {
        float telegraphPct = 0.5f; // default mid-telegraph
        if (entity.hasComponent<AIComponent>()) {
            auto& aiTel = entity.getComponent<AIComponent>();
            if (aiTel.telegraphDuration > 0.01f) {
                telegraphPct = 1.0f - (aiTel.telegraphTimer / aiTel.telegraphDuration);
            }
        }
        // Dot spacing gets denser as shot approaches (20px -> 4px)
        int dotSpacing = std::max(4, static_cast<int>(20.0f * (1.0f - telegraphPct)));
        // Brightness increases as shot approaches
        Uint8 laserBase = static_cast<Uint8>(80 + 175 * telegraphPct);
        Uint8 laserPulse = static_cast<Uint8>(laserBase * (0.7f + 0.3f * std::sin(time * 15.0f)));
        int laserLen = 400;
        int laserDir = flipped ? -1 : 1;
        int laserStartX = flipped ? x : x + w;
        // Draw dotted line
        for (int d = 0; d < laserLen; d += dotSpacing) {
            int dotX = laserStartX + laserDir * d;
            int dotW = std::max(1, dotSpacing / 3);
            fillRect(renderer, dotX, rifleY, dotW, 1, 255, 50, 50, laserPulse);
        }
        // Crosshair reticle at end of telegraph line
        int reticleX = laserStartX + laserDir * laserLen;
        int reticleSize = 6 + static_cast<int>(3.0f * telegraphPct);
        Uint8 retA = static_cast<Uint8>(160 * telegraphPct * alpha);
        drawLine(renderer, reticleX - reticleSize, rifleY, reticleX + reticleSize, rifleY, 255, 60, 60, retA);
        drawLine(renderer, reticleX, rifleY - reticleSize, reticleX, rifleY + reticleSize, 255, 60, 60, retA);
        // Outer ring approximation (4 corner lines)
        int rs = reticleSize - 1;
        drawLine(renderer, reticleX - rs, rifleY - rs, reticleX - rs + 2, rifleY - rs, 255, 60, 60, retA);
        drawLine(renderer, reticleX + rs - 2, rifleY - rs, reticleX + rs, rifleY - rs, 255, 60, 60, retA);
        drawLine(renderer, reticleX - rs, rifleY + rs, reticleX - rs + 2, rifleY + rs, 255, 60, 60, retA);
        drawLine(renderer, reticleX + rs - 2, rifleY + rs, reticleX + rs, rifleY + rs, 255, 60, 60, retA);
    }
}

void RenderSystem::renderTeleporter(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    int cx = x + w / 2;
    auto& sprite = entity.getComponent<SpriteComponent>();
    float time = SDL_GetTicks() * 0.004f;

    // Drop shadow
    fillRect(renderer, x + 2, y + h, w - 4, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Hooded robe body (trapezoid: narrow top, wide bottom)
    int robeTopW = w / 2;
    int robeBotW = w + 4;
    // Draw trapezoid body row by row
    for (int row = 0; row < h * 2 / 3; row++) {
        float t = static_cast<float>(row) / (h * 2.0f / 3.0f);
        int rowW = static_cast<int>(robeTopW + (robeBotW - robeTopW) * t);
        int rowX = cx - rowW / 2;
        int rowY = y + h / 3 + row;
        Uint8 robeR = static_cast<Uint8>(90 + 30 * t);
        Uint8 robeG = static_cast<Uint8>(40 + 15 * t);
        Uint8 robeB = static_cast<Uint8>(140 + 40 * t);
        fillRect(renderer, rowX, rowY, rowW, 1, robeR, robeG, robeB, a);
    }

    // Hood outline + fill (dark purple dome)
    int hoodW = w * 2 / 3;
    int hoodH = h / 3;
    fillRect(renderer, cx - hoodW / 2 - 1, y - 1, hoodW + 2, hoodH + 2, 30, 15, 55, a);
    fillRect(renderer, cx - hoodW / 2, y, hoodW, hoodH, 60, 30, 100, a);
    // Hood top highlight
    fillRect(renderer, cx - hoodW / 2 + 2, y + 1, hoodW - 4, 2, 90, 50, 140, static_cast<Uint8>(a * 0.5f));
    fillRect(renderer, cx - hoodW / 2 + 2, y + 2, hoodW - 4, hoodH / 2, 50, 25, 85, a);

    // Cyan-purple pulsing eyes under hood
    float eyePulse = 0.5f + 0.5f * std::sin(time * 3.0f);
    Uint8 eyeR = static_cast<Uint8>(80 + 100 * eyePulse);
    Uint8 eyeG = static_cast<Uint8>(180 * eyePulse);
    Uint8 eyeB = static_cast<Uint8>(200 + 55 * eyePulse);
    int eyeY = y + hoodH / 2 + 2;
    fillRect(renderer, cx - 5, eyeY, 3, 3, eyeR, eyeG, eyeB, a);
    fillRect(renderer, cx + 2, eyeY, 3, 3, eyeR, eyeG, eyeB, a);

    // Dimensional crack lines around body (animated)
    for (int i = 0; i < 4; i++) {
        float angle = time * 1.5f + i * 1.5708f;
        float crackLen = 8.0f + 4.0f * std::sin(time * 2.0f + i);
        int cx1 = cx + static_cast<int>(std::cos(angle) * (w / 2 + 3));
        int cy1 = y + h / 2 + static_cast<int>(std::sin(angle) * (h / 2 + 3));
        int cx2 = cx1 + static_cast<int>(std::cos(angle + 0.5f) * crackLen);
        int cy2 = cy1 + static_cast<int>(std::sin(angle + 0.5f) * crackLen);
        Uint8 crackA = static_cast<Uint8>(120 * eyePulse * alpha);
        drawLine(renderer, cx1, cy1, cx2, cy2, 160, 80, 220, crackA);
    }

    // Teleport afterimage: fading ghost at previous position when justTeleported
    if (entity.hasComponent<AIComponent>()) {
        auto& ai = entity.getComponent<AIComponent>();
        // Use afterimage data if available (afterimageTimer > 0 means recent teleport)
        // The other agent adds afterimageTimer/afterimagePos to AIComponent
        // We render a fading ghost copy for visual feedback
        if (ai.eliteTeleportTimer < 0.3f && ai.eliteTeleportTimer > 0) {
            // Brief purple flash at current position to show arrival
            Uint8 flashA = static_cast<Uint8>(120 * (ai.eliteTeleportTimer / 0.3f) * alpha);
            fillRect(renderer, x - 2, y - 2, w + 4, h + 4, 140, 60, 200, flashA);
        }
    }
}

void RenderSystem::renderReflector(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    int cx = x + w / 2;
    int cy = y + h / 2;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;
    float time = SDL_GetTicks() * 0.004f;

    // Drop shadow
    fillRect(renderer, x + 3, y + h, w - 6, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Body outline
    fillRect(renderer, x, y + h / 5 - 1, w, h * 3 / 5 + 2, 80, 90, 110, a);
    // Broad silver-blue body
    fillRect(renderer, x + 1, y + h / 5, w - 2, h * 3 / 5, 140, 160, 190, a);
    // Top highlight
    fillRect(renderer, x + 2, y + h / 5 + 1, w - 4, 2, 180, 200, 230, static_cast<Uint8>(a * 0.6f));
    // Darker core
    fillRect(renderer, x + 3, y + h / 4, w - 6, h / 2, 100, 120, 150, a);

    // Head outline + fill
    int headW = w * 3 / 4;
    int headH = h / 5;
    fillRect(renderer, cx - headW / 2 - 1, y - 1, headW + 2, headH + 2, 90, 100, 130, a);
    fillRect(renderer, cx - headW / 2, y, headW, headH, 160, 170, 200, a);
    // Eye slit (narrow horizontal line)
    fillRect(renderer, cx - headW / 3, y + headH / 2, headW * 2 / 3, 2, 200, 220, 255, a);

    // Legs
    int legY = y + h * 4 / 5;
    fillRect(renderer, x + w / 4, legY, 4, h / 5, 120, 140, 170, a);
    fillRect(renderer, x + w * 3 / 4 - 4, legY, 4, h / 5, 120, 140, 170, a);

    // Rotating rhombus mirror-shield on front (4-point diamond using drawLine)
    bool shieldActive = true;
    if (entity.hasComponent<AIComponent>()) {
        shieldActive = entity.getComponent<AIComponent>().shieldUp;
    }

    float shieldRotAngle = time * 2.0f;
    int shieldCX = flipped ? x - 8 : x + w + 8;
    int shieldCY = cy;
    int shieldSize = 10;

    if (shieldActive) {
        // Bright pulsing mirror-shield
        float shieldPulse = 0.6f + 0.4f * std::sin(time * 4.0f);
        Uint8 shieldA = static_cast<Uint8>(200 * shieldPulse * alpha);

        // Diamond shape (4 points: top, right, bottom, left) with rotation
        int dx1 = static_cast<int>(std::cos(shieldRotAngle) * shieldSize);
        int dy1 = static_cast<int>(std::sin(shieldRotAngle) * shieldSize);
        int dx2 = static_cast<int>(std::cos(shieldRotAngle + 1.5708f) * shieldSize);
        int dy2 = static_cast<int>(std::sin(shieldRotAngle + 1.5708f) * shieldSize);

        drawLine(renderer, shieldCX + dx1, shieldCY + dy1, shieldCX + dx2, shieldCY + dy2,
                 200, 220, 255, shieldA);
        drawLine(renderer, shieldCX + dx2, shieldCY + dy2, shieldCX - dx1, shieldCY - dy1,
                 200, 220, 255, shieldA);
        drawLine(renderer, shieldCX - dx1, shieldCY - dy1, shieldCX - dx2, shieldCY - dy2,
                 200, 220, 255, shieldA);
        drawLine(renderer, shieldCX - dx2, shieldCY - dy2, shieldCX + dx1, shieldCY + dy1,
                 200, 220, 255, shieldA);

        // Inner glow fill
        fillRect(renderer, shieldCX - 4, shieldCY - 4, 8, 8, 180, 210, 255, static_cast<Uint8>(shieldA * 0.4f));
    } else {
        // Shield down: fragmented lines (broken diamond)
        Uint8 fragA = static_cast<Uint8>(80 * alpha);
        int dx1 = static_cast<int>(std::cos(shieldRotAngle * 0.3f) * shieldSize);
        int dy1 = static_cast<int>(std::sin(shieldRotAngle * 0.3f) * shieldSize);
        drawLine(renderer, shieldCX + dx1, shieldCY + dy1, shieldCX - 2, shieldCY + 3,
                 130, 150, 180, fragA);
        drawLine(renderer, shieldCX - dx1, shieldCY - dy1, shieldCX + 3, shieldCY - 2,
                 130, 150, 180, fragA);
    }
}

void RenderSystem::renderLeech(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    int cx = x + w / 2;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;
    float time = SDL_GetTicks() * 0.005f;

    // Drop shadow
    fillRect(renderer, x + 3, y + h, w - 6, 3, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Low, wide blob body (no legs)
    int bodyY = y + h / 4;
    int bodyH = h * 3 / 4;
    // Body outline
    fillRect(renderer, x - 1, bodyY + 1, w + 2, bodyH - 2, 25, 50, 20, a);
    fillRect(renderer, x, bodyY + 2, w, bodyH - 4, 50, 100, 40, a);
    // Top highlight
    fillRect(renderer, x + 1, bodyY + 3, w - 2, 2, 80, 140, 65, static_cast<Uint8>(a * 0.5f));
    // Rounded top (slightly lighter)
    fillRect(renderer, x + 2, bodyY, w - 4, bodyH / 3, 60, 120, 50, a);

    // Wavy underside using sin wave
    for (int i = 0; i < w; i += 2) {
        float wave = std::sin(time * 3.0f + i * 0.3f) * 3.0f;
        int waveY = y + h - 2 + static_cast<int>(wave);
        fillRect(renderer, x + i, waveY, 2, 2, 40, 80, 35, a);
    }

    // Pulsing green core (glows rhythmically)
    float corePulse = 0.4f + 0.6f * std::sin(time * 2.5f);
    Uint8 coreR = static_cast<Uint8>(30 + 80 * corePulse);
    Uint8 coreG = static_cast<Uint8>(180 + 75 * corePulse);
    Uint8 coreB = static_cast<Uint8>(40 + 40 * corePulse);
    int coreSize = w / 4;
    fillRect(renderer, cx - coreSize / 2, bodyY + bodyH / 3, coreSize, coreSize,
             coreR, coreG, coreB, a);
    // Core glow halo
    fillRect(renderer, cx - coreSize, bodyY + bodyH / 4, coreSize * 2, coreSize + 4,
             coreR, coreG, coreB, static_cast<Uint8>(40 * corePulse * alpha));

    // Dripping green dots (animated particles falling from body)
    for (int i = 0; i < 3; i++) {
        float dripPhase = std::fmod(time * 1.5f + i * 1.3f, 2.0f);
        int dripX = x + w / 4 + i * (w / 4);
        int dripY = y + h + static_cast<int>(dripPhase * 8.0f);
        Uint8 dripA = static_cast<Uint8>(a * (1.0f - dripPhase / 2.0f));
        fillRect(renderer, dripX, dripY, 2, 2, 80, 200, 60, dripA);
    }

    // Red mouth opening when attacking
    bool attacking = false;
    if (entity.hasComponent<AIComponent>()) {
        auto aiSt = entity.getComponent<AIComponent>().state;
        attacking = (aiSt == AIState::Attack || aiSt == AIState::Chase);
    }
    int mouthX = flipped ? x + 1 : x + w - 7;
    int mouthY = bodyY + bodyH / 2;
    if (attacking) {
        // Wide open mouth (red maw)
        int mouthW = 6;
        int mouthH = 5;
        fillRect(renderer, mouthX, mouthY, mouthW, mouthH, 200, 40, 40, a);
        // Inner darkness
        fillRect(renderer, mouthX + 1, mouthY + 1, mouthW - 2, mouthH - 2, 80, 15, 15, a);
    } else {
        // Closed mouth (thin line)
        fillRect(renderer, mouthX, mouthY + 1, 5, 2, 160, 40, 40, a);
    }
}
