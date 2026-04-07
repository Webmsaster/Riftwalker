#pragma once
#include "Component.h"
#include <vector>
#include <memory>
#include <array>
#include <bitset>
#include <string>

class EntityManager;

// Render dispatch enum — set automatically by setTag(), used by RenderSystem
enum class EntityRenderType : uint8_t {
    Unknown = 0,
    Player, EnemyWalker, EnemyFlyer, EnemyTurret, EnemyCharger,
    EnemyPhaser, EnemyExploder, EnemyShielder, EnemyCrawler, EnemySummoner,
    EnemySniper, EnemyTeleporter, EnemyReflector, EnemyLeech, EnemySwarmer,
    EnemyGravityWell, EnemyMimic, EnemyMinion, EnemyBoss,
    EnemyEntropyMinion, EnemyShadowClone, EnemyEcho,
    PlayerTurret, PlayerTrap, EnemyCrate,
    PickupHealth, PickupShield, PickupSpeed, PickupDamage, PickupShard,
    Projectile, DimResidue, GrappleHook,
    Pickup // generic fallback
};

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
        // Render dispatch type
        if (tag == "player") renderType = EntityRenderType::Player;
        else if (tag == "enemy_walker") renderType = EntityRenderType::EnemyWalker;
        else if (tag == "enemy_flyer") renderType = EntityRenderType::EnemyFlyer;
        else if (tag == "enemy_turret") renderType = EntityRenderType::EnemyTurret;
        else if (tag == "enemy_charger") renderType = EntityRenderType::EnemyCharger;
        else if (tag == "enemy_phaser") renderType = EntityRenderType::EnemyPhaser;
        else if (tag == "enemy_exploder") renderType = EntityRenderType::EnemyExploder;
        else if (tag == "enemy_shielder") renderType = EntityRenderType::EnemyShielder;
        else if (tag == "enemy_crawler") renderType = EntityRenderType::EnemyCrawler;
        else if (tag == "enemy_summoner") renderType = EntityRenderType::EnemySummoner;
        else if (tag == "enemy_sniper") renderType = EntityRenderType::EnemySniper;
        else if (tag == "enemy_teleporter") renderType = EntityRenderType::EnemyTeleporter;
        else if (tag == "enemy_reflector") renderType = EntityRenderType::EnemyReflector;
        else if (tag == "enemy_leech") renderType = EntityRenderType::EnemyLeech;
        else if (tag == "enemy_swarmer") renderType = EntityRenderType::EnemySwarmer;
        else if (tag == "enemy_gravitywell") renderType = EntityRenderType::EnemyGravityWell;
        else if (tag == "enemy_mimic") renderType = EntityRenderType::EnemyMimic;
        else if (tag == "enemy_minion") renderType = EntityRenderType::EnemyMinion;
        else if (tag == "enemy_boss") renderType = EntityRenderType::EnemyBoss;
        else if (tag == "enemy_entropy_minion") renderType = EntityRenderType::EnemyEntropyMinion;
        else if (tag == "enemy_shadow_clone") renderType = EntityRenderType::EnemyShadowClone;
        else if (tag == "enemy_echo") renderType = EntityRenderType::EnemyEcho;
        else if (tag == "player_turret") renderType = EntityRenderType::PlayerTurret;
        else if (tag == "player_trap") renderType = EntityRenderType::PlayerTrap;
        else if (tag == "enemy_crate") renderType = EntityRenderType::EnemyCrate;
        else if (tag == "pickup_health") renderType = EntityRenderType::PickupHealth;
        else if (tag == "pickup_shield") renderType = EntityRenderType::PickupShield;
        else if (tag == "pickup_speed") renderType = EntityRenderType::PickupSpeed;
        else if (tag == "pickup_damage") renderType = EntityRenderType::PickupDamage;
        else if (tag == "pickup_shard") renderType = EntityRenderType::PickupShard;
        else if (tag == "projectile") renderType = EntityRenderType::Projectile;
        else if (tag == "dim_residue") renderType = EntityRenderType::DimResidue;
        else if (tag == "grapple_hook") renderType = EntityRenderType::GrappleHook;
        else if (isPickup) renderType = EntityRenderType::Pickup;
        else renderType = EntityRenderType::Unknown;
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
    EntityRenderType renderType = EntityRenderType::Unknown;

private:
    EntityManager* m_manager;
    bool m_alive = true;
    std::string m_tag;
    std::vector<std::unique_ptr<Component>> m_components;
    std::array<Component*, MAX_COMPONENTS> m_componentArray{};
    std::bitset<MAX_COMPONENTS> m_componentBitset;
};
