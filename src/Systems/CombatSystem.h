#pragma once
#include "ECS/EntityManager.h"
#include "Systems/ParticleSystem.h"
#include "Core/Camera.h"
#include "Game/WeaponSystem.h"
#include <vector>
#include <cstring>

struct DamageEvent {
    Vec2 position;
    float damage;
    bool isPlayerDamage; // true = player took damage, false = enemy took damage
    bool isCritical = false;
};

struct KillEvent {
    bool wasDash = false;
    bool wasComboFinisher = false;
    bool wasCharged = false;
    bool wasRanged = false;
    bool wasAerial = false;    // player was airborne when killing
    bool wasSlam = false;      // ground slam kill
};

class CombatSystem {
public:
    void update(EntityManager& entities, float dt, int currentDimension);
    void setParticleSystem(ParticleSystem* ps) { m_particles = ps; }
    void setCamera(Camera* cam) { m_camera = cam; }
    void setCritChance(float chance) { m_critChance = chance; }
    void setComboBonus(float bonus) { m_comboBonus = bonus; }
    float getComboBonus() const { return m_comboBonus; }
    void setPlayer(class Player* p) { m_player = p; }
    void setLifesteal(float pct) { m_lifesteal = pct; }
    void setElementWeapon(int type) { m_elementWeapon = type; } // 0=none 1=fire 2=ice 3=electric
    void setDashRefreshOnKill(bool v) { m_dashRefreshOnKill = v; }
    bool getDashRefreshOnKill() const { return m_dashRefreshOnKill; }
    void setDimensionManager(class DimensionManager* dm) { m_dimMgr = dm; }

    // Reset per-run state (call on new run start)
    void resetRunState() {
        std::memset(weaponKills, 0, sizeof(weaponKills));
        killCount = 0;
        killedMiniBoss = false;
        killedElemental = false;
        killEvents.clear();
        voidResonanceProcs = 0;
        m_pendingHitFreeze = 0;
        m_critChance = 0;
        m_comboBonus = 0;
        m_lifesteal = 0;
        m_elementWeapon = 0;
        m_dashRefreshOnKill = false;
        m_damageEvents.clear();
    }

    // Hit-freeze: returns accumulated freeze time and resets
    float consumeHitFreeze();
    void addHitFreeze(float amount) { m_pendingHitFreeze += amount; }
    void addDamageEvent(Vec2 pos, float dmg, bool isPlayerDmg, bool isCrit = false) {
        m_damageEvents.push_back({pos, dmg, isPlayerDmg, isCrit});
    }

    // Damage events for floating numbers
    std::vector<DamageEvent> consumeDamageEvents() {
        auto events = std::move(m_damageEvents);
        m_damageEvents.clear();
        return events;
    }

    void createProjectile(EntityManager& entities, Vec2 pos, Vec2 dir,
                          float damage, float speed, int dimension,
                          bool piercing = false, bool isPlayerOwned = false);

    // Kill tracking flags (consumed each frame by PlayState for achievements)
    bool killedMiniBoss = false;
    bool killedElemental = false;
    int killCount = 0;
    int weaponKills[static_cast<int>(WeaponID::COUNT)] = {};

    // Kill events with metadata (consumed each frame by PlayState for combat challenges)
    std::vector<KillEvent> killEvents;

    // Balance tracking (consumed by PlayState for run summary)
    int voidResonanceProcs = 0;

private:
    void processAttack(Entity& attacker, EntityManager& entities, int currentDim);

    ParticleSystem* m_particles = nullptr;
    Camera* m_camera = nullptr;
    float m_pendingHitFreeze = 0;
    float m_critChance = 0;
    float m_comboBonus = 0;
    float m_lifesteal = 0;
    int m_elementWeapon = 0;   // 0=none 1=fire 2=ice 3=electric
    bool m_dashRefreshOnKill = false;
    class Player* m_player = nullptr;
    class DimensionManager* m_dimMgr = nullptr;
    std::vector<DamageEvent> m_damageEvents;
};
