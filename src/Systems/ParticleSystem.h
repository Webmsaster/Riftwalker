#pragma once
#include "Core/Camera.h"
#include <SDL2/SDL.h>
#include <vector>
#include <cstdlib>

struct Particle {
    Vec2 position;
    Vec2 velocity;
    SDL_Color color;
    float lifetime;
    float maxLifetime;
    float size;
    float sizeDecay;
    bool alive = true;
};

struct ParticleEmitter {
    Vec2 position;
    SDL_Color colorStart{255, 255, 255, 255};
    SDL_Color colorEnd{255, 255, 255, 0};
    float emitRate = 10.0f; // particles per second
    float speed = 100.0f;
    float speedVariance = 50.0f;
    float lifetime = 1.0f;
    float lifetimeVariance = 0.3f;
    float size = 4.0f;
    float sizeDecay = 0.5f;
    float spread = 360.0f; // degrees
    float direction = 0.0f; // degrees
    float gravity = 0.0f;
    int burstCount = 0; // 0 = continuous
    bool active = true;
    float timer = 0;
};

class ParticleSystem {
public:
    void update(float dt);
    void render(SDL_Renderer* renderer, const Camera& camera);

    void addEmitter(const ParticleEmitter& emitter);
    void burst(Vec2 pos, int count, SDL_Color color, float speed = 150.0f, float size = 3.0f);
    void dimensionSwitch(Vec2 pos, SDL_Color colorA, SDL_Color colorB);
    void damageEffect(Vec2 pos, SDL_Color color);
    void clear();

private:
    void spawnParticle(const ParticleEmitter& emitter);

    std::vector<Particle> m_particles;
    std::vector<ParticleEmitter> m_emitters;
    static constexpr int MAX_PARTICLES = 2000;
};
