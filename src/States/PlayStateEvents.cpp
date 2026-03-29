// PlayStateEvents.cpp — Event/NPC/Relic/Chain/Unlock handler implementations
// Split from PlayState.cpp — all member functions unchanged, just moved.
#include "PlayState.h"
#include "Core/Game.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AbilityComponent.h"
#include "Core/AudioManager.h"
#include "Game/AchievementSystem.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/LoreSystem.h"
#include "Game/DailyRun.h"
#include "Game/Bestiary.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

void PlayState::showRelicChoice() {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    m_relicChoices = RelicSystem::generateChoice(m_currentDifficulty, relics.relics);
    if (m_relicChoices.empty()) return;
    m_showRelicChoice = true;
    m_relicChoiceSelected = 0;
}

void PlayState::showCursedRelicChoice() {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    m_relicChoices = RelicSystem::generateCursedChoice(m_currentDifficulty, relics.relics);
    if (m_relicChoices.empty()) return; // All cursed relics already owned, skip
    m_showRelicChoice = true;
    m_relicChoiceSelected = 0;
}

void PlayState::selectRelic(int index) {
    if (index < 0 || index >= static_cast<int>(m_relicChoices.size())) return;
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    RelicID choice = m_relicChoices[index];
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    relics.addRelic(choice);

    // Apply stat effects
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    RelicSystem::applyStatEffects(relics, *m_player, hp, combat);

    // RiftMantle: reduce dimension switch cooldown (clamped by safety rail)
    m_dimManager.switchCooldown = std::max(0.20f, 0.5f * RelicSystem::getSwitchCooldownMult(relics));

    // TimeDilator: -30% ability cooldowns (applied once on pickup)
    if (choice == RelicID::TimeDilator && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float cdMult = RelicSystem::getAbilityCooldownMultiplier(relics);
        abil.abilities[0].cooldown *= cdMult;
        abil.abilities[1].cooldown *= cdMult;
        abil.abilities[2].cooldown *= cdMult;
    }

    // Visual/audio feedback
    AudioManager::instance().play(SFX::RiftRepair);
    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto& data = RelicSystem::getRelicData(choice);
    m_particles.burst(pPos, 30, data.glowColor, 200.0f, 4.0f);
    m_particles.burst(pPos, 15, {255, 255, 255, 255}, 150.0f, 3.0f);

    m_showRelicChoice = false;
    m_relicChoices.clear();
}

void PlayState::checkBreakableWalls() {
    if (!m_player || !m_level) return;

    // Only break walls during dash or charged attack
    bool canBreak = m_player->isDashing || m_player->isChargingAttack;
    if (!canBreak) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    int tileSize = m_level->getTileSize();
    int currentDim = m_dimManager.getCurrentDimension();

    // Check tiles around player
    int cx = static_cast<int>(pt.getCenter().x) / tileSize;
    int cy = static_cast<int>(pt.getCenter().y) / tileSize;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int tx = cx + dx;
            int ty = cy + dy;
            if (!m_level->inBounds(tx, ty)) continue;

            auto& tile = m_level->getTile(tx, ty, currentDim);
            if (tile.type == TileType::Breakable) {
                // Destroy the breakable wall
                Tile empty;
                empty.type = TileType::Empty;
                m_level->setTile(tx, ty, currentDim, empty);
                // Also break in other dimension
                int otherDim = (currentDim == 1) ? 2 : 1;
                auto& otherTile = m_level->getTile(tx, ty, otherDim);
                if (otherTile.type == TileType::Breakable) {
                    m_level->setTile(tx, ty, otherDim, empty);
                }

                AudioManager::instance().play(SFX::BreakableWall);
                m_camera.shake(6.0f, 0.2f);

                // Debris particles
                Vec2 breakPos = {static_cast<float>(tx * tileSize + tileSize / 2),
                                 static_cast<float>(ty * tileSize + tileSize / 2)};
                m_particles.burst(breakPos, 20, tile.color, 200.0f, 3.0f);
                m_particles.burst(breakPos, 10, {200, 180, 160, 255}, 150.0f, 2.0f);
            }
        }
    }
}

void PlayState::checkSecretRoomDiscovery() {
    if (!m_player || !m_level) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    int tileSize = m_level->getTileSize();
    int px = static_cast<int>(pt.getCenter().x) / tileSize;
    int py = static_cast<int>(pt.getCenter().y) / tileSize;

    for (auto& sr : m_level->getSecretRooms()) {
        if (sr.discovered) continue;

        // Check if player is inside the secret room bounds
        if (px >= sr.tileX && px < sr.tileX + sr.width &&
            py >= sr.tileY && py < sr.tileY + sr.height) {
            sr.discovered = true;
            AudioManager::instance().play(SFX::SecretRoomDiscover);
            // Lore: The Rift - discovered via secret room
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::TheRift)) {
                    lore->discover(LoreID::TheRift);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }
            m_camera.shake(4.0f, 0.15f);

            Vec2 roomCenter = {
                static_cast<float>((sr.tileX + sr.width / 2) * tileSize),
                static_cast<float>((sr.tileY + sr.height / 2) * tileSize)
            };
            m_particles.burst(roomCenter, 30, {255, 215, 60, 255}, 200.0f, 4.0f);
            m_particles.burst(roomCenter, 15, {180, 150, 255, 255}, 150.0f, 3.0f);

            // Apply rewards based on type
            if (!sr.completed) {
                sr.completed = true;
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();

                switch (sr.type) {
                    case SecretRoomType::TreasureVault: {
                        int shards = sr.shardReward;
                        shards = static_cast<int>(shards * game->getRunBuffSystem().getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        hp.heal(static_cast<float>(sr.hpReward));
                        break;
                    }
                    case SecretRoomType::ShrineRoom: {
                        // Secret shrine: guaranteed blessing (reward for discovery)
                        AudioManager::instance().play(SFX::ShrineActivate);
                        AudioManager::instance().play(SFX::ShrineBlessing);
                        hp.maxHP += 20;
                        hp.heal(20.0f);
                        m_player->damageBoostTimer = 30.0f;
                        m_player->damageBoostMultiplier = 1.2f;
                        m_particles.burst(roomCenter, 25, {180, 120, 255, 255}, 180.0f, 3.0f);
                        m_camera.shake(0.3f, 4.0f);
                        break;
                    }
                    case SecretRoomType::DimensionCache: {
                        // Dimension Cache: shards + guaranteed cursed relic offer
                        int shards = sr.shardReward; // Normal reward (not doubled — the relic is the bonus)
                        shards = static_cast<int>(shards * game->getRunBuffSystem().getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        // Offer a cursed relic from the forbidden vault
                        showCursedRelicChoice();
                        break;
                    }
                    case SecretRoomType::AncientWeapon:
                        // Temporary damage boost
                        m_player->damageBoostTimer = 30.0f; // 30 seconds
                        m_player->damageBoostMultiplier = 2.0f;
                        break;
                    case SecretRoomType::ChallengeRoom:
                        // Reward given after clearing enemies (checked by wave system)
                        break;
                }
            }
        }
    }
}

void PlayState::updateSecretRoomHints(float dt) {
    if (!m_player || !m_level) return;

    m_secretHintTimer += dt;
    if (m_secretHintTimer < 0.4f) return; // Emit every 0.4s
    m_secretHintTimer = 0;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 pPos = pt.getCenter();
    int tileSize = m_level->getTileSize();
    float hintRadius = 6.0f * tileSize; // 6 tiles detection range

    for (const auto& sr : m_level->getSecretRooms()) {
        if (sr.discovered) continue;

        // Check if breakable wall still exists (not already broken)
        int currentDim = m_dimManager.getCurrentDimension();
        if (!m_level->inBounds(sr.entranceX, sr.entranceY)) continue;
        const auto& tile = m_level->getTile(sr.entranceX, sr.entranceY, currentDim);
        if (tile.type != TileType::Breakable) continue;

        // Entrance world position
        Vec2 entrancePos = {
            static_cast<float>(sr.entranceX * tileSize + tileSize / 2),
            static_cast<float>(sr.entranceY * tileSize + tileSize / 2)
        };

        float dist = std::sqrt((pPos.x - entrancePos.x) * (pPos.x - entrancePos.x) +
                                (pPos.y - entrancePos.y) * (pPos.y - entrancePos.y));
        if (dist > hintRadius) continue;

        // Intensity increases as player gets closer (0.0 at edge, 1.0 at wall)
        float proximity = 1.0f - (dist / hintRadius);

        // Emit subtle dust/sparkle particles from the breakable wall
        int particleCount = 1 + static_cast<int>(proximity * 3); // 1-4 particles
        SDL_Color hintColor;
        switch (sr.type) {
            case SecretRoomType::TreasureVault:
                hintColor = {255, 215, 60, static_cast<Uint8>(40 + proximity * 60)};  // Gold
                break;
            case SecretRoomType::ChallengeRoom:
                hintColor = {255, 80, 80, static_cast<Uint8>(40 + proximity * 60)};   // Red
                break;
            case SecretRoomType::ShrineRoom:
                hintColor = {180, 120, 255, static_cast<Uint8>(40 + proximity * 60)}; // Purple
                break;
            case SecretRoomType::DimensionCache:
                hintColor = {100, 200, 255, static_cast<Uint8>(40 + proximity * 60)}; // Cyan
                break;
            case SecretRoomType::AncientWeapon:
                hintColor = {255, 180, 60, static_cast<Uint8>(40 + proximity * 60)};  // Orange
                break;
        }

        // Floating motes rising from the wall
        for (int i = 0; i < particleCount; i++) {
            Vec2 spawnPos = {
                entrancePos.x + (std::rand() % 20 - 10),
                entrancePos.y + (std::rand() % 10 - 5)
            };
            m_particles.burst(spawnPos, 1, hintColor, 20.0f + proximity * 30.0f, 1.5f + proximity);
        }
    }
}

void PlayState::checkEventInteraction() {
    if (!m_player || !m_level) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 playerCenter = pt.getCenter();
    int currentDim = m_dimManager.getCurrentDimension();
    const auto& input = game->getInput();

    m_nearEventIndex = -1;

    auto& events = m_level->getRandomEvents();
    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        float dx = playerCenter.x - event.position.x;
        float dy = playerCenter.y - event.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 60.0f) {
            m_nearEventIndex = i;

            // Interact with Up/W
            if (input.isActionPressed(Action::Jump) || input.isActionPressed(Action::MoveUp)) {
                // Not jump - use a dedicated check. Just use E key via event
            }

            // Interact with Enter or E (check raw key)
            if (input.isActionPressed(Action::Interact)) {
                event.used = true;
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                auto& buffs = game->getRunBuffSystem();

                switch (event.type) {
                    case RandomEventType::Merchant:
                        AudioManager::instance().play(SFX::MerchantGreet);
                        // Buy a random available buff (cheaper)
                        {
                            auto offerings = buffs.generateShopOffering(m_currentDifficulty);
                            if (!offerings.empty()) {
                                auto& pick = offerings[0];
                                int shards = game->getUpgradeSystem().getRiftShards();
                                int discountCost = pick.cost * 3 / 4; // 25% cheaper
                                if (shards >= discountCost) {
                                    game->getUpgradeSystem().addRiftShards(-discountCost);
                                    // Bypass purchase() shard deduction - pass dummy with enough
                                    int dummy = pick.cost;
                                    buffs.purchase(pick.id, dummy);
                                    AudioManager::instance().play(SFX::Pickup);
                                    m_particles.burst(event.position, 15, {255, 215, 60, 255}, 120.0f, 2.0f);
                                } else {
                                    AudioManager::instance().play(SFX::RiftFail);
                                    event.used = false; // Can try again
                                }
                            }
                        }
                        break;

                    case RandomEventType::Shrine: {
                        AudioManager::instance().play(SFX::ShrineActivate);
                        SDL_Color sc = event.getShrineColor();
                        switch (event.shrineType) {
                            case ShrineType::Power:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                m_player->damageBoostTimer = 60.0f;
                                m_player->damageBoostMultiplier = 1.3f;
                                hp.maxHP = std::max(1.0f, hp.maxHP - 15);
                                hp.currentHP = std::min(hp.currentHP, hp.maxHP);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Vitality:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                hp.maxHP += 25;
                                hp.heal(25.0f);
                                m_entropy.addEntropy(8.0f);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Speed:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                m_player->speedBoostTimer = 45.0f;
                                m_player->speedBoostMultiplier = 1.25f;
                                hp.maxHP = std::max(1.0f, hp.maxHP - 10);
                                hp.currentHP = std::min(hp.currentHP, hp.maxHP);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Entropy:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                m_entropy.reduceEntropy(25.0f);
                                hp.currentHP = std::max(1.0f, hp.currentHP - 15);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Shards: {
                                AudioManager::instance().play(SFX::Pickup);
                                int shards = event.reward > 0 ? event.reward : 40;
                                shards = static_cast<int>(shards * buffs.getShardMultiplier());
                                shardsCollected += shards;
                                game->getUpgradeSystem().addRiftShards(shards);
                                m_entropy.addEntropy(12.0f);
                                m_particles.burst(event.position, 25, sc, 180.0f, 3.0f);
                                break;
                            }
                            case ShrineType::Renewal:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                hp.heal(hp.maxHP);
                                m_entropy.addEntropy(5.0f);
                                m_particles.burst(event.position, 25, sc, 150.0f, 3.0f);
                                break;
                            default:
                                break;
                        }
                        m_camera.shake(0.3f, 4.0f);
                        break;
                    }

                    case RandomEventType::DimensionalAnomaly:
                        AudioManager::instance().play(SFX::AnomalySpawn);
                        // Spawn bonus enemies directly (don't re-spawn all)
                        for (int e = 0; e < 3 + m_currentDifficulty; e++) {
                            float sx = event.position.x + (std::rand() % 200 - 100);
                            float sy = event.position.y - 50 - (std::rand() % 100);
                            int etype = std::rand() % std::min(10, 3 + m_currentDifficulty);
                            Enemy::createByType(m_entities, etype, {sx, sy}, currentDim);
                        }
                        m_particles.burst(event.position, 25, {180, 100, 255, 255}, 200.0f, 4.0f);
                        break;

                    case RandomEventType::RiftEcho: {
                        int shards = event.reward;
                        shards = static_cast<int>(shards * buffs.getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(event.position, 20, {200, 150, 255, 255}, 180.0f, 3.0f);
                        break;
                    }

                    case RandomEventType::SuitRepairStation:
                        hp.heal(hp.maxHP);
                        m_entropy.reduceEntropy(30.0f);
                        AudioManager::instance().play(SFX::RiftRepair);
                        m_particles.burst(event.position, 25, {100, 255, 100, 255}, 150.0f, 3.0f);
                        break;

                    case RandomEventType::GamblingRift: {
                        int cost = event.cost;
                        int shards = game->getUpgradeSystem().getRiftShards();
                        if (shards >= cost) {
                            game->getUpgradeSystem().addRiftShards(-cost);
                            // Random reward: 0x (40%), 2x (35%), 4x (25%)
                            int roll = std::rand() % 100;
                            int reward = 0;
                            if (roll < 40) {
                                reward = 0;
                                AudioManager::instance().play(SFX::RiftFail);
                                m_particles.burst(event.position, 10, {100, 100, 100, 255}, 80.0f, 2.0f);
                            } else if (roll < 75) {
                                reward = cost * 2;
                                AudioManager::instance().play(SFX::Pickup);
                                m_particles.burst(event.position, 15, {200, 170, 255, 255}, 120.0f, 3.0f);
                            } else {
                                reward = cost * 4;
                                AudioManager::instance().play(SFX::LevelComplete);
                                m_particles.burst(event.position, 30, {255, 215, 60, 255}, 200.0f, 4.0f);
                            }
                            reward = static_cast<int>(reward * buffs.getShardMultiplier());
                            shardsCollected += reward;
                            game->getUpgradeSystem().addRiftShards(reward);
                        } else {
                            AudioManager::instance().play(SFX::RiftFail);
                            event.used = false;
                        }
                        break;
                    }
                }
            }
            break; // Only interact with nearest event
        }
    }
}

void PlayState::checkNPCInteraction() {
    if (!m_player || !m_level || m_showNPCDialog) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 playerCenter = pt.getCenter();
    int currentDim = m_dimManager.getCurrentDimension();
    const auto& input = game->getInput();

    m_nearNPCIndex = -1;

    auto& npcs = m_level->getNPCs();
    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        float dx = playerCenter.x - npc.position.x;
        float dy = playerCenter.y - npc.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 60.0f) {
            m_nearNPCIndex = i;

            if (input.isActionPressed(Action::Interact)) {
                m_showNPCDialog = true;
                m_npcDialogChoice = 0;
                AudioManager::instance().play(SFX::MenuConfirm);
            }
            break;
        }
    }
}

void PlayState::handleNPCDialogChoice(int npcIndex, int choice) {
    if (!m_level || npcIndex < 0) return;
    auto& npcs = m_level->getNPCs();
    if (npcIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[npcIndex];
    int npcStage = getNPCStoryStage(npc.type);
    bool hasQuest = m_activeQuest.active || m_activeQuest.completed;
    auto options = NPCSystem::getDialogOptions(npc.type, npcStage, hasQuest);

    // Last option is always [Leave]
    if (choice == static_cast<int>(options.size()) - 1) {
        m_showNPCDialog = false;
        return;
    }

    if (!m_player->getEntity()->hasComponent<HealthComponent>() ||
        !m_player->getEntity()->hasComponent<CombatComponent>()) return;
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();

    // Lore: Forgotten Craft - NPC interaction
    if (auto* lore = game->getLoreSystem()) {
        if (!lore->isDiscovered(LoreID::ForgottenCraft)) {
            lore->discover(LoreID::ForgottenCraft);
            AudioManager::instance().play(SFX::LoreDiscover);
        }
    }

    int stage = getNPCStoryStage(npc.type);
    // Advance story progress after this interaction
    int typeIdx = static_cast<int>(npc.type);
    if (typeIdx >= 0 && typeIdx < static_cast<int>(NPCType::COUNT))
        m_npcStoryProgress[typeIdx]++;

    switch (npc.type) {
        case NPCType::RiftScholar:
            if (stage >= 2) {
                // Stage 2: Learn the truth — big shards + full heal
                AudioManager::instance().play(SFX::LoreDiscover);
                game->getUpgradeSystem().addRiftShards(40);
                shardsCollected += 40;
                hp.heal(hp.maxHP);
                m_player->damageBoostTimer = 20.0f;
                m_player->damageBoostMultiplier = 1.2f;
                m_particles.burst(npc.position, 25, {180, 120, 255, 255}, 200.0f, 4.0f);
                m_particles.burst(npc.position, 12, {255, 255, 200, 255}, 120.0f, 3.0f);
                if (auto* lore = game->getLoreSystem()) {
                    lore->discover(LoreID::SovereignTruth);
                }
            } else if (stage >= 1) {
                if (choice == 0) {
                    // Listen: better shards + entropy reduction
                    AudioManager::instance().play(SFX::Pickup);
                    game->getUpgradeSystem().addRiftShards(25);
                    shardsCollected += 25;
                    m_entropy.addEntropy(-15.0f);
                    m_particles.burst(npc.position, 18, {120, 200, 255, 255}, 150.0f, 3.0f);
                } else if (choice == 1) {
                    // Ask about Sovereign: lore + speed
                    AudioManager::instance().play(SFX::LoreDiscover);
                    m_player->speedBoostTimer = 30.0f;
                    m_player->speedBoostMultiplier = 1.2f;
                    game->getUpgradeSystem().addRiftShards(15);
                    shardsCollected += 15;
                    m_particles.burst(npc.position, 15, {180, 140, 255, 255}, 120.0f, 2.5f);
                }
            } else {
                if (choice == 0) {
                    // Listen to tip: shard bonus
                    AudioManager::instance().play(SFX::Pickup);
                    game->getUpgradeSystem().addRiftShards(15);
                    shardsCollected += 15;
                    m_particles.burst(npc.position, 12, {100, 180, 255, 255}, 100.0f, 2.0f);
                } else if (choice == 1) {
                    // Ask about enemies: brief speed boost
                    AudioManager::instance().play(SFX::LoreDiscover);
                    m_player->speedBoostTimer = 20.0f;
                    m_player->speedBoostMultiplier = 1.15f;
                    game->getUpgradeSystem().addRiftShards(10);
                    shardsCollected += 10;
                    m_particles.burst(npc.position, 15, {180, 220, 255, 255}, 120.0f, 2.5f);
                } else if (choice == 2 && !m_activeQuest.active && !m_activeQuest.completed) {
                    // Accept kill quest from Rift Scholar
                    m_activeQuest = NPCQuest{};
                    m_activeQuest.description = "Defeat 10 rift creatures";
                    m_activeQuest.targetKills = 10;
                    m_activeQuest.shardReward = 50;
                    m_activeQuest.questGiver = NPCType::RiftScholar;
                    m_activeQuest.active = true;
                    m_questKillSnapshot = enemiesKilled;
                    m_questRiftSnapshot = riftsRepaired;
                    AudioManager::instance().play(SFX::LoreDiscover);
                    m_particles.burst(npc.position, 18, {100, 200, 255, 255}, 150.0f, 3.0f);
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::DimRefugee:
            if (stage >= 2) {
                // Stage 2: Free relic gift
                if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
                    RelicID gift = RelicSystem::generateDrop(m_currentDifficulty, relics.relics);
                    relics.addRelic(gift);
                    RelicSystem::applyStatEffects(relics, *m_player, hp, combat);
                    AudioManager::instance().play(SFX::ShrineBlessing);
                    m_camera.shake(8.0f, 0.3f);
                    m_particles.burst(npc.position, 30, {255, 215, 80, 255}, 250.0f, 5.0f);
                    m_particles.burst(npc.position, 15, {255, 200, 60, 255}, 180.0f, 3.0f);
                }
            } else if (stage >= 1) {
                if (choice == 0) {
                    // Free healing +30 HP
                    hp.heal(30.0f);
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 15, {80, 255, 120, 255}, 120.0f, 2.0f);
                } else if (choice == 1) {
                    // Better trade: 15 HP for 60 shards
                    if (hp.currentHP > 20) {
                        hp.currentHP -= 15;
                        game->getUpgradeSystem().addRiftShards(60);
                        shardsCollected += 60;
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(npc.position, 15, {255, 200, 60, 255}, 120.0f, 2.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                        m_showNPCDialog = false;
                        return;
                    }
                }
            } else {
                if (choice == 0) {
                    // Trade 20 HP for 50 Shards
                    if (hp.currentHP > 25) {
                        hp.currentHP -= 20;
                        game->getUpgradeSystem().addRiftShards(50);
                        shardsCollected += 50;
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(npc.position, 15, {255, 200, 60, 255}, 120.0f, 2.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                        m_showNPCDialog = false;
                        return;
                    }
                } else if (choice == 1) {
                    // Trade 30 HP for 80 Shards
                    if (hp.currentHP > 35) {
                        hp.currentHP -= 30;
                        game->getUpgradeSystem().addRiftShards(80);
                        shardsCollected += 80;
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(npc.position, 20, {255, 200, 60, 255}, 150.0f, 2.5f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                        m_showNPCDialog = false;
                        return;
                    }
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::LostEngineer:
            if (stage >= 2) {
                // Stage 2: Permanent +25% damage boost for rest of run
                m_player->damageBoostTimer = 99999.0f; // effectively permanent
                m_player->damageBoostMultiplier = 1.25f;
                AudioManager::instance().play(SFX::ShrineBlessing);
                m_camera.shake(8.0f, 0.3f);
                m_particles.burst(npc.position, 25, {100, 255, 80, 255}, 250.0f, 5.0f);
                m_particles.burst(npc.position, 12, {255, 255, 200, 255}, 150.0f, 3.0f);
            } else if (stage >= 1) {
                if (choice == 0) {
                    // +40% damage for 60s
                    m_player->damageBoostTimer = 60.0f;
                    m_player->damageBoostMultiplier = 1.4f;
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 20, {180, 255, 120, 255}, 150.0f, 3.0f);
                } else if (choice == 1) {
                    // +20% attack speed for 45s
                    m_player->speedBoostTimer = 45.0f;
                    m_player->speedBoostMultiplier = 1.2f;
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 18, {120, 255, 200, 255}, 140.0f, 2.5f);
                }
            } else {
                if (choice == 0) {
                    // Stage 0: +30% damage for 45s
                    m_player->damageBoostTimer = 45.0f;
                    m_player->damageBoostMultiplier = 1.3f;
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 20, {180, 255, 120, 255}, 150.0f, 3.0f);
                } else if (choice == 1 && !m_activeQuest.active && !m_activeQuest.completed) {
                    // Accept rift repair quest from Lost Engineer
                    m_activeQuest = NPCQuest{};
                    m_activeQuest.description = "Repair 3 rifts";
                    m_activeQuest.targetRifts = 3;
                    m_activeQuest.shardReward = 30;
                    m_activeQuest.entropyReduction = 20.0f;
                    m_activeQuest.questGiver = NPCType::LostEngineer;
                    m_activeQuest.active = true;
                    m_questKillSnapshot = enemiesKilled;
                    m_questRiftSnapshot = riftsRepaired;
                    AudioManager::instance().play(SFX::LoreDiscover);
                    m_particles.burst(npc.position, 18, {120, 255, 200, 255}, 150.0f, 3.0f);
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::EchoOfSelf:
            if (choice == 0) {
                // Fight! Spawn a tough enemy as "mirror match"
                // Scales with story stage — harder echo, better reward
                Vec2 spawnPos = {npc.position.x + 60, npc.position.y};
                auto& mirror = Enemy::createByType(m_entities, static_cast<int>(EnemyType::Charger),
                                                   spawnPos, m_dimManager.getCurrentDimension());
                mirror.setTag("enemy_echo");
                m_echoSpawned = true;
                m_echoRewarded = false;
                auto& mirrorHP = mirror.getComponent<HealthComponent>();
                // Higher stages = tougher echo
                float hpScale = 1.0f + stage * 0.4f; // 1.0x, 1.4x, 1.8x
                mirrorHP.maxHP = hp.maxHP * hpScale;
                mirrorHP.currentHP = mirrorHP.maxHP;
                auto& mirrorCombat = mirror.getComponent<CombatComponent>();
                mirrorCombat.meleeAttack.damage = combat.meleeAttack.damage * (1.0f + stage * 0.2f);
                Enemy::makeElite(mirror, EliteModifier::Berserker);
                AudioManager::instance().play(SFX::AnomalySpawn);
                m_camera.shake(6.0f + stage * 3.0f, 0.2f + stage * 0.1f);
                m_particles.burst(npc.position, 25 + stage * 10, {220, 120, 255, 255}, 200.0f + stage * 50.0f, 3.0f);
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::Blacksmith: {
            auto options = NPCSystem::getDialogOptions(npc.type, stage, hasQuest);
            bool isLeave = (choice == static_cast<int>(options.size()) - 1);
            if (!isLeave && m_player) {
                if (stage >= 2) {
                    // Free masterwork: +30% both weapons
                    m_player->smithMeleeDmgMult += 0.30f;
                    m_player->smithRangedDmgMult += 0.30f;
                    AudioManager::instance().play(SFX::ShrineBlessing);
                    m_camera.shake(8.0f, 0.3f);
                    m_particles.burst(npc.position, 30, {255, 200, 50, 255}, 200.0f, 4.0f);
                } else if (stage >= 1) {
                    int cost = (choice == 2) ? 45 : 35;
                    if (shardsCollected >= cost) {
                        shardsCollected -= cost;
                        if (choice == 0) m_player->smithMeleeDmgMult += 0.25f;
                        else if (choice == 1) m_player->smithRangedDmgMult += 0.25f;
                        else m_player->smithAtkSpdMult += 0.15f;
                        AudioManager::instance().play(SFX::ShrineBlessing);
                        m_camera.shake(5.0f, 0.2f);
                        m_particles.burst(npc.position, 20, {255, 180, 50, 255}, 150.0f, 3.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                    }
                } else {
                    int cost = 40;
                    if (shardsCollected >= cost) {
                        shardsCollected -= cost;
                        if (choice == 0) m_player->smithMeleeDmgMult += 0.20f;
                        else m_player->smithRangedDmgMult += 0.20f;
                        AudioManager::instance().play(SFX::ShrineBlessing);
                        m_camera.shake(5.0f, 0.2f);
                        m_particles.burst(npc.position, 20, {255, 180, 50, 255}, 150.0f, 3.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                    }
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;
        }

        case NPCType::FortuneTeller: {
            // Lore: Dimensional Theory on first interaction
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::DimensionTheory)) {
                    lore->discover(LoreID::DimensionTheory);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }
            if (stage >= 2) {
                if (choice == 0) {
                    // Reveal all secrets (free) — mark all secret rooms visible
                    if (m_level) {
                        auto& rooms = m_level->getSecretRooms();
                        for (auto& sr : rooms) sr.discovered = true;
                    }
                    AudioManager::instance().play(SFX::SecretRoomDiscover);
                    m_camera.shake(6.0f, 0.25f);
                    m_particles.burst(npc.position, 25, {200, 180, 255, 255}, 200.0f, 4.0f);
                } else if (choice == 1) {
                    // Boss foresight: +20% damage vs boss for rest of level
                    m_player->damageBoostTimer = 99999.0f;
                    m_player->damageBoostMultiplier = 1.2f;
                    AudioManager::instance().play(SFX::ShrineBlessing);
                    m_particles.burst(npc.position, 20, {255, 200, 255, 255}, 180.0f, 3.0f);
                }
            } else if (stage >= 1) {
                if (choice == 0) {
                    // Reveal hidden rooms (20 shards)
                    if (shardsCollected >= 20) {
                        shardsCollected -= 20;
                        if (m_level) {
                            auto& rooms = m_level->getSecretRooms();
                            for (auto& sr : rooms) sr.discovered = true;
                        }
                        AudioManager::instance().play(SFX::SecretRoomDiscover);
                        m_particles.burst(npc.position, 18, {180, 150, 255, 255}, 150.0f, 3.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                    }
                } else if (choice == 1) {
                    // Reveal ambushes (15 shards) — speed boost to avoid danger
                    if (shardsCollected >= 15) {
                        shardsCollected -= 15;
                        m_player->speedBoostTimer = 30.0f;
                        m_player->speedBoostMultiplier = 1.25f;
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(npc.position, 15, {150, 120, 255, 255}, 120.0f, 2.5f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                    }
                }
            } else {
                if (choice == 0) {
                    // Reveal secrets (30 shards)
                    if (shardsCollected >= 30) {
                        shardsCollected -= 30;
                        if (m_level) {
                            auto& rooms = m_level->getSecretRooms();
                            for (auto& sr : rooms) sr.discovered = true;
                        }
                        AudioManager::instance().play(SFX::SecretRoomDiscover);
                        m_particles.burst(npc.position, 18, {180, 150, 255, 255}, 150.0f, 3.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                    }
                } else if (choice == 1) {
                    // Read fortune — free lore + small shard bonus
                    AudioManager::instance().play(SFX::LoreDiscover);
                    game->getUpgradeSystem().addRiftShards(10);
                    shardsCollected += 10;
                    m_particles.burst(npc.position, 12, {200, 160, 255, 255}, 100.0f, 2.0f);
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;
        }

        case NPCType::VoidMerchant: {
            // Lore: Void Commerce on first interaction
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::VoidCommerce)) {
                    lore->discover(LoreID::VoidCommerce);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }
            auto canAfford = [&](int cost) { return shardsCollected >= cost; };
            auto buyRelic = [&](int cost) {
                if (!canAfford(cost)) {
                    AudioManager::instance().play(SFX::RiftFail);
                    return;
                }
                shardsCollected -= cost;
                if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
                    RelicID gift = RelicSystem::generateDrop(m_currentDifficulty, relics.relics);
                    relics.addRelic(gift);
                    RelicSystem::applyStatEffects(relics, *m_player, hp, combat);
                    AudioManager::instance().play(SFX::ShrineBlessing);
                    m_camera.shake(6.0f, 0.25f);
                    m_particles.burst(npc.position, 25, {180, 120, 255, 255}, 200.0f, 4.0f);
                    m_particles.burst(npc.position, 10, {255, 200, 60, 255}, 120.0f, 2.0f);
                }
            };

            if (stage >= 2) {
                if (choice == 0) buyRelic(80);       // Legendary relic
                else if (choice == 1) buyRelic(40);  // Random relic
            } else if (stage >= 1) {
                if (choice == 0) buyRelic(60);
                else if (choice == 1) buyRelic(35);
            } else {
                if (choice == 0) buyRelic(50);
                else if (choice == 1) {
                    // Browse wares — just a flavor dialog, small shard bonus
                    AudioManager::instance().play(SFX::MerchantGreet);
                    game->getUpgradeSystem().addRiftShards(5);
                    shardsCollected += 5;
                    m_particles.burst(npc.position, 8, {180, 120, 255, 255}, 80.0f, 1.5f);
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;
        }

        default:
            m_showNPCDialog = false;
            break;
    }
}

void PlayState::spawnChainEvent() {
    if (m_eventChain.stage <= 0 || m_eventChain.completed || m_chainEventSpawned) return;
    m_chainEventSpawned = true;

    // Place a chain-themed random event in the level
    auto& events = m_level->getRandomEvents();
    m_chainEventIndex = static_cast<int>(events.size()); // Will be the index of the new event
    Vec2 spawnPos = m_level->getSpawnPoint();
    // Offset from spawn to make it discoverable but not immediate
    spawnPos.x += 200.0f + static_cast<float>(std::rand() % 300);
    spawnPos.y -= 32.0f;

    RandomEvent chainEvt;
    chainEvt.dimension = 0; // Visible in both dimensions
    chainEvt.position = spawnPos;
    chainEvt.used = false;
    chainEvt.active = true;

    switch (m_eventChain.type) {
        case EventChainType::MerchantQuest:
            if (m_eventChain.stage == 1 || m_eventChain.stage == 3) {
                chainEvt.type = RandomEventType::Merchant;
                chainEvt.cost = 0; // Free — chain merchant
            } else {
                // Stage 2: artifact is a shard pickup (RiftEcho)
                chainEvt.type = RandomEventType::RiftEcho;
                chainEvt.reward = 15 + m_currentDifficulty * 5;
            }
            break;
        case EventChainType::DimensionalTear:
            chainEvt.type = RandomEventType::DimensionalAnomaly;
            chainEvt.reward = 10 * m_eventChain.stage;
            break;
        case EventChainType::EntropySurge:
            if (m_eventChain.stage < 3) {
                chainEvt.type = RandomEventType::SuitRepairStation;
            } else {
                chainEvt.type = RandomEventType::Shrine;
                chainEvt.shrineType = ShrineType::Entropy;
            }
            break;
        case EventChainType::LostCache:
            if (m_eventChain.stage < 3) {
                chainEvt.type = RandomEventType::RiftEcho;
                chainEvt.reward = 10 + m_eventChain.stage * 10;
            } else {
                chainEvt.type = RandomEventType::GamblingRift;
                chainEvt.cost = 0; // Free final cache
                chainEvt.reward = 80 + m_currentDifficulty * 10;
            }
            break;
        default: break;
    }

    events.push_back(chainEvt);
}

void PlayState::advanceChain() {
    if (m_eventChain.stage <= 0 || m_eventChain.completed) return;

    m_eventChain.stage++;
    if (m_eventChain.stage > m_eventChain.maxStages) {
        completeChain();
    } else {
        m_chainNotifyTimer = 3.5f; // Show stage advance notification
        AudioManager::instance().play(SFX::LoreDiscover);
    }
}

void PlayState::completeChain() {
    m_eventChain.completed = true;
    m_chainRewardShards = m_eventChain.getCompletionReward(m_currentDifficulty);

    // Grant shards
    float shardMult = game->getRunBuffSystem().getShardMultiplier() * m_achievementShardMult;
    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>())
        shardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
    m_chainRewardShards = static_cast<int>(m_chainRewardShards * shardMult);
    shardsCollected += m_chainRewardShards;
    game->getUpgradeSystem().addRiftShards(m_chainRewardShards);

    // Chain-specific completion bonus
    switch (m_eventChain.type) {
        case EventChainType::MerchantQuest:
            // Damage boost
            if (m_player) {
                m_player->damageBoostTimer = 45.0f;
                m_player->damageBoostMultiplier = 1.25f;
            }
            break;
        case EventChainType::DimensionalTear:
            // Heal to full
            if (m_player) {
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.heal(hp.maxHP);
            }
            break;
        case EventChainType::EntropySurge:
            // Big entropy reduction
            m_entropy.reduceEntropy(40.0f);
            break;
        case EventChainType::LostCache:
            // Extra shard bonus already in higher reward
            break;
        default: break;
    }

    m_chainRewardTimer = 4.0f;
    AudioManager::instance().play(SFX::LevelComplete);
    m_camera.shake(12.0f, 0.5f);
    if (m_player) {
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        SDL_Color cc = m_eventChain.getColor();
        m_particles.burst(pPos, 50, cc, 300.0f, 5.0f);
        m_particles.burst(pPos, 25, {255, 215, 0, 255}, 200.0f, 4.0f);
    }
}

void PlayState::updateEventChain(float dt) {
    if (m_chainNotifyTimer > 0) m_chainNotifyTimer -= dt;
    if (m_chainRewardTimer > 0) m_chainRewardTimer -= dt;

    // Chain stage auto-advance: when the chain event on this level is interacted with
    if (m_eventChain.stage > 0 && !m_eventChain.completed && m_chainEventSpawned && m_chainEventIndex >= 0) {
        auto& events = m_level->getRandomEvents();
        if (m_chainEventIndex < static_cast<int>(events.size()) && events[m_chainEventIndex].used) {
            advanceChain();
            m_chainEventSpawned = false; // Prevent double-advance
        }
    }
}

void PlayState::addKillFeedEntry(const KillEvent& ke) {
    auto& entry = m_killFeed[m_killFeedHead % MAX_KILL_FEED];
    entry.timer = 4.0f; // visible for 4 seconds

    // Get enemy name from Bestiary
    const char* enemyName = "Enemy";
    if (ke.enemyType >= 0 && ke.enemyType < static_cast<int>(EnemyType::Boss)) {
        enemyName = Bestiary::getEntry(static_cast<EnemyType>(ke.enemyType)).name;
    } else if (ke.wasBoss) {
        enemyName = "BOSS";
    }

    // Build kill text with context
    const char* prefix = "";
    if (ke.wasMiniBoss) prefix = "[MINI-BOSS] ";
    else if (ke.wasElite) prefix = "[ELITE] ";

    const char* method = "";
    if (ke.wasDash) method = " (Dash)";
    else if (ke.wasCharged) method = " (Charged)";
    else if (ke.wasSlam) method = " (Slam)";
    else if (ke.wasComboFinisher) method = " (Finisher)";
    else if (ke.wasAerial) method = " (Aerial)";
    else if (ke.wasRanged) method = " (Ranged)";

    std::snprintf(entry.text, sizeof(entry.text), "%s%s killed%s", prefix, enemyName, method);

    // Color based on kill type
    if (ke.wasBoss) entry.color = {255, 80, 80, 255};
    else if (ke.wasMiniBoss) entry.color = {255, 160, 60, 255};
    else if (ke.wasElite) entry.color = {200, 140, 255, 255};
    else if (ke.wasDash || ke.wasCharged || ke.wasSlam) entry.color = {255, 215, 80, 255};
    else if (ke.wasAerial || ke.wasComboFinisher) entry.color = {100, 220, 255, 255};
    else entry.color = {180, 180, 200, 220};

    m_killFeedHead++;
}

void PlayState::updateKillFeed(float dt) {
    for (int i = 0; i < MAX_KILL_FEED; i++) {
        if (m_killFeed[i].timer > 0) m_killFeed[i].timer -= dt;
    }
}

void PlayState::pushUnlockNotification(const char* name, bool isWeapon) {
    auto& notif = m_unlockNotifs[m_unlockNotifHead % MAX_UNLOCK_NOTIFS];
    std::snprintf(notif.text, sizeof(notif.text),
                  "%s UNLOCKED: %s", isWeapon ? "WEAPON" : "CLASS", name);
    notif.timer = notif.maxTimer;
    m_unlockNotifHead++;

    // Gold particle burst at notification position (screen-center top)
    Vec2 camPos = m_camera.getPosition();
    float z = m_camera.zoom;
    float sw = static_cast<float>(m_camera.getViewWidth());
    float sh = static_cast<float>(m_camera.getViewHeight());
    Vec2 worldPos = {(640.0f - sw / 2.0f) / z + camPos.x,
                     (155.0f - sh / 2.0f) / z + camPos.y};
    SDL_Color gold = {255, 200, 50, 255};
    m_particles.burst(worldPos, 20, gold, 120.0f, 3.0f);
}

void PlayState::updateUnlockNotifications(float dt) {
    for (int i = 0; i < MAX_UNLOCK_NOTIFS; i++) {
        if (m_unlockNotifs[i].timer > 0) m_unlockNotifs[i].timer -= dt;
    }
}

void PlayState::checkUnlockConditions() {
    auto& upgrades = game->getUpgradeSystem();

    // ── Class unlock checks ──

    // Berserker: kill 50 total enemies across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Berserker) &&
        upgrades.totalEnemiesKilled >= 50) {
        ClassSystem::unlock(PlayerClass::Berserker);
        pushUnlockNotification("Berserker", false);
    }

    // Phantom: complete floor 3 in any run (m_currentDifficulty > 3)
    if (!ClassSystem::isUnlocked(PlayerClass::Phantom) &&
        m_currentDifficulty > 3) {
        ClassSystem::unlock(PlayerClass::Phantom);
        pushUnlockNotification("Phantom", false);
    }

    // Technomancer: repair 30 rifts across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Technomancer) &&
        upgrades.totalRiftsRepaired >= 30) {
        ClassSystem::unlock(PlayerClass::Technomancer);
        pushUnlockNotification("Technomancer", false);
    }

    // ── Weapon unlock checks ──

    // PhaseDaggers: get a 10-hit combo
    if (!WeaponSystem::isUnlocked(WeaponID::PhaseDaggers) &&
        m_bestCombo >= 10) {
        WeaponSystem::unlock(WeaponID::PhaseDaggers);
        pushUnlockNotification("Phase Daggers", true);
    }

    // VoidHammer: defeat any boss
    if (!WeaponSystem::isUnlocked(WeaponID::VoidHammer) &&
        m_bossDefeated && !m_unlockedBossWeaponThisRun) {
        WeaponSystem::unlock(WeaponID::VoidHammer);
        m_unlockedBossWeaponThisRun = true;
        pushUnlockNotification("Void Hammer", true);
    }

    // VoidBeam: reach floor 5
    if (!WeaponSystem::isUnlocked(WeaponID::VoidBeam) &&
        m_currentDifficulty >= 5) {
        WeaponSystem::unlock(WeaponID::VoidBeam);
        pushUnlockNotification("Void Beam", true);
    }

    // GrapplingHook: 5 dash-kills in one run
    if (!WeaponSystem::isUnlocked(WeaponID::GrapplingHook) &&
        m_dashKillsThisRun >= 5) {
        WeaponSystem::unlock(WeaponID::GrapplingHook);
        pushUnlockNotification("Grappling Hook", true);
    }

    // RiftShotgun: 3+ kills from one attack (tracked in kill event loop)
    if (!WeaponSystem::isUnlocked(WeaponID::RiftShotgun) &&
        m_aoeKillCountThisRun >= 3) {
        WeaponSystem::unlock(WeaponID::RiftShotgun);
        m_aoeKillCountThisRun = 0;
        pushUnlockNotification("Rift Shotgun", true);
    }

    // EntropyScythe: reach floor 10
    if (!WeaponSystem::isUnlocked(WeaponID::EntropyScythe) &&
        m_currentDifficulty >= 10) {
        WeaponSystem::unlock(WeaponID::EntropyScythe);
        pushUnlockNotification("Entropy Scythe", true);
    }

    // ChainWhip: kill 100 enemies total
    if (!WeaponSystem::isUnlocked(WeaponID::ChainWhip) &&
        game->getUpgradeSystem().totalEnemiesKilled >= 100) {
        WeaponSystem::unlock(WeaponID::ChainWhip);
        pushUnlockNotification("Chain Whip", true);
    }

    // GravityGauntlet: defeat the Dimensional Architect (reach floor 13+)
    if (!WeaponSystem::isUnlocked(WeaponID::GravityGauntlet) &&
        m_currentDifficulty >= 13) {
        WeaponSystem::unlock(WeaponID::GravityGauntlet);
        pushUnlockNotification("Gravity Gauntlet", true);
    }

    // DimLauncher: switch dimensions 20 times in one run
    if (!WeaponSystem::isUnlocked(WeaponID::DimLauncher) &&
        m_dimensionSwitches >= 20) {
        WeaponSystem::unlock(WeaponID::DimLauncher);
        pushUnlockNotification("Dim Launcher", true);
    }

    // RiftCrossbow: reach floor 15
    if (!WeaponSystem::isUnlocked(WeaponID::RiftCrossbow) &&
        m_currentDifficulty >= 15) {
        WeaponSystem::unlock(WeaponID::RiftCrossbow);
        pushUnlockNotification("Rift Crossbow", true);
    }

    // Arsenal Master: all weapons unlocked
    if (!game->getAchievements().isUnlocked("weapon_collector")) {
        int unlockedCount = 0;
        for (int w = 0; w < static_cast<int>(WeaponID::COUNT); w++) {
            if (WeaponSystem::isUnlocked(static_cast<WeaponID>(w))) unlockedCount++;
        }
        if (unlockedCount >= static_cast<int>(WeaponID::COUNT)) {
            game->getAchievements().unlock("weapon_collector");
        }
    }
}

