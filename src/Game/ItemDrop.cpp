#include "ItemDrop.h"
#include "Player.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/HealthComponent.h"
#include "Core/AudioManager.h"
#include <cstdlib>

Entity& ItemDrop::spawnHealthOrb(EntityManager& entities, Vec2 pos, int dimension) {
    auto& e = entities.addEntity("pickup_health");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 14, 14);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(80, 230, 80);
    sprite.renderLayer = 3;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 600.0f;
    phys.velocity.y = -200.0f;
    phys.velocity.x = (std::rand() % 200) - 100.0f;
    phys.friction = 800.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 14;
    col.height = 14;
    col.layer = LAYER_PICKUP;
    col.mask = LAYER_TILE | LAYER_PLAYER;
    col.type = ColliderType::Trigger;
    col.onTrigger = [](Entity* self, Entity* other) {
        if (other->getTag() == "player" && other->hasComponent<HealthComponent>()) {
            auto& hp = other->getComponent<HealthComponent>();
            hp.currentHP = std::min(hp.currentHP + 20.0f, hp.maxHP);
            AudioManager::instance().play(SFX::Pickup);
            self->destroy();
        }
    };

    return e;
}

Entity& ItemDrop::spawnRiftShard(EntityManager& entities, Vec2 pos, int dimension, int value) {
    auto& e = entities.addEntity("pickup_shard");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 12, 12);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(180, 130, 255);
    sprite.renderLayer = 3;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 600.0f;
    phys.velocity.y = -180.0f;
    phys.velocity.x = (std::rand() % 160) - 80.0f;
    phys.friction = 800.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 12;
    col.height = 12;
    col.layer = LAYER_PICKUP;
    col.mask = LAYER_TILE | LAYER_PLAYER;
    col.type = ColliderType::Trigger;
    col.onTrigger = [value](Entity* self, Entity* other) {
        if (other->getTag() == "player") {
            AudioManager::instance().play(SFX::Pickup);
            self->destroy();
            // Shards collected via PlayState counting
        }
    };

    return e;
}

Entity& ItemDrop::spawnShieldOrb(EntityManager& entities, Vec2 pos, int dimension, Player* player) {
    auto& e = entities.addEntity("pickup_shield");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 14, 14);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(100, 180, 255); // Blue shield
    sprite.renderLayer = 3;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 600.0f;
    phys.velocity.y = -200.0f;
    phys.velocity.x = (std::rand() % 200) - 100.0f;
    phys.friction = 800.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 14;
    col.height = 14;
    col.layer = LAYER_PICKUP;
    col.mask = LAYER_TILE | LAYER_PLAYER;
    col.type = ColliderType::Trigger;
    col.onTrigger = [player](Entity* self, Entity* other) {
        if (other->getTag() == "player" && player) {
            player->hasShield = true;
            player->shieldTimer = 8.0f;
            AudioManager::instance().play(SFX::Pickup);
            self->destroy();
        }
    };

    return e;
}

Entity& ItemDrop::spawnSpeedBoost(EntityManager& entities, Vec2 pos, int dimension, Player* player) {
    auto& e = entities.addEntity("pickup_speed");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 12, 12);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(255, 255, 80); // Yellow speed
    sprite.renderLayer = 3;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 600.0f;
    phys.velocity.y = -200.0f;
    phys.velocity.x = (std::rand() % 200) - 100.0f;
    phys.friction = 800.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 12;
    col.height = 12;
    col.layer = LAYER_PICKUP;
    col.mask = LAYER_TILE | LAYER_PLAYER;
    col.type = ColliderType::Trigger;
    col.onTrigger = [player](Entity* self, Entity* other) {
        if (other->getTag() == "player" && player) {
            player->speedBoostTimer = 6.0f;
            AudioManager::instance().play(SFX::Pickup);
            self->destroy();
        }
    };

    return e;
}

Entity& ItemDrop::spawnDamageBoost(EntityManager& entities, Vec2 pos, int dimension, Player* player) {
    auto& e = entities.addEntity("pickup_damage");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 12, 12);
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(255, 80, 80); // Red damage
    sprite.renderLayer = 3;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 600.0f;
    phys.velocity.y = -200.0f;
    phys.velocity.x = (std::rand() % 200) - 100.0f;
    phys.friction = 800.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 12;
    col.height = 12;
    col.layer = LAYER_PICKUP;
    col.mask = LAYER_TILE | LAYER_PLAYER;
    col.type = ColliderType::Trigger;
    col.onTrigger = [player](Entity* self, Entity* other) {
        if (other->getTag() == "player" && player) {
            player->damageBoostTimer = 8.0f;
            AudioManager::instance().play(SFX::Pickup);
            self->destroy();
        }
    };

    return e;
}

void ItemDrop::spawnRandomDrop(EntityManager& entities, Vec2 pos, int dimension, int difficulty, Player* player) {
    int roll = std::rand() % 100;
    if (roll < 20) {
        // 20% chance: health orb
        spawnHealthOrb(entities, pos, dimension);
    } else if (roll < 45) {
        // 25% chance: rift shard
        spawnRiftShard(entities, pos, dimension, 3 + difficulty * 2);
    } else if (roll < 55) {
        // 10% chance: shield
        if (player) spawnShieldOrb(entities, pos, dimension, player);
    } else if (roll < 63) {
        // 8% chance: speed boost
        if (player) spawnSpeedBoost(entities, pos, dimension, player);
    } else if (roll < 70) {
        // 7% chance: damage boost
        if (player) spawnDamageBoost(entities, pos, dimension, player);
    }
    // 30% chance: nothing
}
