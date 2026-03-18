#include "CollisionSystem.h"
#include "Components/ColliderComponent.h"
#include "Components/TransformComponent.h"
#include <cmath>

void CollisionSystem::update(EntityManager& entities, int currentDimension) {
    m_collisions.clear();
    m_active.clear();
    m_rects.clear();

    // Gather active, dimension-visible entities and cache their world rects
    auto ents = entities.getEntitiesWithComponent<ColliderComponent>();
    m_active.reserve(ents.size());
    m_rects.reserve(ents.size());

    for (auto* e : ents) {
        auto& col = e->getComponent<ColliderComponent>();
        if (!col.enabled) continue;
        if (e->dimension != 0 && e->dimension != currentDimension) continue;

        m_active.push_back(e);
        m_rects.push_back(col.getWorldRect());
    }

    if (m_active.size() < 2) return;

    // Build spatial grid and check pairs
    buildGrid();
    queryPairs();
}

void CollisionSystem::buildGrid() {
    // Clear all cell vectors but keep the map buckets allocated
    for (auto& [key, cell] : m_grid) {
        cell.clear();
    }

    const int count = static_cast<int>(m_active.size());
    for (int i = 0; i < count; i++) {
        const SDL_FRect& r = m_rects[i];

        // Determine which grid cells this AABB overlaps
        int minCX = static_cast<int>(std::floor(r.x / CELL_SIZE));
        int maxCX = static_cast<int>(std::floor((r.x + r.w) / CELL_SIZE));
        int minCY = static_cast<int>(std::floor(r.y / CELL_SIZE));
        int maxCY = static_cast<int>(std::floor((r.y + r.h) / CELL_SIZE));

        for (int cy = minCY; cy <= maxCY; cy++) {
            for (int cx = minCX; cx <= maxCX; cx++) {
                m_grid[cellHash(cx, cy)].push_back(i);
            }
        }
    }
}

void CollisionSystem::queryPairs() {
    // Clear dedup set but keep bucket memory from previous frames
    m_testedPairs.clear();

    const int n = static_cast<int>(m_active.size());

    for (auto& [key, cell] : m_grid) {
        const int cellCount = static_cast<int>(cell.size());
        for (int ci = 0; ci < cellCount; ci++) {
            int i = cell[ci];
            for (int cj = ci + 1; cj < cellCount; cj++) {
                int j = cell[cj];

                // Canonical pair key to deduplicate across cells
                int lo = (i < j) ? i : j;
                int hi = (i < j) ? j : i;
                int64_t pairKey = static_cast<int64_t>(lo) * n + hi;

                if (!m_testedPairs.insert(pairKey).second) continue;

                // Layer/mask compatibility
                auto& colA = m_active[i]->getComponent<ColliderComponent>();
                auto& colB = m_active[j]->getComponent<ColliderComponent>();
                if (!(colA.layer & colB.mask) && !(colB.layer & colA.mask)) continue;

                // Narrow-phase AABB check (rects already cached)
                if (ColliderComponent::checkOverlap(m_rects[i], m_rects[j])) {
                    m_collisions.push_back({m_active[i], m_active[j]});

                    // Fire trigger callbacks
                    if (colA.type == ColliderType::Trigger && colA.onTrigger) {
                        colA.onTrigger(m_active[i], m_active[j]);
                    }
                    if (colB.type == ColliderType::Trigger && colB.onTrigger) {
                        colB.onTrigger(m_active[j], m_active[i]);
                    }
                }
            }
        }
    }
}
