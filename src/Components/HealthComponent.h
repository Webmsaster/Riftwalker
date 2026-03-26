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

    // Damage display timer (for enemy HP bars — visible briefly after taking damage)
    float damageShowTimer = 0;
    static constexpr float DAMAGE_SHOW_DURATION = 2.5f;

    // Lag HP: trails behind currentHP to show a "damage dealt" indicator
    float lagHP = -1.0f; // -1 = uninitialized (will snap to currentHP on first use)
    static constexpr float LAG_HP_DELAY = 0.4f;    // seconds before lag bar starts draining
    static constexpr float LAG_HP_SPEED = 60.0f;   // HP per second drain rate
    float lagHPDelay = 0;  // current delay countdown

    // Hit flash: brief white tint on sprite when damaged
    float hitFlashTimer = 0;
    static constexpr float HIT_FLASH_DURATION = 0.12f;

    // Callbacks
    std::function<void(float damage)> onDamage;
    std::function<void()> onDeath;
    std::function<void(float amount)> onHeal;

    void takeDamage(float amount) {
        if (isInvincible() || currentHP <= 0) return;
        float clampedArmor = std::max(0.0f, std::min(1.0f, armor));
        float actual = amount * (1.0f - clampedArmor);
        currentHP -= actual;
        invincibilityTimer = invincibilityTime;
        damageShowTimer = DAMAGE_SHOW_DURATION;
        hitFlashTimer = HIT_FLASH_DURATION;
        lagHPDelay = LAG_HP_DELAY; // reset lag bar delay on new damage
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

    float getPercent() const { return maxHP > 0 ? currentHP / maxHP : 0.0f; }

    float getLagPercent() const {
        float lag = (lagHP < 0) ? currentHP : lagHP;
        return maxHP > 0 ? lag / maxHP : 0.0f;
    }

    void update(float dt) override {
        if (invincibilityTimer > 0) invincibilityTimer -= dt;
        if (damageShowTimer > 0) damageShowTimer -= dt;
        if (hitFlashTimer > 0) hitFlashTimer -= dt;

        // Animate lag HP towards currentHP
        if (lagHP < 0) lagHP = currentHP; // initialize
        if (lagHP > currentHP) {
            if (lagHPDelay > 0) {
                lagHPDelay -= dt;
            } else {
                lagHP -= LAG_HP_SPEED * dt;
                if (lagHP < currentHP) lagHP = currentHP;
            }
        } else {
            lagHP = currentHP; // snap up on heal
        }
    }
};
