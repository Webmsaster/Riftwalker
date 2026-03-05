#pragma once
#include "Components/RelicComponent.h"

enum class SynergyID {
    VampiricFrenzy = 0,  // BloodFrenzy + SoulSiphon
    ChainThorns,         // ThornMail + ChainLightning
    TemporalEcho,        // TimeDilator + EchoStrike
    PhaseHunter,         // PhaseCloak + DimensionalEcho
    VoidCatalyst,        // VoidHunger + BerserkerCore
    LuckyPhoenix,        // LuckyCoin + PhoenixFeather
    FortifiedSoul,       // IronHeart + GlassHeart
    EntropyVortex,       // EntropyAnchor + EntropySponge
    Quicksilver,         // SwiftBoots + QuickHands
    CursedFortune,       // CursedBlade + LuckyCoin
    RiftMaster,          // RiftConduit + StabilityMatrix
    DualNature,          // DualityGem + RiftMantle
    VoidEcho,            // VoidResonance + DimResidue
    COUNT
};

struct SynergyData {
    SynergyID id;
    const char* name;
    const char* description;
    RelicID relicA;
    RelicID relicB;
};

class RelicSynergy {
public:
    static const SynergyData& getData(SynergyID id);
    static bool isActive(const RelicComponent& relics, SynergyID id);

    // Query active synergies for display
    static int getActiveSynergyCount(const RelicComponent& relics);

    // Synergy-specific queries
    static float getKillHealBonus(const RelicComponent& relics, float hpPercent);
    static float getThornChainDamage(const RelicComponent& relics);
    static float getEchoStrikeChance(const RelicComponent& relics);
    static float getPhaseHunterDamageMult(const RelicComponent& relics);
    static float getVoidHungerBonusPerKill(const RelicComponent& relics);
    static float getPhoenixShardRetain(const RelicComponent& relics);
    static float getGlassHeartDamageMult(const RelicComponent& relics);
    static float getKillEntropyReduction(const RelicComponent& relics);
    static float getDashSpeedBonus(const RelicComponent& relics);
    static float getShardDropBonus(const RelicComponent& relics);

    // Dimension relic synergies
    static bool isRiftMasterActive(const RelicComponent& relics);
    static float getStabilityDmgPerSec(const RelicComponent& relics);
    static bool isDualNatureActive(const RelicComponent& relics);
    static float getResidueDuration(const RelicComponent& relics);
    static float getResidueDamage(const RelicComponent& relics);
};
