#pragma once
#include "ECS/Entity.h"
#include "ECS/EntityManager.h"
#include "Components/AIComponent.h"
#include "Core/Camera.h"

class Enemy {
public:
    static Entity& createWalker(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createFlyer(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createTurret(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createCharger(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createPhaser(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createExploder(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createShielder(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createCrawler(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createSummoner(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createSniper(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createMinion(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& createBoss(EntityManager& entities, Vec2 pos, int dimension, int difficulty);
    static Entity& createVoidWyrm(EntityManager& entities, Vec2 pos, int dimension, int difficulty);
    static Entity& createDimensionalArchitect(EntityManager& entities, Vec2 pos, int dimension, int difficulty);
    static Entity& createByType(EntityManager& entities, int type, Vec2 pos, int dimension);
    static Entity& createTemporalWeaver(EntityManager& entities, Vec2 pos, int dimension, int difficulty);
    static void applyElement(Entity& e, EnemyElement element);
    static void makeMiniBoss(Entity& e);
    static void makeElite(Entity& e, EliteModifier mod);
};
