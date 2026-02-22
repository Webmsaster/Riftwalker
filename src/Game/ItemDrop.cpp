#include "ItemDrop.h"
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

    // Auto-destroy after 10 seconds
    auto& hp = e.addComponent<HealthComponent>();
    hp.maxHP = 10.0f;
    hp.currentHP = 10.0f;

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

void ItemDrop::spawnRandomDrop(EntityManager& entities, Vec2 pos, int dimension, int difficulty) {
    int roll = std::rand() % 100;
    if (roll < 25) {
        // 25% chance: health orb
        spawnHealthOrb(entities, pos, dimension);
    } else if (roll < 60) {
        // 35% chance: rift shard
        spawnRiftShard(entities, pos, dimension, 3 + difficulty * 2);
    }
    // 40% chance: nothing
}
