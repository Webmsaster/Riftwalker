#pragma once
#include "ECS/EntityManager.h"
#include "Core/Camera.h"
#include "Components/AIComponent.h"

class ParticleSystem;
class CombatSystem;

class AISystem {
public:
    void update(EntityManager& entities, float dt, Vec2 playerPos, int playerDimension);

    void setParticleSystem(ParticleSystem* ps) { m_particles = ps; }
    void setCamera(Camera* cam) { m_camera = cam; }
    void setCombatSystem(CombatSystem* cs) { m_combatSystem = cs; }
    void setLevel(class Level* lvl) { m_level = lvl; }

private:
    void updateWalker(Entity& entity, float dt, Vec2 playerPos);
    void updateFlyer(Entity& entity, float dt, Vec2 playerPos);
    void updateTurret(Entity& entity, float dt, Vec2 playerPos);
    void updateCharger(Entity& entity, float dt, Vec2 playerPos);
    void updatePhaser(Entity& entity, float dt, Vec2 playerPos, int playerDim);
    void updateExploder(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateShielder(Entity& entity, float dt, Vec2 playerPos);
    void updateCrawler(Entity& entity, float dt, Vec2 playerPos);
    void updateSummoner(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateSniper(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateBoss(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateVoidWyrm(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateDimensionalArchitect(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateTemporalWeaver(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateVoidSovereign(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateEntropyIncarnate(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);

    void explode(Entity& entity, EntityManager& entities);
    void updateEnemyAnimation(Entity& entity);
    void attackWindupEffect(Entity& entity, float timer, float windupTime);

    float distanceTo(Vec2 a, Vec2 b) const;

    ParticleSystem* m_particles = nullptr;
    Camera* m_camera = nullptr;
    CombatSystem* m_combatSystem = nullptr;
    class Level* m_level = nullptr;
};
