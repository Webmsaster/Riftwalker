#include "ParticleSystem.h"
#include <tracy/Tracy.hpp>
#include "Core/Game.h"
#include <cmath>
#include <algorithm>

static float randFloat(float min, float max) {
    return min + static_cast<float>(std::rand()) / RAND_MAX * (max - min);
}

static Uint8 lerpByte(Uint8 a, Uint8 b, float t) {
    return static_cast<Uint8>(a + (b - a) * t);
}

ParticleSystem::ParticleSystem() {
    // Pre-allocate particle pool to avoid runtime reallocations
    m_particles.resize(MAX_PARTICLES);
    for (auto& p : m_particles) p.alive = false;
    m_emitters.reserve(64);
}

void ParticleSystem::update(float dt) {
    ZoneScopedN("ParticleUpdate");
    // Update emitters
    for (auto& emitter : m_emitters) {
        if (!emitter.active) continue;

        if (emitter.burstCount > 0) {
            for (int i = 0; i < emitter.burstCount; i++) spawnParticle(emitter);
            emitter.active = false;
            continue;
        }

        emitter.timer += dt;
        if (emitter.emitRate <= 0) continue;
        float interval = 1.0f / emitter.emitRate;
        while (emitter.timer >= interval) {
            emitter.timer -= interval;
            spawnParticle(emitter);
        }
    }

    // Remove inactive emitters
    m_emitters.erase(
        std::remove_if(m_emitters.begin(), m_emitters.end(),
            [](const ParticleEmitter& e) { return !e.active; }),
        m_emitters.end()
    );

    // Update particles in-place (no erase — dead slots are reused)
    m_activeCount = 0;
    for (auto& p : m_particles) {
        if (!p.alive) continue;
        p.lifetime -= dt;
        if (p.lifetime <= 0) { p.alive = false; continue; }

        p.velocity.y += p.gravity * dt;
        p.position += p.velocity * dt;
        p.size -= p.sizeDecay * dt;
        if (p.size < 0) p.size = 0;
        ++m_activeCount;
    }
}

void ParticleSystem::render(SDL_Renderer* renderer, const Camera& camera) {
    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    constexpr int CULL_MARGIN = 50;

    for (auto& p : m_particles) {
        if (!p.alive || p.size <= 0) continue;

        Vec2 screen = camera.worldToScreen(p.position);
        // Viewport culling: skip off-screen particles
        if (screen.x < -CULL_MARGIN || screen.x > screenW + CULL_MARGIN ||
            screen.y < -CULL_MARGIN || screen.y > screenH + CULL_MARGIN) continue;

        float lifeRatio = 1.0f - (p.lifetime / std::max(0.001f, p.maxLifetime));

        Uint8 r = p.color.r, g = p.color.g, b = p.color.b;
        if (p.useColorLerp) {
            r = lerpByte(p.color.r, p.colorEnd.r, lifeRatio);
            g = lerpByte(p.color.g, p.colorEnd.g, lifeRatio);
            b = lerpByte(p.color.b, p.colorEnd.b, lifeRatio);
        }
        Uint8 alpha = static_cast<Uint8>(p.color.a * (1.0f - lifeRatio * lifeRatio));

        // Apply color-blind filter to particle colors
        SDL_Color filtered = applyColorBlind({r, g, b, alpha});
        r = filtered.r; g = filtered.g; b = filtered.b;

        SDL_Rect rect = {
            static_cast<int>(screen.x - p.size / 2),
            static_cast<int>(screen.y - p.size / 2),
            static_cast<int>(p.size),
            static_cast<int>(p.size)
        };

        SDL_SetRenderDrawColor(renderer, r, g, b, alpha);
        SDL_RenderFillRect(renderer, &rect);
    }
}

int ParticleSystem::findFreeSlot() {
    // Search from hint position, wrap around
    int size = static_cast<int>(m_particles.size());
    for (int i = 0; i < size; ++i) {
        int idx = (m_firstFree + i) % size;
        if (!m_particles[idx].alive) {
            m_firstFree = (idx + 1) % size;
            return idx;
        }
    }
    return -1; // Pool full
}

void ParticleSystem::spawnParticle(const ParticleEmitter& emitter) {
    int slot = findFreeSlot();
    if (slot < 0) return; // Pool full, drop particle

    Particle& p = m_particles[slot];
    p.position = emitter.position;

    float angle = (emitter.direction + randFloat(-emitter.spread / 2, emitter.spread / 2)) * 3.14159f / 180.0f;
    float speed = emitter.speed + randFloat(-emitter.speedVariance, emitter.speedVariance);
    p.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};

    p.color = emitter.colorStart;
    p.colorEnd = emitter.colorEnd;
    p.useColorLerp = (emitter.colorEnd.a > 0);
    p.gravity = emitter.gravity;
    p.lifetime = emitter.lifetime + randFloat(-emitter.lifetimeVariance, emitter.lifetimeVariance);
    if (p.lifetime <= 0.01f) p.lifetime = 0.01f;
    p.maxLifetime = p.lifetime;
    p.size = emitter.size;
    p.sizeDecay = emitter.sizeDecay;
    p.alive = true;
    ++m_activeCount;
}

void ParticleSystem::burst(Vec2 pos, int count, SDL_Color color, float speed, float size) {
    ParticleEmitter e;
    e.position = pos;
    e.colorStart = color;
    e.burstCount = count;
    e.speed = speed;
    e.speedVariance = speed * 0.5f;
    e.lifetime = 0.5f;
    e.size = size;
    e.sizeDecay = size * 1.5f;
    e.spread = 360.0f;
    addEmitter(e);
}

void ParticleSystem::directionalBurst(Vec2 pos, int count, SDL_Color color, float dirDeg, float spreadDeg, float speed, float size) {
    ParticleEmitter e;
    e.position = pos;
    e.colorStart = color;
    e.burstCount = count;
    e.speed = speed;
    e.speedVariance = speed * 0.4f;
    e.lifetime = 0.35f;
    e.lifetimeVariance = 0.1f;
    e.size = size;
    e.sizeDecay = size * 2.0f;
    e.direction = dirDeg;
    e.spread = spreadDeg;
    e.gravity = 200.0f;
    addEmitter(e);
}

void ParticleSystem::dimensionSwitch(Vec2 pos, SDL_Color colorA, SDL_Color colorB) {
    burst(pos, 30, colorA, 200.0f, 5.0f);
    burst(pos, 30, colorB, 200.0f, 5.0f);
}

void ParticleSystem::damageEffect(Vec2 pos, SDL_Color color) {
    burst(pos, 15, color, 120.0f, 3.0f);
}

void ParticleSystem::weaponTrail(Vec2 origin, Vec2 tipPos, SDL_Color color, float intensity) {
    // Spawn trail particles along the weapon line (origin -> tip)
    int count = static_cast<int>(2 + intensity * 2); // 2-4 particles per frame
    for (int i = 0; i < count; i++) {
        int slot = findFreeSlot();
        if (slot < 0) return; // Pool full

        float t = randFloat(0.3f, 1.0f); // bias toward tip
        Vec2 pos = {
            origin.x + (tipPos.x - origin.x) * t,
            origin.y + (tipPos.y - origin.y) * t
        };
        // Small random offset perpendicular to weapon direction
        pos.x += randFloat(-3.0f, 3.0f);
        pos.y += randFloat(-3.0f, 3.0f);

        Particle& p = m_particles[slot];
        p.position = pos;
        // Slow drift away from weapon line
        p.velocity = {randFloat(-20.0f, 20.0f), randFloat(-30.0f, 10.0f)};
        p.color = color;
        p.color.a = static_cast<Uint8>(200 * intensity);
        p.colorEnd = {color.r, color.g, color.b, 0};
        p.useColorLerp = true;
        p.lifetime = 0.1f + randFloat(0.0f, 0.05f);
        p.maxLifetime = p.lifetime;
        p.size = 2.0f + randFloat(0.0f, 2.0f) * intensity;
        p.sizeDecay = p.size * 6.0f; // fast shrink
        p.gravity = 0;
        p.alive = true;
        ++m_activeCount;
    }
}

void ParticleSystem::chargeGather(Vec2 center, int count, SDL_Color color, float radius, float inwardSpeed, float size) {
    for (int i = 0; i < count; i++) {
        int slot = findFreeSlot();
        if (slot < 0) return;
        float angle = randFloat(0.0f, 360.0f) * 3.14159f / 180.0f;
        Particle& p = m_particles[slot];
        p.position = {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
        p.velocity = {-std::cos(angle) * inwardSpeed, -std::sin(angle) * inwardSpeed};
        p.color = color;
        p.colorEnd = {255, 255, 255, 0};
        p.useColorLerp = true;
        p.lifetime = radius / std::max(inwardSpeed, 1.0f);
        p.maxLifetime = p.lifetime;
        p.size = size;
        p.sizeDecay = size * 0.3f;
        p.gravity = 0;
        p.alive = true;
        ++m_activeCount;
    }
}

void ParticleSystem::ambientDust(Vec2 pos, SDL_Color color, float radius) {
    ParticleEmitter e;
    e.position = {pos.x + randFloat(-radius, radius), pos.y + randFloat(-radius, radius)};
    e.colorStart = color;
    e.colorEnd = {color.r, color.g, color.b, 0};
    e.burstCount = 1;
    e.speed = 15.0f;
    e.speedVariance = 10.0f;
    e.lifetime = 2.5f;
    e.lifetimeVariance = 1.0f;
    e.size = 2.0f;
    e.sizeDecay = 0.3f;
    e.gravity = -15.0f;
    e.spread = 360.0f;
    addEmitter(e);
}

void ParticleSystem::ambientThemeParticle(Vec2 pos, SDL_Color color, float dirDeg,
                                          float speed, float size, float lifetime,
                                          float gravity, float radius) {
    ParticleEmitter e;
    e.position = {pos.x + randFloat(-radius, radius), pos.y + randFloat(-radius, radius)};
    e.colorStart = color;
    e.colorEnd = {color.r, color.g, color.b, 0};
    e.burstCount = 1;
    e.speed = speed;
    e.speedVariance = speed * 0.4f;
    e.lifetime = lifetime;
    e.lifetimeVariance = lifetime * 0.3f;
    e.size = size;
    e.sizeDecay = size * 0.3f;
    e.direction = dirDeg;
    e.spread = 40.0f;
    e.gravity = gravity;
    addEmitter(e);
}

void ParticleSystem::addEmitter(const ParticleEmitter& emitter) {
    m_emitters.push_back(emitter);
}

void ParticleSystem::clear() {
    // Mark all particles as dead — keep pool allocated
    for (auto& p : m_particles) p.alive = false;
    m_activeCount = 0;
    m_firstFree = 0;
    m_emitters.clear();
}
