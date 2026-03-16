#pragma once
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/AIComponent.h"
#include "Core/ResourceManager.h"
#include <string>

// Animation name constants
namespace Anim {
    constexpr const char* Idle      = "idle";
    constexpr const char* Run       = "run";
    constexpr const char* Jump      = "jump";
    constexpr const char* Fall      = "fall";
    constexpr const char* Dash      = "dash";
    constexpr const char* WallSlide = "wallslide";
    constexpr const char* Attack    = "attack";
    constexpr const char* Hurt      = "hurt";
    constexpr const char* Dead      = "dead";
    constexpr const char* Walk      = "walk";
    constexpr const char* Move      = "move";
    constexpr const char* Attack1   = "attack1";
    constexpr const char* Attack2   = "attack2";
    constexpr const char* Attack3   = "attack3";
    constexpr const char* PhaseTransition = "phase_transition";
    constexpr const char* Enrage    = "enrage";
}

namespace SpriteConfig {
    // Setup player sprite and animations. Returns true if texture loaded.
    bool setupPlayer(AnimationComponent& anim, SpriteComponent& sprite);

    // Setup enemy sprite and animations by type. Returns true if texture loaded.
    bool setupEnemy(AnimationComponent& anim, SpriteComponent& sprite, EnemyType type);

    // Setup boss sprite and animations by boss type. Returns true if texture loaded.
    bool setupBoss(AnimationComponent& anim, SpriteComponent& sprite, int bossType);

    // Map AnimState enum to animation name string
    const char* animStateToName(AnimState state);

    // Placeholder directory (relative to working directory / build output)
    constexpr const char* PLACEHOLDER_DIR = "assets/textures/placeholders";

    // Apply a placeholder texture to sprite if no real texture is loaded.
    // Uses a single-frame static texture; no AnimationComponent update needed.
    // Returns true if a placeholder was applied.
    bool applyPlayerPlaceholder(SpriteComponent& sprite);
    bool applyEnemyPlaceholder(SpriteComponent& sprite, EnemyType type);
    bool applyBossPlaceholder(SpriteComponent& sprite, int bossType);

    // Tile placeholder texture path (single 16x16 tile for solid tiles)
    const char* getTilePlaceholderPath();
}
