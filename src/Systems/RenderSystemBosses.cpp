// RenderSystemBosses.cpp -- Split from RenderSystem.cpp (boss rendering)
#include "RenderSystem.h"
#include "Components/SpriteComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/AIComponent.h"
#include <cmath>
#include <algorithm>

void RenderSystem::renderBoss(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;
    float time = SDL_GetTicks() * 0.004f;

    int bossPhase = 1;
    if (entity.hasComponent<AIComponent>()) {
        bossPhase = entity.getComponent<AIComponent>().bossPhase;
    }

    // Aura glow (grows with phase)
    int auraSize = 4 + bossPhase * 3;
    Uint8 auraA = static_cast<Uint8>(a * (0.15f + 0.1f * bossPhase));
    float auraPulse = 0.7f + 0.3f * std::sin(time * bossPhase);
    fillRect(renderer, x - auraSize, y - auraSize,
             w + auraSize * 2, h + auraSize * 2,
             sprite.color.r, sprite.color.g, sprite.color.b,
             static_cast<Uint8>(auraA * auraPulse));

    // Thick legs
    int legH = h / 5;
    int legW = w / 3;
    float legAnim = std::sin(time * 2.0f) * 3.0f;
    fillRect(renderer, x + 4, y + h - legH + static_cast<int>(legAnim),
             legW, legH, 100, 30, 90, a);
    fillRect(renderer, x + w - legW - 4, y + h - legH - static_cast<int>(legAnim),
             legW, legH, 100, 30, 90, a);

    // Drop shadow (wide for boss)
    fillRect(renderer, x + 4, y + h, w - 8, 4, 0, 0, 0, static_cast<Uint8>(a * 0.35f));
    fillRect(renderer, x + 8, y + h + 3, w - 16, 2, 0, 0, 0, static_cast<Uint8>(a * 0.15f));

    // Pulsing glow outline (intensifies with phase)
    float glowPulse = 0.5f + 0.5f * std::sin(time * 2.5f * bossPhase);
    Uint8 glowOutA = static_cast<Uint8>((60 + 40 * bossPhase) * glowPulse * alpha);
    SDL_Rect glowOutline = {x - 3, y - 3, w + 6, h + 6};
    SDL_SetRenderDrawColor(renderer, sprite.color.r, sprite.color.g, sprite.color.b, glowOutA);
    SDL_RenderDrawRect(renderer, &glowOutline);
    SDL_Rect glowOutline2 = {x - 2, y - 2, w + 4, h + 4};
    SDL_SetRenderDrawColor(renderer,
        static_cast<Uint8>(std::min(255, sprite.color.r + 60)),
        static_cast<Uint8>(std::min(255, sprite.color.g + 40)),
        static_cast<Uint8>(std::min(255, sprite.color.b + 40)),
        static_cast<Uint8>(glowOutA * 0.6f));
    SDL_RenderDrawRect(renderer, &glowOutline2);

    // Massive body
    int bodyH = h * 3 / 5;
    int bodyY = y + h / 5;
    // Body outline
    fillRect(renderer, x - 1, bodyY - 1, w + 2, bodyH + 2,
             static_cast<Uint8>(sprite.color.r * 0.3f),
             static_cast<Uint8>(sprite.color.g * 0.3f),
             static_cast<Uint8>(sprite.color.b * 0.3f), a);
    fillRect(renderer, x, bodyY, w, bodyH, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Body top highlight
    fillRect(renderer, x + 1, bodyY + 1, w - 2, 3,
             static_cast<Uint8>(std::min(255, sprite.color.r + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 50)),
             static_cast<Uint8>(a * 0.6f));

    // Armor plates
    fillRect(renderer, x + 2, bodyY + 4, w - 4, bodyH / 3,
             static_cast<Uint8>(std::min(255, sprite.color.r + 30)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 20)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 20)),
             static_cast<Uint8>(a * 0.7f));
    // Body bottom darkening
    fillRect(renderer, x + 1, bodyY + bodyH - 4, w - 2, 3,
             static_cast<Uint8>(sprite.color.r * 0.5f),
             static_cast<Uint8>(sprite.color.g * 0.5f),
             static_cast<Uint8>(sprite.color.b * 0.5f),
             static_cast<Uint8>(a * 0.5f));

    // Rift core in chest (glowing)
    int coreX = x + w / 2;
    int coreY = bodyY + bodyH / 2;
    int coreR = 5 + bossPhase;
    float corePulse = 0.5f + 0.5f * std::sin(time * 3.0f);
    Uint8 coreA = static_cast<Uint8>(200 + 55 * corePulse);
    fillRect(renderer, coreX - coreR, coreY - coreR, coreR * 2, coreR * 2, 255, 200, 255, coreA);
    fillRect(renderer, coreX - coreR + 2, coreY - coreR + 2, coreR * 2 - 4, coreR * 2 - 4,
             255, 255, 255, static_cast<Uint8>(coreA * 0.6f));

    // Head with crown/horns
    int headH = h / 4;
    int headW = w * 2 / 3;
    int headX = x + (w - headW) / 2;
    // Head outline
    fillRect(renderer, headX - 1, y - 1, headW + 2, headH + 2,
             static_cast<Uint8>(sprite.color.r * 0.3f),
             static_cast<Uint8>(sprite.color.g * 0.3f),
             static_cast<Uint8>(sprite.color.b * 0.3f), a);
    fillRect(renderer, headX, y, headW, headH, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Head top highlight
    fillRect(renderer, headX + 1, y + 1, headW - 2, 2,
             static_cast<Uint8>(std::min(255, sprite.color.r + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 50)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 50)),
             static_cast<Uint8>(a * 0.5f));

    // Crown spikes
    int spikeH = 8 + bossPhase * 2;
    for (int i = 0; i < 3; i++) {
        int sx = headX + headW / 4 + i * (headW / 4);
        drawLine(renderer, sx, y, sx, y - spikeH, 255, 200, 100, a);
        drawLine(renderer, sx - 1, y, sx - 1, y - spikeH + 2, 255, 180, 80, static_cast<Uint8>(a * 0.6f));
    }

    // Eyes (glow more intensely per phase)
    int eyeY = y + headH / 3;
    Uint8 eyeGlow = static_cast<Uint8>(200 + 55 * std::sin(time * 2.0f));
    int eyeW = 5, eyeH = 4;
    fillRect(renderer, headX + headW / 4, eyeY, eyeW, eyeH, eyeGlow, 50, 50, a);
    fillRect(renderer, headX + 3 * headW / 4 - eyeW, eyeY, eyeW, eyeH, eyeGlow, 50, 50, a);

    // Arms/weapons
    int armY = bodyY + bodyH / 3;
    int armLen = w / 2 + 4;
    bool attacking = false;
    if (entity.hasComponent<CombatComponent>()) {
        attacking = entity.getComponent<CombatComponent>().isAttacking;
    }
    if (attacking) armLen += 10;

    if (!flipped) {
        fillRect(renderer, x + w, armY, armLen, 6, sprite.color.r, sprite.color.g, sprite.color.b, a);
        // Weapon tip
        fillRect(renderer, x + w + armLen - 4, armY - 4, 6, 14, 255, 150, 200, a);
    } else {
        fillRect(renderer, x - armLen, armY, armLen, 6, sprite.color.r, sprite.color.g, sprite.color.b, a);
        fillRect(renderer, x - armLen - 2, armY - 4, 6, 14, 255, 150, 200, a);
    }

    // Phase 2+: energy wisps
    if (bossPhase >= 2) {
        for (int i = 0; i < bossPhase * 2; i++) {
            float angle = time * 1.5f + i * (6.283185f / (bossPhase * 2));
            int wx = x + w / 2 + static_cast<int>(std::cos(angle) * (w / 2 + 10));
            int wy = y + h / 2 + static_cast<int>(std::sin(angle) * (h / 2 + 5));
            Uint8 wa = static_cast<Uint8>(a * 0.5f);
            fillRect(renderer, wx - 2, wy - 2, 4, 4, 255, 100, 200, wa);
        }
    }

    // Shield burst visual (spinning energy ring when invulnerable)
    if (entity.hasComponent<HealthComponent>() && entity.hasComponent<AIComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        auto& bossAI = entity.getComponent<AIComponent>();
        if (hp.invulnerable && bossAI.bossShieldActiveTimer > 0) {
            float shieldPulse = 0.5f + 0.5f * std::sin(time * 8.0f);
            Uint8 sa = static_cast<Uint8>(a * (0.3f + 0.3f * shieldPulse));
            for (int i = 0; i < 12; i++) {
                float angle = i * (6.283185f / 12) + time * 3.0f;
                int sx = x + w / 2 + static_cast<int>(std::cos(angle) * (w / 2 + 16));
                int sy = y + h / 2 + static_cast<int>(std::sin(angle) * (h / 2 + 12));
                fillRect(renderer, sx - 3, sy - 3, 6, 6, 100, 200, 255, sa);
            }
            // Inner glow
            fillRect(renderer, x - 2, y - 2, w + 4, h + 4, 80, 180, 255, static_cast<Uint8>(a * 0.15f * shieldPulse));
        }
    }

    // HP bar (large, above boss)
    renderEnemyHPBar(renderer, x, y, w, entity, alpha, true);
}

void RenderSystem::renderVoidWyrm(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;
    float time = SDL_GetTicks() * 0.004f;

    int bossPhase = 1;
    bool diving = false;
    if (entity.hasComponent<AIComponent>()) {
        auto& ai = entity.getComponent<AIComponent>();
        bossPhase = ai.bossPhase;
        diving = ai.wyrmDiving;
    }

    // Poison mist aura (larger than Rift Guardian's)
    int auraSize = 6 + bossPhase * 4;
    Uint8 auraA = static_cast<Uint8>(a * (0.1f + 0.08f * bossPhase));
    float auraPulse = 0.6f + 0.4f * std::sin(time * 1.5f * bossPhase);
    fillRect(renderer, x - auraSize, y - auraSize,
             w + auraSize * 2, h + auraSize * 2,
             60, 220, 100, static_cast<Uint8>(auraA * auraPulse));

    // Serpentine body segments (3 trailing segments)
    for (int seg = 2; seg >= 0; seg--) {
        float segOffset = (seg + 1) * 12.0f;
        float segAngle = time * 2.0f + seg * 0.8f;
        int sx = x + w / 2 + static_cast<int>(std::sin(segAngle) * 6.0f) - (flipped ? -1 : 1) * static_cast<int>(segOffset);
        int sy = y + h / 2 + static_cast<int>(std::cos(segAngle * 0.7f) * 4.0f);
        int segW = w / 3 - seg * 2;
        int segH = h / 3 - seg * 2;
        Uint8 segA = static_cast<Uint8>(a * (0.7f - seg * 0.15f));
        fillRect(renderer, sx - segW / 2, sy - segH / 2, segW, segH,
                 static_cast<Uint8>(sprite.color.r * 0.8f),
                 static_cast<Uint8>(sprite.color.g * 0.8f),
                 static_cast<Uint8>(sprite.color.b * 0.8f), segA);
    }

    // Main body (elongated oval shape via stacked rects)
    int bodyW = w * 3 / 4;
    int bodyH = h * 3 / 5;
    int bodyX = x + (w - bodyW) / 2;
    int bodyY = y + h / 4;
    fillRect(renderer, bodyX, bodyY, bodyW, bodyH, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Belly stripe
    fillRect(renderer, bodyX + 3, bodyY + bodyH / 2, bodyW - 6, bodyH / 3,
             static_cast<Uint8>(std::min(255, sprite.color.r + 40)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 30)),
             static_cast<Uint8>(sprite.color.b * 0.7f),
             static_cast<Uint8>(a * 0.6f));

    // Venom core (chest glow)
    int coreX = x + w / 2;
    int coreY = bodyY + bodyH / 2;
    int coreR = 4 + bossPhase;
    float corePulse = 0.5f + 0.5f * std::sin(time * 4.0f);
    Uint8 coreA = static_cast<Uint8>(180 + 75 * corePulse);
    fillRect(renderer, coreX - coreR, coreY - coreR, coreR * 2, coreR * 2, 120, 255, 80, coreA);
    fillRect(renderer, coreX - coreR + 2, coreY - coreR + 2, coreR * 2 - 4, coreR * 2 - 4,
             200, 255, 200, static_cast<Uint8>(coreA * 0.5f));

    // Head (angular, serpent-like)
    int headH = h / 3;
    int headW = w * 3 / 5;
    int headX = x + (w - headW) / 2;
    fillRect(renderer, headX, y, headW, headH, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Fangs
    int fangH = 6 + bossPhase;
    if (!flipped) {
        fillRect(renderer, headX + headW - 3, y + headH - 2, 2, fangH, 220, 255, 180, a);
        fillRect(renderer, headX + headW - 8, y + headH - 2, 2, fangH - 2, 220, 255, 180, a);
    } else {
        fillRect(renderer, headX + 1, y + headH - 2, 2, fangH, 220, 255, 180, a);
        fillRect(renderer, headX + 6, y + headH - 2, 2, fangH - 2, 220, 255, 180, a);
    }

    // Eyes (slit-like, green glow)
    int eyeY = y + headH / 3;
    Uint8 eyeGlow = static_cast<Uint8>(200 + 55 * std::sin(time * 3.0f));
    int eyeW = 4, eyeH = 5;
    fillRect(renderer, headX + headW / 4, eyeY, eyeW, eyeH, eyeGlow, 255, 80, a);
    fillRect(renderer, headX + 3 * headW / 4 - eyeW, eyeY, eyeW, eyeH, eyeGlow, 255, 80, a);
    // Slit pupils
    fillRect(renderer, headX + headW / 4 + 1, eyeY + 1, 2, eyeH - 2, 20, 60, 20, a);
    fillRect(renderer, headX + 3 * headW / 4 - eyeW + 1, eyeY + 1, 2, eyeH - 2, 20, 60, 20, a);

    // Wings (small, vestigial)
    float wingFlap = std::sin(time * 4.0f) * 5.0f;
    int wingY = bodyY + 2;
    int wingW = 14 + bossPhase * 2;
    int wingH = 8 + static_cast<int>(wingFlap);
    fillRect(renderer, x - wingW + 4, wingY, wingW, wingH,
             static_cast<Uint8>(sprite.color.r * 0.6f), static_cast<Uint8>(sprite.color.g * 0.7f),
             sprite.color.b, static_cast<Uint8>(a * 0.7f));
    fillRect(renderer, x + w - 4, wingY, wingW, wingH,
             static_cast<Uint8>(sprite.color.r * 0.6f), static_cast<Uint8>(sprite.color.g * 0.7f),
             sprite.color.b, static_cast<Uint8>(a * 0.7f));

    // Dive trail
    if (diving) {
        for (int i = 0; i < 5; i++) {
            int tx = x + w / 2 + static_cast<int>(std::sin(time * 3.0f + i) * 8.0f);
            int ty = y - 10 * (i + 1);
            Uint8 ta = static_cast<Uint8>(a * (0.4f - i * 0.07f));
            fillRect(renderer, tx - 3, ty, 6, 6, 80, 255, 120, ta);
        }
    }

    // Phase 2+: orbiting poison orbs
    if (bossPhase >= 2) {
        for (int i = 0; i < bossPhase * 2; i++) {
            float angle = time * 2.0f + i * (6.283185f / (bossPhase * 2));
            int ox = x + w / 2 + static_cast<int>(std::cos(angle) * (w / 2 + 14));
            int oy = y + h / 2 + static_cast<int>(std::sin(angle) * (h / 2 + 8));
            Uint8 oa = static_cast<Uint8>(a * 0.6f);
            fillRect(renderer, ox - 3, oy - 3, 6, 6, 100, 255, 60, oa);
        }
    }

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha, true);
}

void RenderSystem::renderDimensionalArchitect(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    float time = SDL_GetTicks() * 0.004f;

    int bossPhase = 1;
    float beamAngle = 0;
    bool constructing = false;
    if (entity.hasComponent<AIComponent>()) {
        auto& ai = entity.getComponent<AIComponent>();
        bossPhase = ai.bossPhase;
        beamAngle = ai.archBeamAngle;
        constructing = ai.archConstructing;
    }

    // Geometric grid aura
    int auraSize = 10 + bossPhase * 6;
    Uint8 auraA = static_cast<Uint8>(a * (0.08f + 0.05f * bossPhase));
    float auraPulse = 0.5f + 0.5f * std::sin(time * 1.2f);
    for (int i = -auraSize; i <= auraSize; i += 8) {
        drawLine(renderer, x + w / 2 + i, y - auraSize, x + w / 2 + i, y + h + auraSize,
                 100, 150, 255, static_cast<Uint8>(auraA * auraPulse));
        drawLine(renderer, x - auraSize, y + h / 2 + i, x + w + auraSize, y + h / 2 + i,
                 100, 150, 255, static_cast<Uint8>(auraA * auraPulse));
    }

    // 4 orbital plates
    for (int i = 0; i < 4; i++) {
        float angle = beamAngle + i * (3.14159f / 2.0f);
        int radius = w / 2 + 12 + bossPhase * 4;
        int px = x + w / 2 + static_cast<int>(std::cos(angle) * radius);
        int py = y + h / 2 + static_cast<int>(std::sin(angle) * radius * 0.6f);
        int plateW = 10 + bossPhase;
        int plateH = 6 + bossPhase;
        Uint8 plateA = static_cast<Uint8>(a * 0.8f);
        fillRect(renderer, px - plateW / 2, py - plateH / 2, plateW, plateH,
                 static_cast<Uint8>(sprite.color.r * 0.7f),
                 static_cast<Uint8>(sprite.color.g * 0.7f),
                 sprite.color.b, plateA);
        // Plate inner glow
        fillRect(renderer, px - plateW / 4, py - plateH / 4, plateW / 2, plateH / 2,
                 180, 220, 255, static_cast<Uint8>(plateA * 0.6f));
    }

    // Two floating hands
    float handBob = std::sin(time * 2.0f) * 4.0f;
    int handSize = 8 + bossPhase;
    // Left hand
    int lhx = x - 14 - bossPhase * 2;
    int lhy = y + h / 2 + static_cast<int>(handBob);
    fillRect(renderer, lhx, lhy, handSize, handSize + 2,
             sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Fingers
    fillRect(renderer, lhx - 2, lhy - 3, 3, 4, sprite.color.r, sprite.color.g, 255, a);
    fillRect(renderer, lhx + handSize - 1, lhy - 3, 3, 4, sprite.color.r, sprite.color.g, 255, a);
    // Right hand
    int rhx = x + w + 6 + bossPhase * 2;
    int rhy = y + h / 2 - static_cast<int>(handBob);
    fillRect(renderer, rhx, rhy, handSize, handSize + 2,
             sprite.color.r, sprite.color.g, sprite.color.b, a);
    fillRect(renderer, rhx - 2, rhy - 3, 3, 4, sprite.color.r, sprite.color.g, 255, a);
    fillRect(renderer, rhx + handSize - 1, rhy - 3, 3, 4, sprite.color.r, sprite.color.g, 255, a);

    // Constructing effect: energy lines between hands
    if (constructing) {
        for (int i = 0; i < 3; i++) {
            float off = std::sin(time * 5.0f + i * 1.5f) * 6.0f;
            drawLine(renderer, lhx + handSize / 2, lhy + handSize / 2,
                     rhx + handSize / 2, rhy + handSize / 2 + static_cast<int>(off),
                     200, 150, 255, static_cast<Uint8>(a * (0.5f + 0.2f * i)));
        }
    }

    // Main body: cube-like core
    int coreW = w * 2 / 3;
    int coreH = h * 2 / 3;
    int coreX = x + (w - coreW) / 2;
    int coreY = y + (h - coreH) / 2;
    // Outer shell
    fillRect(renderer, coreX, coreY, coreW, coreH, sprite.color.r, sprite.color.g, sprite.color.b, a);
    // Inner lighter cube
    fillRect(renderer, coreX + 4, coreY + 4, coreW - 8, coreH - 8,
             static_cast<Uint8>(std::min(255, sprite.color.r + 60)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 40)),
             255, static_cast<Uint8>(a * 0.7f));
    // Rotating inner diamond
    float diamondAngle = time * 1.5f;
    int dSize = 6 + bossPhase * 2;
    int dcx = coreX + coreW / 2;
    int dcy = coreY + coreH / 2;
    int dx0 = dcx + static_cast<int>(std::cos(diamondAngle) * dSize);
    int dy0 = dcy + static_cast<int>(std::sin(diamondAngle) * dSize);
    int dx1 = dcx + static_cast<int>(std::cos(diamondAngle + 1.57f) * dSize);
    int dy1 = dcy + static_cast<int>(std::sin(diamondAngle + 1.57f) * dSize);
    int dx2 = dcx + static_cast<int>(std::cos(diamondAngle + 3.14f) * dSize);
    int dy2 = dcy + static_cast<int>(std::sin(diamondAngle + 3.14f) * dSize);
    int dx3 = dcx + static_cast<int>(std::cos(diamondAngle + 4.71f) * dSize);
    int dy3 = dcy + static_cast<int>(std::sin(diamondAngle + 4.71f) * dSize);
    Uint8 dAlpha = static_cast<Uint8>(a * 0.9f);
    drawLine(renderer, dx0, dy0, dx1, dy1, 220, 200, 255, dAlpha);
    drawLine(renderer, dx1, dy1, dx2, dy2, 220, 200, 255, dAlpha);
    drawLine(renderer, dx2, dy2, dx3, dy3, 220, 200, 255, dAlpha);
    drawLine(renderer, dx3, dy3, dx0, dy0, 220, 200, 255, dAlpha);

    // Big eye in center
    int eyeR = 5 + bossPhase;
    float eyePulse = 0.6f + 0.4f * std::sin(time * 3.0f);
    // Eye white
    fillRect(renderer, dcx - eyeR, dcy - eyeR / 2, eyeR * 2, eyeR,
             220, 220, 255, static_cast<Uint8>(a * eyePulse));
    // Pupil
    int pupilR = eyeR / 2;
    fillRect(renderer, dcx - pupilR, dcy - pupilR / 2, pupilR * 2, pupilR,
             40, 20, 80, a);
    // Eye glow
    fillRect(renderer, dcx - 1, dcy - 1, 3, 2,
             255, 200, 255, static_cast<Uint8>(200 + 55 * eyePulse));

    // Phase 2+: rift energy sparks
    if (bossPhase >= 2) {
        for (int i = 0; i < bossPhase * 3; i++) {
            float sparkAngle = time * 3.0f + i * (6.283185f / (bossPhase * 3));
            int sr = w / 2 + 20 + static_cast<int>(std::sin(time * 2.0f + i) * 8.0f);
            int sx = x + w / 2 + static_cast<int>(std::cos(sparkAngle) * sr);
            int sy = y + h / 2 + static_cast<int>(std::sin(sparkAngle) * sr);
            fillRect(renderer, sx - 2, sy - 2, 4, 4,
                     static_cast<Uint8>(180 + 75 * std::sin(time + i)), 100, 255,
                     static_cast<Uint8>(a * 0.5f));
        }
    }

    // Phase 3: collapse warning ring
    if (bossPhase >= 3) {
        float ringPulse = (std::sin(time * 4.0f) + 1.0f) * 0.5f;
        int ringR = w + 10 + static_cast<int>(ringPulse * 20.0f);
        Uint8 ringA = static_cast<Uint8>(a * 0.15f * ringPulse);
        for (int angle = 0; angle < 360; angle += 10) {
            float rad = angle * 3.14159f / 180.0f;
            int rx = x + w / 2 + static_cast<int>(std::cos(rad) * ringR);
            int ry = y + h / 2 + static_cast<int>(std::sin(rad) * ringR);
            fillRect(renderer, rx - 1, ry - 1, 3, 3, 255, 60, 180, ringA);
        }
    }

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha, true);
}

void RenderSystem::renderTemporalWeaver(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    float time = SDL_GetTicks() * 0.003f;

    int bossPhase = 1;
    if (entity.hasComponent<AIComponent>()) {
        bossPhase = entity.getComponent<AIComponent>().bossPhase;
    }

    // Temporal distortion aura - sandy time particles
    int auraR = w / 2 + 14 + bossPhase * 6;
    Uint8 auraA = static_cast<Uint8>(a * (0.06f + 0.03f * bossPhase));
    float auraPulse = 0.5f + 0.5f * std::sin(time * 0.8f);
    for (int i = 0; i < 12 + bossPhase * 4; i++) {
        float angle = time * 0.5f + i * (6.283185f / (12 + bossPhase * 4));
        float r2 = auraR + std::sin(time * 2.0f + i * 0.7f) * 6.0f;
        int px = x + w / 2 + static_cast<int>(std::cos(angle) * r2);
        int py = y + h / 2 + static_cast<int>(std::sin(angle) * r2);
        fillRect(renderer, px - 1, py - 1, 3, 3,
                 220, 190, 120, static_cast<Uint8>(auraA * auraPulse));
    }

    // Outer gear ring (large rotating clockwork)
    int gearR = w / 2 + 8;
    int gearTeeth = 12;
    float gearAngle = time * 0.7f;
    for (int i = 0; i < gearTeeth; i++) {
        float tAngle = gearAngle + i * (6.283185f / gearTeeth);
        int innerR = gearR - 3;
        int outerR = gearR + 3;
        int ix = x + w / 2 + static_cast<int>(std::cos(tAngle) * innerR);
        int iy = y + h / 2 + static_cast<int>(std::sin(tAngle) * innerR);
        int ox = x + w / 2 + static_cast<int>(std::cos(tAngle) * outerR);
        int oy = y + h / 2 + static_cast<int>(std::sin(tAngle) * outerR);
        drawLine(renderer, ix, iy, ox, oy, 200, 170, 80, static_cast<Uint8>(a * 0.7f));
        float nextAngle = tAngle + (6.283185f / gearTeeth) * 0.3f;
        int nx = x + w / 2 + static_cast<int>(std::cos(nextAngle) * outerR);
        int ny = y + h / 2 + static_cast<int>(std::sin(nextAngle) * outerR);
        drawLine(renderer, ox, oy, nx, ny, 220, 190, 100, static_cast<Uint8>(a * 0.6f));
    }
    // Gear ring circle
    for (int i = 0; i < 36; i++) {
        float cAngle = i * (6.283185f / 36);
        int cx1 = x + w / 2 + static_cast<int>(std::cos(cAngle) * (gearR - 2));
        int cy1 = y + h / 2 + static_cast<int>(std::sin(cAngle) * (gearR - 2));
        float cAngle2 = (i + 1) * (6.283185f / 36);
        int cx2 = x + w / 2 + static_cast<int>(std::cos(cAngle2) * (gearR - 2));
        int cy2 = y + h / 2 + static_cast<int>(std::sin(cAngle2) * (gearR - 2));
        drawLine(renderer, cx1, cy1, cx2, cy2, 180, 160, 80, static_cast<Uint8>(a * 0.5f));
    }

    // Inner smaller gear (counter-rotating)
    int innerGearR = w / 4 + 2;
    int innerTeeth = 8;
    float innerAngle = -time * 1.2f;
    for (int i = 0; i < innerTeeth; i++) {
        float tAngle = innerAngle + i * (6.283185f / innerTeeth);
        int ir = innerGearR - 2;
        int outerR2 = innerGearR + 2;
        int ix2 = x + w / 2 + static_cast<int>(std::cos(tAngle) * ir);
        int iy2 = y + h / 2 + static_cast<int>(std::sin(tAngle) * ir);
        int ox2 = x + w / 2 + static_cast<int>(std::cos(tAngle) * outerR2);
        int oy2 = y + h / 2 + static_cast<int>(std::sin(tAngle) * outerR2);
        drawLine(renderer, ix2, iy2, ox2, oy2, 230, 200, 100, static_cast<Uint8>(a * 0.8f));
    }

    // Main body: clock face
    int coreR = w / 3;
    int coreX = x + w / 2;
    int coreY = y + h / 2;
    for (int dy2 = -coreR; dy2 <= coreR; dy2++) {
        int halfW = static_cast<int>(std::sqrt(static_cast<float>(coreR * coreR - dy2 * dy2)));
        fillRect(renderer, coreX - halfW, coreY + dy2, halfW * 2, 1,
                 sprite.color.r, sprite.color.g, sprite.color.b, a);
    }
    // Clock face rim
    for (int ca = 0; ca < 36; ca++) {
        float cAngle3 = ca * (6.283185f / 36);
        int rx2 = coreX + static_cast<int>(std::cos(cAngle3) * coreR);
        int ry2 = coreY + static_cast<int>(std::sin(cAngle3) * coreR);
        float cAngle4 = (ca + 1) * (6.283185f / 36);
        int rx3 = coreX + static_cast<int>(std::cos(cAngle4) * coreR);
        int ry3 = coreY + static_cast<int>(std::sin(cAngle4) * coreR);
        drawLine(renderer, rx2, ry2, rx3, ry3, 255, 220, 130, a);
    }

    // Hour markers (12 marks)
    for (int i = 0; i < 12; i++) {
        float mAngle = i * (6.283185f / 12);
        int m1 = coreR - 3;
        int m2 = coreR - 1;
        int mx1 = coreX + static_cast<int>(std::cos(mAngle) * m1);
        int my1 = coreY + static_cast<int>(std::sin(mAngle) * m1);
        int mx2 = coreX + static_cast<int>(std::cos(mAngle) * m2);
        int my2 = coreY + static_cast<int>(std::sin(mAngle) * m2);
        drawLine(renderer, mx1, my1, mx2, my2, 255, 240, 180, a);
    }

    // Clock hands
    float hourAngle = time * 0.3f;
    int hourLen = coreR - 6;
    int hx2 = coreX + static_cast<int>(std::cos(hourAngle) * hourLen);
    int hy2 = coreY + static_cast<int>(std::sin(hourAngle) * hourLen);
    drawLine(renderer, coreX, coreY, hx2, hy2, 255, 220, 100, a);
    drawLine(renderer, coreX + 1, coreY, hx2 + 1, hy2, 255, 220, 100, a);

    float minuteAngle = time * 1.5f;
    int minuteLen = coreR - 3;
    int mhx = coreX + static_cast<int>(std::cos(minuteAngle) * minuteLen);
    int mhy = coreY + static_cast<int>(std::sin(minuteAngle) * minuteLen);
    drawLine(renderer, coreX, coreY, mhx, mhy, 255, 240, 160, static_cast<Uint8>(a * 0.9f));

    // Center hub
    fillRect(renderer, coreX - 2, coreY - 2, 4, 4, 255, 230, 120, a);

    // Pendulum arms (swinging below body)
    for (int p = 0; p < 2; p++) {
        float pendAngle = std::sin(time * 2.5f + p * 3.14159f) * 0.6f;
        int pendLen = 16 + bossPhase * 3;
        int pendBaseX = x + w / 4 + p * w / 2;
        int pendBaseY = y + h - 2;
        int pendEndX = pendBaseX + static_cast<int>(std::sin(pendAngle) * pendLen);
        int pendEndY = pendBaseY + static_cast<int>(std::cos(pendAngle) * pendLen);
        drawLine(renderer, pendBaseX, pendBaseY, pendEndX, pendEndY,
                 180, 160, 80, static_cast<Uint8>(a * 0.8f));
        fillRect(renderer, pendEndX - 3, pendEndY - 3, 6, 6, 220, 190, 80, a);
        fillRect(renderer, pendEndX - 2, pendEndY - 2, 4, 4, 255, 220, 120, a);
    }

    // Phase 2+: temporal echoes (shadow clones)
    if (bossPhase >= 2) {
        for (int c = 0; c < bossPhase - 1; c++) {
            float echoAngle = time * 0.4f + c * (6.283185f / std::max(1, bossPhase - 1));
            int echoOff = 18 + c * 8;
            int ex = x + static_cast<int>(std::cos(echoAngle) * echoOff);
            int ey = y + static_cast<int>(std::sin(echoAngle) * echoOff * 0.5f);
            Uint8 echoA = static_cast<Uint8>(a * 0.25f);
            fillRect(renderer, ex + w / 3, ey + h / 3, w / 3, h / 3,
                     sprite.color.r, sprite.color.g, sprite.color.b, echoA);
            for (int t = 0; t < 6; t++) {
                float tA = echoAngle + t * (6.283185f / 6);
                int etx = ex + w / 2 + static_cast<int>(std::cos(tA) * (w / 4));
                int ety = ey + h / 2 + static_cast<int>(std::sin(tA) * (w / 4));
                fillRect(renderer, etx - 1, ety - 1, 2, 2, 200, 180, 80, echoA);
            }
        }
    }

    // Phase 3: time distortion rings
    if (bossPhase >= 3) {
        for (int ring = 0; ring < 3; ring++) {
            float ringTime = std::fmod(time * 1.5f + ring * 0.7f, 3.0f);
            float ringR2 = w * 0.5f + ringTime * 25.0f;
            Uint8 ringA = static_cast<Uint8>(a * 0.2f * (1.0f - ringTime / 3.0f));
            for (int seg = 0; seg < 24; seg++) {
                float segAngle = seg * (6.283185f / 24);
                float nextSeg = (seg + 1) * (6.283185f / 24);
                int sx1 = coreX + static_cast<int>(std::cos(segAngle) * ringR2);
                int sy1 = coreY + static_cast<int>(std::sin(segAngle) * ringR2);
                int sx2 = coreX + static_cast<int>(std::cos(nextSeg) * ringR2);
                int sy2 = coreY + static_cast<int>(std::sin(nextSeg) * ringR2);
                drawLine(renderer, sx1, sy1, sx2, sy2, 255, 200, 80, ringA);
            }
        }
    }

    // HP bar
    renderEnemyHPBar(renderer, x, y, w, entity, alpha, true);
}

void RenderSystem::renderVoidSovereign(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int cx = x + w / 2, cy = y + h / 2;

    auto& ai = entity.getComponent<AIComponent>();
    float hpPercent = 1.0f;
    if (entity.hasComponent<HealthComponent>())
        hpPercent = entity.getComponent<HealthComponent>().getPercent();

    int phase = (hpPercent > 0.7f) ? 1 : (hpPercent > 0.4f) ? 2 : 3;
    float pulse = ai.vsVoidKernPulse;

    // Void aura rings
    for (int ring = 0; ring < 3; ring++) {
        float ringR = (w / 2 + 12 + ring * 8) + std::sin(pulse * 2.0f + ring) * 3.0f;
        Uint8 ringA = static_cast<Uint8>(a * (0.15f - ring * 0.04f));
        int segments = 16;
        for (int s = 0; s < segments; s++) {
            float a1 = (s / (float)segments) * 2.0f * 3.14159f;
            float a2 = ((s + 1) / (float)segments) * 2.0f * 3.14159f;
            int rx1 = cx + static_cast<int>(std::cos(a1) * ringR);
            int ry1 = cy + static_cast<int>(std::sin(a1) * ringR);
            int rx2 = cx + static_cast<int>(std::cos(a2) * ringR);
            int ry2 = cy + static_cast<int>(std::sin(a2) * ringR);
            drawLine(renderer, rx1, ry1, rx2, ry2, 120, 0, 180, ringA);
        }
    }

    // Dark violet body
    fillRect(renderer, x + 4, y + 4, w - 8, h - 8, 80, 0, 120, a);
    fillRect(renderer, x + 8, y + 8, w - 16, h - 16, 100, 10, 150, a);

    // Pulsing void core
    int coreR = static_cast<int>(8 + std::sin(pulse * 3.0f) * 3.0f);
    Uint8 coreA = static_cast<Uint8>(a * (0.6f + std::sin(pulse * 4.0f) * 0.3f));
    fillRect(renderer, cx - coreR, cy - coreR, coreR * 2, coreR * 2, 180, 0, 255, coreA);
    fillRect(renderer, cx - coreR + 2, cy - coreR + 2, coreR * 2 - 4, coreR * 2 - 4, 220, 80, 255, coreA);

    // Eyes - change with phase
    if (phase == 1) {
        // Two glowing purple eyes
        fillRect(renderer, x + w / 3 - 3, y + h / 3 - 2, 6, 4, 200, 100, 255, a);
        fillRect(renderer, x + 2 * w / 3 - 3, y + h / 3 - 2, 6, 4, 200, 100, 255, a);
    } else if (phase == 2) {
        // Four eyes (two extra smaller)
        fillRect(renderer, x + w / 3 - 3, y + h / 3 - 2, 6, 4, 255, 100, 200, a);
        fillRect(renderer, x + 2 * w / 3 - 3, y + h / 3 - 2, 6, 4, 255, 100, 200, a);
        fillRect(renderer, x + w / 4 - 2, y + h / 3 + 4, 4, 3, 255, 50, 150, a);
        fillRect(renderer, x + 3 * w / 4 - 2, y + h / 3 + 4, 4, 3, 255, 50, 150, a);
    } else {
        // Single large void eye
        int eyeR = static_cast<int>(6 + std::sin(pulse * 5.0f) * 2.0f);
        fillRect(renderer, cx - eyeR, y + h / 3 - eyeR / 2, eyeR * 2, eyeR, 255, 0, 180, a);
        fillRect(renderer, cx - 2, y + h / 3 - 1, 4, 3, 255, 255, 255, a);
    }

    // Phase 2: Tentacles
    if (phase >= 2) {
        int numTentacles = (phase == 3) ? 6 : 4;
        for (int t = 0; t < numTentacles; t++) {
            float tAngle = (t / (float)numTentacles) * 2.0f * 3.14159f + pulse * 0.5f;
            int baseX = cx + static_cast<int>(std::cos(tAngle) * (w / 2 - 4));
            int baseY = cy + static_cast<int>(std::sin(tAngle) * (h / 2 - 4));
            float tentLen = 20.0f + std::sin(pulse * 2.0f + t) * 8.0f;
            int tipX = baseX + static_cast<int>(std::cos(tAngle) * tentLen);
            int tipY = baseY + static_cast<int>(std::sin(tAngle) * tentLen);
            drawLine(renderer, baseX, baseY, tipX, tipY, 140, 0, 200, static_cast<Uint8>(a * 0.7f));
            // Tentacle tip glow
            fillRect(renderer, tipX - 2, tipY - 2, 4, 4, 200, 50, 255, static_cast<Uint8>(a * 0.5f));
        }
    }

    // Phase 3: Screen edge glow effect (drawn as rects near entity edges)
    if (phase == 3) {
        Uint8 edgeA = static_cast<Uint8>(a * (0.15f + std::sin(pulse * 6.0f) * 0.1f));
        fillRect(renderer, x - 20, y - 20, w + 40, 4, 120, 0, 180, edgeA);
        fillRect(renderer, x - 20, y + h + 16, w + 40, 4, 120, 0, 180, edgeA);
        fillRect(renderer, x - 20, y - 20, 4, h + 40, 120, 0, 180, edgeA);
        fillRect(renderer, x + w + 16, y - 20, 4, h + 40, 120, 0, 180, edgeA);
    }

    // Laser visual (Reality Tear)
    if (ai.vsLaserActive) {
        float laserAngle = ai.vsLaserAngle;
        int laserLen = 600;
        int lx = cx + static_cast<int>(std::cos(laserAngle) * laserLen);
        int ly = cy + static_cast<int>(std::sin(laserAngle) * laserLen);
        int lx2 = cx - static_cast<int>(std::cos(laserAngle) * laserLen);
        int ly2 = cy - static_cast<int>(std::sin(laserAngle) * laserLen);
        // Outer glow
        drawLine(renderer, lx, ly, lx2, ly2, 180, 0, 255, static_cast<Uint8>(a * 0.3f));
        // Core beam (offset lines for width)
        for (int off = -2; off <= 2; off++) {
            drawLine(renderer, lx, ly + off, lx2, ly2 + off, 255, 100, 255, static_cast<Uint8>(a * 0.7f));
        }
    }

    // Dimension lock indicator
    if (ai.vsDimLockActive > 0.0f) {
        Uint8 lockA = static_cast<Uint8>(a * 0.3f * std::min(1.0f, ai.vsDimLockActive));
        fillRect(renderer, x - 10, y - 10, w + 20, h + 20, 255, 0, 0, lockA);
    }

    // HP bar with phase markers
    renderEnemyHPBar(renderer, x, y, w, entity, alpha, true);
}

void RenderSystem::renderEntropyIncarnate(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    int cx = x + w / 2, cy = y + h / 2;
    auto& sprite = entity.getComponent<SpriteComponent>();
    float time = SDL_GetTicks() * 0.004f;

    auto& ai = entity.getComponent<AIComponent>();
    int phase = ai.bossPhase;
    float pulse = ai.eiCorePulse;

    // Drop shadow
    fillRect(renderer, x + 4, y + h, w - 8, 4, 0, 0, 0, static_cast<Uint8>(a * 0.3f));

    // Entropy aura (sickly green, grows with phase)
    int auraSize = 6 + phase * 4;
    float auraPulse = 0.6f + 0.4f * std::sin(pulse * 2.0f);
    Uint8 auraA = static_cast<Uint8>(a * 0.15f * (1 + phase) * auraPulse);
    fillRect(renderer, x - auraSize, y - auraSize,
             w + auraSize * 2, h + auraSize * 2,
             60, 200, 60, auraA);

    // Outer shell (dark green-purple, decaying look)
    fillRect(renderer, x, y, w, h, 30, 60, 30, a);
    fillRect(renderer, x + 2, y + 2, w - 4, h - 4, 50, 100, 40, a);

    // Inner body (shifts from green to sickly purple with phase)
    Uint8 bodyR = static_cast<Uint8>(40 + phase * 30);
    Uint8 bodyG = static_cast<Uint8>(120 - phase * 15);
    Uint8 bodyB = static_cast<Uint8>(40 + phase * 25);
    fillRect(renderer, x + 4, y + 4, w - 8, h - 8, bodyR, bodyG, bodyB, a);

    // Pulsing entropy core (bright green → purple with damage)
    int coreR = static_cast<int>(8 + std::sin(pulse * 3.0f) * 4.0f);
    Uint8 coreRed = static_cast<Uint8>(80 + phase * 40);
    Uint8 coreGreen = static_cast<Uint8>(220 - phase * 30);
    Uint8 corePurp = static_cast<Uint8>(60 + phase * 50);
    Uint8 coreA = static_cast<Uint8>(a * (0.6f + std::sin(pulse * 4.0f) * 0.3f));
    fillRect(renderer, cx - coreR, cy - coreR, coreR * 2, coreR * 2, coreRed, coreGreen, corePurp, coreA);
    // Inner white-green hot spot
    fillRect(renderer, cx - coreR / 2, cy - coreR / 2, coreR, coreR, 200, 255, 180, coreA);

    // Eyes (grow more menacing with phase)
    if (phase <= 1) {
        // Two green eyes
        fillRect(renderer, cx - w / 4 - 2, y + h / 4, 5, 3, 80, 255, 80, a);
        fillRect(renderer, cx + w / 4 - 3, y + h / 4, 5, 3, 80, 255, 80, a);
    } else if (phase == 2) {
        // Wider, angrier eyes
        fillRect(renderer, cx - w / 4 - 3, y + h / 4 - 1, 7, 4, 140, 255, 60, a);
        fillRect(renderer, cx + w / 4 - 4, y + h / 4 - 1, 7, 4, 140, 255, 60, a);
        // Extra small eyes below
        fillRect(renderer, cx - w / 6, y + h / 3 + 4, 3, 2, 180, 200, 40, a);
        fillRect(renderer, cx + w / 6 - 3, y + h / 3 + 4, 3, 2, 180, 200, 40, a);
    } else {
        // Single large entropy eye
        int eyeR = static_cast<int>(6 + std::sin(pulse * 5.0f) * 2.0f);
        fillRect(renderer, cx - eyeR, y + h / 4 - eyeR / 2, eyeR * 2, eyeR, 60, 255, 60, a);
        fillRect(renderer, cx - 2, y + h / 4 - 1, 4, 3, 0, 80, 0, a); // pupil
    }

    // Entropy decay tendrils (radiating outward, more in later phases)
    int numTendrils = 4 + phase * 2;
    for (int t = 0; t < numTendrils; t++) {
        float tAngle = (t / (float)numTendrils) * 6.283185f + time * 0.3f;
        float tendrilLen = 15.0f + phase * 5.0f + std::sin(time * 2.0f + t * 1.1f) * 6.0f;
        int baseX = cx + static_cast<int>(std::cos(tAngle) * (w / 2 - 4));
        int baseY = cy + static_cast<int>(std::sin(tAngle) * (h / 2 - 4));
        int tipX = baseX + static_cast<int>(std::cos(tAngle) * tendrilLen);
        int tipY = baseY + static_cast<int>(std::sin(tAngle) * tendrilLen);
        Uint8 tA = static_cast<Uint8>(a * 0.6f);
        drawLine(renderer, baseX, baseY, tipX, tipY, 80, 200, 40, tA);
        // Tendril tip drip
        fillRect(renderer, tipX - 1, tipY - 1, 3, 3, 100, 255, 60, static_cast<Uint8>(tA * 0.7f));
    }

    // Entropy pulse visual (expanding ring when pulse fires)
    if (ai.eiPulseFired || ai.eiPulseTimer > 5.5f) {
        float ringPhase = ai.eiPulseTimer > 5.5f ? (6.0f - ai.eiPulseTimer) * 2.0f : 0;
        int ringSize = static_cast<int>(w * 0.5f + ringPhase * 80.0f);
        Uint8 ringA = static_cast<Uint8>(a * std::max(0.0f, 0.6f - ringPhase * 0.3f));
        SDL_SetRenderDrawColor(renderer, 60, 255, 60, ringA);
        SDL_Rect ring = {cx - ringSize, cy - ringSize, ringSize * 2, ringSize * 2};
        SDL_RenderDrawRect(renderer, &ring);
    }

    // Dimension lock visual (red overlay)
    if (ai.eiDimLockActive > 0.0f) {
        Uint8 lockA = static_cast<Uint8>(a * 0.25f * std::min(1.0f, ai.eiDimLockActive));
        fillRect(renderer, x - 8, y - 8, w + 16, h + 16, 255, 0, 60, lockA);
    }

    // Phase 3+: Screen edge corruption
    if (phase >= 3) {
        Uint8 edgeA = static_cast<Uint8>(a * (0.12f + std::sin(pulse * 6.0f) * 0.08f));
        fillRect(renderer, x - 16, y - 16, w + 32, 3, 60, 180, 30, edgeA);
        fillRect(renderer, x - 16, y + h + 13, w + 32, 3, 60, 180, 30, edgeA);
        fillRect(renderer, x - 16, y - 16, 3, h + 32, 60, 180, 30, edgeA);
        fillRect(renderer, x + w + 13, y - 16, 3, h + 32, 60, 180, 30, edgeA);
    }

    // HP bar with phase markers
    renderEnemyHPBar(renderer, x, y, w, entity, alpha, true);
}
