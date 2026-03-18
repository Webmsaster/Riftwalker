#include "DimensionManager.h"
#include <cmath>
#include <cstdlib>

DimensionManager::DimensionManager() {}

bool DimensionManager::switchDimension(bool force) {
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

    // Glitch scanlines during switch
    if (m_glitchIntensity > 0.1f) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        int lineCount = static_cast<int>(m_glitchIntensity * 20);
        for (int i = 0; i < lineCount; i++) {
            int y = std::rand() % screenH;
            int h = 1 + std::rand() % 3;
            int offset = static_cast<int>((std::rand() % 20 - 10) * m_glitchIntensity);
            Uint8 a = static_cast<Uint8>(m_glitchIntensity * 100);

            SDL_Color& c = (std::rand() % 2 == 0) ? m_dimColorA : m_dimColorB;
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, a);
            SDL_Rect line = {offset, y, screenW, h};
            SDL_RenderFillRect(renderer, &line);
        }
    }
}
