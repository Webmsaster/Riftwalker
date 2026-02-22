#pragma once
#include "ECS/Component.h"
#include "Core/Camera.h"

enum class AttackType {
    Melee,
    Ranged,
    Dash
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

    bool isAttacking = false;
    float attackTimer = 0;
    float cooldownTimer = 0;
    Vec2 attackDirection{1, 0};
    AttackType currentAttack = AttackType::Melee;

    // Combo system
    int comboCount = 0;
    float comboTimer = 0;
    float comboWindow = 0.5f;

    bool canAttack() const { return cooldownTimer <= 0 && !isAttacking; }

    void startAttack(AttackType type, Vec2 direction) {
        if (!canAttack()) return;
        currentAttack = type;
        isAttacking = true;
        attackDirection = direction;
        auto& atk = (type == AttackType::Melee) ? meleeAttack : rangedAttack;
        attackTimer = atk.duration;
        cooldownTimer = atk.cooldown;

        if (comboTimer > 0) comboCount++;
        else comboCount = 1;
        comboTimer = comboWindow;
    }

    AttackData& getCurrentAttackData() {
        return (currentAttack == AttackType::Melee) ? meleeAttack : rangedAttack;
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
    }
};
