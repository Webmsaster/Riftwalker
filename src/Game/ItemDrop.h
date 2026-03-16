#pragma once
#include "ECS/EntityManager.h"
#include "Core/Camera.h"
#include "WeaponSystem.h"

enum class DropType { HealthOrb, RiftShard, ShieldOrb, SpeedBoost, DamageBoost };

class Player;

class ItemDrop {
public:
    static Entity& spawnHealthOrb(EntityManager& entities, Vec2 pos, int dimension, Player* player = nullptr);
    static Entity& spawnRiftShard(EntityManager& entities, Vec2 pos, int dimension, int value = 5, Player* player = nullptr);
    static Entity& spawnShieldOrb(EntityManager& entities, Vec2 pos, int dimension, Player* player);
    static Entity& spawnSpeedBoost(EntityManager& entities, Vec2 pos, int dimension, Player* player);
    static Entity& spawnDamageBoost(EntityManager& entities, Vec2 pos, int dimension, Player* player);
    static void spawnRandomDrop(EntityManager& entities, Vec2 pos, int dimension, int difficulty, Player* player = nullptr);
    static Entity& spawnWeaponDrop(EntityManager& entities, Vec2 pos, int dimension, WeaponID weapon, Player* player);
};
