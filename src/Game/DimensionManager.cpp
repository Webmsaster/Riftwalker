#include "DimensionManager.h"
#include <cmath>
#include <cstdlib>

DimensionManager::DimensionManager() {}

bool DimensionManager::switchDimension(bool force) {
    if (locked && !force) return false; // DimensionLock challenge
    if (m_switching) return false;
    if (!force && m_cooldownTimer > 0) return false;

    m_switching = true;
    m_switchTimer = 0;
    m_hasSwitchedMidpoint = false;
    m_switchCount++;
    m_glitchIntensity = 1.0f;

    // Build resonance on each switch
    m_resonance = std::min(1.0f, m_resonance + m_resonancePerSwitch);

    // Particle effect at player position
    if (particles) {
        SDL_Color cA = m_dimColorA;
        SDL_Color cB = m_dimColorB;
        particles->dimensionSwitch(playerPos, cA, cB);
    }
    return true;
}

void DimensionManager::update(float dt) {
    if (m_cooldownTimer > 0) m_cooldownTimer -= dt;

    if (m_switching) {
        m_switchTimer += dt;
        if (switchDuration <= 0) switchDuration = 0.01f;
        float progress = m_switchTimer / switchDuration;

        if (progress < 0.5f) {
            // Blend out current dimension
            m_blendAlpha = progress * 2.0f;
        } else {
            // At midpoint, actually switch (once per transition)
            if (!m_hasSwitchedMidpoint) {
                m_currentDim = (m_currentDim == 1) ? 2 : 1;
                m_hasSwitchedMidpoint = true;
            }
            m_blendAlpha = 1.0f - (progress - 0.5f) * 2.0f;
        }

        if (m_switchTimer >= switchDuration) {
            m_switching = false;
            m_blendAlpha = 0;
            m_cooldownTimer = switchCooldown;
        }
    }

    // Decay glitch intensity
    if (m_glitchIntensity > 0) {
        m_glitchIntensity -= dt * 3.0f;
        if (m_glitchIntensity < 0) m_glitchIntensity = 0;
    }

    // Decay resonance over time (not during switch animation — switching should never drain)
    if (m_resonance > 0 && !m_switching) {
        m_resonance -= m_resonanceDecay * dt;
        if (m_resonance < 0) m_resonance = 0;
    }
}

int DimensionManager::getResonanceTier() const {
    if (m_resonance >= 0.85f) return 3; // High
    if (m_resonance >= 0.5f) return 2;  // Mid
    if (m_resonance >= 0.25f) return 1; // Low
    return 0;
}

float DimensionManager::getResonanceDamageMult() const {
    switch (getResonanceTier()) {
        case 1: return 1.15f; // +15%
        case 2: return 1.30f; // +30%
        case 3: return 1.50f; // +50%
        default: return 1.0f;
    }
}

float DimensionManager::getResonanceSpeedMult() const {
    switch (getResonanceTier()) {
        case 2: return 1.10f; // +10%
        case 3: return 1.20f; // +20%
        default: return 1.0f;
    }
}

void DimensionManager::applyVisualEffect(SDL_Renderer* renderer, int screenW, int screenH) {
    if (m_blendAlpha <= 0.01f && m_glitchIntensity <= 0.01f) return;

    // Dimension transition overlay
    if (m_blendAlpha > 0.01f) {
        SDL_Color& dimColor = (m_currentDim == 1) ? m_dimColorB : m_dimColorA;
        Uint8 alpha = static_cast<Uint8>(m_blendAlpha * 60);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, dimColor.r, dimColor.g, dimColor.b, alpha);
        SDL_Rect fullscreen = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &fullscreen);
    }

    // Enhanced glitch effect during dimension switch
    if (m_glitchIntensity > 0.1f) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint32 ticks = SDL_GetTicks();

        // Glitch scanlines (horizontal displacement bands)
        int lineCount = static_cast<int>(m_glitchIntensity * 25);
        for (int i = 0; i < lineCount; i++) {
            int y = std::rand() % screenH;
            int h = 1 + std::rand() % 4;
            int offset = static_cast<int>((std::rand() % 30 - 15) * m_glitchIntensity);
            Uint8 a = static_cast<Uint8>(m_glitchIntensity * 120);
            SDL_Color& c = (std::rand() % 2 == 0) ? m_dimColorA : m_dimColorB;
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, a);
            SDL_Rect line = {offset, y, screenW, h};
            SDL_RenderFillRect(renderer, &line);
        }

        // Rift tear: bright horizontal crack across screen center
        if (m_glitchIntensity > 0.3f) {
            float tearIntensity = (m_glitchIntensity - 0.3f) / 0.7f; // 0-1 in upper range
            int centerY = screenH / 2 + static_cast<int>(std::sin(ticks * 0.01f) * 20 * tearIntensity);
            int tearH = 2 + static_cast<int>(tearIntensity * 6);

            // Bright core of the tear
            Uint8 tearAlpha = static_cast<Uint8>(200 * tearIntensity);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, tearAlpha);
            SDL_Rect tearCore = {0, centerY - tearH / 2, screenW, tearH};
            SDL_RenderFillRect(renderer, &tearCore);

            // Color-shifted edges (chromatic aberration simulation)
            Uint8 chromAlpha = static_cast<Uint8>(100 * tearIntensity);
            SDL_SetRenderDrawColor(renderer, m_dimColorA.r, m_dimColorA.g, m_dimColorA.b, chromAlpha);
            SDL_Rect tearTop = {0, centerY - tearH - 2, screenW, 2};
            SDL_RenderFillRect(renderer, &tearTop);
            SDL_SetRenderDrawColor(renderer, m_dimColorB.r, m_dimColorB.g, m_dimColorB.b, chromAlpha);
            SDL_Rect tearBot = {0, centerY + tearH / 2, screenW, 2};
            SDL_RenderFillRect(renderer, &tearBot);

            // Sparks along the tear
            for (int s = 0; s < 8; s++) {
                float sparkX = (ticks * 0.5f + s * 173.7f);
                int sx = static_cast<int>(std::fmod(sparkX, static_cast<float>(screenW)));
                int sy = centerY + static_cast<int>(std::sin(sparkX * 0.03f) * tearH * 2);
                Uint8 sa = static_cast<Uint8>(180 * tearIntensity * (0.5f + 0.5f * std::sin(ticks * 0.02f + s)));
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, sa);
                SDL_Rect spark = {sx - 1, sy - 1, 3, 3};
                SDL_RenderFillRect(renderer, &spark);
            }
        }
    }
}
