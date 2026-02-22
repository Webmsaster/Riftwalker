#pragma once
#include "ECS/EntityManager.h"
#include "Core/Camera.h"

class ParticleSystem;
class CombatSystem;

class AISystem {
public:
    void update(EntityManager& entities, float dt, Vec2 playerPos, int playerDimension);

    void setParticleSystem(ParticleSystem* ps) { m_particles = ps; }
    void setCamera(Camera* cam) { m_camera = cam; }
    void setCombatSystem(CombatSystem* cs) { m_combatSystem = cs; }

private:
    void updateWalker(Entity& entity, float dt, Vec2 playerPos);
    void updateFlyer(Entity& entity, float dt, Vec2 playerPos);
    void updateTurret(Entity& entity, float dt, Vec2 playerPos);
    void updateCharger(Entity& entity, float dt, Vec2 playerPos);
    void updatePhaser(Entity& entity, float dt, Vec2 playerPos, int playerDim);
    void updateExploder(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);
    void updateShielder(Entity& entity, float dt, Vec2 playerPos);
    void updateBoss(Entity& entity, float dt, Vec2 playerPos, EntityManager& entities);

    void explode(Entity& entity, EntityManager& entities);

    float distanceTo(Vec2 a, Vec2 b) const;

    ParticleSystem* m_particles = nullptr;
    Camera* m_camera = nullptr;
    CombatSystem* m_combatSystem = nullptr;
};
