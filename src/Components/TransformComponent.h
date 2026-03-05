#pragma once
#include "ECS/Component.h"
#include "Core/Camera.h" // for Vec2

struct TransformComponent : public Component {
    Vec2 position;
    Vec2 scale{1.0f, 1.0f};
    int width = 32;
    int height = 32;

    TransformComponent() = default;
    TransformComponent(float x, float y, int w = 32, int h = 32)
        : position(x, y), width(w), height(h) {}

    SDL_FRect getRect() const {
        return {position.x, position.y,
                static_cast<float>(width) * scale.x,
                static_cast<float>(height) * scale.y};
    }

    Vec2 getCenter() const {
        return {position.x + width * scale.x * 0.5f,
                position.y + height * scale.y * 0.5f};
    }
};
