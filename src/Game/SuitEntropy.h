#pragma once
#include <SDL2/SDL.h>
#include <functional>

class SuitEntropy {
public:
    SuitEntropy();

    void update(float dt);
    void applyVisualEffects(SDL_Renderer* renderer, int screenW, int screenH);

    // Entropy modifiers
    void addEntropy(float amount);
    void reduceEntropy(float amount);
    void onDimensionSwitch();
    void onDamageTaken(float damage);
    void onRiftRepaired();

    float getEntropy() const { return m_entropy; }
    float getMaxEntropy() const { return m_maxEntropy; }
    float getPercent() const { return m_maxEntropy > 0 ? m_entropy / m_maxEntropy : 0.0f; }
    bool isCritical() const { return m_entropy >= m_maxEntropy; }

    // Input distortion amount (0-1)
    float getInputDistortion() const;
    // Should force dimension switch?
    bool shouldForceSwitch() const { return m_forceSwitch; }
    void clearForceSwitch() { m_forceSwitch = false; }

    // Upgradeable stats
    float passiveDecay = 0.0f;      // Entropy decay per second
    float switchCost = 5.0f;         // Entropy per dimension switch
    float damageCostMultiplier = 0.2f; // Entropy per damage point
    float repairReduction = 15.0f;   // Entropy reduced per rift repair
    float entropyGainMultiplier = 1.0f; // Run buff: reduces entropy gain
    float passiveGainModifier = 1.0f;   // Relic: Entropy Anchor modifier
    // FIX: EntropyResistance upgrade was purchased but never applied
    float upgradeResistance = 1.0f;     // Upgrade: reduces all entropy gain

private:
    float m_entropy = 0;
    float m_maxEntropy = 100.0f;
    float m_passiveGain = 1.0f; // per second

    // Visual effect timers
    float m_flickerTimer = 0;
    float m_glitchTimer = 0;

    // Forced dimension switch at high entropy
    bool m_forceSwitch = false;
    float m_forceSwitchTimer = 0;
    float m_forceSwitchInterval = 5.0f; // gets shorter at higher entropy
};
