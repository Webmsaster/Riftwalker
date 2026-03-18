#include "RenderSystem.h"
#include "Core/Game.h"
#include "States/GameState.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/AIComponent.h"
#include "Components/AbilityComponent.h"
#include <algorithm>
#include <cmath>

bool RenderSystem::renderSprite(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    auto& sprite = entity.getComponent<SpriteComponent>();
    if (!sprite.texture) return false;

    // Update animation frame if entity has AnimationComponent
    SDL_Rect srcRect = sprite.srcRect;
    if (entity.hasComponent<AnimationComponent>()) {
        auto& anim = entity.getComponent<AnimationComponent>();
        srcRect = anim.getCurrentSrcRect();
    }

    // Apply color modulation and alpha
    SDL_SetTextureColorMod(sprite.texture, sprite.color.r, sprite.color.g, sprite.color.b);
    SDL_SetTextureAlphaMod(sprite.texture, static_cast<Uint8>(sprite.color.a * alpha));
    SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND);

    // Flip based on facing direction
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (sprite.flipX) flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_HORIZONTAL);
    if (sprite.flipY) flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_VERTICAL);

    SDL_RenderCopyEx(renderer, sprite.texture, &srcRect, &rect, 0.0, nullptr, flip);
    return true;
}

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
                          const Camera& camera, int currentDimension, float dimBlendAlpha,
                          float interpolation) {
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
            // Ghost shimmer for other-dimension entities
            bool isEnemy = e.getTag().find("enemy") != std::string::npos;
            if (isEnemy) {
                float shimmer = 0.15f + 0.1f * std::sin(SDL_GetTicks() * 0.005f + e.dimension * 3.14f);
                alpha = shimmer;
            } else {
                alpha = dimBlendAlpha * 0.3f;
            }
            if (alpha < 0.05f) return;
        }

        entries.push_back({&e, sprite.renderLayer, alpha});
    });

    std::sort(entries.begin(), entries.end(),
        [](const RenderEntry& a, const RenderEntry& b) { return a.layer < b.layer; });

    for (auto& entry : entries) {
        renderEntity(renderer, *entry.entity, camera, entry.alpha, interpolation);
    }
}

void RenderSystem::renderEntity(SDL_Renderer* renderer, Entity& entity,
                                 const Camera& camera, float alpha, float interpolation) {
    auto& transform = entity.getComponent<TransformComponent>();
    // Use interpolated position for smooth rendering between physics steps
    SDL_FRect worldRect = transform.getInterpolatedRect(interpolation);
    SDL_Rect screenRect = camera.worldToScreen(worldRect);

    // Viewport culling: skip entities entirely off-screen (with generous margin)
    constexpr int CULL_MARGIN = 100;
    int screenW = GameState::SCREEN_WIDTH;
    int screenH = GameState::SCREEN_HEIGHT;
    if (screenRect.x + screenRect.w < -CULL_MARGIN || screenRect.x > screenW + CULL_MARGIN ||
        screenRect.y + screenRect.h < -CULL_MARGIN || screenRect.y > screenH + CULL_MARGIN) {
        return;
    }

    const std::string& tag = entity.getTag();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Dash afterimage ghost trail: render fading copies behind the entity
    {
        auto& sprite = entity.getComponent<SpriteComponent>();
        for (int i = 0; i < sprite.afterimageCount; i++) {
            auto& ai = sprite.afterimages[i];
            float t = ai.timer / Afterimage::MAX_LIFE; // 1.0 → 0.0
            Uint8 ghostA = static_cast<Uint8>(t * 140 * alpha);
            SDL_FRect ghostWorld = {ai.worldX, ai.worldY, ai.width, ai.height};
            SDL_Rect ghostRect = camera.worldToScreen(ghostWorld);
            // Slightly shrink ghost over time for dissolve feel
            int shrink = static_cast<int>((1.0f - t) * 3);
            ghostRect.x += shrink;
            ghostRect.y += shrink;
            ghostRect.w -= shrink * 2;
            ghostRect.h -= shrink * 2;
            if (ghostRect.w > 0 && ghostRect.h > 0) {
                fillRect(renderer, ghostRect.x, ghostRect.y, ghostRect.w, ghostRect.h,
                         ai.color.r, ai.color.g, ai.color.b, ghostA);
            }
        }
    }

    // Spawn-in flicker: enemies fade in with rapid flashing during spawn animation
    if (tag.find("enemy") != std::string::npos && entity.hasComponent<AIComponent>()) {
        float spawnT = entity.getComponent<AIComponent>().spawnTimer;
        if (spawnT > 0) {
            // Rapid flicker (on/off) that stabilizes as timer approaches 0
            float flickerRate = 10.0f + spawnT * 30.0f; // faster flicker at start
            bool visible = static_cast<int>(spawnT * flickerRate) % 2 == 0;
            if (!visible) return; // skip rendering this frame (flicker off)
            alpha *= (1.0f - spawnT * 1.5f); // fade in (0.4s -> starts at ~0.4 alpha)
            alpha = std::max(alpha, 0.15f);   // never fully invisible
        }
    }

    // Hybrid rendering: try sprite first, fall back to procedural
    if (renderSprite(renderer, screenRect, entity, alpha)) {
        // Sprite rendered successfully — skip procedural, but still apply overlays below
    } else
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
        if (bt == 4) renderVoidSovereign(renderer, screenRect, entity, alpha);
        else if (bt == 3) renderTemporalWeaver(renderer, screenRect, entity, alpha);
        else if (bt == 2) renderDimensionalArchitect(renderer, screenRect, entity, alpha);
        else if (bt == 1) renderVoidWyrm(renderer, screenRect, entity, alpha);
        else renderBoss(renderer, screenRect, entity, alpha);
    } else if (tag == "player_turret") {
        renderPlayerTurret(renderer, screenRect, entity, alpha);
    } else if (tag == "player_trap") {
        renderShockTrap(renderer, screenRect, entity, alpha);
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

    // Freeze tint: blue overlay while enemy is slowed by ice weapon
    if (tag.find("enemy") != std::string::npos && entity.hasComponent<AIComponent>()) {
        float freezeT = entity.getComponent<AIComponent>().freezeTimer;
        if (freezeT > 0) {
            // Pulsing blue tint intensity
            float pulse = 0.5f + 0.5f * std::sin(freezeT * 10.0f);
            Uint8 fa = static_cast<Uint8>(80 * pulse * alpha);
            fillRect(renderer, screenRect.x, screenRect.y, screenRect.w, screenRect.h,
                     100, 180, 255, fa);
        }
    }

    // HP bar for regular enemies (visible briefly after taking damage)
    if (tag.find("enemy") != std::string::npos && entity.hasComponent<HealthComponent>()) {
        auto& hpShow = entity.getComponent<HealthComponent>();
        bool hasOwnBar = false;
        if (entity.hasComponent<AIComponent>()) {
            auto& aiCheck = entity.getComponent<AIComponent>();
            hasOwnBar = aiCheck.isElite || aiCheck.isMiniBoss || tag == "enemy_boss";
        }
        if (!hasOwnBar && hpShow.damageShowTimer > 0 && hpShow.getPercent() < 1.0f) {
            float fadeAlpha = std::min(1.0f, hpShow.damageShowTimer / 0.5f); // fade out in last 0.5s
            int barW = screenRect.w + 4;
            int barH = 3;
            int barX = screenRect.x - 2;
            int barY = screenRect.y - 6;
            Uint8 barA = static_cast<Uint8>(180 * fadeAlpha * alpha);
            // Background
            fillRect(renderer, barX, barY, barW, barH, 20, 10, 10, barA);
            // HP fill — green to yellow to red based on HP percent
            float pct = hpShow.getPercent();
            Uint8 hpR = pct > 0.5f ? static_cast<Uint8>((1.0f - pct) * 2 * 255) : 255;
            Uint8 hpG = pct > 0.5f ? 220 : static_cast<Uint8>(pct * 2 * 220);
            SDL_Color hpCol = applyColorBlind({hpR, hpG, 20, 255});
            int fillW = std::max(1, static_cast<int>(barW * pct));
            fillRect(renderer, barX, barY, fillW, barH, hpCol.r, hpCol.g, hpCol.b, barA);
            // Thin border
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, static_cast<Uint8>(120 * fadeAlpha * alpha));
            SDL_Rect barBorder = {barX, barY, barW, barH};
            SDL_RenderDrawRect(renderer, &barBorder);
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

        // Juggle visual: ascending arrows + hit count dots
        if (ai.state == AIState::Juggled) {
            float time = SDL_GetTicks() * 0.006f;
            int centerX = screenRect.x + screenRect.w / 2;
            int topY = screenRect.y - 6;
            Uint8 ja = static_cast<Uint8>(200 * alpha);

            // Ascending arrow indicators (three moving upward)
            for (int i = 0; i < 3; i++) {
                float offset = std::fmod(time + i * 0.7f, 2.0f);
                int arrowY = topY - static_cast<int>(offset * 10);
                int arrowX = centerX - 6 + i * 6;
                Uint8 arrowA = static_cast<Uint8>(ja * (1.0f - offset / 2.0f));
                drawLine(renderer, arrowX, arrowY + 3, arrowX + 2, arrowY, 180, 140, 255, arrowA);
                drawLine(renderer, arrowX + 4, arrowY + 3, arrowX + 2, arrowY, 180, 140, 255, arrowA);
            }

            // Hit count dots
            int hits = ai.juggleHitCount;
            for (int i = 0; i < hits && i < 8; i++) {
                int dotX = centerX - hits * 2 + i * 4;
                int dotY = topY - 14;
                fillRect(renderer, dotX, dotY, 3, 3, 255, 200, 255, ja);
            }
        }

        // Stun visual: rotating stars above stunned enemies
        if (ai.state == AIState::Stunned) {
            float time = SDL_GetTicks() * 0.005f;
            int starCount = 3;
            int orbitR = screenRect.w / 2 + 4;
            int centerX = screenRect.x + screenRect.w / 2;
            int starY = screenRect.y - 8;
            for (int i = 0; i < starCount; i++) {
                float angle = time + i * (2.0f * 3.14159f / starCount);
                int sx = centerX + static_cast<int>(orbitR * std::cos(angle));
                int sy = starY + static_cast<int>(4 * std::sin(angle * 2.0f));
                Uint8 sa = static_cast<Uint8>(220 * alpha);
                fillRect(renderer, sx - 2, sy - 2, 4, 4, 255, 255, 100, sa);
                fillRect(renderer, sx - 1, sy - 3, 2, 6, 255, 255, 200, sa);
                fillRect(renderer, sx - 3, sy - 1, 6, 2, 255, 255, 200, sa);
            }
        }

        // Elite modifier aura + HP bar
        if (ai.isElite) {
            float time = ai.eliteGlowTimer * 3.0f;
            float pulse = 0.4f + 0.6f * std::abs(std::sin(time));
            Uint8 elR = 255, elG = 255, elB = 255;
            switch (ai.eliteMod) {
                case EliteModifier::Berserker:  elR = 255; elG = 50;  elB = 30; break;
                case EliteModifier::Shielded:   elR = 60;  elG = 140; elB = 255; break;
                case EliteModifier::Teleporter:  elR = 160; elG = 60;  elB = 220; break;
                case EliteModifier::Splitter:    elR = 60;  elG = 220; elB = 80; break;
                case EliteModifier::Vampiric:    elR = 180; elG = 20;  elB = 40; break;
                case EliteModifier::Explosive:   elR = 255; elG = 160; elB = 30; break;
                default: break;
            }
            Uint8 eliteGlowA = static_cast<Uint8>(45 * pulse * alpha);
            SDL_Rect elGlow = {screenRect.x - 4, screenRect.y - 4, screenRect.w + 8, screenRect.h + 8};
            SDL_SetRenderDrawColor(renderer, elR, elG, elB, eliteGlowA);
            SDL_RenderFillRect(renderer, &elGlow);
            SDL_SetRenderDrawColor(renderer, elR, elG, elB, static_cast<Uint8>(100 * pulse * alpha));
            SDL_RenderDrawRect(renderer, &elGlow);

            // Shielded: draw shield bar below HP
            if (ai.eliteMod == EliteModifier::Shielded && ai.eliteShieldHP > 0) {
                int shBarW = screenRect.w + 8;
                int shBarH = 3;
                int shBarX = screenRect.x - 4;
                int shBarY = screenRect.y - 6;
                Uint8 shA = static_cast<Uint8>(200 * alpha);
                fillRect(renderer, shBarX, shBarY, shBarW, shBarH, 20, 30, 60, shA);
                int shFillW = static_cast<int>(shBarW * (ai.eliteShieldHP / 30.0f));
                fillRect(renderer, shBarX, shBarY, shFillW, shBarH, 80, 150, 255, shA);
            }

            // Elite HP bar
            if (entity.hasComponent<HealthComponent>()) {
                auto& elHP = entity.getComponent<HealthComponent>();
                int elBarW = screenRect.w + 8;
                int elBarH = 4;
                int elBarX = screenRect.x - 4;
                int elBarY = screenRect.y - 10;
                Uint8 barA = static_cast<Uint8>(200 * alpha);
                fillRect(renderer, elBarX, elBarY, elBarW, elBarH, 25, 15, 10, barA);
                int fillW = static_cast<int>(elBarW * elHP.getPercent());
                fillRect(renderer, elBarX, elBarY, fillW, elBarH, elR, elG, elB, barA);
            }
        }

        // Mini-boss golden aura + HP bar
        if (ai.isMiniBoss) {
            float time = SDL_GetTicks() * 0.003f;
            float pulse = 0.5f + 0.5f * std::sin(time * 2.0f);
            Uint8 auraA = static_cast<Uint8>(50 * pulse * alpha);
            SDL_Rect mbGlow = {screenRect.x - 5, screenRect.y - 5, screenRect.w + 10, screenRect.h + 10};
            SDL_SetRenderDrawColor(renderer, 255, 200, 50, auraA);
            SDL_RenderFillRect(renderer, &mbGlow);
            SDL_SetRenderDrawColor(renderer, 255, 220, 80, static_cast<Uint8>(120 * pulse * alpha));
            SDL_RenderDrawRect(renderer, &mbGlow);

            // Mini-boss HP bar above entity
            if (entity.hasComponent<HealthComponent>()) {
                auto& mbHP = entity.getComponent<HealthComponent>();
                int mbBarW = screenRect.w + 10;
                int mbBarH = 4;
                int mbBarX = screenRect.x - 5;
                int mbBarY = screenRect.y - 10;
                Uint8 barA = static_cast<Uint8>(200 * alpha);
                fillRect(renderer, mbBarX, mbBarY, mbBarW, mbBarH, 30, 20, 10, barA);
                int fillW = static_cast<int>(mbBarW * mbHP.getPercent());
                fillRect(renderer, mbBarX, mbBarY, fillW, mbBarH, 255, 200, 50, barA);
            }
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

    // Charge glow: golden aura intensifies with charge
    if (entity.hasComponent<CombatComponent>()) {
        auto& cb = entity.getComponent<CombatComponent>();
        if (cb.isCharging && cb.chargeTimer >= cb.minChargeTime) {
            float cp = cb.getChargePercent();
            Uint8 glowA = static_cast<Uint8>(40 + 120 * cp);
            int glowR = static_cast<int>(4 + 8 * cp);
            SDL_Rect glow = {x - glowR, y - glowR, w + glowR * 2, h + glowR * 2};
            SDL_SetRenderDrawColor(renderer, 255, 200, 50, static_cast<Uint8>(glowA * alpha));
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, &glow);
            // Brighter inner glow
            SDL_Rect innerGlow = {x - glowR / 2, y - glowR / 2, w + glowR, h + glowR};
            SDL_SetRenderDrawColor(renderer, 255, 255, 150, static_cast<Uint8>(glowA * 0.5f * alpha));
            SDL_RenderFillRect(renderer, &innerGlow);
        }

        // Parry success aura: golden rim when next hit is guaranteed crit
        if (cb.parrySuccessTimer > 0) {
            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.008f);
            Uint8 rimA = static_cast<Uint8>((80 + 80 * pulse) * alpha);
            SDL_Rect rim = {x - 3, y - 3, w + 6, h + 6};
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, rimA);
            SDL_RenderDrawRect(renderer, &rim);
            SDL_Rect rim2 = {x - 2, y - 2, w + 4, h + 4};
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, static_cast<Uint8>(rimA * 0.7f));
            SDL_RenderDrawRect(renderer, &rim2);
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
    if (entity.hasComponent<HealthComponent>()) {
        auto& hpComp = entity.getComponent<HealthComponent>();
        int barW = w + 24;
        int barH = 5;
        int bX = x - 12;
        int bY = y - 16;
        fillRect(renderer, bX, bY, barW, barH, 20, 20, 50, a);
        fillRect(renderer, bX, bY, static_cast<int>(barW * hpComp.getPercent()), barH, 100, 140, 255, a);
        fillRect(renderer, bX + barW * 2 / 3, bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
        fillRect(renderer, bX + barW / 3, bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
    }
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
    if (entity.hasComponent<HealthComponent>()) {
        auto& hpComp = entity.getComponent<HealthComponent>();
        int barW = w + 24;
        int barH = 5;
        int bX = x - 12;
        int bY = y - 16;
        fillRect(renderer, bX, bY, barW, barH, 30, 25, 10, a);
        fillRect(renderer, bX, bY, static_cast<int>(barW * hpComp.getPercent()), barH, 220, 190, 80, a);
        fillRect(renderer, bX + barW * 2 / 3, bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
        fillRect(renderer, bX + barW / 3, bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.4f));
    }
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

    // HP bar with phase markers at 70% and 40%
    if (entity.hasComponent<HealthComponent>()) {
        auto& hpComp = entity.getComponent<HealthComponent>();
        int barW = w + 30;
        int barH = 6;
        int bX = x - 15;
        int bY = y - 18;
        fillRect(renderer, bX, bY, barW, barH, 30, 10, 40, a);
        // HP fill - color changes by phase
        Uint8 hpR = (phase == 1) ? 120 : (phase == 2) ? 180 : 255;
        Uint8 hpG = 0;
        Uint8 hpB = (phase == 1) ? 180 : (phase == 2) ? 100 : 50;
        fillRect(renderer, bX, bY, static_cast<int>(barW * hpComp.getPercent()), barH, hpR, hpG, hpB, a);
        // Phase markers at 70% and 40%
        fillRect(renderer, bX + static_cast<int>(barW * 0.7f), bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.5f));
        fillRect(renderer, bX + static_cast<int>(barW * 0.4f), bY, 1, barH, 255, 255, 255, static_cast<Uint8>(a * 0.5f));
    }
}

void RenderSystem::renderPlayerTurret(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    int cx = x + w / 2;
    bool flipped = entity.hasComponent<SpriteComponent>() && entity.getComponent<SpriteComponent>().flipX;

    // Lifetime glow: pulse faster as turret nears expiry
    float lifePct = 1.0f;
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        lifePct = std::max(0.0f, hp.currentHP / std::max(0.01f, hp.maxHP));
    }
    float time = SDL_GetTicks() * 0.001f;
    float pulse = 0.7f + 0.3f * std::sin(time * (3.0f + (1.0f - lifePct) * 8.0f));

    // Base platform
    fillRect(renderer, x - 2, y + h - 6, w + 4, 6, 180, 140, 40, a);
    fillRect(renderer, x, y + h - 8, w, 4, 160, 120, 30, a);

    // Main body (small square)
    Uint8 bodyR = static_cast<Uint8>(200 * pulse);
    Uint8 bodyG = static_cast<Uint8>(160 * pulse);
    Uint8 bodyB = static_cast<Uint8>(40 * pulse);
    fillRect(renderer, x + 2, y + 2, w - 4, h - 10, bodyR, bodyG, bodyB, a);
    // Inner detail
    fillRect(renderer, x + 4, y + 4, w - 8, h - 14, 170, 130, 30, a);

    // Barrel (extends in facing direction)
    int barrelY = y + h / 3;
    int barrelLen = w / 2 + 4;
    if (!flipped) {
        fillRect(renderer, x + w - 2, barrelY, barrelLen, 4, 200, 170, 50, a);
        fillRect(renderer, x + w + barrelLen - 3, barrelY - 1, 3, 6, 230, 200, 80, a);
        // Muzzle glow
        fillRect(renderer, x + w + barrelLen - 1, barrelY, 2, 4,
                 255, 220, static_cast<Uint8>(80 * pulse), a);
    } else {
        fillRect(renderer, x - barrelLen + 2, barrelY, barrelLen, 4, 200, 170, 50, a);
        fillRect(renderer, x - barrelLen, barrelY - 1, 3, 6, 230, 200, 80, a);
        fillRect(renderer, x - barrelLen, barrelY, 2, 4,
                 255, 220, static_cast<Uint8>(80 * pulse), a);
    }

    // Top indicator light (yellow/orange glow)
    fillRect(renderer, cx - 2, y, 4, 3, 255, 200, static_cast<Uint8>(50 * pulse), a);

    // Lifetime bar underneath
    if (lifePct < 1.0f) {
        int barW2 = w + 4;
        fillRect(renderer, x - 2, y + h + 2, barW2, 2, 40, 30, 20, static_cast<Uint8>(a * 0.6f));
        Uint8 lR = static_cast<Uint8>(230 * lifePct + 200 * (1.0f - lifePct));
        Uint8 lG = static_cast<Uint8>(180 * lifePct + 50 * (1.0f - lifePct));
        fillRect(renderer, x - 2, y + h + 2, static_cast<int>(barW2 * lifePct), 2, lR, lG, 40, a);
    }
}

void RenderSystem::renderShockTrap(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    int cx = x + w / 2;
    int cy = y + h / 2;
    float time = SDL_GetTicks() * 0.001f;

    // Pulse effect (flat diamond shape)
    float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
    Uint8 trapR = 255;
    Uint8 trapG = static_cast<Uint8>(180 + 50 * pulse);
    Uint8 trapB = static_cast<Uint8>(30 + 70 * pulse);

    // Diamond shape using 4 triangles approximated by rotated rects
    // Top half diamond
    for (int dy = -h / 2; dy <= 0; dy++) {
        int halfW = static_cast<int>((w / 2) * (1.0f - static_cast<float>(-dy) / (h / 2)));
        SDL_SetRenderDrawColor(renderer, trapR, trapG, trapB, a);
        SDL_RenderDrawLine(renderer, cx - halfW, cy + dy, cx + halfW, cy + dy);
    }
    // Bottom half diamond
    for (int dy = 0; dy <= h / 2; dy++) {
        int halfW = static_cast<int>((w / 2) * (1.0f - static_cast<float>(dy) / (h / 2)));
        SDL_SetRenderDrawColor(renderer, trapR, trapG, trapB, a);
        SDL_RenderDrawLine(renderer, cx - halfW, cy + dy, cx + halfW, cy + dy);
    }

    // Inner glow (smaller diamond)
    Uint8 innerA = static_cast<Uint8>(180 * pulse);
    SDL_SetRenderDrawColor(renderer, 255, 255, 100, innerA);
    for (int dy = -h / 4; dy <= h / 4; dy++) {
        int halfW = static_cast<int>((w / 4) * (1.0f - static_cast<float>(std::abs(dy)) / (h / 4)));
        SDL_RenderDrawLine(renderer, cx - halfW, cy + dy, cx + halfW, cy + dy);
    }

    // Electric spark lines (animated)
    SDL_SetRenderDrawColor(renderer, 200, 255, 255, static_cast<Uint8>(120 * pulse));
    for (int i = 0; i < 4; i++) {
        float angle = time * 3.0f + i * 1.5708f; // 90 degree spacing, rotating
        int sx = cx + static_cast<int>(std::cos(angle) * (w / 2 + 2));
        int sy = cy + static_cast<int>(std::sin(angle) * (h / 2 + 1));
        int ex = cx + static_cast<int>(std::cos(angle) * (w / 2 + 6));
        int ey = cy + static_cast<int>(std::sin(angle) * (h / 2 + 3));
        SDL_RenderDrawLine(renderer, sx, sy, ex, ey);
    }
}
