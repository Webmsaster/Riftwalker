#pragma once
#include "Entity.h"
#include <vector>
#include <memory>
#include <functional>

class EntityManager {
public:
    Entity& addEntity(const std::string& tag = "");
    void update(float dt);
    void refresh(); // Remove dead entities

    std::vector<Entity*> getEntitiesByTag(const std::string& tag) const;
    std::vector<Entity*> getEntitiesInDimension(int dim) const;

    template <typename T>
    std::vector<Entity*> getEntitiesWithComponent() const {
        std::vector<Entity*> result;
        for (auto& e : m_entities) {
            if (e->isAlive() && e->hasComponent<T>()) {
                result.push_back(e.get());
            }
        }
        return result;
    }

    void forEach(const std::function<void(Entity&)>& func);
    void clear();
    size_t size() const { return m_entities.size(); }

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
};
