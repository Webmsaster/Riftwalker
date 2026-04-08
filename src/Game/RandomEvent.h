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
            case ShrineType::Renewal:  return LOC("event.shrine_renewal");
            default:                   return LOC("event.shrine_generic");
        }
    }

    const char* getShrineEffect() const {
        switch (shrineType) {
            case ShrineType::Power:    return LOC("event.shrine_fx_power");
            case ShrineType::Vitality: return LOC("event.shrine_fx_vitality");
            case ShrineType::Speed:    return LOC("event.shrine_fx_speed");
            case ShrineType::Entropy:  return LOC("event.shrine_fx_entropy");
            case ShrineType::Shards:   return LOC("event.shrine_fx_shards");
            case ShrineType::Renewal:  return LOC("event.shrine_fx_renewal");
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
                if (stage == 1) return LOC("chain.merchant_s1");
                if (stage == 2) return LOC("chain.merchant_s2");
                if (stage == 3) return LOC("chain.merchant_s3");
                break;
            case EventChainType::DimensionalTear:
                if (stage == 1) return LOC("chain.tear_s1");
                if (stage == 2) return LOC("chain.tear_s2");
                if (stage == 3) return LOC("chain.tear_s3");
                break;
            case EventChainType::EntropySurge:
                if (stage == 1) return LOC("chain.entropy_s1");
                if (stage == 2) return LOC("chain.entropy_s2");
                if (stage == 3) return LOC("chain.entropy_s3");
                break;
            case EventChainType::LostCache:
                if (stage == 1) return LOC("chain.cache_s1");
                if (stage == 2) return LOC("chain.cache_s2");
                if (stage == 3) return LOC("chain.cache_s3");
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
