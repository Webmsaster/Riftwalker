#include "PhysicsSystem.h"
#include "Components/TransformComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Game/Level.h"
#include <algorithm>
#include <cmath>

void PhysicsSystem::update(EntityManager& entities, float dt, Level* level, int currentDimension) {
    auto ents = entities.getEntitiesWithComponent<PhysicsBody>();
    for (auto* e : ents) {
        if (!e->hasComponent<TransformComponent>()) continue;
        auto& phys = e->getComponent<PhysicsBody>();

        phys.wasOnGround = phys.onGround;

        applyGravity(*e, dt);
        applyVelocity(*e, dt);
        phys.onIce = false; // Reset before this frame's tile checks

        if (level && e->hasComponent<ColliderComponent>()) {
            resolveTerrainCollision(*e, level, currentDimension);

            // Ice tile: set flag for next frame's friction reduction
            auto& t = e->getComponent<TransformComponent>();
            int tileX = static_cast<int>(t.getCenter().x) / level->getTileSize();
            int tileY = static_cast<int>(t.position.y + t.height + 1) / level->getTileSize();
            int dim = (e->dimension == 0) ? currentDimension : e->dimension;
            if (phys.onGround && level->isIceTile(tileX, tileY, dim)) {
                phys.onIce = true;
            }

            // Gravity Well: attract/repel entities
            int centerTX = static_cast<int>(t.getCenter().x) / level->getTileSize();
            int centerTY = static_cast<int>(t.getCenter().y) / level->getTileSize();
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int gx = centerTX + dx;
                    int gy = centerTY + dy;
                    if (level->isGravityWell(gx, gy, dim)) {
                        float wellCX = (gx + 0.5f) * level->getTileSize();
                        float wellCY = (gy + 0.5f) * level->getTileSize();
                        float pullDX = wellCX - t.getCenter().x;
                        float pullDY = wellCY - t.getCenter().y;
                        float dist = std::sqrt(pullDX * pullDX + pullDY * pullDY);
                        if (dist > 4.0f && dist < 96.0f) {
                            float force = 200.0f / (dist + 10.0f);
                            // Dimension 1: attract, Dimension 2: repel
                            float sign = (dim == 1) ? 1.0f : -1.0f;
                            phys.velocity.x += (pullDX / dist) * force * sign * dt;
                            phys.velocity.y += (pullDY / dist) * force * sign * dt;
                        }
                    }
                }
            }

            // Crumbling tile: trigger when entity stands on it
            if (phys.onGround) {
                int footTX = static_cast<int>(t.getCenter().x) / level->getTileSize();
                int footTY = static_cast<int>(t.position.y + t.height + 1) / level->getTileSize();
                level->triggerCrumble(footTX, footTY, dim);
            }
        }

        // Coyote time
        if (phys.wasOnGround && !phys.onGround) {
            phys.coyoteTimer = phys.coyoteTime;
        }
        if (phys.coyoteTimer > 0) {
            phys.coyoteTimer -= dt;
        }
    }
}

void PhysicsSystem::applyGravity(Entity& entity, float dt) {
    auto& phys = entity.getComponent<PhysicsBody>();
    if (!phys.useGravity) return;

    // Apex hang-time: reduce gravity when near the peak of a jump
    float effectiveGravity = phys.gravity;
    if (!phys.onGround && phys.apexThreshold > 0.0f &&
        std::abs(phys.velocity.y) < phys.apexThreshold) {
        effectiveGravity = phys.gravity * phys.apexGravityMultiplier;
    }

    phys.velocity.y += effectiveGravity * dt;
    if (phys.velocity.y > phys.maxFallSpeed) {
        phys.velocity.y = phys.maxFallSpeed;
    }
}

void PhysicsSystem::applyVelocity(Entity& entity, float dt) {
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();

    // Apply acceleration
    phys.velocity += phys.acceleration * dt;
    phys.acceleration = {0, 0};

    // Apply friction (ice tiles reduce ground friction dramatically)
    float fric = phys.onGround ? (phys.onIce ? phys.friction * 0.1f : phys.friction) : phys.airResistance;
    if (fric > 0 && std::abs(phys.velocity.x) > 0.01f) {
        float decel = fric * dt;
        if (std::abs(phys.velocity.x) < decel) {
            phys.velocity.x = 0;
        } else {
            phys.velocity.x -= decel * (phys.velocity.x > 0 ? 1.0f : -1.0f);
        }
    }

    transform.position += phys.velocity * dt;
}

void PhysicsSystem::resolveTerrainCollision(Entity& entity, Level* level, int currentDimension) {
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    auto& collider = entity.getComponent<ColliderComponent>();

    SDL_FRect rect = collider.getWorldRect();
    int tileSize = level->getTileSize();
    if (tileSize <= 0) return;

    phys.onGround = false;
    phys.onWallLeft = false;
    phys.onWallRight = false;
    phys.onCeiling = false;

    // For entities in both dimensions (dimension=0), use current active dimension
    int collisionDim = (entity.dimension == 0) ? currentDimension : entity.dimension;

    // Check tiles around the entity
    int startX = static_cast<int>(rect.x) / tileSize - 1;
    int startY = static_cast<int>(rect.y) / tileSize - 1;
    int endX = static_cast<int>(rect.x + rect.w) / tileSize + 1;
    int endY = static_cast<int>(rect.y + rect.h) / tileSize + 1;

    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(level->getWidth() - 1, endX);
    endY = std::min(level->getHeight() - 1, endY);

    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            if (!level->isSolid(x, y, collisionDim)) continue;

            SDL_FRect tileRect = {
                static_cast<float>(x * tileSize),
                static_cast<float>(y * tileSize),
                static_cast<float>(tileSize),
                static_cast<float>(tileSize)
            };

            // Check one-way platform
            bool isOneWay = level->isOneWay(x, y, collisionDim);

            rect = collider.getWorldRect();
            if (!ColliderComponent::checkOverlap(rect, tileRect)) continue;

            // Calculate overlap on each axis
            float overlapLeft = (rect.x + rect.w) - tileRect.x;
            float overlapRight = (tileRect.x + tileRect.w) - rect.x;
            float overlapTop = (rect.y + rect.h) - tileRect.y;
            float overlapBottom = (tileRect.y + tileRect.h) - rect.y;

            // Find minimum overlap axis
            float minOverlapX = (overlapLeft < overlapRight) ? -overlapLeft : overlapRight;
            float minOverlapY = (overlapTop < overlapBottom) ? -overlapTop : overlapBottom;

            if (isOneWay) {
                // Only resolve downward collision for one-way platforms
                if (phys.velocity.y > 0 && overlapTop < 8.0f) {
                    transform.position.y -= overlapTop;
                    phys.velocity.y = 0;
                    phys.onGround = true;
                }
                continue;
            }

            if (std::abs(minOverlapX) < std::abs(minOverlapY)) {
                transform.position.x += minOverlapX;
                if (minOverlapX < 0) phys.onWallRight = true;
                else phys.onWallLeft = true;
                // Wall impact: capture speed and bounce if bounciness > 0
                float impactSpeed = std::abs(phys.velocity.x);
                if (impactSpeed > 150.0f && phys.bounciness > 0) {
                    phys.wallImpactSpeed = impactSpeed;
                    phys.velocity.x = -phys.velocity.x * phys.bounciness;
                } else {
                    phys.velocity.x = 0;
                }
            } else {
                transform.position.y += minOverlapY;
                if (minOverlapY < 0) {
                    // Capture fall speed on first ground contact before zeroing
                    if (!phys.onGround && phys.velocity.y > 0) {
                        phys.landingImpactSpeed = phys.velocity.y;
                    }
                    phys.onGround = true;
                    phys.velocity.y = 0;
                } else {
                    phys.onCeiling = true;
                    if (phys.velocity.y < 0) phys.velocity.y = 0;
                }
            }
        }
    }
}
