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

    // Scale sprite to be visually larger than collision box, anchored at bottom-center.
    // Collision boxes are small for precise hit detection; sprites need to be 2x taller
    // to look correct. Aspect ratio is preserved from the sprite source dimensions.
    if (srcRect.w > 0 && srcRect.h > 0 && rect.w > 0 && rect.h > 0) {
        float spriteAR = static_cast<float>(srcRect.w) / srcRect.h;
        int renderH = rect.h * 2;
        int renderW = static_cast<int>(renderH * spriteAR);

        // Anchor at bottom-center of collision box
        int centerX = rect.x + rect.w / 2;
        int bottomY = rect.y + rect.h;
        rect.x = centerX - renderW / 2;
        rect.y = bottomY - renderH;
        rect.w = renderW;
        rect.h = renderH;
    }

    // Apply color modulation and alpha (run through color-blind filter)
    SDL_Color filteredColor = applyColorBlind(sprite.color);

    // Hit flash: brief white tint when entity takes damage
    float hitFlashIntensity = 0;
    if (entity.hasComponent<HealthComponent>()) {
        float flashT = entity.getComponent<HealthComponent>().hitFlashTimer;
        if (flashT > 0) {
            hitFlashIntensity = flashT / HealthComponent::HIT_FLASH_DURATION;
            // Stronger white tint (was +120, now +200)
            int boost = static_cast<int>(200 * hitFlashIntensity);
            filteredColor.r = static_cast<Uint8>(std::min(255, static_cast<int>(filteredColor.r) + boost));
            filteredColor.g = static_cast<Uint8>(std::min(255, static_cast<int>(filteredColor.g) + boost));
            filteredColor.b = static_cast<Uint8>(std::min(255, static_cast<int>(filteredColor.b) + boost));

            // Hit scale pulse: expand 10% during flash, then shrink back
            float scalePulse = 1.0f + 0.1f * hitFlashIntensity;
            int newW = static_cast<int>(rect.w * scalePulse);
            int newH = static_cast<int>(rect.h * scalePulse);
            rect.x -= (newW - rect.w) / 2;
            rect.y -= (newH - rect.h) / 2;
            rect.w = newW;
            rect.h = newH;
        }
    }

    // Flip based on facing direction
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (sprite.flipX) flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_HORIZONTAL);
    if (sprite.flipY) flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_VERTICAL);

    // Code-driven animation transforms (single-frame sprites animated via rect/angle)
    double renderAngle = 0.0;
    {
        float t = m_frameTicks * 0.001f; // Time in seconds (cached per-frame)
        AnimState state = sprite.animState;

        switch (state) {
            case AnimState::Idle: {
                // Gentle vertical bob (sine wave, ±2px at ~1.5Hz)
                float bob = std::sin(t * 3.0f) * 2.0f;
                rect.y += static_cast<int>(bob);
                break;
            }
            case AnimState::Run: {
                // Faster bob (±3px at 6Hz) + slight lean
                float bob = std::sin(t * 12.0f) * 3.0f;
                rect.y += static_cast<int>(bob);
                renderAngle = std::sin(t * 6.0f) * 3.0; // ±3° tilt
                break;
            }
            case AnimState::Jump: {
                // Upward stretch: slightly taller, narrower (squash/stretch)
                int stretchH = rect.h / 16;
                int squashW = rect.w / 24;
                rect.y -= stretchH;
                rect.h += stretchH;
                rect.x += squashW;
                rect.w -= squashW * 2;
                break;
            }
            case AnimState::Fall: {
                // Downward squash: slightly wider, shorter
                int squashH = rect.h / 16;
                int stretchW = rect.w / 24;
                rect.h -= squashH;
                rect.x -= stretchW;
                rect.w += stretchW * 2;
                break;
            }
            case AnimState::Dash: {
                // Horizontal stretch in movement direction
                int stretch = rect.w / 6;
                rect.w += stretch;
                if (!sprite.flipX) rect.x -= stretch / 4;
                else rect.x -= stretch * 3 / 4;
                int squash = rect.h / 10;
                rect.y += squash;
                rect.h -= squash;
                break;
            }
            case AnimState::WallSlide: {
                // Slight vertical stretch, lean into wall
                int stretchH = rect.h / 20;
                rect.h += stretchH;
                renderAngle = sprite.flipX ? -5.0 : 5.0; // Lean toward wall
                break;
            }
            case AnimState::Attack: {
                // Quick forward punch scale + rotation
                float attackT = std::fmod(t * 8.0f, 1.0f); // Fast cycle
                if (attackT < 0.3f) {
                    float punch = attackT / 0.3f;
                    int scaleW = static_cast<int>(rect.w * 0.15f * punch);
                    rect.w += scaleW;
                    if (!sprite.flipX) rect.x -= scaleW / 4;
                    else rect.x -= scaleW * 3 / 4;
                    renderAngle = (sprite.flipX ? 8.0 : -8.0) * punch;
                }
                break;
            }
            case AnimState::Hurt: {
                // Rapid horizontal shake (±4px at 30Hz)
                float shake = std::sin(t * 60.0f) * 4.0f;
                rect.x += static_cast<int>(shake);
                break;
            }
            case AnimState::Dead: {
                // Slow tilt to ground + shrink
                renderAngle = 90.0; // Fallen over
                int shrink = rect.w / 8;
                rect.x += shrink / 2;
                rect.w -= shrink;
                rect.y += shrink / 2;
                rect.h -= shrink;
                break;
            }
        }
    }

    // Outline pass: draw sprite 4x in black at 1px offsets (Hollow Knight method)
    // Skip for tiny entities or low alpha
    if (alpha > 0.4f && rect.w >= 8 && rect.h >= 8) {
        SDL_SetTextureColorMod(sprite.texture, 0, 0, 0);
        SDL_SetTextureAlphaMod(sprite.texture, static_cast<Uint8>(200 * alpha));
        SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND);
        static const int offsets[][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto& off : offsets) {
            SDL_Rect outlineRect = {rect.x + off[0], rect.y + off[1], rect.w, rect.h};
            SDL_RenderCopyEx(renderer, sprite.texture, &srcRect, &outlineRect, renderAngle, nullptr, flip);
        }
    }

    // Normal sprite pass
    SDL_SetTextureColorMod(sprite.texture, filteredColor.r, filteredColor.g, filteredColor.b);
    SDL_SetTextureAlphaMod(sprite.texture, static_cast<Uint8>(sprite.color.a * alpha));
    SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND);
    SDL_RenderCopyEx(renderer, sprite.texture, &srcRect, &rect, renderAngle, nullptr, flip);

    // Additive white glow overlay during hit flash (makes hit POP visually)
    if (hitFlashIntensity > 0.3f) {
        SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_ADD);
        Uint8 addA = static_cast<Uint8>(hitFlashIntensity * 150 * alpha);
        SDL_SetTextureColorMod(sprite.texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(sprite.texture, addA);
        SDL_RenderCopyEx(renderer, sprite.texture, &srcRect, &rect, renderAngle, nullptr, flip);
        // Restore blend mode
        SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND);
    }

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
    m_frameTicks = SDL_GetTicks(); // Cache once per frame
    m_renderEntries.clear(); // Reuse capacity from previous frame

    entities.forEach([&](Entity& e) {
        if (!e.visible || !e.hasComponent<SpriteComponent>() || !e.hasComponent<TransformComponent>())
            return;

        // Frustum culling: skip entities far off-screen
        auto& tf = e.getComponent<TransformComponent>();
        if (!camera.isInView(tf.position.x, tf.position.y, tf.width, tf.height))
            return;

        auto& sprite = e.getComponent<SpriteComponent>();
        float alpha = 1.0f;

        if (e.dimension == 0) {
            alpha = 1.0f;
        } else if (e.dimension == currentDimension) {
            alpha = 1.0f;
        } else {
            // Ghost shimmer for other-dimension entities
            bool isEnemy = e.isEnemy;
            if (isEnemy) {
                float shimmer = 0.15f + 0.1f * std::sin(m_frameTicks * 0.005f + e.dimension * 3.14f);
                alpha = shimmer;
            } else {
                alpha = dimBlendAlpha * 0.3f;
            }
            if (alpha < 0.05f) return;
        }

        m_renderEntries.push_back({&e, sprite.renderLayer, alpha});
    });

    std::sort(m_renderEntries.begin(), m_renderEntries.end(),
        [](const RenderEntry& a, const RenderEntry& b) { return a.layer < b.layer; });

    for (auto& entry : m_renderEntries) {
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
    constexpr int CULL_MARGIN = 200;
    int screenW = GameState::SCREEN_WIDTH;
    int screenH = GameState::SCREEN_HEIGHT;
    if (screenRect.x + screenRect.w < -CULL_MARGIN || screenRect.x > screenW + CULL_MARGIN ||
        screenRect.y + screenRect.h < -CULL_MARGIN || screenRect.y > screenH + CULL_MARGIN) {
        return;
    }

    const auto rType = entity.renderType;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Pickup hover animation: bob up/down + pulsing glow ring
    if (entity.isPickup) {
        // Sine-wave hover (±3 pixel bob)
        float hover = std::sin(m_frameTicks * 0.004f + entity.dimension * 1.5f) * 3.0f;
        screenRect.y += static_cast<int>(hover);
        // Pulsing glow aura behind pickup
        float pulse = 0.5f + 0.5f * std::sin(m_frameTicks * 0.005f + entity.dimension * 2.0f);
        auto& sprite = entity.getComponent<SpriteComponent>();
        Uint8 glowA = static_cast<Uint8>(20 + 25 * pulse * alpha);
        int glowExpand = 4 + static_cast<int>(pulse * 3);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(renderer, sprite.color.r, sprite.color.g, sprite.color.b, glowA);
        SDL_Rect glowRect = {
            screenRect.x - glowExpand, screenRect.y - glowExpand,
            screenRect.w + glowExpand * 2, screenRect.h + glowExpand * 2
        };
        SDL_RenderFillRect(renderer, &glowRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

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

    // Spawn-in effect: dimensional rift opening + scale-up + white tint
    bool spawning = false;
    float spawnProgress = 1.0f;
    if (entity.isEnemy && entity.hasComponent<AIComponent>()) {
        auto& ai = entity.getComponent<AIComponent>();
        if (ai.spawnTimer > 0 && ai.spawnTimerInitial > 0) {
            spawning = true;
            spawnProgress = 1.0f - (ai.spawnTimer / ai.spawnTimerInitial); // 0→1

            // Phase 1 (0-40%): dimensional rift opening (just the portal, entity invisible)
            // Phase 2 (40-100%): entity materializes from rift
            if (spawnProgress < 0.4f) {
                float riftT = spawnProgress / 0.4f; // 0→1 within rift phase
                // Render expanding rift ring
                int centerX = screenRect.x + screenRect.w / 2;
                int centerY = screenRect.y + screenRect.h / 2;
                float riftSize = screenRect.w * 0.5f * riftT;
                Uint8 riftA = static_cast<Uint8>(180 * (1.0f - riftT * 0.5f) * alpha);

                // Outer glow ring
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
                for (int ring = 2; ring >= 0; ring--) {
                    int expand = ring * 3;
                    Uint8 ra = static_cast<Uint8>(riftA / (ring + 1));
                    SDL_SetRenderDrawColor(renderer, 160, 80, 255, ra);
                    SDL_Rect riftRect = {
                        static_cast<int>(centerX - riftSize - expand),
                        static_cast<int>(centerY - riftSize * 0.3f - expand),
                        static_cast<int>(riftSize * 2 + expand * 2),
                        static_cast<int>(riftSize * 0.6f + expand * 2)
                    };
                    SDL_RenderFillRect(renderer, &riftRect);
                }
                // Bright center line
                SDL_SetRenderDrawColor(renderer, 220, 200, 255, riftA);
                SDL_Rect line = {
                    static_cast<int>(centerX - riftSize * 0.8f),
                    centerY - 1,
                    static_cast<int>(riftSize * 1.6f),
                    2
                };
                SDL_RenderFillRect(renderer, &line);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

                // Hide entity during rift phase
                alpha = 0;
            } else {
                float matT = (spawnProgress - 0.4f) / 0.6f; // 0→1 within materialize phase
                // Scale: grow from 30% to 100% (more dramatic)
                float scale = 0.3f + 0.7f * matT;
                int sw = static_cast<int>(screenRect.w * scale);
                int sh = static_cast<int>(screenRect.h * scale);
                screenRect.x += (screenRect.w - sw) / 2;
                screenRect.y += (screenRect.h - sh);
                screenRect.w = sw;
                screenRect.h = sh;
                // Fade in alpha with white tint
                alpha *= (0.2f + 0.8f * matT);
            }
        }
    }

    // Idle breathing now handled inside renderSprite() code-driven animation system

    // Hybrid rendering: try sprite first, fall back to procedural
    if (entity.getComponent<SpriteComponent>().texture) {
        // Soft gradient drop shadow (4 bands, decreasing alpha, expanding outward)
        int baseW = screenRect.w * 3 / 4;
        int baseH = std::max(2, screenRect.h / 5);
        int baseX = screenRect.x + (screenRect.w - baseW) / 2;
        int baseY = screenRect.y + screenRect.h - baseH / 3;
        for (int band = 3; band >= 0; band--) {
            int expand = band * 2;
            int bw = baseW + expand * 2;
            int bh = baseH + expand;
            int bx = baseX - expand;
            int by = baseY - expand / 2;
            // Alpha: inner band darkest (40), outer lightest (8)
            Uint8 sa = static_cast<Uint8>((10 + (3 - band) * 10) * alpha);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, sa);
            SDL_Rect shadowRect = {bx, by, bw, bh};
            SDL_RenderFillRect(renderer, &shadowRect);
        }
    }
    if (renderSprite(renderer, screenRect, entity, alpha)) {
        // Sprite rendered successfully — skip procedural, but still apply overlays below
    } else
    // Procedural rendering based on entity type (switch on cached enum — no string compares)
    switch (rType) {
    case EntityRenderType::Player:
        renderPlayer(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyWalker:
    case EntityRenderType::EnemyMinion:
        renderWalker(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyFlyer:
        renderFlyer(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyTurret:
        renderTurret(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyCharger:
        renderCharger(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyPhaser:
        renderPhaser(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyExploder:
        renderExploder(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyShielder:
        renderShielder(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyCrawler:
        renderCrawler(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemySummoner:
        renderSummoner(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemySniper:
        renderSniper(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyTeleporter:
        renderTeleporter(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyReflector:
        renderReflector(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyLeech:
        renderLeech(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemySwarmer:
        renderSwarmer(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyGravityWell:
        renderGravityWell(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyMimic:
        renderMimic(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyBoss: {
        int bt = 0;
        if (entity.hasComponent<AIComponent>()) bt = entity.getComponent<AIComponent>().bossType;
        if (bt == 5) renderEntropyIncarnate(renderer, screenRect, entity, alpha);
        else if (bt == 4) renderVoidSovereign(renderer, screenRect, entity, alpha);
        else if (bt == 3) renderTemporalWeaver(renderer, screenRect, entity, alpha);
        else if (bt == 2) renderDimensionalArchitect(renderer, screenRect, entity, alpha);
        else if (bt == 1) renderVoidWyrm(renderer, screenRect, entity, alpha);
        else renderBoss(renderer, screenRect, entity, alpha);
        break;
    }
    case EntityRenderType::PlayerTurret:
        renderPlayerTurret(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::PlayerTrap:
        renderShockTrap(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyCrate: {
        // Professional breakable crate with wood grain, metal bands, gradient
        auto& sprite = entity.getComponent<SpriteComponent>();
        Uint8 a = static_cast<Uint8>(sprite.color.a * alpha);
        int dx = screenRect.x, dy = screenRect.y, dw = screenRect.w, dh = screenRect.h;
        // Multi-layer shadow
        for (int s = 3; s >= 0; s--) {
            fillRect(renderer, dx + 1 + s, dy + dh + s, dw - 2, 3 - s,
                     0, 0, 0, static_cast<Uint8>(a * (0.12f + 0.06f * s)));
        }
        // Dark outline frame
        fillRect(renderer, dx, dy, dw, dh, 55, 35, 15, a);
        // Wood body with 3-band vertical gradient (lighter top, darker bottom)
        int bandH = dh / 3;
        fillRect(renderer, dx + 1, dy + 1, dw - 2, bandH, 160, 110, 55, a);
        fillRect(renderer, dx + 1, dy + 1 + bandH, dw - 2, bandH, 139, 90, 43, a);
        fillRect(renderer, dx + 1, dy + 1 + bandH * 2, dw - 2, dh - 2 - bandH * 2, 115, 75, 35, a);
        // Wood grain lines (horizontal)
        for (int gy = 0; gy < dh - 4; gy += 4) {
            int gx_off = (gy * 7) % 3;  // slight variation
            fillRect(renderer, dx + 3 + gx_off, dy + 2 + gy, dw - 6, 1,
                     100, 65, 28, static_cast<Uint8>(a * 0.3f));
        }
        // Horizontal plank seam (center)
        fillRect(renderer, dx + 2, dy + dh / 2 - 1, dw - 4, 2, 70, 45, 18, a);
        // Vertical plank seam (center)
        fillRect(renderer, dx + dw / 2 - 1, dy + 2, 2, dh - 4, 70, 45, 18, a);
        // Metal bands (top and bottom, darker with highlight)
        fillRect(renderer, dx + 1, dy + 2, dw - 2, 3, 90, 85, 75, a);
        fillRect(renderer, dx + 1, dy + 3, dw - 2, 1, 130, 125, 110, static_cast<Uint8>(a * 0.5f));
        fillRect(renderer, dx + 1, dy + dh - 5, dw - 2, 3, 90, 85, 75, a);
        fillRect(renderer, dx + 1, dy + dh - 4, dw - 2, 1, 130, 125, 110, static_cast<Uint8>(a * 0.5f));
        // Nails at band intersections
        fillRect(renderer, dx + 3, dy + 3, 2, 2, 180, 175, 160, a);
        fillRect(renderer, dx + dw - 5, dy + 3, 2, 2, 180, 175, 160, a);
        fillRect(renderer, dx + 3, dy + dh - 5, 2, 2, 180, 175, 160, a);
        fillRect(renderer, dx + dw - 5, dy + dh - 5, 2, 2, 180, 175, 160, a);
        // Top-left specular highlight
        fillRect(renderer, dx + 2, dy + 1, dw / 3, 1, 200, 160, 90, static_cast<Uint8>(a * 0.4f));
        break;
    }
    case EntityRenderType::PickupHealth:
    case EntityRenderType::PickupShield:
    case EntityRenderType::PickupSpeed:
    case EntityRenderType::PickupDamage:
    case EntityRenderType::PickupShard:
    case EntityRenderType::Pickup:
        renderPickup(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::Projectile:
        renderProjectile(renderer, screenRect, entity, alpha); break;
    case EntityRenderType::EnemyEntropyMinion:
    case EntityRenderType::EnemyShadowClone:
    case EntityRenderType::EnemyEcho:
    case EntityRenderType::DimResidue:
    case EntityRenderType::GrappleHook:
    case EntityRenderType::Unknown:
    default: {
        // Professional default renderer: layered body with glow, gradient, and details
        auto& sprite = entity.getComponent<SpriteComponent>();
        Uint8 a = static_cast<Uint8>(sprite.color.a * alpha);
        int dx = screenRect.x, dy = screenRect.y, dw = screenRect.w, dh = screenRect.h;
        Uint8 cr = sprite.color.r, cg = sprite.color.g, cb = sprite.color.b;

        // Soft multi-layer ground shadow
        for (int s = 3; s >= 0; s--) {
            int expand = s * 2;
            fillRect(renderer, dx - expand + 2, dy + dh + s, dw + expand * 2 - 4, 3,
                     0, 0, 0, static_cast<Uint8>(a * (0.10f + 0.05f * s)));
        }

        // Subtle glow aura (pulsing)
        Uint32 ticks = m_frameTicks;
        float glowPulse = 0.5f + 0.5f * std::sin(ticks * 0.004f);
        Uint8 ga = static_cast<Uint8>(20 * glowPulse * alpha);
        fillRect(renderer, dx - 3, dy - 3, dw + 6, dh + 6, cr, cg, cb, ga);
        fillRect(renderer, dx - 1, dy - 1, dw + 2, dh + 2, cr, cg, cb, static_cast<Uint8>(ga * 0.7f));

        // Dark outline
        fillRect(renderer, dx, dy, dw, dh,
                 static_cast<Uint8>(cr * 0.3f), static_cast<Uint8>(cg * 0.3f),
                 static_cast<Uint8>(cb * 0.3f), a);

        // 3-band gradient body
        int bandH = std::max(1, dh / 3);
        // Top: bright
        fillRect(renderer, dx + 1, dy + 1, dw - 2, bandH,
                 static_cast<Uint8>(std::min(255, cr + 30)),
                 static_cast<Uint8>(std::min(255, cg + 30)),
                 static_cast<Uint8>(std::min(255, cb + 25)), a);
        // Mid: base
        fillRect(renderer, dx + 1, dy + 1 + bandH, dw - 2, bandH, cr, cg, cb, a);
        // Bottom: dark
        fillRect(renderer, dx + 1, dy + 1 + bandH * 2, dw - 2, dh - 2 - bandH * 2,
                 static_cast<Uint8>(cr * 0.7f), static_cast<Uint8>(cg * 0.7f),
                 static_cast<Uint8>(cb * 0.7f), a);

        // Specular highlight (1px bright line at top)
        fillRect(renderer, dx + 2, dy + 1, dw - 4, 1,
                 255, 255, 255, static_cast<Uint8>(a * 0.25f));

        // Left-side highlight (3D effect)
        fillRect(renderer, dx + 1, dy + 2, 1, dh - 4,
                 static_cast<Uint8>(std::min(255, cr + 50)),
                 static_cast<Uint8>(std::min(255, cg + 50)),
                 static_cast<Uint8>(std::min(255, cb + 40)),
                 static_cast<Uint8>(a * 0.3f));

        // Glowing eyes (if entity is large enough)
        if (dw >= 10 && dh >= 10) {
            int eyeY2 = dy + dh / 4;
            // Eye glow
            fillRect(renderer, dx + dw / 3 - 1, eyeY2 - 1, 4, 4,
                     255, 255, 255, static_cast<Uint8>(a * 0.15f));
            fillRect(renderer, dx + 2 * dw / 3 - 3, eyeY2 - 1, 4, 4,
                     255, 255, 255, static_cast<Uint8>(a * 0.15f));
            // Eye cores
            fillRect(renderer, dx + dw / 3, eyeY2, 2, 2, 255, 255, 255, a);
            fillRect(renderer, dx + 2 * dw / 3 - 2, eyeY2, 2, 2, 255, 255, 255, a);
        }
        break;
    }
    } // end switch

    // Hit flash: overlay when enemy just took damage (differentiated by weight class)
    if (entity.isEnemy && entity.hasComponent<HealthComponent>()) {
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

    // Spawn glow: white overlay that fades as enemy materializes
    if (spawning) {
        Uint8 glowA = static_cast<Uint8>(200 * (1.0f - spawnProgress) * alpha);
        fillRect(renderer, screenRect.x, screenRect.y, screenRect.w, screenRect.h,
                 255, 255, 255, glowA);
    }

    // Freeze tint: blue overlay while enemy is slowed by ice weapon
    if (entity.isEnemy && entity.hasComponent<AIComponent>()) {
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
    if (entity.isEnemy && entity.hasComponent<HealthComponent>()) {
        auto& hpShow = entity.getComponent<HealthComponent>();
        bool hasOwnBar = false;
        if (entity.hasComponent<AIComponent>()) {
            auto& aiCheck = entity.getComponent<AIComponent>();
            hasOwnBar = aiCheck.isElite || aiCheck.isMiniBoss || entity.isBoss;
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
    if (entity.isEnemy && entity.hasComponent<AIComponent>()) {
        auto& ai = entity.getComponent<AIComponent>();
        if (ai.element != EnemyElement::None) {
            float time = m_frameTicks * 0.004f;
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
            float time = m_frameTicks * 0.006f;
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
            float time = m_frameTicks * 0.005f;
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

        // Aggro alert: "!" exclamation mark when enemy first spots the player
        if (ai.aggroAlertTimer > 0) {
            float fade = ai.aggroAlertTimer / 0.5f; // 1→0 over 0.5s
            Uint8 alertA = static_cast<Uint8>(255 * fade * alpha);
            int cx = screenRect.x + screenRect.w / 2;
            int ey = screenRect.y - 8;
            // "!" as a vertical bar + dot
            fillRect(renderer, cx - 1, ey - 7, 3, 5, 255, 240, 80, alertA);
            fillRect(renderer, cx - 1, ey,     3, 2, 255, 240, 80, alertA);
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
            float time = m_frameTicks * 0.003f;
            float pulse = 0.5f + 0.5f * std::sin(time * 2.0f);
            Uint8 auraA = static_cast<Uint8>(50 * pulse * alpha);
            SDL_Rect mbGlow = {screenRect.x - 5, screenRect.y - 5, screenRect.w + 10, screenRect.h + 10};
            SDL_SetRenderDrawColor(renderer, 255, 200, 50, auraA);
            SDL_RenderFillRect(renderer, &mbGlow);
            SDL_SetRenderDrawColor(renderer, 255, 220, 80, static_cast<Uint8>(120 * pulse * alpha));
            SDL_RenderDrawRect(renderer, &mbGlow);

            // Mini-boss crown horns (two golden triangles above head)
            {
                Uint8 crownA = static_cast<Uint8>((180 + 75 * pulse) * alpha); // pulse 180-255
                int cx = screenRect.x + screenRect.w / 2;
                int hornTop = screenRect.y - 10; // 4px above entity top + 6px height
                // Left horn
                SDL_SetRenderDrawColor(renderer, 255, 200, 50, crownA);
                SDL_RenderDrawLine(renderer, cx - 3 - 2, hornTop + 6, cx - 3, hornTop);
                SDL_RenderDrawLine(renderer, cx - 3, hornTop, cx - 3 + 2, hornTop + 6);
                SDL_RenderDrawLine(renderer, cx - 3 - 2, hornTop + 6, cx - 3 + 2, hornTop + 6);
                // Right horn
                SDL_RenderDrawLine(renderer, cx + 3 - 2, hornTop + 6, cx + 3, hornTop);
                SDL_RenderDrawLine(renderer, cx + 3, hornTop, cx + 3 + 2, hornTop + 6);
                SDL_RenderDrawLine(renderer, cx + 3 - 2, hornTop + 6, cx + 3 + 2, hornTop + 6);
            }

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

    // Player edge glow: dimension-colored sprite outline to make player pop
    // against dark backgrounds. Uses 4-direction sprite offsets (like enemy
    // rim light) so the glow follows the actual sprite shape, not the
    // collision box — otherwise the player appears framed in a rectangle.
    if (entity.isPlayer && alpha > 0.5f && entity.hasComponent<SpriteComponent>() &&
        entity.getComponent<SpriteComponent>().texture) {
        Uint32 ticks = m_frameTicks;
        float pulse = 0.6f + 0.4f * std::sin(ticks * 0.003f);
        // Dimension color: blue for A, red for B
        Uint8 glowR, glowG, glowB;
        if (entity.dimension == 2) {
            glowR = 255; glowG = 80; glowB = 80;
        } else {
            glowR = 80; glowG = 140; glowB = 255;
        }

        // Recompute the sprite rect exactly the same way renderSprite() does
        // (2x collision height, anchored at bottom-center) so the glow lines
        // up with the actual drawn sprite.
        auto& spr = entity.getComponent<SpriteComponent>();
        SDL_Rect srcRect = spr.srcRect;
        if (entity.hasComponent<AnimationComponent>())
            srcRect = entity.getComponent<AnimationComponent>().getCurrentSrcRect();

        SDL_Rect spriteRect = screenRect;
        if (srcRect.w > 0 && srcRect.h > 0 && spriteRect.w > 0 && spriteRect.h > 0) {
            float spriteAR = static_cast<float>(srcRect.w) / srcRect.h;
            int renderH = spriteRect.h * 2;
            int renderW = static_cast<int>(renderH * spriteAR);
            int centerX = spriteRect.x + spriteRect.w / 2;
            int bottomY = spriteRect.y + spriteRect.h;
            spriteRect.x = centerX - renderW / 2;
            spriteRect.y = bottomY - renderH;
            spriteRect.w = renderW;
            spriteRect.h = renderH;
        }

        Uint8 glowA = static_cast<Uint8>(40 * pulse * alpha);
        SDL_RendererFlip flip = spr.flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

        SDL_SetTextureBlendMode(spr.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(spr.texture, glowR, glowG, glowB);
        SDL_SetTextureAlphaMod(spr.texture, glowA);
        // 4-direction pixel offsets (same pattern as enemy rim light)
        static const int glowOff[][2] = {{2,0},{-2,0},{0,2},{0,-2}};
        for (auto& off : glowOff) {
            SDL_Rect glowRect = {spriteRect.x + off[0], spriteRect.y + off[1], spriteRect.w, spriteRect.h};
            SDL_RenderCopyEx(renderer, spr.texture, &srcRect, &glowRect, 0.0, nullptr, flip);
        }
        SDL_SetTextureBlendMode(spr.texture, SDL_BLENDMODE_BLEND);
    }

    // Enemy rim light: element/type-specific colored additive outline
    if (entity.isEnemy && alpha > 0.4f &&
        entity.hasComponent<AIComponent>() && entity.hasComponent<SpriteComponent>() &&
        entity.getComponent<SpriteComponent>().texture) {
        auto& ai = entity.getComponent<AIComponent>();
        Uint8 rimR = 180, rimG = 180, rimB = 180; // Default: neutral white
        // Element-based rim color
        switch (static_cast<int>(ai.element)) {
            case 1: rimR = 255; rimG = 120; rimB = 40; break;  // Fire: orange
            case 2: rimR = 80;  rimG = 180; rimB = 255; break; // Ice: blue
            case 3: rimR = 255; rimG = 240; rimB = 60; break;  // Electric: yellow
            default: // Type-based fallback
                if (ai.enemyType == EnemyType::Boss) { rimR = 220; rimG = 40; rimB = 60; }
                else if (ai.isElite) { rimR = 200; rimG = 160; rimB = 40; }
                else if (ai.isMiniBoss) { rimR = 255; rimG = 200; rimB = 50; }
                else { rimR = 140; rimG = 100; rimB = 160; } // Normal: dim purple (void theme)
                break;
        }
        // Subtle additive rim (2px expanded sprite in rim color)
        auto& spr = entity.getComponent<SpriteComponent>();
        SDL_Rect srcRect2 = spr.srcRect;
        if (entity.hasComponent<AnimationComponent>())
            srcRect2 = entity.getComponent<AnimationComponent>().getCurrentSrcRect();
        SDL_RendererFlip flip2 = spr.flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        Uint8 rimA = static_cast<Uint8>(18 * alpha);
        SDL_SetTextureBlendMode(spr.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(spr.texture, rimR, rimG, rimB);
        SDL_SetTextureAlphaMod(spr.texture, rimA);
        static const int rimOff[][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto& off : rimOff) {
            SDL_Rect rimRect = {screenRect.x + off[0], screenRect.y + off[1], screenRect.w, screenRect.h};
            SDL_RenderCopyEx(renderer, spr.texture, &srcRect2, &rimRect, 0.0, nullptr, flip2);
        }
        SDL_SetTextureBlendMode(spr.texture, SDL_BLENDMODE_BLEND);
    }
}

void RenderSystem::renderPickup(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha) {
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    float time = m_frameTicks * 0.005f;

    // Floating bob
    int bob = static_cast<int>(std::sin(time) * 3);

    const auto pType = entity.renderType;
    if (pType == EntityRenderType::PickupHealth) {
        // Green cross
        int cy = y + bob;
        fillRect(renderer, x + w / 3, cy, w / 3, h, 60, 220, 60, a);
        fillRect(renderer, x, cy + h / 3, w, h / 3, 60, 220, 60, a);
        fillRect(renderer, x - 1, cy - 1, w + 2, h + 2, 80, 255, 80, static_cast<Uint8>(a * 0.2f));
    } else if (pType == EntityRenderType::PickupShield) {
        // Blue circle with inner ring
        int cy = y + bob;
        int cx = x + w / 2;
        fillRect(renderer, x + 1, cy + 1, w - 2, h - 2, 80, 160, 255, a);
        fillRect(renderer, cx - 3, cy + h / 3, 6, h / 3, 200, 230, 255, a);
        fillRect(renderer, x - 1, cy - 1, w + 2, h + 2, 100, 180, 255, static_cast<Uint8>(a * 0.3f));
    } else if (pType == EntityRenderType::PickupSpeed) {
        // Yellow lightning bolt shape
        int cy = y + bob;
        fillRect(renderer, x + w / 3, cy, w / 3, h / 2, 255, 255, 80, a);
        fillRect(renderer, x + w / 6, cy + h / 3, w * 2 / 3, h / 4, 255, 255, 80, a);
        fillRect(renderer, x + w / 3, cy + h / 2, w / 3, h / 2, 255, 255, 80, a);
        fillRect(renderer, x - 1, cy - 1, w + 2, h + 2, 255, 255, 120, static_cast<Uint8>(a * 0.25f));
    } else if (pType == EntityRenderType::PickupDamage) {
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
    auto& sprite = entity.getComponent<SpriteComponent>();
    Uint8 a = static_cast<Uint8>(255 * alpha);
    int cx = rect.x + rect.w / 2;
    int cy = rect.y + rect.h / 2;
    int r = rect.w / 2 + 1;

    // Determine projectile color from sprite
    Uint8 pr = sprite.color.r, pg = sprite.color.g, pb = sprite.color.b;

    // Additive outer glow (2 layers for soft look)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    fillRect(renderer, cx - r - 4, cy - r - 4, (r + 4) * 2, (r + 4) * 2, pr, pg, pb, static_cast<Uint8>(a * 0.15f));
    fillRect(renderer, cx - r - 2, cy - r - 2, (r + 2) * 2, (r + 2) * 2, pr, pg, pb, static_cast<Uint8>(a * 0.3f));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Core
    fillRect(renderer, cx - r, cy - r, r * 2, r * 2, pr, pg, pb, a);
    // Bright white center
    fillRect(renderer, cx - 1, cy - 1, 2, 2,
             static_cast<Uint8>(std::min(255, pr + 100)),
             static_cast<Uint8>(std::min(255, pg + 100)),
             static_cast<Uint8>(std::min(255, pb + 80)), a);

    // Trail behind projectile (longer, fading, with glow)
    if (entity.hasComponent<PhysicsBody>()) {
        auto& phys = entity.getComponent<PhysicsBody>();
        float speedSq = phys.velocity.x * phys.velocity.x + phys.velocity.y * phys.velocity.y;
        if (speedSq > 100.0f) { // > 10.0 speed
            float speed = std::sqrt(speedSq);
            float nx = -phys.velocity.x / speed;
            float ny = -phys.velocity.y / speed;
            // Extended trail (6 segments instead of 4, with size falloff)
            for (int i = 1; i <= 6; i++) {
                float falloff = 1.0f - i * 0.14f;
                int tx = cx + static_cast<int>(nx * i * 3.5f);
                int ty = cy + static_cast<int>(ny * i * 3.5f);
                int ts = std::max(1, static_cast<int>(r * falloff));
                Uint8 ta = static_cast<Uint8>(a * falloff * 0.7f);
                fillRect(renderer, tx - ts, ty - ts, ts * 2, ts * 2, pr, pg, pb, ta);
            }
            // Additive trail glow
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            for (int i = 1; i <= 3; i++) {
                int tx = cx + static_cast<int>(nx * i * 5);
                int ty = cy + static_cast<int>(ny * i * 5);
                Uint8 ga = static_cast<Uint8>(a * 0.12f * (1.0f - i * 0.25f));
                fillRect(renderer, tx - r - 1, ty - r - 1, (r + 1) * 2, (r + 1) * 2, pr, pg, pb, ga);
            }
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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
    float time = m_frameTicks * 0.001f;
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
    float time = m_frameTicks * 0.001f;

    // Pulse effect (flat diamond shape)
    float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
    Uint8 trapR = 255;
    Uint8 trapG = static_cast<Uint8>(180 + 50 * pulse);
    Uint8 trapB = static_cast<Uint8>(30 + 70 * pulse);

    // Diamond shape using 4 triangles approximated by rotated rects.
    // Color is constant across all rows — set once before each loop.
    SDL_SetRenderDrawColor(renderer, trapR, trapG, trapB, a);
    // Top half diamond
    for (int dy = -h / 2; dy <= 0; dy++) {
        int halfW = static_cast<int>((w / 2) * (1.0f - static_cast<float>(-dy) / (h / 2)));
        SDL_RenderDrawLine(renderer, cx - halfW, cy + dy, cx + halfW, cy + dy);
    }
    // Bottom half diamond
    for (int dy = 0; dy <= h / 2; dy++) {
        int halfW = static_cast<int>((w / 2) * (1.0f - static_cast<float>(dy) / (h / 2)));
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
