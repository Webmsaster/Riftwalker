#pragma once
#include "ECS/EntityManager.h"
#include "Core/Camera.h"
#include <SDL2/SDL.h>
#include <vector>

class Level;

struct ProjectileImpact {
    Vec2 position;
    Vec2 velocity;
    SDL_Color color;
};

class PhysicsSystem {
public:
    void update(EntityManager& entities, float dt, Level* level, int currentDimension = 1);

    // Projectile terrain impacts (consumed by PlayState for particles)
    const std::vector<ProjectileImpact>& getProjectileImpacts() const { return m_projectileImpacts; }
    void clearProjectileImpacts() { m_projectileImpacts.clear(); }

private:
    void applyGravity(Entity& entity, float dt);
    void applyVelocity(Entity& entity, float dt);
    void resolveTerrainCollision(Entity& entity, Level* level, int currentDimension);

    std::vector<ProjectileImpact> m_projectileImpacts;
};
