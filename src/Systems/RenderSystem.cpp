#include "RenderSystem.h"
#include <tracy/Tracy.hpp>
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

    // Apply color modulation and alpha (run through color-blind filter)
    SDL_Color filteredColor = applyColorBlind(sprite.color);

    // Hit flash: brief white tint when entity takes damage
    if (entity.hasComponent<HealthComponent>()) {
        float flashT = entity.getComponent<HealthComponent>().hitFlashTimer;
        if (flashT > 0) {
            float intensity = flashT / HealthComponent::HIT_FLASH_DURATION;
            filteredColor.r = static_cast<Uint8>(std::min(255, static_cast<int>(filteredColor.r + 120 * intensity)));
            filteredColor.g = static_cast<Uint8>(std::min(255, static_cast<int>(filteredColor.g + 120 * intensity)));
            filteredColor.b = static_cast<Uint8>(std::min(255, static_cast<int>(filteredColor.b + 120 * intensity)));
        }
    }

    SDL_SetTextureColorMod(sprite.texture, filteredColor.r, filteredColor.g, filteredColor.b);
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
    ZoneScopedN("RenderEntities");
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
    } else if (tag == "enemy_teleporter") {
        renderTeleporter(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_reflector") {
        renderReflector(renderer, screenRect, entity, alpha);
    } else if (tag == "enemy_leech") {
        renderLeech(renderer, screenRect, entity, alpha);
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

    // Hit flash: overlay when enemy just took damage (differentiated by weight class)
    if (tag.find("enemy") != std::string::npos && entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();

        // Determine flash color + duration by enemy weight class
        Uint8 flashR = 255, flashG = 255, flashB = 255;
        float flashDuration = 0.12f; // default
        if (entity.hasComponent<AIComponent>()) {
            auto eType = entity.getComponent<AIComponent>().enemyType;
            if (eType == EnemyType::Shielder || eType == EnemyType::Charger) {
                // Heavy: yellow-orange flash, shorter
                flashR = 255; flashG = 200; flashB = 60;
                flashDuration = 0.08f;
            } else if (eType == EnemyType::Flyer || eType == EnemyType::Crawler) {
                // Light: white flash, longer
                flashDuration = 0.16f;
            } else if (eType == EnemyType::Exploder) {
                // Exploder: red flash, danger
                flashR = 255; flashG = 60; flashB = 40;
                flashDuration = 0.15f;
            }
        }

        float flashStart = hp.invincibilityTime - flashDuration;
        if (hp.invincibilityTimer > flashStart && hp.invincibilityTimer > 0) {
            float flashT = (hp.invincibilityTimer - flashStart) / flashDuration;
            Uint8 fa = static_cast<Uint8>(180 * flashT * alpha);
            fillRect(renderer, screenRect.x, screenRect.y, screenRect.w, screenRect.h,
                     flashR, flashG, flashB, fa);
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
