#include "EntityManager.h"
#include <algorithm>

EntityManager::EntityManager() {
    m_entities.reserve(INITIAL_ENTITY_RESERVE);
    m_snapshotBuffer.reserve(INITIAL_ENTITY_RESERVE);
    m_tagQueryBuffer.reserve(64);
    m_dimQueryBuffer.reserve(INITIAL_ENTITY_RESERVE);
    m_componentQueryBuffer.reserve(INITIAL_ENTITY_RESERVE);
}

Entity& EntityManager::addEntity(const std::string& tag) {
    auto entity = std::make_unique<Entity>(this, tag);
    Entity& ref = *entity;
    m_entities.push_back(std::move(entity));
    return ref;
}

void EntityManager::update(float dt) {
    m_snapshotBuffer.clear();
    if (m_snapshotBuffer.capacity() < m_entities.size())
        m_snapshotBuffer.reserve(m_entities.size());
    for (auto& e : m_entities) {
        if (e->isAlive()) m_snapshotBuffer.push_back(e.get());
    }
    for (Entity* e : m_snapshotBuffer) {
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

const std::vector<Entity*>& EntityManager::getEntitiesByTag(const std::string& tag) const {
    m_tagQueryBuffer.clear();
    for (auto& e : m_entities) {
        if (e->isAlive() && e->getTag() == tag) {
            m_tagQueryBuffer.push_back(e.get());
        }
    }
    return m_tagQueryBuffer;
}

const std::vector<Entity*>& EntityManager::getEntitiesInDimension(int dim) const {
    m_dimQueryBuffer.clear();
    for (auto& e : m_entities) {
        if (e->isAlive() && (e->dimension == 0 || e->dimension == dim)) {
            m_dimQueryBuffer.push_back(e.get());
        }
    }
    return m_dimQueryBuffer;
}

void EntityManager::forEach(const std::function<void(Entity&)>& func) {
    m_snapshotBuffer.clear();
    if (m_snapshotBuffer.capacity() < m_entities.size())
        m_snapshotBuffer.reserve(m_entities.size());
    for (auto& e : m_entities) {
        if (e->isAlive()) m_snapshotBuffer.push_back(e.get());
    }
    for (Entity* e : m_snapshotBuffer) {
        if (e && e->isAlive()) func(*e);
    }
}

void EntityManager::clear() {
    m_entities.clear();
}
