// EnemyBoss.cpp -- Boss + elite modifier factories split from Enemy.cpp
#include "Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/AnimationComponent.h"
#include "Game/SpriteConfig.h"

Entity& Enemy::createVoidWyrm(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");
    e.dimension = dimension;

    // Wyrm is 48x48, more agile than Rift Guardian
    e.addComponent<TransformComponent>(pos.x, pos.y, 48, 48);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(40, 180, 120); // Deep teal-green
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = false; // Flying boss
    phys.airResistance = 300.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 40;
    col.height = 44;
    col.offset = {4, 4};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Void Wyrm HP 180+diff*70 -> 150+diff*40 (shorter fights)
    hp.maxHP = 150.0f + difficulty * 40.0f;
    hp.currentHP = hp.maxHP;
    // BALANCE: Wyrm armor base 0.2 -> 0.15, scale 0.1 -> 0.05 (less damage reduction)
    hp.armor = std::min(0.15f + difficulty * 0.05f, 0.6f);

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Wyrm melee 20+diff*4 -> 18+diff*3
    combat.meleeAttack.damage = 18.0f + difficulty * 3.0f;
    combat.meleeAttack.knockback = 350.0f;
    combat.meleeAttack.cooldown = 1.0f;
    combat.rangedAttack.damage = 12.0f + difficulty * 3.0f;
    combat.rangedAttack.range = 350.0f;
    combat.rangedAttack.knockback = 120.0f;
    combat.rangedAttack.cooldown = 1.5f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Boss;
    ai.bossType = 1; // Void Wyrm
    ai.detectRange = 500.0f;
    ai.attackRange = 40.0f;
    ai.chaseSpeed = 130.0f + difficulty * 15.0f;
    ai.patrolSpeed = 80.0f;
    ai.bossPhase = 1;
    ai.bossAttackTimer = 0;
    ai.bossAttackPattern = 0;
    ai.wyrmOrbitRadius = 140.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 200.0f, pos.y - 100.0f};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupBoss(anim, sprite, 1);

    return e;
}

Entity& Enemy::createBoss(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");
    e.dimension = dimension;

    // Boss is large: 64x64
    e.addComponent<TransformComponent>(pos.x, pos.y, 64, 64);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(200, 40, 180); // Dark magenta
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 500.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 56;
    col.height = 60;
    col.offset = {4, 4};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Rift Guardian HP 200+diff*80 -> 180+diff*50 (first boss, must be beatable)
    hp.maxHP = 180.0f + difficulty * 50.0f;
    hp.currentHP = hp.maxHP;
    // BALANCE: Guardian armor base 0.3 -> 0.15, scale 0.1 -> 0.05 (player deals meaningful damage)
    hp.armor = std::min(0.15f + difficulty * 0.05f, 0.6f);

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Guardian melee 25+diff*5 -> 20+diff*3 (25% of player HP, not 40%)
    combat.meleeAttack.damage = 20.0f + difficulty * 3.0f;
    combat.meleeAttack.knockback = 400.0f;
    combat.meleeAttack.cooldown = 1.2f;
    // BALANCE: Guardian ranged 15+diff*3 -> 12+diff*2
    combat.rangedAttack.damage = 12.0f + difficulty * 2.0f;
    combat.rangedAttack.range = 400.0f;
    combat.rangedAttack.knockback = 150.0f;
    combat.rangedAttack.cooldown = 2.0f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Boss;
    ai.detectRange = 500.0f; // Always aware
    ai.attackRange = 60.0f;
    ai.chaseSpeed = 100.0f + difficulty * 10.0f;
    ai.patrolSpeed = 50.0f;
    ai.bossPhase = 1;
    ai.bossAttackTimer = 0;
    ai.bossAttackPattern = 0;
    ai.bossLeapTimer = 0;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 200.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupBoss(anim, sprite, 0);

    return e;
}

void Enemy::makeElite(Entity& e, EliteModifier mod) {
    auto& ai = e.getComponent<AIComponent>();
    ai.isElite = true;
    ai.eliteMod = mod;
    ai.eliteGlowTimer = 0;

    // BALANCE: Elite HP multiplier 1.5 -> 1.3, DMG 1.25 -> 1.15 (noticeable but not overwhelming)
    auto& hp = e.getComponent<HealthComponent>();
    hp.maxHP *= 1.3f;
    hp.currentHP = hp.maxHP;

    auto& combat = e.getComponent<CombatComponent>();
    combat.meleeAttack.damage *= 1.15f;
    combat.rangedAttack.damage *= 1.15f;

    auto& sprite = e.getComponent<SpriteComponent>();

    switch (mod) {
        case EliteModifier::Berserker:
            combat.meleeAttack.damage *= 1.5f;
            combat.rangedAttack.damage *= 1.5f;
            ai.chaseSpeed *= 1.3f;
            ai.patrolSpeed *= 1.3f;
            sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 120));
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.3f);
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.3f);
            break;
        case EliteModifier::Shielded:
            ai.eliteShieldHP = 30.0f;
            ai.eliteShieldRegenTimer = 5.0f;
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.4f);
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.6f);
            sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b + 120));
            break;
        case EliteModifier::Teleporter:
            ai.eliteTeleportTimer = 3.0f;
            sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 60));
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.4f);
            sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b + 100));
            break;
        case EliteModifier::Splitter:
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.5f);
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 100));
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.5f);
            break;
        case EliteModifier::Vampiric:
            sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 80));
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.2f);
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.3f);
            break;
        case EliteModifier::Explosive:
            sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 100));
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 60));
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.2f);
            break;
        case EliteModifier::FireAura:
            sprite.color.r = 255;
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g / 2 + 60));
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.2f);
            break;
        case EliteModifier::HealAura:
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.3f);
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 130));
            sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b / 2 + 40));
            break;
        case EliteModifier::ShieldAura:
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.3f);
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 80));
            sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b + 150));
            break;
        case EliteModifier::Phasing:
            sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 80));
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 80));
            sprite.color.b = 255;
            break;
        case EliteModifier::Regenerating:
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.4f);
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 100));
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.5f);
            break;
        case EliteModifier::Magnetic:
            sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 100));
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.3f);
            sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b + 140));
            break;
        default: break;
    }
    sprite.baseColor = sprite.color; // update base for wind-up restore
}

Entity& Enemy::createTemporalWeaver(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");
    e.dimension = dimension;

    // Temporal Weaver: 52x52, floating clockwork entity
    e.addComponent<TransformComponent>(pos.x, pos.y, 52, 52);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(180, 160, 80); // Golden clockwork
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = false; // Floating boss
    phys.airResistance = 250.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 44;
    col.height = 44;
    col.offset = {4, 4};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Temporal Weaver HP 280+diff*90 -> 220+diff*50
    hp.maxHP = 220.0f + difficulty * 50.0f;
    hp.currentHP = hp.maxHP;
    // BALANCE: Weaver armor base 0.25 -> 0.15, scale 0.1 -> 0.05
    hp.armor = std::min(0.15f + difficulty * 0.05f, 0.6f);

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Weaver melee 20+diff*4 -> 18+diff*3
    combat.meleeAttack.damage = 18.0f + difficulty * 3.0f;
    combat.meleeAttack.knockback = 350.0f;
    combat.meleeAttack.cooldown = 1.2f;
    // BALANCE: Weaver ranged 14+diff*3 -> 12+diff*2
    combat.rangedAttack.damage = 12.0f + difficulty * 2.0f;
    combat.rangedAttack.range = 380.0f;
    combat.rangedAttack.knockback = 130.0f;
    combat.rangedAttack.cooldown = 1.8f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Boss;
    ai.bossType = 3; // Temporal Weaver
    ai.detectRange = 600.0f;
    ai.attackRange = 200.0f;
    ai.chaseSpeed = 80.0f + difficulty * 10.0f;
    ai.patrolSpeed = 50.0f;
    ai.bossPhase = 1;
    ai.bossAttackTimer = 0;
    ai.bossAttackPattern = 0;
    ai.twSlowZoneTimer = 4.0f;
    ai.twSweepTimer = 6.0f;
    ai.twRewindTimer = 12.0f;
    ai.twStopTimer = 10.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 150.0f, pos.y - 60.0f};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupBoss(anim, sprite, 3);

    return e;
}

Entity& Enemy::createDimensionalArchitect(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");
    e.dimension = dimension;

    // Architect: 56x56, floating geometric entity
    e.addComponent<TransformComponent>(pos.x, pos.y, 56, 56);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(120, 80, 200); // Deep purple
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = false; // Floating boss
    phys.airResistance = 200.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 48;
    col.height = 48;
    col.offset = {4, 4};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Dim. Architect HP 250+diff*80 -> 200+diff*45
    hp.maxHP = 200.0f + difficulty * 45.0f;
    hp.currentHP = hp.maxHP;
    // BALANCE: Architect armor base 0.3 -> 0.2, scale 0.1 -> 0.05
    hp.armor = std::min(0.2f + difficulty * 0.05f, 0.6f);

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Architect melee 18+diff*3 -> 16+diff*2
    combat.meleeAttack.damage = 16.0f + difficulty * 2.0f;
    combat.meleeAttack.knockback = 300.0f;
    combat.meleeAttack.cooldown = 1.5f;
    // BALANCE: Architect ranged 12+diff*2 -> 10+diff*2
    combat.rangedAttack.damage = 10.0f + difficulty * 2.0f;
    combat.rangedAttack.range = 400.0f;
    combat.rangedAttack.knockback = 100.0f;
    combat.rangedAttack.cooldown = 2.0f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Boss;
    ai.bossType = 2; // Dimensional Architect
    ai.detectRange = 600.0f;
    ai.attackRange = 300.0f; // Long range
    ai.chaseSpeed = 60.0f + difficulty * 8.0f; // Slow but powerful
    ai.patrolSpeed = 40.0f;
    ai.bossPhase = 1;
    ai.bossAttackTimer = 0;
    ai.bossAttackPattern = 0;
    ai.archSwapTimer = 3.0f;
    ai.archRiftTimer = 8.0f;
    ai.archConstructTimer = 5.0f;
    ai.archCollapseTimer = 15.0f;
    ai.archSwapSize = 3;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 150.0f, pos.y - 80.0f};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupBoss(anim, sprite, 2);

    return e;
}

Entity& Enemy::createVoidSovereign(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");

    e.addComponent<TransformComponent>(pos.x, pos.y, 80, 80);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(80, 0, 120); // Dark violet
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = false;
    phys.gravity = 0;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 72;
    col.height = 72;
    col.offset = {4, 4};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Void Sovereign HP 350+diff*60 -> 280+diff*35 (final boss, epic but beatable)
    hp.maxHP = 280.0f + difficulty * 35.0f;
    hp.currentHP = hp.maxHP;
    // BALANCE: Sovereign armor base 0.4 -> 0.25, scale 0.1 -> 0.05
    hp.armor = std::min(0.25f + difficulty * 0.05f, 0.6f);

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Sovereign melee 25+diff*5 -> 22+diff*3
    combat.meleeAttack.damage = 22.0f + difficulty * 3.0f;
    combat.meleeAttack.range = 80.0f;
    combat.meleeAttack.knockback = 350.0f;
    // BALANCE: Sovereign ranged 18+diff*3 -> 15+diff*2
    combat.rangedAttack.damage = 15.0f + difficulty * 2.0f;
    combat.rangedAttack.range = 500.0f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Boss;
    ai.detectRange = 600.0f;
    ai.loseRange = 800.0f;
    ai.chaseSpeed = 120.0f;
    ai.bossType = 4; // Void Sovereign = type 4
    ai.bossPhase = 1;
    ai.bossAttackTimer = 0;
    ai.bossAttackPattern = 0;
    ai.vsOrbTimer = 2.0f;
    ai.vsSlamTimer = 5.0f;
    ai.vsTeleportTimer = 8.0f;
    ai.vsDimLockTimer = 12.0f;
    ai.vsStormTimer = 20.0f;
    ai.vsLaserTimer = 15.0f;
    ai.vsAutoSwitchTimer = 3.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 200.0f, pos.y - 100.0f};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupBoss(anim, sprite, 4);

    e.dimension = dimension;
    return e;
}

Entity& Enemy::createEntropyIncarnate(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");
    e.dimension = dimension;

    // Entropy Incarnate: 64x64, dark purple void entity with glowing core
    e.addComponent<TransformComponent>(pos.x, pos.y, 64, 64);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(100, 20, 140); // Dark purple-void
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = false; // Floating boss
    phys.airResistance = 200.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 56;
    col.height = 56;
    col.offset = {4, 4};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // HP: 800 + 100*difficulty as specified
    hp.maxHP = 800.0f + difficulty * 100.0f;
    hp.currentHP = hp.maxHP;
    hp.armor = std::min(0.2f + difficulty * 0.05f, 0.6f);

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 20.0f + difficulty * 3.0f;
    combat.meleeAttack.knockback = 350.0f;
    combat.meleeAttack.cooldown = 1.2f;
    combat.rangedAttack.damage = 12.0f + difficulty * 2.0f;
    combat.rangedAttack.range = 400.0f;
    combat.rangedAttack.knockback = 120.0f;
    combat.rangedAttack.cooldown = 1.5f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Boss;
    ai.bossType = 5; // Entropy Incarnate
    ai.detectRange = 600.0f;
    ai.loseRange = 800.0f;
    ai.attackRange = 200.0f;
    ai.chaseSpeed = 90.0f + difficulty * 10.0f;
    ai.patrolSpeed = 50.0f;
    ai.bossPhase = 1;
    ai.bossAttackTimer = 0;
    ai.bossAttackPattern = 0;
    // Entropy Incarnate timers
    ai.eiPulseTimer = 4.0f;
    ai.eiTendrilTimer = 3.0f;
    ai.eiTeleportTimer = 8.0f;
    ai.eiStormTimer = 10.0f;
    ai.eiDimLockTimer = 12.0f;
    ai.eiMissileTimer = 6.0f;
    ai.eiMinionTimer = 15.0f;
    ai.eiShatterTimer = 6.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 180.0f, pos.y - 80.0f};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupBoss(anim, sprite, 5);

    return e;
}
