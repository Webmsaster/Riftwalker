#pragma once
#include "Components/RelicComponent.h"

enum class SynergyID {
    VampiricFrenzy = 0,  // BloodFrenzy + SoulSiphon
    ChainThorns,         // ThornMail + ChainLightning
    TemporalEcho,        // TimeDilator + EchoStrike
    PhaseHunter,         // PhaseCloak + DimensionalEcho
    VoidCatalyst,        // VoidHunger + BerserkerCore
    LuckyPhoenix,        // LuckyCoin + PhoenixFeather
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
};
