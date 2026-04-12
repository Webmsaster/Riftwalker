#pragma once
#include "Core/Camera.h"
#include <SDL2/SDL.h>
#include <vector>
#include <cstdlib>

struct Particle {
    Vec2 position;
    Vec2 velocity;
    SDL_Color color;
    SDL_Color colorEnd{0, 0, 0, 0};
    float lifetime;
    float maxLifetime;
    float size;
    float sizeDecay;
    float gravity = 0;
    bool useColorLerp = false;
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
    ParticleSystem();

    void update(float dt);
    void render(SDL_Renderer* renderer, const Camera& camera);

    void addEmitter(const ParticleEmitter& emitter);
    void burst(const Vec2& pos, int count, SDL_Color color, float speed = 150.0f, float size = 3.0f);
    void directionalBurst(const Vec2& pos, int count, SDL_Color color, float dirDeg, float spreadDeg = 90.0f, float speed = 180.0f, float size = 3.0f);
    void dimensionSwitch(const Vec2& pos, SDL_Color colorA, SDL_Color colorB);
    void damageEffect(const Vec2& pos, SDL_Color color);
    void weaponTrail(const Vec2& origin, const Vec2& tipPos, SDL_Color color, float intensity = 1.0f);
    void chargeGather(const Vec2& center, int count, SDL_Color color, float radius, float inwardSpeed, float size = 2.5f);
    void ambientDust(const Vec2& pos, SDL_Color color, float radius = 200.0f);
    void ambientThemeParticle(const Vec2& pos, SDL_Color color, float dirDeg = 270.0f,
                              float speed = 15.0f, float size = 2.0f,
                              float lifetime = 3.0f, float gravity = 0.0f,
                              float radius = 300.0f);
    void clear();

    // Debug info
    int getActiveCount() const { return m_activeCount; }
    int getPoolSize() const { return static_cast<int>(m_particles.size()); }
    int getEmitterCount() const { return static_cast<int>(m_emitters.size()); }

private:
    void spawnParticle(const ParticleEmitter& emitter);
    int findFreeSlot();

    // Pre-computed per-particle draw data — built in render() first pass,
    // consumed in subsequent glow/core/hot passes to eliminate per-particle
    // SDL_SetRenderDrawBlendMode ping-pong (2000-4000 pipeline flushes/frame).
    struct DrawParticle {
        SDL_Rect rect;
        Uint8 r, g, b, alpha;
        Uint8 coreR, coreG, coreB, coreA;
        int coreSize;
        bool hasGlow;
        bool hasCore;
    };

    std::vector<Particle> m_particles;
    std::vector<ParticleEmitter> m_emitters;
    std::vector<DrawParticle> m_drawBuf;  // Reused across frames; reserved to pool size
    int m_activeCount = 0;       // Cached count of alive particles
    int m_firstFree = 0;         // Hint for next free slot search
    static constexpr int MAX_PARTICLES = 2000;
};
