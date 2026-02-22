#include "CollisionSystem.h"
#include "Components/ColliderComponent.h"
#include "Components/TransformComponent.h"

void CollisionSystem::update(EntityManager& entities, int currentDimension) {
    m_collisions.clear();

    auto ents = entities.getEntitiesWithComponent<ColliderComponent>();

    for (size_t i = 0; i < ents.size(); i++) {
        auto* a = ents[i];
        auto& colA = a->getComponent<ColliderComponent>();
        if (!colA.enabled) continue;

        // Check dimension
        if (a->dimension != 0 && a->dimension != currentDimension) continue;

        for (size_t j = i + 1; j < ents.size(); j++) {
            auto* b = ents[j];
            auto& colB = b->getComponent<ColliderComponent>();
            if (!colB.enabled) continue;

            if (b->dimension != 0 && b->dimension != currentDimension) continue;

            // Check layer/mask compatibility
            if (!(colA.layer & colB.mask) && !(colB.layer & colA.mask)) continue;

            SDL_FRect rectA = colA.getWorldRect();
            SDL_FRect rectB = colB.getWorldRect();

            if (ColliderComponent::checkOverlap(rectA, rectB)) {
                m_collisions.push_back({a, b});

                // Fire trigger callbacks
                if (colA.type == ColliderType::Trigger && colA.onTrigger) {
                    colA.onTrigger(a, b);
                }
                if (colB.type == ColliderType::Trigger && colB.onTrigger) {
                    colB.onTrigger(b, a);
                }
            }
        }
    }
}
