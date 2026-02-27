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
    Decoration, // Visual only
    LaserEmitter, // Emits horizontal/vertical laser beam
    Fire,         // Fire pit - damage over time
    Conveyor      // Conveyor belt - pushes player left/right
};

struct Tile {
    TileType type = TileType::Empty;
    SDL_Color color{0, 0, 0, 0};
    int textureIndex = -1;
    bool visited = false; // for generation
    int variant = 0;      // direction/subtype: laser=0 right,1 left,2 down,3 up; conveyor=0 right,1 left

    bool isSolid() const {
        return type == TileType::Solid || type == TileType::LaserEmitter;
    }

    bool isOneWay() const {
        return type == TileType::OneWay;
    }

    bool isDangerous() const {
        return type == TileType::Spike || type == TileType::Fire;
    }
};
