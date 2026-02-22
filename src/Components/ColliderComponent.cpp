#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "ECS/Entity.h"

SDL_FRect ColliderComponent::getWorldRect() const {
    if (entity && entity->hasComponent<TransformComponent>()) {
        auto& t = entity->getComponent<TransformComponent>();
        return {
            t.position.x + offset.x,
            t.position.y + offset.y,
            width, height
        };
    }
    return {offset.x, offset.y, width, height};
}
