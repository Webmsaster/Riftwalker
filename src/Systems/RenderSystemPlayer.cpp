// RenderSystemPlayer.cpp -- Split from RenderSystem.cpp (player rendering)
#include "RenderSystem.h"
#include "Components/SpriteComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/AbilityComponent.h"
#include <cmath>
#include <algorithm>

void RenderSystem::renderPlayer(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    auto& sprite = entity.getComponent<SpriteComponent>();
    bool flipped = sprite.flipX;
    Uint8 a = static_cast<Uint8>(255 * alpha);

    bool attacking = false;
    if (entity.hasComponent<CombatComponent>()) {
        attacking = entity.getComponent<CombatComponent>().isAttacking;
    }

    bool onGround = false;
    float velY = 0;
    if (entity.hasComponent<PhysicsBody>()) {
        auto& phys = entity.getComponent<PhysicsBody>();
        onGround = phys.onGround;
        velY = phys.velocity.y;
    }

    bool invincible = false;
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        invincible = hp.isInvincible() && !hp.invulnerable;
    }

    if (invincible && (SDL_GetTicks() / 80) % 2 == 0) {
        a = static_cast<Uint8>(a * 0.4f);
    }

    // Apply squash/stretch based on animation state
    float squashX = 1.0f, stretchY = 1.0f;
    switch (sprite.animState) {
        case AnimState::Jump:
            squashX = 0.9f; stretchY = 1.1f;
            break;
        case AnimState::Fall:
            squashX = 1.05f; stretchY = 0.95f;
            break;
        case AnimState::Dash:
            squashX = 1.15f; stretchY = 0.85f;
            break;
        case AnimState::Attack: {
            // Forward lean during attack — quick squash then recover
            float t = std::min(sprite.animTimer * 6.0f, 1.0f);
            float attackSquash = (t < 0.3f) ? (t / 0.3f) * 0.12f : 0.12f * (1.0f - (t - 0.3f) / 0.7f);
            squashX = 1.0f + attackSquash;
            stretchY = 1.0f - attackSquash * 0.6f;
            break;
        }
        case AnimState::WallSlide:
            squashX = 0.88f; stretchY = 1.08f;
            break;
        case AnimState::Hurt: {
            // Quick flinch squash
            float t = std::min(sprite.animTimer * 8.0f, 1.0f);
            float flinch = (1.0f - t) * 0.15f;
            squashX = 1.0f + flinch;
            stretchY = 1.0f - flinch;
            break;
        }
        case AnimState::Idle: {
            float breath = std::sin(sprite.animTimer * 2.0f);
            squashX = 1.0f + breath * 0.02f;
            stretchY = 1.0f - breath * 0.02f;
            break;
        }
        default: break;
    }

    // Landing squash override: wide+short on impact, decays to normal
    if (sprite.landingSquashTimer > 0) {
        float t = sprite.landingSquashTimer / 0.15f; // 1.0 at start, 0.0 when done
        float intensity = sprite.landingSquashIntensity * t;
        squashX = 1.0f + intensity;       // wider
        stretchY = 1.0f - intensity * 0.7f; // shorter
    }

    int w = static_cast<int>(rect.w * squashX);
    int h = static_cast<int>(rect.h * stretchY);
    int x = rect.x + (rect.w - w) / 2;
    int y = rect.y + (rect.h - h);

    // Drop shadow below player (dark ellipse at feet)
    fillRect(renderer, x + 3, y + h, w - 6, 3,
             0, 0, 0, static_cast<Uint8>(a * 0.3f));
    fillRect(renderer, x + 5, y + h + 2, w - 10, 2,
             0, 0, 0, static_cast<Uint8>(a * 0.15f));

    // Legs (state-based animation)
    int legH = h / 4;
    int legW = w / 3;
    int leftLegOff = 0, rightLegOff = 0;

    switch (sprite.animState) {
        case AnimState::Run: {
            float cycle = sprite.animTimer * 8.0f;
            leftLegOff = static_cast<int>(std::sin(cycle) * 4);
            rightLegOff = -leftLegOff;
            break;
        }
        case AnimState::Idle: {
            float breath = std::sin(sprite.animTimer * 2.0f);
            leftLegOff = static_cast<int>(breath * 1);
            rightLegOff = leftLegOff;
            break;
        }
        case AnimState::Jump:
            leftLegOff = -2; rightLegOff = -2;
            break;
        case AnimState::Fall:
            leftLegOff = 2; rightLegOff = 3;
            break;
        case AnimState::Dash:
            leftLegOff = -1; rightLegOff = -1;
            break;
        case AnimState::WallSlide:
            leftLegOff = 2; rightLegOff = -2;
            break;
        case AnimState::Attack:
            leftLegOff = 3; rightLegOff = -3;
            break;
        default: break;
    }

    fillRect(renderer, x + w / 4 - legW / 2, y + h - legH + leftLegOff, legW, legH, 30, 60, 140, a);
    fillRect(renderer, x + 3 * w / 4 - legW / 2, y + h - legH + rightLegOff, legW, legH, 30, 60, 140, a);

    // Body (suit)
    int bodyH = h * 3 / 5;
    int bodyY = y + h / 5;
    Uint8 bodyR = 50, bodyG = 100, bodyB = 200;
    if (attacking) { bodyR = 220; bodyG = 200; bodyB = 80; }

    // Body outline (dark border)
    fillRect(renderer, x + 1, bodyY - 1, w - 2, bodyH + 2,
             static_cast<Uint8>(bodyR * 0.3f), static_cast<Uint8>(bodyG * 0.3f),
             static_cast<Uint8>(bodyB * 0.3f), a);
    // Body fill
    fillRect(renderer, x + 2, bodyY, w - 4, bodyH, bodyR, bodyG, bodyB, a);
    // Body top highlight (lighter band)
    fillRect(renderer, x + 3, bodyY + 1, w - 6, 2,
             static_cast<Uint8>(std::min(255, bodyR + 50)),
             static_cast<Uint8>(std::min(255, bodyG + 50)),
             static_cast<Uint8>(std::min(255, bodyB + 40)), static_cast<Uint8>(a * 0.6f));
    // Body side highlight (left edge)
    fillRect(renderer, x + 3, bodyY + 1, w / 3, bodyH - 2,
             static_cast<Uint8>(std::min(255, bodyR + 40)),
             static_cast<Uint8>(std::min(255, bodyG + 40)),
             static_cast<Uint8>(std::min(255, bodyB + 30)), static_cast<Uint8>(a * 0.5f));
    // Body bottom darkening
    fillRect(renderer, x + 2, bodyY + bodyH - 3, w - 4, 3,
             static_cast<Uint8>(bodyR * 0.6f), static_cast<Uint8>(bodyG * 0.6f),
             static_cast<Uint8>(bodyB * 0.6f), static_cast<Uint8>(a * 0.5f));

    // Chest emblem (diamond)
    int cx = x + w / 2, cy = bodyY + bodyH / 3;
    drawLine(renderer, cx, cy - 3, cx + 3, cy, 150, 220, 255, a);
    drawLine(renderer, cx + 3, cy, cx, cy + 3, 150, 220, 255, a);
    drawLine(renderer, cx, cy + 3, cx - 3, cy, 150, 220, 255, a);
    drawLine(renderer, cx - 3, cy, cx, cy - 3, 150, 220, 255, a);

    // Head
    int headH = h / 4;
    int headW = w - 4;
    // Head outline
    fillRect(renderer, x + 1, y - 1, headW + 2, headH + 2, 20, 40, 90, a);
    fillRect(renderer, x + 2, y, headW, headH, 45, 85, 180, a);
    // Head top highlight
    fillRect(renderer, x + 3, y + 1, headW - 2, 2, 70, 120, 210, static_cast<Uint8>(a * 0.6f));

    // Visor (glowing)
    int visorY = y + headH / 3;
    int visorH = headH / 3;
    int visorX = flipped ? x + 3 : x + w / 3;
    int visorW = w / 2;
    fillRect(renderer, visorX, visorY, visorW, visorH, 100, 220, 255, a);
    fillRect(renderer, visorX + 1, visorY + 1, visorW - 2, visorH - 2, 180, 240, 255, a);

    // Arm with weapon (combo-stage & directional visual variation)
    int comboStage = 0;
    Vec2 atkDir = {flipped ? -1.0f : 1.0f, 0.0f};
    bool isDashAtk = false;
    if (entity.hasComponent<CombatComponent>()) {
        auto& cb = entity.getComponent<CombatComponent>();
        if (cb.comboCount > 0) comboStage = (cb.comboCount - 1) % 3;
        if (attacking) atkDir = cb.attackDirection;
        isDashAtk = attacking && cb.currentAttack == AttackType::Dash;
    }

    bool atkUp = attacking && atkDir.y < -0.5f;
    bool atkDown = attacking && atkDir.y > 0.5f;

    int armBaseY = bodyY + bodyH / 4;
    int armY = armBaseY;
    int armLen = attacking ? w : w / 2;

    if (atkUp) {
        // Upward attack: arm points straight up
        int armX = x + w / 2 - 2;
        int weapLen = w + 6;
        fillRect(renderer, armX, y - weapLen + 4, 4, weapLen, bodyR, bodyG, bodyB, a);
        // Weapon tip glow
        fillRect(renderer, armX - 4, y - weapLen, 12, 6, 100, 220, 255, a);
        fillRect(renderer, armX - 2, y - weapLen - 2, 8, 4, 200, 255, 255, static_cast<Uint8>(a * 0.8f));
    } else if (atkDown) {
        // Downward attack: arm points straight down
        int armX = x + w / 2 - 2;
        int weapLen = w + 6;
        fillRect(renderer, armX, y + h - 4, 4, weapLen, bodyR, bodyG, bodyB, a);
        // Weapon tip glow
        fillRect(renderer, armX - 4, y + h + weapLen - 6, 12, 6, 255, 150, 50, a);
        fillRect(renderer, armX - 2, y + h + weapLen - 4, 8, 4, 255, 255, 150, static_cast<Uint8>(a * 0.8f));
    } else if (isDashAtk) {
        // Dash attack: wide horizontal slash
        int slashLen = w + 12;
        if (!flipped) {
            fillRect(renderer, x + w - 2, armBaseY - 2, slashLen, 6, bodyR, bodyG, bodyB, a);
            // Cyan energy blade
            fillRect(renderer, x + w + slashLen - 10, armBaseY - 6, 8, 14, 80, 200, 255, a);
            fillRect(renderer, x + w + slashLen - 8, armBaseY - 8, 4, 18, 180, 240, 255, static_cast<Uint8>(a * 0.7f));
            // Motion trail
            fillRect(renderer, x + w, armBaseY, slashLen - 10, 2, 100, 200, 255, static_cast<Uint8>(a * 0.4f));
        } else {
            fillRect(renderer, x - slashLen + 2, armBaseY - 2, slashLen, 6, bodyR, bodyG, bodyB, a);
            fillRect(renderer, x - slashLen + 2, armBaseY - 6, 8, 14, 80, 200, 255, a);
            fillRect(renderer, x - slashLen + 4, armBaseY - 8, 4, 18, 180, 240, 255, static_cast<Uint8>(a * 0.7f));
            fillRect(renderer, x - slashLen + 10, armBaseY, slashLen - 10, 2, 100, 200, 255, static_cast<Uint8>(a * 0.4f));
        }
    } else {
        // Normal horizontal melee (with combo variation)
        if (attacking) {
            switch (comboStage) {
                case 0: break;
                case 1: armY = armBaseY - 4; break;
                case 2: armY = armBaseY - 8; armLen = w + 4; break;
            }
        }

        if (!flipped) {
            fillRect(renderer, x + w - 2, armY, armLen, 4, bodyR, bodyG, bodyB, a);
            if (attacking) {
                Uint8 wR = 255, wG = 255, wB = 200;
                if (comboStage == 1) { wR = 255; wG = 200; wB = 80; }
                if (comboStage == 2) { wR = 255; wG = 100; wB = 50; }
                int weapH = 12 + comboStage * 4;
                fillRect(renderer, x + w + armLen - 6, armY - weapH / 3, 4, weapH, wR, wG, wB, a);
                fillRect(renderer, x + w + armLen - 4, armY - weapH / 3 - 2, 2, weapH + 4, 255, 255, 255, static_cast<Uint8>(a * 0.7f));
            }
        } else {
            fillRect(renderer, x - armLen + 2, armY, armLen, 4, bodyR, bodyG, bodyB, a);
            if (attacking) {
                Uint8 wR = 255, wG = 255, wB = 200;
                if (comboStage == 1) { wR = 255; wG = 200; wB = 80; }
                if (comboStage == 2) { wR = 255; wG = 100; wB = 50; }
                int weapH = 12 + comboStage * 4;
                fillRect(renderer, x - armLen + 2, armY - weapH / 3, 4, weapH, wR, wG, wB, a);
                fillRect(renderer, x - armLen + 2, armY - weapH / 3 - 2, 2, weapH + 4, 255, 255, 255, static_cast<Uint8>(a * 0.7f));
            }
        }
    }

    // Jump thrusters (when airborne going up)
    if (!onGround && velY < 0) {
        Uint32 t = SDL_GetTicks();
        int flameH = 4 + static_cast<int>(t % 4);
        fillRect(renderer, x + w / 4 - 2, y + h, 4, flameH, 255, 180, 50, static_cast<Uint8>(a * 0.8f));
        fillRect(renderer, x + 3 * w / 4 - 2, y + h, 4, flameH, 255, 180, 50, static_cast<Uint8>(a * 0.8f));
        fillRect(renderer, x + w / 4 - 1, y + h, 2, flameH - 1, 255, 255, 150, static_cast<Uint8>(a * 0.6f));
        fillRect(renderer, x + 3 * w / 4 - 1, y + h, 2, flameH - 1, 255, 255, 150, static_cast<Uint8>(a * 0.6f));
    }

    // Charge glow: golden aura intensifies with charge (visible from start)
    if (entity.hasComponent<CombatComponent>()) {
        auto& cb = entity.getComponent<CombatComponent>();
        if (cb.isCharging) {
            float cp = cb.getChargePercent();
            // Pulse effect: subtle breathing that speeds up near full charge
            float pulse = 0.85f + 0.15f * std::sin(SDL_GetTicks() * (0.006f + cp * 0.012f));
            Uint8 glowA = static_cast<Uint8>((20 + 140 * cp) * pulse);
            int glowR = static_cast<int>(2 + 10 * cp);
            SDL_Rect glow = {x - glowR, y - glowR, w + glowR * 2, h + glowR * 2};
            SDL_SetRenderDrawColor(renderer, 255, 200, 50, static_cast<Uint8>(glowA * alpha));
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, &glow);
            // Brighter inner glow (appears after min charge)
            if (cb.chargeTimer >= cb.minChargeTime) {
                SDL_Rect innerGlow = {x - glowR / 2, y - glowR / 2, w + glowR, h + glowR};
                SDL_SetRenderDrawColor(renderer, 255, 255, 150, static_cast<Uint8>(glowA * 0.5f * alpha));
                SDL_RenderFillRect(renderer, &innerGlow);
            }
        }

        // Parry success aura: golden rim when next hit is guaranteed crit
        if (cb.parrySuccessTimer > 0 && !cb.counterReady && !cb.isCounterAttacking) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.008f);
            Uint8 rimA = static_cast<Uint8>((80 + 80 * pulse) * alpha);
            SDL_Rect rim = {x - 3, y - 3, w + 6, h + 6};
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, rimA);
            SDL_RenderDrawRect(renderer, &rim);
            SDL_Rect rim2 = {x - 2, y - 2, w + 4, h + 4};
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, static_cast<Uint8>(rimA * 0.7f));
            SDL_RenderDrawRect(renderer, &rim2);
        }

        // Counter-ready aura: bright golden pulsing border (stronger than parry success)
        if (cb.counterReady) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.015f); // faster pulse
            Uint8 rimA = static_cast<Uint8>((140 + 115 * pulse) * alpha);
            // Triple border for emphasis
            SDL_Rect rim1 = {x - 5, y - 5, w + 10, h + 10};
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, rimA);
            SDL_RenderDrawRect(renderer, &rim1);
            SDL_Rect rim2 = {x - 4, y - 4, w + 8, h + 8};
            SDL_SetRenderDrawColor(renderer, 255, 240, 80, static_cast<Uint8>(rimA * 0.9f));
            SDL_RenderDrawRect(renderer, &rim2);
            SDL_Rect rim3 = {x - 3, y - 3, w + 6, h + 6};
            SDL_SetRenderDrawColor(renderer, 255, 255, 150, static_cast<Uint8>(rimA * 0.7f));
            SDL_RenderDrawRect(renderer, &rim3);
            // Golden fill glow
            SDL_Rect glow = {x - 6, y - 6, w + 12, h + 12};
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, static_cast<Uint8>(30 * pulse * alpha));
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, &glow);
        }

        // Counter-attacking glow: weapon-colored overlay on entire sprite
        if (cb.isCounterAttacking) {
            Uint8 glowR = 255, glowG = 215, glowB = 0;
            // Color by melee weapon
            switch (cb.currentMelee) {
                case WeaponID::RiftBlade:    glowR = 100; glowG = 200; glowB = 255; break;
                case WeaponID::PhaseDaggers: glowR = 180; glowG = 100; glowB = 255; break;
                case WeaponID::VoidHammer:   glowR = 160; glowG = 80;  glowB = 255; break;
                default: break;
            }
            float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.02f);
            Uint8 glowA = static_cast<Uint8>(120 * pulse * alpha);
            SDL_Rect counterGlow = {x - 3, y - 3, w + 6, h + 6};
            SDL_SetRenderDrawColor(renderer, glowR, glowG, glowB, glowA);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, &counterGlow);
            // Bright border
            SDL_Rect counterRim = {x - 4, y - 4, w + 8, h + 8};
            SDL_SetRenderDrawColor(renderer, glowR, glowG, glowB, static_cast<Uint8>(180 * pulse * alpha));
            SDL_RenderDrawRect(renderer, &counterRim);
        }
    }

    // Rift Shield hexagonal barrier
    if (entity.hasComponent<AbilityComponent>()) {
        auto& abil = entity.getComponent<AbilityComponent>();
        if (abil.abilities[1].active) {
            float shieldPulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.008f);
            float shieldAlpha = (0.4f + 0.3f * shieldPulse) * alpha;
            Uint8 sa = static_cast<Uint8>(255 * shieldAlpha);

            int shieldCX = x + w / 2;
            int shieldCY = y + h / 2;
            int shieldR = std::max(w, h) / 2 + 12;

            // Draw hexagon shield
            int hexPoints = 6;
            float rotSpeed = SDL_GetTicks() * 0.002f;
            for (int i = 0; i < hexPoints; i++) {
                float a1 = rotSpeed + i * 6.283185f / hexPoints;
                float a2 = rotSpeed + (i + 1) * 6.283185f / hexPoints;
                int x1 = shieldCX + static_cast<int>(std::cos(a1) * shieldR);
                int y1 = shieldCY + static_cast<int>(std::sin(a1) * shieldR);
                int x2 = shieldCX + static_cast<int>(std::cos(a2) * shieldR);
                int y2 = shieldCY + static_cast<int>(std::sin(a2) * shieldR);

                // Outer hex
                drawLine(renderer, x1, y1, x2, y2, 80, 200, 255, sa);
                // Inner hex
                int ix1 = shieldCX + static_cast<int>(std::cos(a1) * (shieldR - 3));
                int iy1 = shieldCY + static_cast<int>(std::sin(a1) * (shieldR - 3));
                int ix2 = shieldCX + static_cast<int>(std::cos(a2) * (shieldR - 3));
                int iy2 = shieldCY + static_cast<int>(std::sin(a2) * (shieldR - 3));
                drawLine(renderer, ix1, iy1, ix2, iy2, 120, 230, 255, static_cast<Uint8>(sa * 0.6f));
            }

            // Shield hit indicator dots
            for (int i = 0; i < abil.shieldHitsRemaining; i++) {
                float da = rotSpeed + i * 6.283185f / abil.shieldMaxHits;
                int dx = shieldCX + static_cast<int>(std::cos(da) * (shieldR + 5));
                int dy = shieldCY + static_cast<int>(std::sin(da) * (shieldR + 5));
                fillRect(renderer, dx - 2, dy - 2, 4, 4, 100, 255, 220, sa);
            }
        }

        // Ground Slam charge glow (while falling)
        if (abil.slamFalling) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.015f);
            Uint8 glowA = static_cast<Uint8>((120 + 80 * pulse) * alpha);

            // Glow ring around player
            SDL_Rect glow1 = {x - 4, y - 4, w + 8, h + 8};
            SDL_SetRenderDrawColor(renderer, 255, 180, 60, glowA);
            SDL_RenderDrawRect(renderer, &glow1);
            SDL_Rect glow2 = {x - 6, y - 6, w + 12, h + 12};
            SDL_SetRenderDrawColor(renderer, 255, 120, 30, static_cast<Uint8>(glowA * 0.5f));
            SDL_RenderDrawRect(renderer, &glow2);

            // Downward speed lines
            for (int i = 0; i < 4; i++) {
                int lx = x + (i + 1) * w / 5;
                int lineLen = 8 + static_cast<int>(pulse * 6);
                drawLine(renderer, lx, y + h, lx, y + h + lineLen, 255, 200, 80, glowA);
            }
        }
    }
}
