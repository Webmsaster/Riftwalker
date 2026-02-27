#include "Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"

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

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 24;
    col.height = 30;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 45.0f;
    hp.currentHP = 45.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 12.0f;
    combat.meleeAttack.knockback = 200.0f;
    combat.meleeAttack.cooldown = 1.0f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Walker;
    ai.detectRange = 180.0f;
    ai.attackRange = 40.0f;
    ai.patrolSpeed = 60.0f;
    ai.chaseSpeed = 100.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 150.0f, pos.y};

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
    hp.maxHP = 30.0f;
    hp.currentHP = 30.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 12.0f;
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
    hp.maxHP = 50.0f;
    hp.currentHP = 50.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.rangedAttack.damage = 10.0f;
    combat.rangedAttack.range = 300.0f;
    combat.rangedAttack.knockback = 80.0f;
    combat.rangedAttack.cooldown = 1.8f;
    combat.rangedAttack.type = AttackType::Ranged;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Turret;
    ai.detectRange = 300.0f;
    ai.attackRange = 280.0f;

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

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 32;
    col.height = 26;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 55.0f;
    hp.currentHP = 55.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 22.0f;
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

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 22;
    col.height = 38;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 35.0f;
    hp.currentHP = 35.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 18.0f;
    combat.meleeAttack.knockback = 200.0f;
    combat.meleeAttack.cooldown = 1.2f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Phaser;
    ai.detectRange = 220.0f;
    ai.attackRange = 45.0f;
    ai.phaseCooldown = 3.0f;
    ai.chaseSpeed = 110.0f;
    ai.patrolSpeed = 70.0f;

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

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 20;
    col.height = 20;
    col.offset = {1, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 15.0f; // Very fragile glass cannon
    hp.currentHP = 15.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 40.0f; // High damage on contact
    combat.meleeAttack.knockback = 400.0f;
    combat.meleeAttack.cooldown = 10.0f; // Only explodes once

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Exploder;
    ai.detectRange = 200.0f;
    ai.attackRange = 25.0f;
    ai.chaseSpeed = 180.0f; // Fast runner
    ai.patrolSpeed = 40.0f;
    ai.explodeRadius = 80.0f;
    ai.explodeDamage = 35.0f;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 80.0f, pos.y};

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

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 28;
    col.height = 34;
    col.offset = {2, 2};
    col.layer = LAYER_ENEMY;
    col.mask = LAYER_TILE | LAYER_PLAYER | LAYER_PROJECTILE;

    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 80.0f; // Very tanky
    hp.currentHP = 80.0f;
    hp.armor = 5.0f; // Damage reduction

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 18.0f;
    combat.meleeAttack.knockback = 280.0f;
    combat.meleeAttack.cooldown = 1.5f;

    auto& ai = e.addComponent<AIComponent>();
    ai.enemyType = EnemyType::Shielder;
    ai.detectRange = 180.0f;
    ai.attackRange = 40.0f;
    ai.patrolSpeed = 40.0f;
    ai.chaseSpeed = 80.0f; // Slow but tanky
    ai.shieldUp = true;
    ai.patrolStart = pos;
    ai.patrolEnd = {pos.x + 120.0f, pos.y};

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
    hp.maxHP = 25.0f;
    hp.currentHP = 25.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 15.0f;
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
    combat.rangedAttack.damage = 20.0f;
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
    combat.meleeAttack.damage = 8.0f;
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
    hp.maxHP = 180.0f + difficulty * 70.0f;
    hp.currentHP = hp.maxHP;
    hp.armor = 1.0f + difficulty * 1.5f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 20.0f + difficulty * 4.0f;
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
    hp.maxHP = 200.0f + difficulty * 80.0f;
    hp.currentHP = hp.maxHP;
    hp.armor = 2.0f + difficulty * 2.0f;

    auto& combat = e.addComponent<CombatComponent>();
    combat.meleeAttack.damage = 25.0f + difficulty * 5.0f;
    combat.meleeAttack.knockback = 400.0f;
    combat.meleeAttack.cooldown = 1.2f;
    combat.rangedAttack.damage = 15.0f + difficulty * 3.0f;
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

    return e;
}
