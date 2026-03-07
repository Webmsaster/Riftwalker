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
            // BALANCE: Health orb heal 20 -> 30 (25% of base HP, meaningful recovery)
            hp.heal(30.0f);
            AudioManager::instance().play(SFX::Pickup);
            self->destroy();
        }
    };

    return e;
}

Entity& ItemDrop::spawnRiftShard(EntityManager& entities, Vec2 pos, int dimension, int value, Player* player) {
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
    col.onTrigger = [value, player](Entity* self, Entity* other) {
        if (other->getTag() == "player") {
            // Add shards to player's pending counter (consumed by PlayState each frame)
            if (player) {
                player->riftShardsCollected += value;
            }
            AudioManager::instance().play(SFX::Pickup);
            self->destroy();
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
    // BALANCE: Health orb rate 20% -> 25%, shard rate 25% -> 30%, shard value 3+diff*2 -> 5+diff*3
    // Goal: ~1 upgrade purchasable per level
    if (roll < 25) {
        // 25% chance: health orb (was 20%)
        spawnHealthOrb(entities, pos, dimension);
    } else if (roll < 55) {
        // 30% chance: rift shard (was 25%), higher value
        spawnRiftShard(entities, pos, dimension, 5 + difficulty * 3, player);
    } else if (roll < 65) {
        // 10% chance: shield
        if (player) spawnShieldOrb(entities, pos, dimension, player);
    } else if (roll < 73) {
        // 8% chance: speed boost
        if (player) spawnSpeedBoost(entities, pos, dimension, player);
    } else if (roll < 80) {
        // 7% chance: damage boost
        if (player) spawnDamageBoost(entities, pos, dimension, player);
    }
    // BALANCE: Nothing chance reduced 30% -> 20% (more rewarding combat)
}
