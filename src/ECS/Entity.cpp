#include "Entity.h"

Entity::Entity(EntityManager* manager, std::string tag)
    : m_manager(manager), m_tag(std::move(tag)) {}

void Entity::update(float dt) {
    for (auto& c : m_components) {
        if (c->active) c->update(dt);
    }
}
