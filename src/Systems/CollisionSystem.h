#pragma once
#include "ECS/EntityManager.h"
#include <vector>
#include <utility>

struct CollisionPair {
    Entity* a;
    Entity* b;
};

class CollisionSystem {
public:
    void update(EntityManager& entities, int currentDimension);
    const std::vector<CollisionPair>& getCollisions() const { return m_collisions; }

private:
    std::vector<CollisionPair> m_collisions;
};
