#include "Entity.h"

Entity::Entity(EntityManager* manager, std::string tag)
    : m_manager(manager) {
    // Most entities have 5-8 components; pre-allocate to avoid
    // per-addComponent reallocations during entity setup
    m_components.reserve(8);
    setTag(tag); // Sets m_tag + fast type flags
}

void Entity::update(float dt) {
    for (auto& c : m_components) {
        if (c->active) c->update(dt);
    }
}
