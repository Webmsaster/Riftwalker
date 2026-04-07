#pragma once
#include "Component.h"
#include <vector>
#include <memory>
#include <array>
#include <bitset>
#include <string>

class EntityManager;

class Entity {
public:
    Entity(EntityManager* manager, std::string tag = "");
    ~Entity() = default;

    void update(float dt);
    void destroy() { m_alive = false; }
    bool isAlive() const { return m_alive; }

    const std::string& getTag() const { return m_tag; }
    void setTag(const std::string& tag) {
        m_tag = tag;
        isEnemy = (tag.find("enemy") != std::string::npos);
        isPlayer = (tag == "player");
        isPickup = (tag.find("pickup") != std::string::npos);
        isProjectile = (tag.find("projectile") != std::string::npos);
        isBoss = (tag == "enemy_boss");
        isDimResidue = (tag == "dim_residue");
        isPlayerTurret = (tag == "player_turret");
        isPlayerTrap = (tag == "player_trap");
        isMinion = (tag == "enemy_minion");
        isEntropyMinion = (tag == "enemy_entropy_minion");
        isShadowClone = (tag == "enemy_shadow_clone");
        isEnemyEcho = (tag == "enemy_echo");
        isHealthPickup = (tag.find("health") != std::string::npos);
        isShardPickup = (tag.find("shard") != std::string::npos);
    }

    template <typename T, typename... Args>
    T& addComponent(Args&&... args) {
        T* comp = new T(std::forward<Args>(args)...);
        comp->entity = this;
        std::unique_ptr<Component> uPtr(comp);
        m_components.push_back(std::move(uPtr));
        m_componentArray[getComponentTypeID<T>()] = comp;
        m_componentBitset[getComponentTypeID<T>()] = true;
        comp->init();
        return *comp;
    }

    template <typename T>
    bool hasComponent() const {
        return m_componentBitset[getComponentTypeID<T>()];
    }

    template <typename T>
    T& getComponent() const {
        return *static_cast<T*>(m_componentArray[getComponentTypeID<T>()]);
    }

    EntityManager* getManager() const { return m_manager; }

    // Dimension the entity belongs to (0 = both, 1 = dim A, 2 = dim B)
    int dimension = 0;
    bool visible = true;

    // Fast type flags (set automatically by setTag, avoids per-frame string::find)
    bool isEnemy = false;
    bool isPlayer = false;
    bool isPickup = false;
    bool isProjectile = false;
    bool isBoss = false;
    bool isDimResidue = false;
    bool isPlayerTurret = false;
    bool isPlayerTrap = false;
    bool isMinion = false;
    bool isEntropyMinion = false;
    bool isShadowClone = false;
    bool isEnemyEcho = false;
    bool isHealthPickup = false;
    bool isShardPickup = false;

private:
    EntityManager* m_manager;
    bool m_alive = true;
    std::string m_tag;
    std::vector<std::unique_ptr<Component>> m_components;
    std::array<Component*, MAX_COMPONENTS> m_componentArray{};
    std::bitset<MAX_COMPONENTS> m_componentBitset;
};
