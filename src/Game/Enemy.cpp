#include "Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/AnimationComponent.h"
#include "Game/SpriteConfig.h"

void Enemy::makeMiniBoss(Entity& e) {
    auto& ai = e.getComponent<AIComponent>();
    ai.isMiniBoss = true;

    // Scale up stats
    auto& hp = e.getComponent<HealthComponent>();
    // BALANCE: MiniBoss HP multiplier 3.0 -> 2.5 (tough but not a full boss fight)
    hp.maxHP *= 2.5f;
    hp.currentHP = hp.maxHP;
    // BALANCE: MiniBoss armor bonus 0.3 -> 0.2, cap 0.75 -> 0.6
    hp.armor = std::min(hp.armor + 0.2f, 0.6f);

    auto& combat = e.getComponent<CombatComponent>();
    // BALANCE: MiniBoss DMG multiplier 1.5 -> 1.3
    combat.meleeAttack.damage *= 1.3f;
    combat.meleeAttack.knockback *= 1.3f;
    combat.rangedAttack.damage *= 1.3f;

    // Scale up size
    auto& t = e.getComponent<TransformComponent>();
    float scaleX = t.width * 0.4f;
    float scaleY = t.height * 0.4f;
    t.width += static_cast<int>(scaleX);
    t.height += static_cast<int>(scaleY);

    auto& col = e.getComponent<ColliderComponent>();
    col.width += static_cast<int>(scaleX * 0.8f);
    col.height += static_cast<int>(scaleY * 0.8f);

    // Boost detection/chase
    ai.detectRange *= 1.3f;
    ai.chaseSpeed *= 1.15f;
}

void Enemy::applyElement(Entity& e, EnemyElement element) {
    if (element == EnemyElement::None) return;
    auto& ai = e.getComponent<AIComponent>();
    ai.element = element;
    auto& sprite = e.getComponent<SpriteComponent>();
    auto& hp = e.getComponent<HealthComponent>();
    auto& combat = e.getComponent<CombatComponent>();

    switch (element) {
        case EnemyElement::Fire:
            sprite.color.r = std::min(255, sprite.color.r + 80);
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.6f);
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.3f);
            combat.meleeAttack.damage *= 1.2f;
            break;
        case EnemyElement::Ice:
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.4f);
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.7f);
            sprite.color.b = std::min(255, sprite.color.b + 100);
            hp.maxHP *= 1.15f;
            hp.currentHP = hp.maxHP;
            break;
        case EnemyElement::Electric:
            sprite.color.r = std::min(255, sprite.color.r + 60);
            sprite.color.g = std::min(255, sprite.color.g + 80);
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.4f);
            combat.meleeAttack.damage *= 1.1f;
            hp.maxHP *= 1.1f;
            hp.currentHP = hp.maxHP;
            break;
        default: break;
    }
}

Entity& Enemy::createByType(EntityManager& entities, int type, Vec2 pos, int dimension) {
    Entity* e = nullptr;
    switch (static_cast<EnemyType>(type)) {
        case EnemyType::Walker:  e = &createWalker(entities, pos, dimension); break;
        case EnemyType::Flyer:   e = &createFlyer(entities, pos, dimension); break;
        case EnemyType::Turret:  e = &createTurret(entities, pos, dimension); break;
        case EnemyType::Charger: e = &createCharger(entities, pos, dimension); break;
        case EnemyType::Phaser:   e = &createPhaser(entities, pos, dimension); break;
        case EnemyType::Exploder: e = &createExploder(entities, pos, dimension); break;
        case EnemyType::Shielder: e = &createShielder(entities, pos, dimension); break;
        case EnemyType::Crawler:  e = &createCrawler(entities, pos, dimension); break;
        case EnemyType::Summoner: e = &createSummoner(entities, pos, dimension); break;
        case EnemyType::Sniper:   e = &createSniper(entities, pos, dimension); break;
        case EnemyType::Boss: e = &createBoss(entities, pos, dimension, 1); break;
        default: e = &createWalker(entities, pos, dimension); break;
    }
    // Spawn animation: brief invulnerability + flicker
    if (e && e->hasComponent<AIComponent>()) {
        auto& ai = e->getComponent<AIComponent>();
        bool isBoss = (static_cast<EnemyType>(type) == EnemyType::Boss);
        ai.spawnTimer = isBoss ? 0.8f : 0.4f;
    }
    return *e;
}

Entity& Enemy::createWalker(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_walker");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 28, 32);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(200, 60, 60);
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 800.0f;
    phys.bounciness = 0.3f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 24;
    col.height = 30;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Walker HP 45 -> 40 (2 melee hits to kill)
    hp.maxHP = 40.0f;
    hp.currentHP = 40.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Walker DMG 12 -> 10 (less punishing per hit)
    combat.meleeAttack.damage = 10.0f;
    combat.meleeAttack.knockback = 200.0f;
    combat.meleeAttack.cooldown = 1.0f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Walker;
    ai.detectRange = 180.0f;
    ai.attackRange = 40.0f;
    ai.patrolSpeed = 60.0f;
    // BALANCE: Walker chase speed 100 -> 90 (easier to kite)
    ai.chaseSpeed = 90.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 150.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Walker);

    return e;
}

Entity& Enemy::createFlyer(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_flyer");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 24, 24);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(180, 80, 200);
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = false;
    phys.airResistance = 400.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 20;
    col.height = 20;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Flyer HP 30 -> 25 (fragile but evasive)
    hp.maxHP = 25.0f;
    hp.currentHP = 25.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Flyer DMG 12 -> 10
    combat.meleeAttack.damage = 10.0f;
    combat.meleeAttack.knockback = 150.0f;
    combat.meleeAttack.cooldown = 1.5f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Flyer;
    ai.detectRange = 250.0f;
    ai.attackRange = 30.0f;
    ai.flyHeight = 80.0f;
    ai.swoopSpeed = 250.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 100.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Flyer);

    return e;
}

Entity& Enemy::createTurret(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_turret");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 28, 28);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(200, 200, 60);
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = true;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 28;
    col.height = 28;
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Turret HP 50 -> 40
    hp.maxHP = 40.0f;
    hp.currentHP = 40.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Turret DMG 10 -> 8
    combat.rangedAttack.damage = 8.0f;
    combat.rangedAttack.range = 300.0f;
    combat.rangedAttack.knockback = 80.0f;
    combat.rangedAttack.cooldown = 1.8f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Turret;
    ai.detectRange = 300.0f;
    ai.attackRange = 280.0f;

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Turret);

    return e;
}

Entity& Enemy::createCharger(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_charger");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 36, 28);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(220, 120, 40);
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 600.0f;
    phys.bounciness = 0.35f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 32;
    col.height = 26;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Charger HP 55 -> 45
    hp.maxHP = 45.0f;
    hp.currentHP = 45.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Charger DMG 22 -> 18 (still hurts but survivable)
    combat.meleeAttack.damage = 18.0f;
    combat.meleeAttack.knockback = 350.0f;
    combat.meleeAttack.cooldown = 2.0f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Charger;
    ai.detectRange = 200.0f;
    ai.attackRange = 150.0f;
    ai.chargeSpeed = 350.0f;
    ai.chargeTime = 0.8f;
    ai.chargeWindup = 0.5f;
    ai.patrolSpeed = 50.0f;

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Charger);

    return e;
}

Entity& Enemy::createPhaser(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_phaser");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 26, 40);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(100, 50, 200);
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 800.0f;
    phys.bounciness = 0.3f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 22;
    col.height = 38;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Phaser HP 35 -> 30
    hp.maxHP = 30.0f;
    hp.currentHP = 30.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Phaser DMG 18 -> 14
    combat.meleeAttack.damage = 14.0f;
    combat.meleeAttack.knockback = 200.0f;
    combat.meleeAttack.cooldown = 1.2f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Phaser;
    ai.detectRange = 220.0f;
    ai.attackRange = 45.0f;
    ai.phaseCooldown = 3.0f;
    ai.chaseSpeed = 110.0f;
    ai.patrolSpeed = 70.0f;

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Phaser);

    return e;
}

Entity& Enemy::createExploder(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_exploder");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 22, 22);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(255, 80, 30); // Bright orange-red
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 600.0f;
    phys.bounciness = 0.25f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 20;
    col.height = 20;
    col.offset = {1, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Exploder HP 25 -> 20 (one melee hit kills)
    hp.maxHP = 20.0f;
    hp.currentHP = 20.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Exploder DMG 40 -> 30 (punishing but not half player HP)
    combat.meleeAttack.damage = 30.0f;
    combat.meleeAttack.knockback = 400.0f;
    combat.meleeAttack.cooldown = 10.0f; // Only explodes once

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Exploder;
    ai.detectRange = 200.0f;
    ai.attackRange = 25.0f;
    ai.chaseSpeed = 180.0f; // Fast runner
    ai.patrolSpeed = 40.0f;
    ai.explodeRadius = 80.0f;
    ai.explodeDamage = 30.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 80.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Exploder);

    return e;
}

Entity& Enemy::createShielder(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_shielder");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 32, 36);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(80, 180, 220); // Blue-cyan
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 900.0f;
    phys.bounciness = 0.2f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 28;
    col.height = 34;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Shielder HP 80 -> 65 (still tanky but not bullet-sponge)
    hp.maxHP = 65.0f;
    hp.currentHP = 65.0f;
    // BALANCE: Shielder armor 0.5 -> 0.35 (was taking 8 hits, now ~5)
    hp.armor = 0.35f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Shielder DMG 18 -> 14
    combat.meleeAttack.damage = 14.0f;
    combat.meleeAttack.knockback = 280.0f;
    combat.meleeAttack.cooldown = 1.5f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Shielder;
    ai.detectRange = 180.0f;
    ai.attackRange = 40.0f;
    ai.patrolSpeed = 40.0f;
    ai.chaseSpeed = 110.0f; // Tanky but can pressure the player
    ai.shieldUp = true;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 120.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Shielder);

    return e;
}

Entity& Enemy::createCrawler(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_crawler");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 26, 18);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(60, 160, 80); // Dark green
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.useGravity = false;
    phys.airResistance = 200.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 24;
    col.height = 16;
    col.offset = {1, 1};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    // BALANCE: Crawler HP 25 -> 20
    hp.maxHP = 20.0f;
    hp.currentHP = 20.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Crawler DMG 15 -> 12
    combat.meleeAttack.damage = 12.0f;
    combat.meleeAttack.knockback = 250.0f;
    combat.meleeAttack.cooldown = 1.0f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Crawler;
    ai.detectRange = 120.0f;
    ai.attackRange = 20.0f;
    ai.onCeiling = true;
    ai.dropSpeed = 400.0f;
    ai.patrolSpeed = 50.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 100.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Crawler);

    return e;
}

Entity& Enemy::createSummoner(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_summoner");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 30, 38);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(180, 50, 220); // Purple
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 800.0f;
    phys.bounciness = 0.25f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 26;
    col.height = 36;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 60.0f;
    hp.currentHP = 60.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 8.0f;
    combat.meleeAttack.knockback = 150.0f;
    combat.meleeAttack.cooldown = 2.0f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Summoner;
    ai.detectRange = 250.0f;
    ai.attackRange = 200.0f;
    ai.summonCooldown = 6.0f;
    ai.maxMinions = 3;
    ai.chaseSpeed = 60.0f;
    ai.patrolSpeed = 30.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 80.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Summoner);

    return e;
}

Entity& Enemy::createSniper(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_sniper");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 24, 34);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(200, 180, 40); // Gold-yellow
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 800.0f;
    phys.bounciness = 0.3f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 20;
    col.height = 32;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 30.0f;
    hp.currentHP = 30.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Sniper DMG 20 -> 16 (still high but survivable)
    combat.rangedAttack.damage = 16.0f;
    combat.rangedAttack.range = 400.0f;
    combat.rangedAttack.knockback = 120.0f;
    combat.rangedAttack.cooldown = 2.5f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Sniper;
    ai.detectRange = 400.0f;
    ai.attackRange = 380.0f;
    ai.sniperRange = 400.0f;
    ai.telegraphDuration = 0.8f;
    ai.retreatSpeed = 120.0f;
    ai.preferredRange = 300.0f;
    ai.patrolSpeed = 40.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 60.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Sniper);

    return e;
}

Entity& Enemy::createMinion(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("enemy_minion");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 16, 16);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(200, 100, 255); // Light purple
    sprite.renderLayer = 2;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 980.0f;
    phys.friction = 600.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 14;
    col.height = 14;
    col.offset = {1, 1};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 10.0f;
    hp.currentHP = 10.0f;

    auto& combat = e.addComponent<CombatComponent>();
    // BALANCE: Minion DMG 8 -> 6
    combat.meleeAttack.damage = 6.0f;
    combat.meleeAttack.knockback = 100.0f;
    combat.meleeAttack.cooldown = 1.5f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Walker; // Minions use walker AI
    ai.detectRange = 150.0f;
    ai.attackRange = 20.0f;
    ai.chaseSpeed = 130.0f;
    ai.patrolSpeed = 70.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 60.0f, pos.y};

    auto& anim = e.addComponent<AnimationComponent>();
    SpriteConfig::setupEnemy(anim, sprite, EnemyType::Walker); // Minions use walker sprites

    return e;
}

Entity& Enemy::createVoidWyrm(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");
    e.dimension = dimension;

    // Wyrm is 48x48, more agile than Rift Guardian
    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 48, 48);
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
    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 64, 64);
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
            sprite.color.r = std::min(255, sprite.color.r + 120);
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.3f);
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.3f);
            break;
        case EliteModifier::Shielded:
            ai.eliteShieldHP = 30.0f;
            ai.eliteShieldRegenTimer = 5.0f;
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.4f);
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.6f);
            sprite.color.b = std::min(255, sprite.color.b + 120);
            break;
        case EliteModifier::Teleporter:
            ai.eliteTeleportTimer = 3.0f;
            sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + 60));
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.4f);
            sprite.color.b = std::min(255, sprite.color.b + 100);
            break;
        case EliteModifier::Splitter:
            sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.5f);
            sprite.color.g = std::min(255, sprite.color.g + 100);
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.5f);
            break;
        case EliteModifier::Vampiric:
            sprite.color.r = std::min(255, sprite.color.r + 80);
            sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.2f);
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.3f);
            break;
        case EliteModifier::Explosive:
            sprite.color.r = std::min(255, sprite.color.r + 100);
            sprite.color.g = static_cast<Uint8>(std::min(255, sprite.color.g + 60));
            sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.2f);
            break;
        default: break;
    }
}

Entity& Enemy::createTemporalWeaver(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    auto& e = entities.addEntity("enemy_boss");
    e.dimension = dimension;

    // Temporal Weaver: 52x52, floating clockwork entity
    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 52, 52);
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
    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 56, 56);
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

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 80, 80);
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
