#pragma once
#include "ECS/Component.h"
#include "Core/Camera.h"

enum class AttackType {
    Melee,
    Ranged,
    Dash,
    Charged
};

struct AttackData {
    float damage = 10.0f;
    float knockback = 200.0f;
    float range = 48.0f;
    float cooldown = 0.4f;
    float duration = 0.2f;
    AttackType type = AttackType::Melee;
};

struct CombatComponent : public Component {
    AttackData meleeAttack;
    AttackData rangedAttack;
    AttackData dashAttack;
    AttackData chargedAttack;

    bool isAttacking = false;
    float attackTimer = 0;
    float cooldownTimer = 0;
    Vec2 attackDirection{1, 0};
    AttackType currentAttack = AttackType::Melee;

    // Combo system
    int comboCount = 0;
    float comboTimer = 0;
    float comboWindow = 0.5f;

    // Parry system
    bool isParrying = false;
    float parryTimer = 0;
    float parryWindow = 0.15f;
    float parryCooldown = 0;
    float parrySuccessTimer = 0; // >0 means next hit is guaranteed crit

    // Charged attack
    bool isCharging = false;
    float chargeTimer = 0;
    float maxChargeTime = 1.5f;
    float minChargeTime = 0.3f;

    void startParry() {
        if (parryCooldown > 0) return;
        isParrying = true;
        parryTimer = parryWindow;
        parryCooldown = 0.5f;
    }

    void onParrySuccess() {
        parrySuccessTimer = 3.0f; // 3 seconds to land guaranteed crit
    }

    void startCharging() {
        isCharging = true;
        chargeTimer = 0;
    }

    float getChargePercent() const {
        if (!isCharging) return 0;
        return std::min(chargeTimer / maxChargeTime, 1.0f);
    }

    void releaseCharged(Vec2 direction) {
        if (chargeTimer < minChargeTime) {
            isCharging = false;
            return;
        }
        isCharging = false;
        currentAttack = AttackType::Charged;
        isAttacking = true;
        attackDirection = direction;
        attackTimer = chargedAttack.duration;
        cooldownTimer = chargedAttack.cooldown;
    }

    bool canAttack() const { return cooldownTimer <= 0 && !isAttacking; }

    void startAttack(AttackType type, Vec2 direction) {
        if (type != AttackType::Dash && !canAttack()) return;
        currentAttack = type;
        isAttacking = true;
        attackDirection = direction;
        auto& atk = getCurrentAttackData();
        attackTimer = atk.duration;
        if (type != AttackType::Dash) {
            cooldownTimer = atk.cooldown;
        }

        if (type == AttackType::Melee) {
            if (comboTimer > 0) comboCount++;
            else comboCount = 1;
            comboTimer = comboWindow;
        }
    }

    AttackData& getCurrentAttackData() {
        switch (currentAttack) {
            case AttackType::Dash: return dashAttack;
            case AttackType::Ranged: return rangedAttack;
            case AttackType::Charged: return chargedAttack;
            default: return meleeAttack;
        }
    }

    void update(float dt) override {
        if (attackTimer > 0) {
            attackTimer -= dt;
            if (attackTimer <= 0) isAttacking = false;
        }
        if (cooldownTimer > 0) cooldownTimer -= dt;
        if (comboTimer > 0) {
            comboTimer -= dt;
            if (comboTimer <= 0) comboCount = 0;
        }
        // Parry timers
        if (parryTimer > 0) {
            parryTimer -= dt;
            if (parryTimer <= 0) isParrying = false;
        }
        if (parryCooldown > 0) parryCooldown -= dt;
        if (parrySuccessTimer > 0) parrySuccessTimer -= dt;
        // Charge timer
        if (isCharging) chargeTimer += dt;
    }
};
