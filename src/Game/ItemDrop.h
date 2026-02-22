#pragma once
#include "ECS/EntityManager.h"
#include "Core/Camera.h"

enum class DropType { HealthOrb, RiftShard };

class ItemDrop {
public:
    static Entity& spawnHealthOrb(EntityManager& entities, Vec2 pos, int dimension);
    static Entity& spawnRiftShard(EntityManager& entities, Vec2 pos, int dimension, int value = 5);
    static void spawnRandomDrop(EntityManager& entities, Vec2 pos, int dimension, int difficulty);
};
