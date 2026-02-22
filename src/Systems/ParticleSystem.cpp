#include "ParticleSystem.h"
#include <cmath>
#include <algorithm>

static float randFloat(float min, float max) {
    return min + static_cast<float>(std::rand()) / RAND_MAX * (max - min);
}

static Uint8 lerpByte(Uint8 a, Uint8 b, float t) {
    return static_cast<Uint8>(a + (b - a) * t);
}

void ParticleSystem::update(float dt) {
    // Update emitters
    for (auto& emitter : m_emitters) {
        if (!emitter.active) continue;

        if (emitter.burstCount > 0) {
            for (int i = 0; i < emitter.burstCount; i++) spawnParticle(emitter);
            emitter.active = false;
            continue;
        }

        emitter.timer += dt;
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

    // Update particles
    for (auto& p : m_particles) {
        if (!p.alive) continue;
        p.lifetime -= dt;
        if (p.lifetime <= 0) { p.alive = false; continue; }

        p.position += p.velocity * dt;
        p.size -= p.sizeDecay * dt;
        if (p.size < 0) p.size = 0;
    }

    // Remove dead particles
    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(),
            [](const Particle& p) { return !p.alive; }),
        m_particles.end()
    );
}

void ParticleSystem::render(SDL_Renderer* renderer, const Camera& camera) {
    for (auto& p : m_particles) {
        if (!p.alive || p.size <= 0) continue;

        float lifeRatio = 1.0f - (p.lifetime / p.maxLifetime);
        Uint8 alpha = static_cast<Uint8>(p.color.a * (1.0f - lifeRatio));

        Vec2 screen = camera.worldToScreen(p.position);
        SDL_Rect rect = {
            static_cast<int>(screen.x - p.size / 2),
            static_cast<int>(screen.y - p.size / 2),
            static_cast<int>(p.size),
            static_cast<int>(p.size)
        };

        SDL_SetRenderDrawColor(renderer, p.color.r, p.color.g, p.color.b, alpha);
        SDL_RenderFillRect(renderer, &rect);
    }
}

void ParticleSystem::spawnParticle(const ParticleEmitter& emitter) {
    if (m_particles.size() >= MAX_PARTICLES) return;

    Particle p;
    p.position = emitter.position;

    float angle = (emitter.direction + randFloat(-emitter.spread / 2, emitter.spread / 2)) * 3.14159f / 180.0f;
    float speed = emitter.speed + randFloat(-emitter.speedVariance, emitter.speedVariance);
    p.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};
    if (emitter.gravity != 0) p.velocity.y += emitter.gravity;

    p.color = emitter.colorStart;
    p.lifetime = emitter.lifetime + randFloat(-emitter.lifetimeVariance, emitter.lifetimeVariance);
    p.maxLifetime = p.lifetime;
    p.size = emitter.size;
    p.sizeDecay = emitter.sizeDecay;
    p.alive = true;

    m_particles.push_back(p);
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

void ParticleSystem::dimensionSwitch(Vec2 pos, SDL_Color colorA, SDL_Color colorB) {
    burst(pos, 30, colorA, 200.0f, 5.0f);
    burst(pos, 30, colorB, 200.0f, 5.0f);
}

void ParticleSystem::damageEffect(Vec2 pos, SDL_Color color) {
    burst(pos, 15, color, 120.0f, 3.0f);
}

void ParticleSystem::addEmitter(const ParticleEmitter& emitter) {
    m_emitters.push_back(emitter);
}

void ParticleSystem::clear() {
    m_particles.clear();
    m_emitters.clear();
}
