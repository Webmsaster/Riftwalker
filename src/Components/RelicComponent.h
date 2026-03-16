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
    // Cursed Relics (strong positive + negative)
    CursedBlade,     // +40% Melee DMG, -20% Ranged DMG
    GlassHeart,      // +50% Max HP, 2x damage taken
    TimeTax,         // -50% Ability CD, abilities cost 10 HP
    EntropySponge,   // No passive entropy, kills +5% entropy
    VoidPact,        // Kill heals 5 HP, max 60% HP healing
    ChaosRift,       // Every 10th kill: random buff 15s, every 5th hit: +50% DMG spike
    // Dimension-Switch Relics
    RiftConduit,     // Dim-switch: +10% attack speed 3s (stacks 3x)
    DualityGem,      // Dim A: +25% DMG, Dim B: +25% Armor
    DimResidue,      // Leave damage zone in old dimension after switch
    RiftMantle,      // -50% switch cooldown, but 5% maxHP per switch
    StabilityMatrix, // +3% DMG/s in current dim (max +30%, resets on switch)
    VoidResonance,   // Kill enemy in wrong dimension: 2x DMG
    // New Cursed Relics (powerful + permanent downside)
    BloodPact,       // +40% damage, but -1 HP per kill
    EntropySiphon,   // Kills reduce entropy 8, but +50% entropy gain from all sources
    GlassCannon,     // +60% damage, but max HP halved
    VampiricEdge,    // Heal 3 HP per kill, but no natural/pickup healing
    ChaosCore,       // Random dim switch every 20s, +25% all stats
    BerserkersCurse, // +15% damage per missing 10% HP (stacks), no shields
    TimeDistortion,  // +30% move+attack speed, entropy decays 50% slower
    SoulLeech,       // 2x shard drops, -5 HP per level transition
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

    // Cursed relic state
    int chaosRiftKillCount = 0;   // ChaosRift: track kills for buff trigger
    int chaosRiftHitCount = 0;    // ChaosRift: track hits taken for DMG spike
    float chaosRiftBuffTimer = 0; // ChaosRift: active buff timer
    int chaosRiftBuffType = 0;    // ChaosRift: which random buff is active

    // Dimension relic state
    float riftConduitTimer = 0;       // RiftConduit: attack speed buff duration
    int riftConduitStacks = 0;        // RiftConduit: current stacks (max 3)
    float stabilityTimer = 0;         // StabilityMatrix: time in current dimension
    int lastSwitchDimension = 0;      // Track which dimension for Residue

    // Internal cooldowns (safety rails)
    float voidResonanceProcCD = 0;    // VoidResonance: ICD between 2x procs (seconds)
    float dimResidueSpawnCD = 0;      // DimResidue: ICD between zone spawns (seconds)

    // New cursed relic state
    float chaosCoreTimer = 20.0f;       // ChaosCore: countdown to next forced dim-switch
    bool vampiricEdgeActive = false;    // VampiricEdge: no-healing flag (cached)

    // Synergy state
    bool phaseHunterBuffActive = false; // Phase Hunter: next attack 2x DMG after dim-switch
    float phaseHunterBuffTimer = 0;     // Phase Hunter: buff expires after 3s

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
        if (voidResonanceProcCD > 0) voidResonanceProcCD -= dt;
        if (dimResidueSpawnCD > 0) dimResidueSpawnCD -= dt;
        if (riftConduitTimer > 0) {
            riftConduitTimer -= dt;
            if (riftConduitTimer <= 0) riftConduitStacks = 0;
        }
        if (hasRelic(RelicID::StabilityMatrix)) {
            stabilityTimer += dt;
            if (stabilityTimer > 10.0f) stabilityTimer = 10.0f;
        }
        // ChaosCore: count down to next forced dimension switch
        if (hasRelic(RelicID::ChaosCore)) {
            chaosCoreTimer -= dt;
        }
    }
};
