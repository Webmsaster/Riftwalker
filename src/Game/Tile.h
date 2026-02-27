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
    Conveyor,     // Conveyor belt - pushes player left/right
    Breakable,    // Destructible wall - broken by dash/charged attack
    ShrineBase,   // Shrine interaction point
    Ice,          // Reduced friction, sliding
    GravityWell,  // Attracts/repels entities, switches per dimension
    Teleporter,   // Pair tiles, teleports on contact (variant = pair ID)
    Crumbling     // Breaks 1.5s after stepping on, respawns after 5s
};

struct Tile {
    TileType type = TileType::Empty;
    SDL_Color color{0, 0, 0, 0};
    int textureIndex = -1;
    bool visited = false; // for generation
    int variant = 0;      // direction/subtype: laser=0 right,1 left,2 down,3 up; conveyor=0 right,1 left
    float crumbleTimer = 0;  // Crumbling: time until break (1.5s)
    float respawnTimer = 0;  // Crumbling: time until respawn (5s)
    bool crumbling = false;  // Crumbling: actively breaking
    bool crumbled = false;   // Crumbling: currently broken

    bool isSolid() const {
        if (type == TileType::Crumbling && crumbled) return false;
        return type == TileType::Solid || type == TileType::LaserEmitter ||
               type == TileType::Breakable || type == TileType::Ice ||
               type == TileType::Crumbling;
    }

    bool isOneWay() const {
        return type == TileType::OneWay;
    }

    bool isDangerous() const {
        return type == TileType::Spike || type == TileType::Fire;
    }
};
