#include "SuitEntropy.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

SuitEntropy::SuitEntropy() {}

void SuitEntropy::update(float dt) {
    // Passive entropy gain (modified by relics and upgrade)
    // FIX: Apply upgradeResistance so EntropyResistance upgrade actually works
    m_entropy += m_passiveGain * passiveGainModifier * upgradeResistance * dt;

    // Passive decay (upgradeable, modifiable by relics e.g. TimeDistortion)
    if (passiveDecay > 0) {
        m_entropy -= passiveDecay * passiveDecayModifier * dt;
    }

    m_entropy = std::clamp(m_entropy, 0.0f, m_maxEntropy);

    // Timers for visual effects
    float percent = getPercent();

    // Flicker at 25%+
    if (percent >= 0.25f) {
        m_flickerTimer += dt;
    }

    // Glitch at 50%+
    if (percent >= 0.5f) {
        m_glitchTimer += dt;
    }

    // Forced dimension switch at 75%+
    if (percent >= 0.75f) {
        m_forceSwitchTimer += dt;
        float interval = m_forceSwitchInterval * (1.0f - (percent - 0.75f) * 2.0f);
        interval = std::max(interval, 1.0f);
        if (m_forceSwitchTimer >= interval) {
            m_forceSwitchTimer = 0;
            m_forceSwitch = true;
        }
    }
}

void SuitEntropy::applyVisualEffects(SDL_Renderer* renderer, int screenW, int screenH) {
    float percent = getPercent();
    if (percent < 0.25f) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 25%+: Screen flicker
    if (percent >= 0.25f) {
        float flickerChance = (percent - 0.25f) * 0.3f;
        if (static_cast<float>(std::rand()) / RAND_MAX < flickerChance * 0.1f) {
            Uint8 a = static_cast<Uint8>(20 + 40 * percent);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, a);
            SDL_Rect rect = {0, 0, screenW, screenH};
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    // 50%+: Scanline distortion
    if (percent >= 0.5f) {
        int glitchLines = static_cast<int>((percent - 0.5f) * 30);
        for (int i = 0; i < glitchLines; i++) {
            if (static_cast<float>(std::rand()) / RAND_MAX > 0.3f) continue;
            int y = std::rand() % screenH;
            int h = 1 + std::rand() % static_cast<int>(2 + percent * 4);
            int offset = static_cast<int>((std::rand() % 30 - 15) * percent);
            Uint8 r = std::rand() % 256;
            Uint8 g = std::rand() % 256;
            Uint8 b = std::rand() % 256;
            Uint8 a = static_cast<Uint8>(30 * percent);
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            SDL_Rect line = {offset, y, screenW + 20, h};
            SDL_RenderFillRect(renderer, &line);
        }
    }

    // 75%+: Color aberration (red/blue offset rectangles)
    if (percent >= 0.75f) {
        float intensity = (percent - 0.75f) * 4.0f;
        if (static_cast<float>(std::rand()) / RAND_MAX < intensity * 0.2f) {
            int blockW = 50 + std::rand() % 200;
            int blockH = 20 + std::rand() % 100;
            int x = std::rand() % screenW;
            int y = std::rand() % screenH;
            Uint8 a = static_cast<Uint8>(20 + 40 * intensity);

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, a);
            SDL_Rect block = {x - 3, y, blockW, blockH};
            SDL_RenderFillRect(renderer, &block);

            SDL_SetRenderDrawColor(renderer, 0, 0, 255, a);
            block = {x + 3, y, blockW, blockH};
            SDL_RenderFillRect(renderer, &block);
        }
    }

    // 90%+: Heavy distortion warning
    if (percent >= 0.9f) {
        float pulse = std::sin(SDL_GetTicks() * 0.01f) * 0.5f + 0.5f;
        Uint8 a = static_cast<Uint8>(pulse * 40);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, a);
        SDL_Rect border = {0, 0, screenW, 4};
        SDL_RenderFillRect(renderer, &border);
        border = {0, screenH - 4, screenW, 4};
        SDL_RenderFillRect(renderer, &border);
        border = {0, 0, 4, screenH};
        SDL_RenderFillRect(renderer, &border);
        border = {screenW - 4, 0, 4, screenH};
        SDL_RenderFillRect(renderer, &border);
    }
}

void SuitEntropy::addEntropy(float amount) {
    // FIX: Apply upgradeResistance so EntropyResistance upgrade reduces all entropy gain
    m_entropy = std::min(m_entropy + amount * entropyGainMultiplier * upgradeResistance, m_maxEntropy);
}

void SuitEntropy::reduceEntropy(float amount) {
    m_entropy = std::max(m_entropy - amount, 0.0f);
}

void SuitEntropy::onDimensionSwitch() {
    addEntropy(switchCost);
}

void SuitEntropy::onDamageTaken(float damage) {
    addEntropy(damage * damageCostMultiplier);
}

void SuitEntropy::onRiftRepaired() {
    reduceEntropy(repairReduction);
}

float SuitEntropy::getInputDistortion() const {
    float percent = getPercent();
    if (percent < 0.5f) return 0.0f;
    return (percent - 0.5f) * 2.0f; // 0-1 range from 50%-100%
}
