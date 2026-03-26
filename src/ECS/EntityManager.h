#pragma once
#include "Entity.h"
#include <vector>
#include <memory>
#include <functional>

class EntityManager {
public:
    static constexpr size_t INITIAL_ENTITY_RESERVE = 200;

    EntityManager();
    Entity& addEntity(const std::string& tag = "");
    void update(float dt);
    void refresh(); // Remove dead entities

    const std::vector<Entity*>& getEntitiesByTag(const std::string& tag) const;
    const std::vector<Entity*>& getEntitiesInDimension(int dim) const;

    template <typename T>
    const std::vector<Entity*>& getEntitiesWithComponent() const {
        m_componentQueryBuffer.clear();
        for (auto& e : m_entities) {
            if (e->isAlive() && e->hasComponent<T>()) {
                m_componentQueryBuffer.push_back(e.get());
            }
        }
        return m_componentQueryBuffer;
    }

    void forEach(const std::function<void(Entity&)>& func);
    void clear();
    size_t size() const { return m_entities.size(); }

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
    mutable std::vector<Entity*> m_snapshotBuffer;       // Reusable buffer for update()/forEach()
    mutable std::vector<Entity*> m_tagQueryBuffer;       // Reusable buffer for getEntitiesByTag()
    mutable std::vector<Entity*> m_dimQueryBuffer;       // Reusable buffer for getEntitiesInDimension()
    mutable std::vector<Entity*> m_componentQueryBuffer; // Reusable buffer for getEntitiesWithComponent()
};
