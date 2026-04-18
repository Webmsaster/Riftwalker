#pragma once
#include "Entity.h"
#include <vector>
#include <memory>

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

    // Template forEach: avoids std::function overhead (type erasure + heap alloc).
    // Re-entrant safe: uses a depth-indexed pool of thread_local snapshot buffers
    // so nested forEach calls (e.g. CombatSystem chain effects inside the melee
    // forEach) don't clobber the outer iteration's snapshot. Each depth keeps its
    // own buffer with sticky capacity, so steady-state cost matches the old
    // shared-buffer version.
    template <typename Func>
    void forEach(Func&& func) {
        static thread_local std::vector<std::vector<Entity*>> s_pool;
        static thread_local int s_depth = 0;
        if (s_pool.size() <= static_cast<size_t>(s_depth)) {
            s_pool.emplace_back();
            s_pool.back().reserve(INITIAL_ENTITY_RESERVE);
        }
        auto& buf = s_pool[s_depth];
        ++s_depth;
        buf.clear();
        if (buf.capacity() < m_entities.size())
            buf.reserve(m_entities.size());
        for (auto& e : m_entities) {
            if (e->isAlive()) buf.push_back(e.get());
        }
        for (Entity* e : buf) {
            if (e && e->isAlive()) func(*e);
        }
        --s_depth;
    }
    void clear();
    size_t size() const { return m_entities.size(); }

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
    mutable std::vector<Entity*> m_snapshotBuffer;       // Reusable buffer for update()/forEach()
    mutable std::vector<Entity*> m_tagQueryBuffer;       // Reusable buffer for getEntitiesByTag()
    mutable std::vector<Entity*> m_dimQueryBuffer;       // Reusable buffer for getEntitiesInDimension()
    mutable std::vector<Entity*> m_componentQueryBuffer; // Reusable buffer for getEntitiesWithComponent()
};
