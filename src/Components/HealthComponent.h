#pragma once
#include "ECS/Component.h"
#include <functional>
#include <algorithm>

struct HealthComponent : public Component {
    float maxHP = 100.0f;
    float currentHP = 100.0f;
    float armor = 0.0f;         // damage reduction (0-1)
    bool invulnerable = false;

    // I-frames
    float invincibilityTime = 0.5f;
    float invincibilityTimer = 0.0f;
    bool isInvincible() const { return invincibilityTimer > 0 || invulnerable; }

    // Callbacks
    std::function<void(float damage)> onDamage;
    std::function<void()> onDeath;
    std::function<void(float amount)> onHeal;

    void takeDamage(float amount) {
        if (isInvincible() || currentHP <= 0) return;
        float actual = amount * (1.0f - armor);
        currentHP -= actual;
        invincibilityTimer = invincibilityTime;
        if (onDamage) onDamage(actual);
        if (currentHP <= 0) {
            currentHP = 0;
            if (onDeath) onDeath();
        }
    }

    void heal(float amount) {
        float old = currentHP;
        currentHP = std::min(currentHP + amount, maxHP);
        if (onHeal) onHeal(currentHP - old);
    }

    float getPercent() const { return currentHP / maxHP; }

    void update(float dt) override {
        if (invincibilityTimer > 0) invincibilityTimer -= dt;
    }
};
