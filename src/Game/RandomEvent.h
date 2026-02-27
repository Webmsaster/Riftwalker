#pragma once
#include "Core/Camera.h"

enum class RandomEventType {
    Merchant,           // Sells 1 run buff (cheaper than shop)
    Shrine,             // 50/50 buff or curse
    DimensionalAnomaly, // Bonus enemies + bonus loot
    RiftEcho,           // Shard rain
    SuitRepairStation,  // Full heal
    GamblingRift        // Pay shards, random reward
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

    const char* getName() const {
        switch (type) {
            case RandomEventType::Merchant:           return "Merchant";
            case RandomEventType::Shrine:             return "Shrine";
            case RandomEventType::DimensionalAnomaly: return "Anomaly";
            case RandomEventType::RiftEcho:           return "Rift Echo";
            case RandomEventType::SuitRepairStation:  return "Repair Station";
            case RandomEventType::GamblingRift:       return "Gambling Rift";
        }
        return "???";
    }
};
