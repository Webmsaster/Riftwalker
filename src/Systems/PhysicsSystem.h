#pragma once
#include "ECS/EntityManager.h"
#include "Core/Camera.h"

class Level;

class PhysicsSystem {
public:
    void update(EntityManager& entities, float dt, Level* level, int currentDimension = 1);

private:
    void applyGravity(Entity& entity, float dt);
    void applyVelocity(Entity& entity, float dt);
    void resolveTerrainCollision(Entity& entity, Level* level, int currentDimension);
};
