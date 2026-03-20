// RenderSystemEnemies.cpp -- Split from RenderSystem.cpp (regular enemy rendering)
#include "RenderSystem.h"
#include "Components/SpriteComponent.h"
#include "Components/HealthComponent.h"
#include "Components/AIComponent.h"
#include <cmath>
#include <algorithm>

void RenderSystem::renderWalker(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;

    // Animated legs
    float time = SDL_GetTicks() * 0.006f;
    int legOff = static_cast<int>(std::sin(time) * 2);
    fillRect(renderer, x + 2, y + h - 8 + legOff, w / 3, 8, 150, 40, 40, a);
    fillRect(renderer, x + w - w / 3 - 2, y + h - 8 - legOff, w / 3, 8, 150, 40, 40, a);

    // Bulky body
    fillRect(renderer, x, y + 4, w, h - 12, 200, 55, 55, a);
    fillRect(renderer, x + 3, y + h / 2, w - 6, h / 4, 160, 40, 40, a);

    // Head
    fillRect(renderer, x + 2, y, w - 4, h / 3, 220, 60, 60, a);

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
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.getPercent() < 1.0f) {
            fillRect(renderer, x, y - 6, w, 3, 40, 20, 20, a);
            fillRect(renderer, x, y - 6, static_cast<int>(w * hp.getPercent()), 3, 220, 50, 50, a);
        }
    }
}

void RenderSystem::renderFlyer(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    int cx = x + w / 2, cy = y + h / 2;

    float time = SDL_GetTicks() * 0.012f;
    int wingOff = static_cast<int>(std::sin(time) * 5);

    // Wings (animated flap)
    SDL_SetRenderDrawColor(renderer, 140, 60, 180, a);
    for (int i = 0; i < 3; i++) {
        SDL_RenderDrawLine(renderer, cx - 2, cy + i * 2, cx - w / 2 - 4, cy + wingOff - 4 + i);
        SDL_RenderDrawLine(renderer, cx + 2, cy + i * 2, cx + w / 2 + 4, cy - wingOff - 4 + i);
    }

    // Diamond body
    fillRect(renderer, cx - 4, cy - 6, 8, 12, 180, 70, 210, a);
    fillRect(renderer, cx - 6, cy - 3, 12, 6, 180, 70, 210, a);

    // Glowing eye
    fillRect(renderer, cx - 2, cy - 3, 4, 3, 255, 100, 255, a);

    // Tail
    drawLine(renderer, cx, cy + 6, cx + static_cast<int>(std::sin(time * 0.7f) * 3), cy + 14, 160, 50, 190, a);

    // HP bar
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.getPercent() < 1.0f) {
            fillRect(renderer, x, y - 6, w, 3, 40, 20, 40, a);
            fillRect(renderer, x, y - 6, static_cast<int>(w * hp.getPercent()), 3, 180, 70, 210, a);
        }
    }
}

void RenderSystem::renderTurret(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    bool flipped = entity.getComponent<SpriteComponent>().flipX;

    // Wide base
    fillRect(renderer, x - 2, y + h - 8, w + 4, 8, 140, 140, 40, a);
    fillRect(renderer, x, y + h - 12, w, 6, 160, 160, 50, a);

    // Box body
    fillRect(renderer, x + 2, y + 4, w - 4, h - 16, 200, 200, 55, a);
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

    // HP bar
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.getPercent() < 1.0f) {
            fillRect(renderer, x, y - 6, w, 3, 40, 40, 20, a);
            fillRect(renderer, x, y - 6, static_cast<int>(w * hp.getPercent()), 3, 200, 200, 50, a);
        }
    }
}

void RenderSystem::renderCharger(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;

    // Thick legs
    fillRect(renderer, x + 2, y + h - 8, w / 3, 8, 180, 90, 30, a);
    fillRect(renderer, x + w - w / 3 - 2, y + h - 8, w / 3, 8, 180, 90, 30, a);

    // Wide body
    fillRect(renderer, x - 2, y + 6, w + 4, h - 14, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Armor plate
    fillRect(renderer, x, y + 8, w, h / 3,
             static_cast<Uint8>(std::min(255, sprite.color.r + 20)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 10)),
             sprite.color.b, a);

    // Head
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
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.getPercent() < 1.0f) {
            fillRect(renderer, x, y - 6, w, 3, 40, 30, 10, a);
            fillRect(renderer, x, y - 6, static_cast<int>(w * hp.getPercent()), 3, 220, 120, 40, a);
        }
    }
}

void RenderSystem::renderPhaser(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(200 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();

    float time = SDL_GetTicks() * 0.005f;
    int shimmer = static_cast<int>(std::sin(time) * 3);

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
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.getPercent() < 1.0f) {
            fillRect(renderer, x, y - 6, w, 3, 30, 20, 40, a);
            fillRect(renderer, x, y - 6, static_cast<int>(w * hp.getPercent()), 3, 100, 50, 200, a);
        }
    }
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
    float pulseSpeed = 1.0f + (1.0f - hpPct) * 4.0f;
    float pulse = 0.7f + 0.3f * std::sin(time * pulseSpeed);

    Uint8 bodyR = static_cast<Uint8>(255 * pulse);
    Uint8 bodyG = static_cast<Uint8>(60 * pulse);
    fillRect(renderer, x, y + 2, w, h - 4, bodyR, bodyG, 20, a);

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
    if (hpPct < 1.0f) {
        fillRect(renderer, x, y - 6, w, 3, 40, 20, 10, a);
        fillRect(renderer, x, y - 6, static_cast<int>(w * hpPct), 3, 255, 80, 30, a);
    }
}

void RenderSystem::renderShielder(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;

    // Legs
    fillRect(renderer, x + 3, y + h - 8, w / 3, 8, 50, 120, 160, a);
    fillRect(renderer, x + w - w / 3 - 3, y + h - 8, w / 3, 8, 50, 120, 160, a);

    // Body (armored)
    fillRect(renderer, x + 2, y + 6, w - 4, h - 14, 70, 160, 200, a);
    // Armor highlight
    fillRect(renderer, x + 3, y + 7, (w - 6) / 2, h / 3, 100, 190, 230, static_cast<Uint8>(a * 0.6f));

    // Head with helmet
    fillRect(renderer, x + 4, y, w - 8, h / 3, 60, 140, 180, a);
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
    } else {
        fillRect(renderer, x - shieldW, y + 2, shieldW, shieldH, 120, 200, 255, shieldAlpha);
        fillRect(renderer, x - shieldW + 1, y + 4, shieldW - 2, shieldH - 4, 180, 230, 255, static_cast<Uint8>(shieldAlpha * 0.6f));
        drawLine(renderer, x - shieldW, y + 2, x - shieldW, y + 2 + shieldH, 200, 240, 255, static_cast<Uint8>(shieldAlpha * 0.4f));
    }

    // HP bar
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.getPercent() < 1.0f) {
            fillRect(renderer, x, y - 6, w, 3, 20, 30, 40, a);
            fillRect(renderer, x, y - 6, static_cast<int>(w * hp.getPercent()), 3, 80, 180, 220, a);
        }
    }
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

    // Flat, wide body (like a bug)
    fillRect(renderer, x + 2, y + 2, w - 4, h - 4, sprite.color.r, sprite.color.g, sprite.color.b, a);

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

    // Robed body (trapezoid shape)
    int robeTop = w / 2;
    int robeBot = w;
    fillRect(renderer, x + (w - robeTop) / 2, y + h / 4, robeTop, h / 4,
             sprite.color.r, sprite.color.g, sprite.color.b, a);
    fillRect(renderer, x, y + h / 2, robeBot, h / 2,
             static_cast<Uint8>(sprite.color.r * 0.7f),
             static_cast<Uint8>(sprite.color.g * 0.7f),
             static_cast<Uint8>(sprite.color.b * 0.7f), a);

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

    // Thin body
    fillRect(renderer, x + 4, y + h / 5, w - 8, h * 3 / 5,
             sprite.color.r, sprite.color.g, sprite.color.b, a);

    // Head (small)
    int headW = w / 2;
    int headH = h / 5;
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

    // Telegraph laser line
    if (telegraphing) {
        Uint8 laserA = static_cast<Uint8>(80 + 80 * std::sin(time * 15.0f));
        int laserX = flipped ? x - 400 : x + w;
        int laserW = 400;
        fillRect(renderer, flipped ? laserX : laserX, rifleY, laserW, 1, 255, 50, 50, laserA);
    }
}
