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

struct Afterimage {
    float worldX, worldY, width, height;
    SDL_Color color;
    bool flipX;
    float timer;
    static constexpr float MAX_LIFE = 0.25f;
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

    // Dash afterimage ghost trail
    static constexpr int MAX_AFTERIMAGES = 8;
    Afterimage afterimages[MAX_AFTERIMAGES];
    int afterimageCount = 0;
    float afterimageSpawnTimer = 0;

    void addAfterimage(float wx, float wy, float w, float h, SDL_Color c, bool flip) {
        if (afterimageCount < MAX_AFTERIMAGES) {
            afterimages[afterimageCount++] = {wx, wy, w, h, c, flip, Afterimage::MAX_LIFE};
        } else {
            // Overwrite oldest (lowest timer)
            int oldest = 0;
            for (int i = 1; i < MAX_AFTERIMAGES; i++) {
                if (afterimages[i].timer < afterimages[oldest].timer) oldest = i;
            }
            afterimages[oldest] = {wx, wy, w, h, c, flip, Afterimage::MAX_LIFE};
        }
    }

    void updateAfterimages(float dt) {
        for (int i = 0; i < afterimageCount; ) {
            afterimages[i].timer -= dt;
            if (afterimages[i].timer <= 0) {
                afterimages[i] = afterimages[--afterimageCount];
            } else {
                i++;
            }
        }
    }

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
        if (!hasBaseColor) { baseColor = color; hasBaseColor = true; }
    }
    void restoreColor() { color = baseColor; }

    SDL_Color baseColor{255, 255, 255, 255};
    bool hasBaseColor = false;
};
