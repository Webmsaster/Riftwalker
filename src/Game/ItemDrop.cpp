#include "ItemDrop.h"
#include "Player.h"
#include "WeaponSystem.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/ColliderComponent.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/RelicComponent.h"
#include "Game/RelicSystem.h"
#include "Core/AudioManager.h"
#include "Core/ResourceManager.h"
#include "Systems/ParticleSystem.h"
#include <cstdlib>
#include <algorithm>

// Pickup spritesheet: assets/textures/pickups/pickups.png
// 4 columns x 5 rows of 16x16 icons
// Row mapping: 0=red/health, 1=blue/shield, 2=purple/shard, 3=yellow/speed+green, 4=misc
static void applyPickupSprite(SpriteComponent& sprite, int col, int row) {
    auto* tex = ResourceManager::instance().getTexture("assets/textures/pickups/pickups.png");
    if (!tex) return;
    sprite.texture = tex;
    sprite.srcRect = {col * 64, row * 64, 64, 64};  // 4x upscaled from 16x16
}

Entity& ItemDrop::spawnHealthOrb(EntityManager& entities, Vec2 pos, int dimension, Player* player) {
    auto& e = entities.addEntity("pickup_health");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 14, 14);
    (void)t;
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(80, 230, 80);
    sprite.renderLayer = 3;
    applyPickupSprite(sprite, 0, 4); // Green gem (row 4, col 0)

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
        if (other->isPlayer && other->hasComponent<HealthComponent>()) {
            // VampiricEdge: block health orb pickup healing
            if (player && other->hasComponent<RelicComponent>()) {
                auto& rc = other->getComponent<RelicComponent>();
                if (RelicSystem::hasVampiricEdge(rc)) {
                    // Destroy orb without healing (still consumed, just wasted)
                    AudioManager::instance().play(SFX::RiftFail);
                    if (player->particles && self->hasComponent<TransformComponent>()) {
                        auto& st = self->getComponent<TransformComponent>();
                        player->particles->burst(st.getCenter(), 6, {100, 30, 30, 255}, 60.0f);
                    }
                    self->destroy();
                    return;
                }
            }
            auto& hp = other->getComponent<HealthComponent>();
            // BALANCE: Health orb heal 20 -> 30 (25% of base HP, meaningful recovery)
            hp.heal(30.0f);
            AudioManager::instance().play(SFX::Pickup);
            // Collection particle burst (green)
            if (player && player->particles && self->hasComponent<TransformComponent>()) {
                auto& st = self->getComponent<TransformComponent>();
                player->particles->burst(st.getCenter(), 8, {80, 230, 80, 255}, 120.0f);
            }
            self->destroy();
        }
    };

    return e;
}

Entity& ItemDrop::spawnRiftShard(EntityManager& entities, Vec2 pos, int dimension, int value, Player* player) {
    auto& e = entities.addEntity("pickup_shard");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 12, 12);
    (void)t;
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(180, 130, 255);
    sprite.renderLayer = 3;
    applyPickupSprite(sprite, 0, 2); // Purple gem (row 2, col 0)

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
        if (other->isPlayer) {
            // Add shards to player's pending counter (consumed by PlayState each frame)
            if (player) {
                player->riftShardsCollected += value;
                // Collection particle burst (purple/gold)
                if (player->particles && self->hasComponent<TransformComponent>()) {
                    auto& st = self->getComponent<TransformComponent>();
                    player->particles->burst(st.getCenter(), 6, {180, 130, 255, 255}, 100.0f);
                    player->particles->burst(st.getCenter(), 4, {255, 220, 80, 255}, 80.0f);
                }
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
    (void)t;
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(100, 180, 255); // Blue shield
    sprite.renderLayer = 3;
    applyPickupSprite(sprite, 0, 1); // Blue gem (row 1, col 0)

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
        if (other->isPlayer && player) {
            player->hasShield = true;
            player->shieldTimer = 8.0f;
            player->pickupShieldPending = true;
            AudioManager::instance().play(SFX::Pickup);
            if (player->particles && self->hasComponent<TransformComponent>()) {
                auto& st = self->getComponent<TransformComponent>();
                player->particles->burst(st.getCenter(), 10, {100, 180, 255, 255}, 130.0f);
            }
            self->destroy();
        }
    };

    return e;
}

Entity& ItemDrop::spawnSpeedBoost(EntityManager& entities, Vec2 pos, int dimension, Player* player) {
    auto& e = entities.addEntity("pickup_speed");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 12, 12);
    (void)t;
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(255, 255, 80); // Yellow speed
    sprite.renderLayer = 3;
    applyPickupSprite(sprite, 1, 3); // Yellow gem (row 3, col 1)

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
        if (other->isPlayer && player) {
            player->speedBoostTimer = 6.0f;
            player->pickupSpeedPending = true;
            AudioManager::instance().play(SFX::Pickup);
            if (player->particles && self->hasComponent<TransformComponent>()) {
                auto& st = self->getComponent<TransformComponent>();
                player->particles->burst(st.getCenter(), 8, {255, 255, 80, 255}, 110.0f);
            }
            self->destroy();
        }
    };

    return e;
}

Entity& ItemDrop::spawnDamageBoost(EntityManager& entities, Vec2 pos, int dimension, Player* player) {
    auto& e = entities.addEntity("pickup_damage");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 12, 12);
    (void)t;
    auto& sprite = e.addComponent<SpriteComponent>();
    sprite.setColor(255, 80, 80); // Red damage
    sprite.renderLayer = 3;
    applyPickupSprite(sprite, 0, 0); // Red gem (row 0, col 0)

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
        if (other->isPlayer && player) {
            // max() preserves longer timers (e.g. GlassCannon challenge sets 99999s permanent boost)
            player->damageBoostTimer = std::max(player->damageBoostTimer, 8.0f);
            player->damageBoostMultiplier = std::max(player->damageBoostMultiplier, 1.5f);
            player->pickupDamagePending = true;
            AudioManager::instance().play(SFX::Pickup);
            if (player->particles && self->hasComponent<TransformComponent>()) {
                auto& st = self->getComponent<TransformComponent>();
                player->particles->burst(st.getCenter(), 8, {255, 80, 80, 255}, 110.0f);
            }
            self->destroy();
        }
    };

    return e;
}

void ItemDrop::spawnRandomDrop(EntityManager& entities, Vec2 pos, int dimension, int difficulty, Player* player) {
    int roll = std::rand() % 100;
    // BALANCE: Health orb rate 20% -> 25%, shard rate 25% -> 30%, shard value 3+diff*2 -> 5+diff*3
    // Goal: ~1 upgrade purchasable per level

    // SoulLeech: 2x shard value from all drops
    float shardMult = 1.0f;
    if (player && player->getEntity() && player->getEntity()->hasComponent<RelicComponent>()) {
        shardMult = RelicSystem::getSoulLeechShardMult(
            player->getEntity()->getComponent<RelicComponent>());
    }

    if (roll < 25) {
        // 25% chance: health orb (was 20%)
        spawnHealthOrb(entities, pos, dimension, player);
    } else if (roll < 55) {
        // 30% chance: rift shard (was 25%), higher value; SoulLeech doubles value
        int shardValue = static_cast<int>((5 + difficulty * 3) * shardMult);
        spawnRiftShard(entities, pos, dimension, shardValue, player);
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

Entity& ItemDrop::spawnWeaponDrop(EntityManager& entities, Vec2 pos, int dimension, WeaponID weapon, Player* player) {
    auto& e = entities.addEntity("pickup_weapon");
    e.dimension = dimension;

    auto& t = e.addComponent<TransformComponent>(pos.x, pos.y, 18, 18);
    (void)t;
    auto& sprite = e.addComponent<SpriteComponent>();
    // Color by weapon type: melee=orange, ranged=cyan
    bool melee = WeaponSystem::isMelee(weapon);
    if (melee) {
        sprite.setColor(255, 180, 60);
        applyPickupSprite(sprite, 2, 3); // Orange gem (row 3, col 2)
    } else {
        sprite.setColor(60, 200, 255);
        applyPickupSprite(sprite, 2, 4); // Cyan gem (row 4, col 2)
    }
    sprite.renderLayer = 3;

    auto& phys = e.addComponent<PhysicsBody>();
    phys.gravity = 600.0f;
    phys.velocity.y = -280.0f;
    phys.velocity.x = (std::rand() % 120) - 60.0f;
    phys.friction = 800.0f;

    auto& col = e.addComponent<ColliderComponent>();
    col.width = 18;
    col.height = 18;
    col.layer = LAYER_PICKUP;
    col.mask = LAYER_TILE | LAYER_PLAYER;
    col.type = ColliderType::Trigger;
    col.onTrigger = [weapon, player, melee](Entity* self, Entity* other) {
        if (other->isPlayer && other->hasComponent<CombatComponent>() && player) {
            auto& combat = other->getComponent<CombatComponent>();
            if (WeaponSystem::isMelee(weapon))
                combat.currentMelee = weapon;
            else
                combat.currentRanged = weapon;
            player->applyWeaponStats();
            player->weaponPickupPending = static_cast<int>(weapon);
            AudioManager::instance().play(SFX::ShrineBlessing);
            // Particle burst on weapon pickup (orange=melee, cyan=ranged)
            if (player->particles && other->hasComponent<TransformComponent>()) {
                auto& st = other->getComponent<TransformComponent>();
                SDL_Color c = melee ? SDL_Color{255, 180, 60, 255} : SDL_Color{60, 200, 255, 255};
                player->particles->burst(st.getCenter(), 10, c, 130.0f);
            }
            self->destroy();
        }
    };

    return e;
}
