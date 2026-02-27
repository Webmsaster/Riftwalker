#include "RenderSystem.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/AIComponent.h"
#include <algorithm>
#include <cmath>

void RenderSystem::fillRect(SDL_Renderer* r, int x, int y, int w, int h,
                            Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void RenderSystem::drawLine(SDL_Renderer* r, int x1, int y1, int x2, int y2,
                            Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

void RenderSystem::render(SDL_Renderer* renderer, EntityManager& entities,
                          const Camera& camera, int currentDimension, float dimBlendAlpha) {
    struct RenderEntry {
        Entity* entity;
        int layer;
        float alpha;
    };
    std::vector<RenderEntry> entries;

    entities.forEach([&](Entity& e) {
        if (!e.visible || !e.hasComponent<SpriteComponent>() || !e.hasComponent<TransformComponent>())
            return;

        auto& sprite = e.getComponent<SpriteComponent>();
        float alpha = 1.0f;

        if (e.dimension == 0) {
            alpha = 1.0f;
        } else if (e.dimension == currentDimension) {
            alpha = 1.0f;
        } else {
            alpha = dimBlendAlpha * 0.3f;
            if (alpha < 0.05f) return;
        }

        entries.push_back({&e, sprite.renderLayer, alpha});
    });

    std::sort(entries.begin(), entries.end(),
        [](const RenderEntry& a, const RenderEntry& b) { return a.layer < b.layer; });

    for (auto& entry : entries) {
        renderEntity(renderer, *entry.entity, camera, entry.alpha);
    }
}

void RenderSystem::renderEntity(SDL_Renderer* renderer, Entity& entity,
                                 const Camera& camera, float alpha) {
    auto& transform = entity.getComponent<TransformComponent>();
    SDL_FRect worldRect = transform.getRect();
    SDL_Rect screenRect = camera.worldToScreen(worldRect);

    const std::string& tag = entity.getTag();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Custom rendering based on entity type
    if (tag == "player") {
        renderPlayer(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_walker") {
        renderWalker(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_flyer") {
        renderFlyer(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_turret") {
        renderTurret(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_charger") {
        renderCharger(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_phaser") {
        renderPhaser(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_exploder") {
        renderExploder(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_shielder") {
        renderShielder(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_crawler") {
        renderCrawler(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_summoner") {
        renderSummoner(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_sniper") {
        renderSniper(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_minion") {
        // Minions use walker renderer but smaller
        renderWalker(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_boss") {
        int bt = 0;
        if (entity.hasComponent<AIComponent>()) bt = entity.getComponent<AIComponent>().bossType;
        if (bt == 1) renderVoidWyrm(renderer, screenRect, entity, alpha);
        else renderBoss(renderer, screenRect, entity, alpha);
    } else if (tag.find("pickup_") == 0) {
        renderPickup(renderer, screenRect, entity, alpha);
    } else if (tag == "projectile") {
        renderProjectile(renderer, screenRect, entity, alpha);
    } else {
        // Default: colored rectangle with outline
        auto& sprite = entity.getComponent<SpriteComponent>();
        Uint8 a = static_cast<Uint8>(sprite.color.a * alpha);
        fillRect(renderer, screenRect.x, screenRect.y, screenRect.w, screenRect.h,
                 sprite.color.r, sprite.color.g, sprite.color.b, a);
    }

    // Hit flash: white overlay when enemy just took damage
    if (tag.find("enemy") != std::string::npos && entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        float flashDuration = 0.12f;
        float flashStart = hp.invincibilityTime - flashDuration;
        if (hp.invincibilityTimer > flashStart && hp.invincibilityTimer > 0) {
            float flashT = (hp.invincibilityTimer - flashStart) / flashDuration;
            Uint8 fa = static_cast<Uint8>(180 * flashT * alpha);
            fillRect(renderer, screenRect.x, screenRect.y, screenRect.w, screenRect.h,
                     255, 255, 255, fa);
        }
    }

    // Element aura for elemental enemies
    if (tag.find("enemy") != std::string::npos && entity.hasComponent<AIComponent>()) {
        auto& ai = entity.getComponent<AIComponent>();
        if (ai.element != EnemyElement::None) {
            float time = SDL_GetTicks() * 0.004f;
            float pulse = 0.4f + 0.6f * std::abs(std::sin(time));
            Uint8 glowA = static_cast<Uint8>(40 * pulse * alpha);
            Uint8 r = 0, g = 0, b = 0;
            switch (ai.element) {
                case EnemyElement::Fire: r = 255; g = 100; b = 20; break;
                case EnemyElement::Ice: r = 80; g = 160; b = 255; break;
                case EnemyElement::Electric: r = 255; g = 255; b = 60; break;
                default: break;
            }
            SDL_Rect glow = {screenRect.x - 3, screenRect.y - 3, screenRect.w + 6, screenRect.h + 6};
            SDL_SetRenderDrawColor(renderer, r, g, b, glowA);
            SDL_RenderFillRect(renderer, &glow);
            SDL_SetRenderDrawColor(renderer, r, g, b, static_cast<Uint8>(80 * pulse * alpha));
            SDL_RenderDrawRect(renderer, &glow);
        }
    }
}

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
        case AnimState::Idle: {
            float breath = std::sin(sprite.animTimer * 2.0f);
            squashX = 1.0f + breath * 0.02f;
            stretchY = 1.0f - breath * 0.02f;
            break;
        }
        default: break;
    }

    int w = static_cast<int>(rect.w * squashX);
    int h = static_cast<int>(rect.h * stretchY);
    int x = rect.x + (rect.w - w) / 2;
    int y = rect.y + (rect.h - h);

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

    fillRect(renderer, x + 2, bodyY, w - 4, bodyH, bodyR, bodyG, bodyB, a);
    // Body highlight
    fillRect(renderer, x + 3, bodyY + 1, w / 3, bodyH - 2,
             static_cast<Uint8>(std::min(255, bodyR + 40)),
             static_cast<Uint8>(std::min(255, bodyG + 40)),
             static_cast<Uint8>(std::min(255, bodyB + 30)), static_cast<Uint8>(a * 0.5f));

    // Chest emblem (diamond)
    int cx = x + w / 2, cy = bodyY + bodyH / 3;
    drawLine(renderer, cx, cy - 3, cx + 3, cy, 150, 220, 255, a);
    drawLine(renderer, cx + 3, cy, cx, cy + 3, 150, 220, 255, a);
    drawLine(renderer, cx, cy + 3, cx - 3, cy, 150, 220, 255, a);
    drawLine(renderer, cx - 3, cy, cx, cy - 3, 150, 220, 255, a);

    // Head
    int headH = h / 4;
    int headW = w - 4;
    fillRect(renderer, x + 2, y, headW, headH, 45, 85, 180, a);

    // Visor (glowing)
    int visorY = y + headH / 3;
    int visorH = headH / 3;
    int visorX = flipped ? x + 3 : x + w / 3;
    int visorW = w / 2;
    fillRect(renderer, visorX, visorY, visorW, visorH, 100, 220, 255, a);
    fillRect(renderer, visorX + 1, visorY + 1, visorW - 2, visorH - 2, 180, 240, 255, a);

    // Arm with weapon (combo-stage visual variation)
    int comboStage = 0;
    if (entity.hasComponent<CombatComponent>()) {
        auto& cb = entity.getComponent<CombatComponent>();
        if (cb.comboCount > 0) comboStage = (cb.comboCount - 1) % 3;
    }
    int armBaseY = bodyY + bodyH / 4;
    int armY = armBaseY;
    int armLen = attacking ? w : w / 2;

    // Vary arm position by combo stage when attacking
    if (attacking) {
        switch (comboStage) {
            case 0: break; // Normal horizontal
            case 1: armY = armBaseY - 4; break; // Diagonal up
            case 2: armY = armBaseY - 8; armLen = w + 4; break; // Uppercut (higher, longer)
        }
    }

    if (!flipped) {
        fillRect(renderer, x + w - 2, armY, armLen, 4, bodyR, bodyG, bodyB, a);
        if (attacking) {
            // Weapon glow color by combo stage
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

    // Jump thrusters (when airborne going up)
    if (!onGround && velY < 0) {
        Uint32 t = SDL_GetTicks();
        int flameH = 4 + static_cast<int>(t % 4);
        fillRect(renderer, x + w / 4 - 2, y + h, 4, flameH, 255, 180, 50, static_cast<Uint8>(a * 0.8f));
        fillRect(renderer, x + 3 * w / 4 - 2, y + h, 4, flameH, 255, 180, 50, static_cast<Uint8>(a * 0.8f));
        fillRect(renderer, x + w / 4 - 1, y + h, 2, flameH - 1, 255, 255, 150, static_cast<Uint8>(a * 0.6f));
        fillRect(renderer, x + 3 * w / 4 - 1, y + h, 2, flameH - 1, 255, 255, 150, static_cast<Uint8>(a * 0.6f));
    }
}

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

void RenderSystem::renderPickup(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    auto& sprite = entity.getComponent<SpriteComponent>();
    float time = SDL_GetTicks() * 0.005f;

    // Floating bob
    int bob = static_cast<int>(std::sin(time) * 3);

    const std::string& tag = entity.getTag();
    if (tag == "pickup_health") {
        // Green cross
        int cy = y + bob;
        fillRect(renderer, x + w / 3, cy, w / 3, h, 60, 220, 60, a);
        fillRect(renderer, x, cy + h / 3, w, h / 3, 60, 220, 60, a);
        fillRect(renderer, x - 1, cy - 1, w + 2, h + 2, 80, 255, 80, static_cast<Uint8>(a * 0.2f));
    } else if (tag == "pickup_shield") {
        // Blue circle with inner ring
        int cy = y + bob;
        int cx = x + w / 2;
        fillRect(renderer, x + 1, cy + 1, w - 2, h - 2, 80, 160, 255, a);
        fillRect(renderer, cx - 3, cy + h / 3, 6, h / 3, 200, 230, 255, a);
        fillRect(renderer, x - 1, cy - 1, w + 2, h + 2, 100, 180, 255, static_cast<Uint8>(a * 0.3f));
    } else if (tag == "pickup_speed") {
        // Yellow lightning bolt shape
        int cy = y + bob;
        fillRect(renderer, x + w / 3, cy, w / 3, h / 2, 255, 255, 80, a);
        fillRect(renderer, x + w / 6, cy + h / 3, w * 2 / 3, h / 4, 255, 255, 80, a);
        fillRect(renderer, x + w / 3, cy + h / 2, w / 3, h / 2, 255, 255, 80, a);
        fillRect(renderer, x - 1, cy - 1, w + 2, h + 2, 255, 255, 120, static_cast<Uint8>(a * 0.25f));
    } else if (tag == "pickup_damage") {
        // Red sword/arrow up
        int cy = y + bob;
        int cx = x + w / 2;
        fillRect(renderer, cx - 1, cy, 3, h, 255, 80, 80, a);
        fillRect(renderer, cx - 4, cy + 2, 9, 3, 255, 80, 80, a);
        fillRect(renderer, x - 1, cy - 1, w + 2, h + 2, 255, 60, 60, static_cast<Uint8>(a * 0.25f));
    } else {
        // Purple diamond (shard)
        int cx = x + w / 2;
        int cy = y + h / 2 + bob;
        fillRect(renderer, cx - 3, cy - 5, 6, 10, 180, 130, 255, a);
        fillRect(renderer, cx - 5, cy - 3, 10, 6, 180, 130, 255, a);
        float sparkle = 0.5f + 0.5f * std::sin(time * 3);
        fillRect(renderer, cx - 1, cy - 1, 2, 2, 255, 255, 255, static_cast<Uint8>(a * sparkle));
    }
}

void RenderSystem::renderProjectile(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int cx = rect.x + rect.w / 2;
    int cy = rect.y + rect.h / 2;
    int r = rect.w / 2 + 1;

    // Outer glow
    fillRect(renderer, cx - r - 2, cy - r - 2, (r + 2) * 2, (r + 2) * 2, 255, 200, 50, static_cast<Uint8>(a * 0.3f));
    // Core
    fillRect(renderer, cx - r, cy - r, r * 2, r * 2, 255, 230, 100, a);
    // Bright center
    fillRect(renderer, cx - 1, cy - 1, 2, 2, 255, 255, 220, a);

    // Trail behind projectile
    if (entity.hasComponent<PhysicsBody>()) {
        auto& phys = entity.getComponent<PhysicsBody>();
        float speed = std::sqrt(phys.velocity.x * phys.velocity.x + phys.velocity.y * phys.velocity.y);
        if (speed > 10.0f) {
            float nx = -phys.velocity.x / speed;
            float ny = -phys.velocity.y / speed;
            for (int i = 1; i <= 4; i++) {
                int tx = cx + static_cast<int>(nx * i * 4);
                int ty = cy + static_cast<int>(ny * i * 4);
                Uint8 ta = static_cast<Uint8>(a * (1.0f - i * 0.2f));
                fillRect(renderer, tx - 1, ty - 1, 2, 2, 255, 200, 50, ta);
            }
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
    bool flipped = sprite.flipX;
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

    // Massive body
    int bodyH = h * 3 / 5;
    int bodyY = y + h / 5;
    fillRect(renderer, x, bodyY, w, bodyH, sprite.color.r, sprite.color.g, sprite.color.b, a);

    // Armor plates
    fillRect(renderer, x + 2, bodyY + 2, w - 4, bodyH / 3,
             static_cast<Uint8>(std::min(255, sprite.color.r + 30)),
             static_cast<Uint8>(std::min(255, sprite.color.g + 20)),
             static_cast<Uint8>(std::min(255, sprite.color.b + 20)),
             static_cast<Uint8>(a * 0.7f));

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
    fillRect(renderer, headX, y, headW, headH, sprite.color.r, sprite.color.g, sprite.color.b, a);

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
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        int barW = w + 20;
        int barH = 5;
        int barX = x - 10;
        int barY = y - 14;
        fillRect(renderer, barX, barY, barW, barH, 40, 20, 40, a);
        fillRect(renderer, barX, barY, static_cast<int>(barW * hp.getPercent()), barH, 200, 40, 180, a);
        // Phase markers
        fillRect(renderer, barX + barW * 2 / 3, barY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
        fillRect(renderer, barX + barW / 3, barY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
    }
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
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        int barW = w + 20;
        int barH = 5;
        int bX = x - 10;
        int bY = y - 14;
        fillRect(renderer, bX, bY, barW, barH, 20, 40, 20, a);
        fillRect(renderer, bX, bY, static_cast<int>(barW * hp.getPercent()), barH, 40, 200, 100, a);
        fillRect(renderer, bX + barW * 2 / 3, bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
        fillRect(renderer, bX + barW / 3, bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
    }
}
