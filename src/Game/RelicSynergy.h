#pragma once
#include "Components/RelicComponent.h"
#include "Game/WeaponSystem.h"

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
    // Weapon-Relic Synergies
    BloodRift,           // RiftBlade + BloodFrenzy
    BerserkerSmash,      // VoidHammer + BerserkerCore
    PhantomRush,         // PhaseDaggers + SwiftBoots
    RapidShards,         // ShardPistol + QuickHands
    StormScatter,        // RiftShotgun + ChainLightning
    EntropyBeam,         // VoidBeam + EntropyAnchor
    EntropyDrain,        // EntropyScythe + EntropySponge
    ChainReaction,       // ChainWhip + ChainLightning
    DimensionalBarrage,  // DimLauncher + DimensionalEcho
    COUNT
};

struct SynergyData {
    SynergyID id;
    const char* name;
    const char* description;
    RelicID relicA;
    RelicID relicB;
    WeaponID requiredWeapon = WeaponID::COUNT; // COUNT = no weapon required (relic-only synergy)
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

    // Weapon-Relic synergy checks (need current weapon to evaluate)
    static bool isWeaponSynergyActive(const RelicComponent& relics, SynergyID id, WeaponID melee, WeaponID ranged);
    static int getActiveWeaponSynergyCount(const RelicComponent& relics, WeaponID melee, WeaponID ranged);

    // BloodRift: RiftBlade + BloodFrenzy — melee kills under 50% HP heal 5
    static float getBloodRiftHeal(const RelicComponent& relics, WeaponID melee, float hpPercent);
    // BerserkerSmash: VoidHammer + BerserkerCore — +50% charged AoE radius, +0.5s stun
    static float getBerserkerSmashRadiusMult(const RelicComponent& relics, WeaponID melee);
    static float getBerserkerSmashStunBonus(const RelicComponent& relics, WeaponID melee);
    // PhantomRush: PhaseDaggers + SwiftBoots — crit every 4th hit, +10% movespeed
    static int getPhantomRushCritThreshold(const RelicComponent& relics, WeaponID melee);
    static float getPhantomRushSpeedBonus(const RelicComponent& relics, WeaponID melee);
    // RapidShards: ShardPistol + QuickHands — +30% proj speed, 25% double-shot
    static float getRapidShardsSpeedMult(const RelicComponent& relics, WeaponID ranged);
    static bool rollRapidShardsDoubleShot(const RelicComponent& relics, WeaponID ranged);
    // StormScatter: RiftShotgun + ChainLightning — kills chain to 2 extra enemies
    static bool isStormScatterActive(const RelicComponent& relics, WeaponID ranged);
    // EntropyBeam: VoidBeam + EntropyAnchor — beam reduces entropy instead of gaining
    static bool isEntropyBeamActive(const RelicComponent& relics, WeaponID ranged);
    // EntropyDrain: EntropyScythe + EntropySponge — entropy reduction 5 per hit instead of 2
    static float getEntropyDrainReduction(const RelicComponent& relics, WeaponID melee);
    // ChainReaction: ChainWhip + ChainLightning — whip hits spawn chain lightning bolts
    static bool isChainReactionActive(const RelicComponent& relics, WeaponID melee);
    // DimensionalBarrage: DimLauncher + DimensionalEcho — +50% projectile damage
    static float getDimensionalBarrageDamageMult(const RelicComponent& relics, WeaponID ranged);
};
