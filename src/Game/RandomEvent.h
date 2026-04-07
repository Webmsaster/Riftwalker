#pragma once
#include "Core/Camera.h"
#include "Core/Localization.h"

enum class RandomEventType {
    Merchant,           // Sells 1 run buff (cheaper than shop)
    Shrine,             // Visible trade-off blessing
    DimensionalAnomaly, // Bonus enemies + bonus loot
    RiftEcho,           // Shard rain
    SuitRepairStation,  // Full heal
    GamblingRift        // Pay shards, random reward
};

// Shrine subtypes — each offers a visible trade-off the player sees before interacting
enum class ShrineType {
    Power,     // +30% damage 60s, -15 max HP
    Vitality,  // +25 max HP & heal, +8 entropy
    Speed,     // +25% speed 45s, -10 max HP
    Entropy,   // -25 entropy, -15 current HP
    Shards,    // +40-60 shards, +12 entropy
    Renewal,   // Full heal, +5 entropy
    COUNT
};

struct RandomEvent {
    RandomEventType type;
    Vec2 position;
    int dimension;       // Which dimension this event appears in
    bool used = false;
    bool active = true;

    // Event-specific data
    int cost = 0;        // For merchant/gambling
    int reward = 0;      // Shard reward amount
    ShrineType shrineType = ShrineType::Power; // Only used when type == Shrine

    const char* getName() const {
        switch (type) {
            case RandomEventType::Merchant:           return LOC("event.merchant");
            case RandomEventType::Shrine:             return getShrineName();
            case RandomEventType::DimensionalAnomaly: return LOC("event.anomaly");
            case RandomEventType::RiftEcho:           return LOC("event.rift_echo");
            case RandomEventType::SuitRepairStation:  return LOC("event.repair_station");
            case RandomEventType::GamblingRift:       return LOC("event.gambling");
        }
        return "???";
    }

    const char* getShrineName() const {
        switch (shrineType) {
            case ShrineType::Power:    return LOC("event.shrine_power");
            case ShrineType::Vitality: return LOC("event.shrine_vitality");
            case ShrineType::Speed:    return LOC("event.shrine_speed");
            case ShrineType::Entropy:  return LOC("event.shrine_entropy");
            case ShrineType::Shards:   return LOC("event.shrine_shards");
            case ShrineType::Renewal:  return "Shrine of Renewal";
            default:                   return "Shrine";
        }
    }

    const char* getShrineEffect() const {
        switch (shrineType) {
            case ShrineType::Power:    return "+30% DMG 60s | -15 Max HP";
            case ShrineType::Vitality: return "+25 Max HP | +8 Entropy";
            case ShrineType::Speed:    return "+25% Speed 45s | -10 Max HP";
            case ShrineType::Entropy:  return "-25 Entropy | -15 HP";
            case ShrineType::Shards:   return "+Shards | +12 Entropy";
            case ShrineType::Renewal:  return "Full Heal | +5 Entropy";
            default:                   return "";
        }
    }

    SDL_Color getShrineColor() const {
        switch (shrineType) {
            case ShrineType::Power:    return {255, 80, 80, 255};   // Red
            case ShrineType::Vitality: return {80, 255, 120, 255};  // Green
            case ShrineType::Speed:    return {80, 200, 255, 255};  // Cyan
            case ShrineType::Entropy:  return {180, 120, 255, 255}; // Purple
            case ShrineType::Shards:   return {255, 215, 60, 255};  // Gold
            case ShrineType::Renewal:  return {100, 255, 200, 255}; // Teal
            default:                   return {180, 120, 255, 255};
        }
    }
};

// Event Chains — multi-stage events that unfold across levels within a run
enum class EventChainType {
    MerchantQuest,    // Merchant gives quest -> find item -> return for reward
    DimensionalTear,  // Growing tears -> dimensional invasion -> bonus loot
    EntropySurge,     // Entropy warnings -> entropy challenge -> entropy relic
    LostCache,        // Treasure clues -> trail -> hidden cache
    COUNT
};

struct EventChain {
    EventChainType type = EventChainType::MerchantQuest;
    int stage = 0;           // 0=inactive, 1-3=active stages
    int maxStages = 3;
    int startLevel = 0;      // Difficulty level when chain started
    bool completed = false;

    const char* getName() const {
        switch (type) {
            case EventChainType::MerchantQuest:   return LOC("chain.merchant_quest");
            case EventChainType::DimensionalTear:  return LOC("chain.dim_tear");
            case EventChainType::EntropySurge:     return LOC("chain.entropy_surge");
            case EventChainType::LostCache:        return LOC("chain.lost_cache");
            default: return "???";
        }
    }

    SDL_Color getColor() const {
        switch (type) {
            case EventChainType::MerchantQuest:   return {255, 200, 80, 255};  // Gold
            case EventChainType::DimensionalTear:  return {200, 80, 255, 255};  // Purple
            case EventChainType::EntropySurge:     return {255, 100, 100, 255}; // Red
            case EventChainType::LostCache:        return {80, 220, 255, 255};  // Cyan
            default: return {200, 200, 200, 255};
        }
    }

    const char* getStageDesc() const {
        switch (type) {
            case EventChainType::MerchantQuest:
                if (stage == 1) return "A merchant seeks a rare artifact...";
                if (stage == 2) return "The artifact glows nearby!";
                if (stage == 3) return "Return the artifact to the merchant!";
                break;
            case EventChainType::DimensionalTear:
                if (stage == 1) return "A faint tear in reality appears...";
                if (stage == 2) return "The tear is growing — enemies pour through!";
                if (stage == 3) return "Seal the rift before it consumes all!";
                break;
            case EventChainType::EntropySurge:
                if (stage == 1) return "Strange entropy fluctuations detected...";
                if (stage == 2) return "Entropy is destabilizing rapidly!";
                if (stage == 3) return "Stabilize the entropy core!";
                break;
            case EventChainType::LostCache:
                if (stage == 1) return "You found an ancient map fragment...";
                if (stage == 2) return "More fragments — the path becomes clear!";
                if (stage == 3) return "The cache is here — claim your reward!";
                break;
            default: break;
        }
        return "";
    }

    // Shard reward on chain completion
    int getCompletionReward(int difficulty) const {
        int base = 60 + difficulty * 15;
        switch (type) {
            case EventChainType::MerchantQuest:   return base;           // Standard
            case EventChainType::DimensionalTear:  return base + 20;      // Combat-heavy = more shards
            case EventChainType::EntropySurge:     return base + 10;      // Medium
            case EventChainType::LostCache:        return base + 30;      // Treasure = most shards
            default: return base;
        }
    }
};
