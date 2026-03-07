#pragma once
#include "Core/Camera.h"

enum class SecretRoomType {
    TreasureVault,    // Extra shards + HP
    ChallengeRoom,    // Mini-boss wave, extra loot
    ShrineRoom,       // Buff or curse
    DimensionCache,   // Loot only visible in one dimension
    AncientWeapon     // Temporary weapon buff
};

struct SecretRoom {
    SecretRoomType type;
    int tileX, tileY;   // Top-left tile
    int width, height;
    bool discovered = false;
    bool completed = false;
    int dimension = 0;   // 0 = both dimensions

    // Entrance position (breakable wall tiles)
    int entranceX = 0, entranceY = 0;

    // Reward tracking
    int shardReward = 0;
    float hpReward = 0;
    float weaponBuffTimer = 0; // AncientWeapon buff duration
};
