#pragma once
#include "ECS/Component.h"
#include <SDL2/SDL.h>

enum class AnimState : uint8_t {
    Idle = 0,
    Run,
    Jump,
    Fall,
    Dash,
    WallSlide,
    Attack,
    Hurt,
    Dead
};

struct SpriteComponent : public Component {
    SDL_Texture* texture = nullptr;
    SDL_Rect srcRect{0, 0, 32, 32};
    SDL_Color color{255, 255, 255, 255};
    bool flipX = false;
    bool flipY = false;
    int renderLayer = 0; // 0=background, 1=tiles, 2=entities, 3=foreground, 4=UI

    // Procedural animation state
    AnimState animState = AnimState::Idle;
    float animTimer = 0;    // Time spent in current state

    // Landing squash effect (set externally, decays over time)
    float landingSquashTimer = 0;
    float landingSquashIntensity = 0; // 0-1, scales with impact speed

    SpriteComponent() = default;
    SpriteComponent(SDL_Texture* tex, int layer = 2)
        : texture(tex), renderLayer(layer) {
        if (tex) {
            int w, h;
            SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
            srcRect = {0, 0, w, h};
        }
    }

    void setColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
        color = {r, g, b, a};
    }
};
