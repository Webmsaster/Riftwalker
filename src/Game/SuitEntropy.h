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
    // BALANCE: Playtest Round 2 - still 100% entropy death. Switch cost is main killer.
    float passiveDecay = 0.15f;      // Entropy decay per second (was 0)
    float switchCost = 0.5f;         // Entropy per dimension switch (was 5 -> 3 -> 1.5 -> 0.5)
    float damageCostMultiplier = 0.1f; // Entropy per damage point (was 0.2)
    float repairReduction = 30.0f;   // Entropy reduced per rift repair (was 15)
    float entropyGainMultiplier = 1.0f; // Run buff: reduces entropy gain
    float passiveGainModifier = 1.0f;   // Relic: Entropy Anchor modifier
    float passiveDecayModifier = 1.0f;  // Relic: TimeDistortion slows entropy decay
    // FIX: EntropyResistance upgrade was purchased but never applied
    float upgradeResistance = 1.0f;     // Upgrade: reduces all entropy gain

private:
    float m_entropy = 0;
    float m_maxEntropy = 100.0f;
    // BALANCE: Passive entropy gain 0.5 -> 0.2 (playtest: entropy kills 100% runs before L1 clear)
    float m_passiveGain = 0.2f; // per second

    // Visual effect timers
    float m_flickerTimer = 0;
    float m_glitchTimer = 0;

    // Forced dimension switch at high entropy (playtest: was triggering death spiral)
    bool m_forceSwitch = false;
    float m_forceSwitchTimer = 0;
    float m_forceSwitchInterval = 8.0f; // was 5.0f - longer interval = less entropy from forced switches
};
