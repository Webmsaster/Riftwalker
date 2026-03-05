#pragma once
#include "ECS/Component.h"

enum class AbilityID { GroundSlam = 0, RiftShield = 1, PhaseStrike = 2, COUNT };

struct AbilityData {
    float cooldown = 6.0f;
    float cooldownTimer = 0;
    float duration = 0;       // active time remaining
    float maxDuration = 0;
    bool active = false;

    bool isReady() const { return cooldownTimer <= 0 && !active; }
    float getCooldownPercent() const {
        if (cooldownTimer <= 0 || cooldown <= 0) return 1.0f;
        return 1.0f - cooldownTimer / cooldown;
    }
};

struct AbilityComponent : public Component {
    AbilityData abilities[3];

    // Ground Slam (Ability 0)
    float slamDamage = 60.0f;
    float slamRadius = 120.0f;
    float slamStunDuration = 0.8f;
    bool slamFalling = false;      // currently in slam fall
    float slamFallStart = 0;       // Y position when slam started

    // Rift Shield (Ability 1)
    float shieldMaxDuration = 2.0f;
    int shieldHitsRemaining = 0;
    int shieldMaxHits = 3;
    float shieldAbsorbedDamage = 0;
    float shieldHealPerAbsorb = 5.0f;
    float shieldBurstDamage = 25.0f;
    float shieldBurstRadius = 100.0f;
    bool shieldBurst = false;      // triggered when 3 hits absorbed

    // Phase Strike (Ability 2)
    float phaseStrikeRange = 200.0f;
    float phaseStrikeDamageMult = 2.0f; // backstab crit multiplier
    float phaseStrikeCDRefund = 0.5f;   // 50% CD refund on kill

    AbilityComponent() {
        abilities[0].cooldown = 6.0f;   // Ground Slam
        abilities[0].maxDuration = 0;    // instant on landing
        abilities[1].cooldown = 10.0f;  // Rift Shield
        abilities[1].maxDuration = 2.0f;
        abilities[2].cooldown = 8.0f;   // Phase Strike
        abilities[2].maxDuration = 0;    // instant
    }

    void update(float dt) override {
        for (int i = 0; i < 3; i++) {
            if (abilities[i].cooldownTimer > 0) {
                abilities[i].cooldownTimer -= dt;
                if (abilities[i].cooldownTimer < 0) abilities[i].cooldownTimer = 0;
            }
            if (abilities[i].active && abilities[i].maxDuration > 0) {
                abilities[i].duration -= dt;
                if (abilities[i].duration <= 0) {
                    abilities[i].active = false;
                    abilities[i].duration = 0;
                }
            }
        }
    }

    void activate(int index) {
        if (index < 0 || index >= 3) return;
        abilities[index].active = true;
        abilities[index].duration = abilities[index].maxDuration;
        abilities[index].cooldownTimer = abilities[index].cooldown;
    }

    void reduceCooldown(int index, float percent) {
        if (index < 0 || index >= 3) return;
        abilities[index].cooldownTimer *= (1.0f - percent);
    }
};
