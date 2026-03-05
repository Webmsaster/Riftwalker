#pragma once
#include "ECS/Component.h"
#include <SDL2/SDL.h>
#include <unordered_map>
#include <string>
#include <vector>

struct AnimFrame {
    SDL_Rect srcRect;
    float duration; // seconds
};

struct Animation {
    std::vector<AnimFrame> frames;
    bool loop = true;
};

struct AnimationComponent : public Component {
    std::unordered_map<std::string, Animation> animations;
    std::string currentAnim;
    int currentFrame = 0;
    float frameTimer = 0;
    bool finished = false;

    void addAnimation(const std::string& name, const Animation& anim) {
        animations[name] = anim;
    }

    // Helper: create animation from spritesheet row
    void addSheetAnimation(const std::string& name, int row, int frameCount,
                           int frameW, int frameH, float frameDuration, bool loop = true) {
        Animation anim;
        anim.loop = loop;
        for (int i = 0; i < frameCount; i++) {
            anim.frames.push_back({{i * frameW, row * frameH, frameW, frameH}, frameDuration});
        }
        animations[name] = anim;
    }

    void play(const std::string& name) {
        if (currentAnim == name && !finished) return;
        currentAnim = name;
        currentFrame = 0;
        frameTimer = 0;
        finished = false;
    }

    void update(float dt) override {
        if (currentAnim.empty() || finished) return;
        auto it = animations.find(currentAnim);
        if (it == animations.end()) return;

        auto& anim = it->second;
        if (anim.frames.empty()) return;

        frameTimer += dt;
        float frameDur = anim.frames[currentFrame].duration;
        if (frameDur <= 0) frameDur = 0.016f; // prevent infinite loop on zero-duration frames
        if (frameTimer >= frameDur) {
            frameTimer -= frameDur;
            currentFrame++;
            if (currentFrame >= static_cast<int>(anim.frames.size())) {
                if (anim.loop) {
                    currentFrame = 0;
                } else {
                    currentFrame = static_cast<int>(anim.frames.size()) - 1;
                    finished = true;
                }
            }
        }
    }

    SDL_Rect getCurrentSrcRect() const {
        auto it = animations.find(currentAnim);
        if (it == animations.end() || it->second.frames.empty())
            return {0, 0, 32, 32};
        return it->second.frames[currentFrame].srcRect;
    }
};
