#pragma once
#include "ECS/Component.h"
#include <SDL2/SDL.h>
#include <vector>
#include <string>

enum class RelicID {
    None = 0,
    // Common (Tier 1)
    IronHeart,       // +15 Max HP
    SwiftBoots,      // +10% Movespeed
    ShardMagnet,     // +50% Shard pickup radius
    ThornMail,       // 8 DMG reflection on hit
    QuickHands,      // +15% Attack speed
    LuckyCoin,       // +10% Shard drop
    // Rare (Tier 2)
    BloodFrenzy,     // +25% DMG under 30% HP
    EchoStrike,      // 20% chance double hit
    PhaseCloak,      // 1s invis after dim-switch
    EntropyAnchor,   // -40% Entropy in boss fights
    ChainLightning,  // Kills send 15 DMG bolt to nearest enemy
    SoulSiphon,      // Kills heal 3 HP
    // Legendary (Tier 3)
    BerserkerCore,   // +50% DMG, -30% Max HP
    TimeDilator,     // -30% Ability cooldowns
    DimensionalEcho, // Attacks hit other dimension too
    PhoenixFeather,  // 1x Auto-Revive (100% HP)
    VoidHunger,      // Each kill +1% permanent DMG bonus (run)
    ChaosOrb,        // Random relic effect every 30s
    COUNT
};

enum class RelicTier { Common = 0, Rare, Legendary };

struct RelicData {
    RelicID id = RelicID::None;
    const char* name = "";
    const char* description = "";
    RelicTier tier = RelicTier::Common;
    SDL_Color glowColor{255, 255, 255, 255};
};

struct ActiveRelic {
    RelicID id = RelicID::None;
    float timer = 0;       // For timed effects (ChaosOrb)
    float bonusAccum = 0;  // For stacking effects (VoidHunger)
    bool consumed = false;  // For one-use relics (PhoenixFeather)
};

struct RelicComponent : public Component {
    std::vector<ActiveRelic> relics;
    float phaseCloakTimer = 0;  // Invisibility after dim-switch
    float chaosOrbTimer = 0;    // ChaosOrb random effect timer
    int chaosOrbCurrentEffect = 0;
    float voidHungerBonus = 0;  // Accumulated +% DMG from VoidHunger

    bool hasRelic(RelicID id) const {
        for (auto& r : relics) {
            if (r.id == id && !r.consumed) return true;
        }
        return false;
    }

    ActiveRelic* getRelic(RelicID id) {
        for (auto& r : relics) {
            if (r.id == id) return &r;
        }
        return nullptr;
    }

    void addRelic(RelicID id) {
        ActiveRelic r;
        r.id = id;
        relics.push_back(r);
    }

    void update(float dt) override {
        if (phaseCloakTimer > 0) phaseCloakTimer -= dt;
        if (chaosOrbTimer > 0) chaosOrbTimer -= dt;
    }
};
