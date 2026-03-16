#include "EntityManager.h"
#include <algorithm>

Entity& EntityManager::addEntity(const std::string& tag) {
    auto entity = std::make_unique<Entity>(this, tag);
    Entity& ref = *entity;
    m_entities.push_back(std::move(entity));
    return ref;
}

void EntityManager::update(float dt) {
    std::vector<Entity*> snapshot;
    snapshot.reserve(m_entities.size());
    for (auto& e : m_entities) {
        if (e->isAlive()) snapshot.push_back(e.get());
    }
    for (Entity* e : snapshot) {
        if (e && e->isAlive()) e->update(dt);
    }
}

void EntityManager::refresh() {
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [](const std::unique_ptr<Entity>& e) { return !e->isAlive(); }),
        m_entities.end()
    );
}

std::vector<Entity*> EntityManager::getEntitiesByTag(const std::string& tag) const {
    std::vector<Entity*> result;
    for (auto& e : m_entities) {
        if (e->isAlive() && e->getTag() == tag) {
            result.push_back(e.get());
        }
    }
    return result;
}

std::vector<Entity*> EntityManager::getEntitiesInDimension(int dim) const {
    std::vector<Entity*> result;
    for (auto& e : m_entities) {
        if (e->isAlive() && (e->dimension == 0 || e->dimension == dim)) {
            result.push_back(e.get());
        }
    }
    return result;
}

void EntityManager::forEach(const std::function<void(Entity&)>& func) {
    std::vector<Entity*> snapshot;
    snapshot.reserve(m_entities.size());
    for (auto& e : m_entities) {
        if (e->isAlive()) snapshot.push_back(e.get());
    }
    for (Entity* e : snapshot) {
        if (e && e->isAlive()) func(*e);
    }
}

void EntityManager::clear() {
    m_entities.clear();
}
