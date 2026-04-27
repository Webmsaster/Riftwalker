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
    m_drawBuf.reserve(MAX_PARTICLES);
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

    // Pass 1: Pre-compute all draw data into m_drawBuf (avoids state ping-pong
    // and double-computation across the three render passes below).
    m_drawBuf.clear();
    for (auto& p : m_particles) {
        if (!p.alive || p.size <= 0) continue;

        Vec2 screen = camera.worldToScreen(p.position);
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

        SDL_Color filtered = applyColorBlind({r, g, b, alpha});

        DrawParticle d;
        d.rect = {
            static_cast<int>(screen.x - p.size / 2),
            static_cast<int>(screen.y - p.size / 2),
            static_cast<int>(p.size),
            static_cast<int>(p.size)
        };
        d.r = filtered.r; d.g = filtered.g; d.b = filtered.b; d.alpha = alpha;
        d.hasGlow = (p.size >= 3.0f && alpha > 30);
        d.hasCore = (p.size >= 2.5f && alpha > 50);
        if (d.hasCore) {
            d.coreA = static_cast<Uint8>(std::min(255, static_cast<int>(alpha) + 50));
            d.coreR = static_cast<Uint8>(std::min(255, static_cast<int>(d.r) + 100));
            d.coreG = static_cast<Uint8>(std::min(255, static_cast<int>(d.g) + 90));
            d.coreB = static_cast<Uint8>(std::min(255, static_cast<int>(d.b) + 70));
            d.coreSize = std::max(1, static_cast<int>(p.size * 0.35f));
        }
        m_drawBuf.push_back(d);
    }

    // Pass 2: All additive glow halos in a single ADD block.
    // Single combined glow rect (was 2 halos at alpha 0.18 + 0.30) — visually
    // near-identical with ADD blending and halves SetColor+FillRect per glow particle.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    for (const auto& d : m_drawBuf) {
        if (!d.hasGlow) continue;
        Uint8 glowA = static_cast<Uint8>(d.alpha * 0.40f);
        SDL_SetRenderDrawColor(renderer, d.r, d.g, d.b, glowA);
        SDL_Rect glow = {d.rect.x - 3, d.rect.y - 3, d.rect.w + 6, d.rect.h + 6};
        SDL_RenderFillRect(renderer, &glow);
    }

    // Pass 3: All core rects + hot centers in a single BLEND block
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const auto& d : m_drawBuf) {
        SDL_SetRenderDrawColor(renderer, d.r, d.g, d.b, d.alpha);
        SDL_RenderFillRect(renderer, &d.rect);
        if (d.hasCore) {
            SDL_SetRenderDrawColor(renderer, d.coreR, d.coreG, d.coreB, d.coreA);
            SDL_Rect core = {
                d.rect.x + d.rect.w / 2 - d.coreSize / 2,
                d.rect.y + d.rect.h / 2 - d.coreSize / 2,
                d.coreSize, d.coreSize
            };
            SDL_RenderFillRect(renderer, &core);
        }
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
    // Size variation: ±30% for visual interest
    float sizeVar = emitter.size * randFloat(0.7f, 1.3f);
    p.size = sizeVar;
    p.sizeDecay = emitter.sizeDecay * (sizeVar / std::max(emitter.size, 0.1f));
    p.alive = true;
    ++m_activeCount;
}

void ParticleSystem::burst(const Vec2& pos, int count, SDL_Color color, float speed, float size) {
    ParticleEmitter e;
    e.position = pos;
    e.colorStart = color;
    // Color lerp: bright start → dark ember end
    e.colorEnd = {
        static_cast<Uint8>(color.r / 3),
        static_cast<Uint8>(color.g / 4),
        static_cast<Uint8>(color.b / 4),
        static_cast<Uint8>(1) // non-zero triggers useColorLerp
    };
    e.burstCount = std::max(1, static_cast<int>(count * g_particleQualityMult));
    e.speed = speed;
    e.speedVariance = speed * 0.5f;
    e.lifetime = 0.5f;
    e.lifetimeVariance = 0.15f; // Size variation via lifetime spread
    e.size = size;
    e.sizeDecay = size * 1.5f;
    e.spread = 360.0f;
    e.gravity = 150.0f; // Particles fall for more physical feel
    addEmitter(e);
}

void ParticleSystem::directionalBurst(const Vec2& pos, int count, SDL_Color color, float dirDeg, float spreadDeg, float speed, float size) {
    ParticleEmitter e;
    e.position = pos;
    e.colorStart = color;
    // Color lerp: bright → ember
    e.colorEnd = {
        static_cast<Uint8>(color.r / 3),
        static_cast<Uint8>(color.g / 4),
        static_cast<Uint8>(color.b / 4),
        static_cast<Uint8>(1)
    };
    e.burstCount = count;
    e.speed = speed;
    e.speedVariance = speed * 0.4f;
    e.lifetime = 0.4f;
    e.lifetimeVariance = 0.12f;
    e.size = size;
    e.sizeDecay = size * 2.0f;
    e.direction = dirDeg;
    e.spread = spreadDeg;
    e.gravity = 200.0f;
    e.burstCount = std::max(1, static_cast<int>(e.burstCount * g_particleQualityMult));
    addEmitter(e);
}

void ParticleSystem::dimensionSwitch(const Vec2& pos, SDL_Color colorA, SDL_Color colorB) {
    burst(pos, 30, colorA, 200.0f, 5.0f);
    burst(pos, 30, colorB, 200.0f, 5.0f);
}

void ParticleSystem::damageEffect(const Vec2& pos, SDL_Color color) {
    burst(pos, 15, color, 120.0f, 3.0f);
}

void ParticleSystem::weaponTrail(const Vec2& origin, const Vec2& tipPos, SDL_Color color, float intensity) {
    // Spawn trail particles along the weapon line (origin -> tip)
    int count = static_cast<int>((3 + intensity * 3) * g_particleQualityMult); // 3-6 particles per frame
    if (count < 1) count = 1;
    for (int i = 0; i < count; i++) {
        int slot = findFreeSlot();
        if (slot < 0) return; // Pool full

        float t = randFloat(0.2f, 1.0f); // bias toward tip
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

        // Bright core particles near tip, dimmer further away
        bool isCore = (t > 0.7f && randFloat(0.0f, 1.0f) < 0.4f);
        if (isCore) {
            // White-hot core particles
            p.color = {
                static_cast<Uint8>(std::min(255, color.r + 100)),
                static_cast<Uint8>(std::min(255, color.g + 100)),
                static_cast<Uint8>(std::min(255, color.b + 80)),
                static_cast<Uint8>(255 * intensity)
            };
            p.colorEnd = color;
            p.colorEnd.a = 1; // Triggers lerp
        } else {
            p.color = color;
            p.color.a = static_cast<Uint8>(200 * intensity);
            p.colorEnd = {static_cast<Uint8>(color.r / 2), static_cast<Uint8>(color.g / 2),
                          static_cast<Uint8>(color.b / 2), 1};
        }
        p.useColorLerp = true;
        p.lifetime = 0.12f + randFloat(0.0f, 0.08f); // Slightly longer persistence
        p.maxLifetime = p.lifetime;
        p.size = 2.0f + randFloat(0.0f, 2.5f) * intensity;
        p.sizeDecay = p.size * 5.0f;
        p.gravity = 0;
        p.alive = true;
        ++m_activeCount;
    }
}

void ParticleSystem::chargeGather(const Vec2& center, int count, SDL_Color color, float radius, float inwardSpeed, float size) {
    count = std::max(1, static_cast<int>(count * g_particleQualityMult));
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

void ParticleSystem::ambientDust(const Vec2& pos, SDL_Color color, float radius) {
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

void ParticleSystem::ambientThemeParticle(const Vec2& pos, SDL_Color color, float dirDeg,
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
