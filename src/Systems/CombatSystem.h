#pragma once
#include "ECS/EntityManager.h"
#include "Systems/ParticleSystem.h"
#include "Core/Camera.h"
#include "Game/WeaponSystem.h"
#include <vector>
#include <cstring>

struct CombatComponent;

struct DamageEvent {
    Vec2 position;
    float damage = 0;
    bool isPlayerDamage = false; // true = player took damage, false = enemy took damage
    bool isCritical = false;
    bool feedbackHandled = false; // true = source already did shake/flash/SFX
    Vec2 sourcePos{0, 0}; // World position of damage source (for directional indicators)
};

struct KillEvent {
    bool wasDash = false;
    bool wasComboFinisher = false;
    bool wasCharged = false;
    bool wasRanged = false;
    bool wasAerial = false;    // player was airborne when killing
    bool wasSlam = false;      // ground slam kill
    int enemyType = -1;        // EnemyType cast to int (-1 = unknown)
    bool wasElite = false;
    bool wasMiniBoss = false;
    bool wasBoss = false;
};

// Visual ghost that persists after entity is destroyed (shrink + fade death animation)
struct DeathEffect {
    Vec2 position;                    // World center position
    float width = 0, height = 0;      // Original entity size (world units)
    SDL_Color color{255, 255, 255, 255}; // Entity sprite color
    SDL_Texture* texture = nullptr;   // Sprite texture (owned by ResourceManager, safe to keep)
    SDL_Rect srcRect{0, 0, 32, 32};   // Sprite source rect
    bool flipX = false;
    float timer = 0;                  // Counts up from 0
    float maxLife = 0.3f;             // Total animation duration (non-zero default prevents NaN)
    bool isBoss = false;
    bool isElite = false;
    float flashPhase = 0;             // 0-1, white flash at start
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
    void setSuitEntropy(class SuitEntropy* se) { m_suitEntropy = se; }

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
        m_damageEvents.reserve(64);
        m_deathEffects.clear();
        m_deathEffects.reserve(16);
        parryEvents.clear();
        parryEvents.reserve(16);
        reflectEvents.clear();
        reflectEvents.reserve(8);
        killEvents.reserve(32);
    }

    // Hit-freeze: returns accumulated freeze time and resets
    float consumeHitFreeze();
    void addHitFreeze(float amount) { m_pendingHitFreeze += amount; }
    void addDamageEvent(const Vec2& pos, float dmg, bool isPlayerDmg, bool isCrit = false, bool fbHandled = false, const Vec2& srcPos = {0, 0}) {
        m_damageEvents.push_back({pos, dmg, isPlayerDmg, isCrit, fbHandled, srcPos});
    }

    // Damage events for floating numbers
    std::vector<DamageEvent> consumeDamageEvents() {
        auto events = std::move(m_damageEvents);
        // After move, m_damageEvents is in valid-but-unspecified state (usually empty with cap=0).
        // Re-reserve so next frame's push_backs don't trigger a fresh allocation.
        m_damageEvents.reserve(64);
        return events;
    }

    void createProjectile(EntityManager& entities, const Vec2& pos, const Vec2& dir,
                          float damage, float speed, int dimension,
                          bool piercing = false, bool isPlayerOwned = false,
                          bool isCrit = false);

    // Shared multiplier chain for player-initiated damage (used by attack path + abilities + finishers).
    // Includes: relic, class, smith, resonance, damage boost. Excludes: crit, mastery, elite shield absorb.
    static float computePlayerDamageMult(Entity& attacker, class Player* player,
                                         class DimensionManager* dimMgr, int currentDim, bool isMelee);

    // Kill tracking flags (consumed each frame by PlayState for achievements)
    bool killedMiniBoss = false;
    bool killedElemental = false;
    int killCount = 0;
    int weaponKills[static_cast<int>(WeaponID::COUNT)] = {};

    // Kill events with metadata (consumed each frame by PlayState for combat challenges)
    std::vector<KillEvent> killEvents;

    // Parry flash events (consumed by PlayState for "PARRY!" text)
    struct ParryEvent { Vec2 position; };
    std::vector<ParryEvent> parryEvents;

    // Reflect events (consumed by PlayState for "REFLECT!" text)
    struct ReflectEvent { Vec2 position; };
    std::vector<ReflectEvent> reflectEvents;

    // Death effects (consumed by PlayState for ghost rendering)
    std::vector<DeathEffect> consumeDeathEffects() {
        auto effects = std::move(m_deathEffects);
        // Re-reserve after move to avoid reallocation on next frame's first push_back
        m_deathEffects.reserve(16);
        return effects;
    }

    // Balance tracking (consumed by PlayState for run summary)
    int voidResonanceProcs = 0;

private:
    void processAttack(Entity& attacker, EntityManager& entities, int currentDim);
    void emitElementDeathFX(const Vec2& pos, int element); // element: 0=none, 1=fire, 2=ice, 3=electric
    void emitEnemyTypeDeathFX(const Vec2& pos, int enemyType, SDL_Color color);

    // Update/combat sub-steps (CombatSystemUpdate.cpp, CombatSystemEffects.cpp)
    void processGroundSlam(EntityManager& entities, int currentDim);
    void processBurnDoT(EntityManager& entities, float dt);
    void processFreezeDecay(EntityManager& entities, float dt);
    void processProjectileLifetime(EntityManager& entities, float dt);
    void processZombieSweep(EntityManager& entities, int currentDim);
    void processCounterAttack(Entity& player, EntityManager& entities, int currentDim);
    void processRangedAttack(Entity& attacker, EntityManager& entities,
                             CombatComponent& combat, bool isPlayer);
    void handleEnemyDeath(Entity& attacker, Entity& target, EntityManager& entities,
                          int currentDim, bool isPlayer, bool isDashAttack,
                          bool isChargedAttack, CombatComponent& combat,
                          const Vec2& targetCenter, float damage, bool isSlamAttack = false);

    ParticleSystem* m_particles = nullptr;
    Camera* m_camera = nullptr;
    // Cached during update() so createProjectile()'s onTrigger lambda can
    // spawn reflected projectiles without having EntityManager captured.
    EntityManager* m_currentEntities = nullptr;
    float m_pendingHitFreeze = 0;
    float m_critChance = 0;
    float m_comboBonus = 0;
    float m_lifesteal = 0;
    int m_elementWeapon = 0;   // 0=none 1=fire 2=ice 3=electric
    bool m_dashRefreshOnKill = false;
    class Player* m_player = nullptr;
    class DimensionManager* m_dimMgr = nullptr;
    class SuitEntropy* m_suitEntropy = nullptr;
    std::vector<DamageEvent> m_damageEvents;
    std::vector<DeathEffect> m_deathEffects;
};
