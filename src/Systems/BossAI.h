#pragma once
// Common includes needed by all boss AI implementations.
// Each BossXxx.cpp implements one AISystem::updateXxx() member function.
// No new classes are introduced; this header simply gathers the shared
// dependencies so individual boss files stay concise.

#include "AISystem.h"
#include "Game/AscensionSystem.h"
#include "Game/RelicSystem.h"
#include "Components/AIComponent.h"
#include "Components/TransformComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/RelicComponent.h"
#include "Systems/ParticleSystem.h"
#include "Systems/CombatSystem.h"
#include "Core/AudioManager.h"
#include "Game/Level.h"
#include "Game/Player.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Shared helper: apply damage to a player target with defensive-relic scaling.
// Bug fix 2026-04-10: boss attacks called `hp.takeDamage(X)` directly, bypassing
// getDamageTakenMult which scales damage by GlassHeart / DualityGem armor /
// FortifiedSoul / VoidCrown defensive relics. Defensive builds had zero
// protection against boss phase attacks. This helper ensures boss damage
// respects defensive relics consistently across all 6 boss files.
inline void applyBossDamageToPlayer(Entity& player, float damage, int dimension) {
    if (!player.hasComponent<HealthComponent>()) return;
    auto& hp = player.getComponent<HealthComponent>();
    if (hp.isInvincible()) return;
    float finalDmg = damage;
    if (player.hasComponent<RelicComponent>()) {
        finalDmg *= RelicSystem::getDamageTakenMult(
            player.getComponent<RelicComponent>(), dimension);
    }
    hp.takeDamage(finalDmg);
}
