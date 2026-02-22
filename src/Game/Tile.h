#pragma once
#include <SDL2/SDL.h>

enum class TileType {
    Empty = 0,
    Solid,
    OneWay,     // One-way platform
    Spike,      // Damage on contact
    Rift,       // Rift location (for puzzles)
    Exit,       // Level exit
    Spawn,      // Player spawn
    EnemySpawn, // Enemy spawn point
    Pickup,     // Item pickup location
    Decoration  // Visual only
};

struct Tile {
    TileType type = TileType::Empty;
    SDL_Color color{0, 0, 0, 0};
    int textureIndex = -1;
    bool visited = false; // for generation

    bool isSolid() const {
        return type == TileType::Solid;
    }

    bool isOneWay() const {
        return type == TileType::OneWay;
    }

    bool isDangerous() const {
        return type == TileType::Spike;
    }
};
