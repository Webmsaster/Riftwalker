#pragma once
#include "ECS/EntityManager.h"
#include <SDL2/SDL.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct CollisionPair {
    Entity* a;
    Entity* b;
};

class CollisionSystem {
public:
    void update(EntityManager& entities, int currentDimension);
    const std::vector<CollisionPair>& getCollisions() const { return m_collisions; }

private:
    // Spatial grid broad-phase
    static constexpr int CELL_SIZE = 128; // pixels per grid cell

    // Hash a cell coordinate to a single int key
    static int cellHash(int cx, int cy) { return cx * 10000 + cy; }

    // Sparse grid: maps cell hash -> list of entity indices
    std::unordered_map<int, std::vector<int>> m_grid;

    // Scratch buffers reused each frame to avoid allocations
    std::vector<Entity*> m_active;          // filtered entities for this frame
    std::vector<SDL_FRect> m_rects;         // cached world rects (parallel to m_active)
    std::vector<CollisionPair> m_collisions;

    // Dedup set for pairs tested across multiple cells (kept as member to reuse allocations)
    std::unordered_set<int64_t> m_testedPairs;

    void buildGrid();
    void queryPairs();
};
