#pragma once
#include "ECS/Component.h"
#include "Core/Camera.h"
#include <SDL2/SDL.h>
#include <functional>

enum class ColliderType {
    Static,     // Walls, platforms
    Dynamic,    // Player, enemies
    Trigger,    // Pickups, zone triggers
    OneWay      // One-way platforms (pass through from below)
};

struct ColliderComponent : public Component {
    Vec2 offset{0, 0};
    float width = 32;
    float height = 32;
    ColliderType type = ColliderType::Dynamic;
    bool enabled = true;

    // Collision layer/mask system
    uint32_t layer = 1;     // What layer this collider is on
    uint32_t mask = 0xFFFF; // What layers this collider checks against

    // Callback for trigger collisions
    std::function<void(Entity*, Entity*)> onTrigger;

    SDL_FRect getWorldRect() const; // implemented in cpp

    static bool checkOverlap(const SDL_FRect& a, const SDL_FRect& b) {
        return a.x < b.x + b.w && a.x + a.w > b.x &&
               a.y < b.y + b.h && a.y + a.h > b.y;
    }
};

// Collision layers
constexpr uint32_t LAYER_PLAYER = 1 << 0;
constexpr uint32_t LAYER_ENEMY = 1 << 1;
constexpr uint32_t LAYER_TILE = 1 << 2;
constexpr uint32_t LAYER_PROJECTILE = 1 << 3;
constexpr uint32_t LAYER_TRIGGER = 1 << 4;
constexpr uint32_t LAYER_PICKUP = 1 << 5;
