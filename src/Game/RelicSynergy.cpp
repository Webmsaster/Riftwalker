#include "RelicSynergy.h"

static const SynergyData s_synergyData[] = {
    {SynergyID::VampiricFrenzy, "Vampiric Frenzy",
     "Kill under 30% HP heals 8 instead of 3",
     RelicID::BloodFrenzy, RelicID::SoulSiphon},

    {SynergyID::ChainThorns, "Chain Thorns",
     "Thorn reflection chains to 2 enemies (10 DMG)",
     RelicID::ThornMail, RelicID::ChainLightning},

    {SynergyID::TemporalEcho, "Temporal Echo",
     "Echo Strike chance 35% instead of 20%",
     RelicID::TimeDilator, RelicID::EchoStrike},

    {SynergyID::PhaseHunter, "Phase Hunter",
     "After dim-switch: next attack deals 2x DMG",
     RelicID::PhaseCloak, RelicID::DimensionalEcho},

    {SynergyID::VoidCatalyst, "Void Catalyst",
     "Void Hunger +1.5%/kill instead of +1%",
     RelicID::VoidHunger, RelicID::BerserkerCore},

    {SynergyID::LuckyPhoenix, "Lucky Phoenix",
     "Revive retains 50% of shards",
     RelicID::LuckyCoin, RelicID::PhoenixFeather},

    {SynergyID::FortifiedSoul, "Fortified Soul",
     "Glass Heart penalty reduced: 1.3x DMG taken",
     RelicID::IronHeart, RelicID::GlassHeart},

    {SynergyID::EntropyVortex, "Entropy Vortex",
     "Kills reduce entropy by 5%",
     RelicID::EntropyAnchor, RelicID::EntropySponge},

    {SynergyID::Quicksilver, "Quicksilver",
     "+20% Dash speed",
     RelicID::SwiftBoots, RelicID::QuickHands},

    {SynergyID::CursedFortune, "Cursed Fortune",
     "+30% Shard Drop instead of 10%",
     RelicID::CursedBlade, RelicID::LuckyCoin},

    {SynergyID::RiftMaster, "Rift Master",
     "Stability +4%/s, stacks persist on switch",
     RelicID::RiftConduit, RelicID::StabilityMatrix},

    {SynergyID::DualNature, "Dual Nature",
     "No HP cost on switch, both dim bonuses active",
     RelicID::DualityGem, RelicID::RiftMantle},

    {SynergyID::VoidEcho, "Void Echo",
     "Residue zone 4s, 20 DMG/s",
     RelicID::VoidResonance, RelicID::DimResidue},

    // Weapon-Relic Synergies
    {SynergyID::BloodRift, "Blood Rift",
     "RiftBlade kills under 50% HP heal 5",
     RelicID::BloodFrenzy, RelicID::None, WeaponID::RiftBlade},

    {SynergyID::BerserkerSmash, "Berserker Smash",
     "VoidHammer charged AoE +50%, stun +0.5s",
     RelicID::BerserkerCore, RelicID::None, WeaponID::VoidHammer},

    {SynergyID::PhantomRush, "Phantom Rush",
     "PhaseDaggers crit every 4th hit, +10% speed",
     RelicID::SwiftBoots, RelicID::None, WeaponID::PhaseDaggers},

    {SynergyID::RapidShards, "Rapid Shards",
     "ShardPistol +30% proj speed, 25% double-shot",
     RelicID::QuickHands, RelicID::None, WeaponID::ShardPistol},

    {SynergyID::StormScatter, "Storm Scatter",
     "RiftShotgun kills chain lightning to 2 enemies",
     RelicID::ChainLightning, RelicID::None, WeaponID::RiftShotgun},

    {SynergyID::EntropyBeam, "Entropy Beam",
     "VoidBeam reduces entropy instead of gaining",
     RelicID::EntropyAnchor, RelicID::None, WeaponID::VoidBeam}
};

const SynergyData& RelicSynergy::getData(SynergyID id) {
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(SynergyID::COUNT)) {
        static const SynergyData empty{};
        return empty;
    }
    return s_synergyData[idx];
}

bool RelicSynergy::isActive(const RelicComponent& relics, SynergyID id) {
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(SynergyID::COUNT)) return false;
    const auto& data = s_synergyData[idx];
    // Weapon-relic synergies: only need the relic (weapon checked separately)
    if (data.requiredWeapon != WeaponID::COUNT) {
        return relics.hasRelic(data.relicA);
    }
    return relics.hasRelic(data.relicA) && relics.hasRelic(data.relicB);
}

bool RelicSynergy::isWeaponSynergyActive(const RelicComponent& relics, SynergyID id, WeaponID melee, WeaponID ranged) {
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(SynergyID::COUNT)) return false;
    const auto& data = s_synergyData[idx];
    if (data.requiredWeapon == WeaponID::COUNT) return false; // Not a weapon synergy
    if (!relics.hasRelic(data.relicA)) return false;
    // Check if player has the required weapon equipped
    return (data.requiredWeapon == melee || data.requiredWeapon == ranged);
}

int RelicSynergy::getActiveWeaponSynergyCount(const RelicComponent& relics, WeaponID melee, WeaponID ranged) {
    int count = 0;
    for (int i = 0; i < static_cast<int>(SynergyID::COUNT); i++) {
        auto sid = static_cast<SynergyID>(i);
        const auto& data = s_synergyData[i];
        if (data.requiredWeapon != WeaponID::COUNT) {
            if (isWeaponSynergyActive(relics, sid, melee, ranged)) count++;
        }
    }
    return count;
}

int RelicSynergy::getActiveSynergyCount(const RelicComponent& relics) {
    int count = 0;
    for (int i = 0; i < static_cast<int>(SynergyID::COUNT); i++) {
        // Skip weapon-relic synergies (they use getActiveWeaponSynergyCount)
        if (s_synergyData[i].requiredWeapon != WeaponID::COUNT) continue;
        if (isActive(relics, static_cast<SynergyID>(i))) count++;
    }
    return count;
}

float RelicSynergy::getKillHealBonus(const RelicComponent& relics, float hpPercent) {
    // VampiricFrenzy: kill under 30% HP heals 8 instead of 3
    if (isActive(relics, SynergyID::VampiricFrenzy) && hpPercent < 0.3f) {
        return 8.0f; // Override normal SoulSiphon 3 HP
    }
    return 0; // No synergy bonus
}

float RelicSynergy::getThornChainDamage(const RelicComponent& relics) {
    if (isActive(relics, SynergyID::ChainThorns)) return 10.0f;
    return 0;
}

float RelicSynergy::getEchoStrikeChance(const RelicComponent& relics) {
    if (isActive(relics, SynergyID::TemporalEcho)) return 0.35f;
    return 0.20f; // Default
}

float RelicSynergy::getPhaseHunterDamageMult(const RelicComponent& relics) {
    if (isActive(relics, SynergyID::PhaseHunter) && relics.phaseHunterBuffActive) {
        return 2.0f;
    }
    return 1.0f;
}

float RelicSynergy::getVoidHungerBonusPerKill(const RelicComponent& relics) {
    if (isActive(relics, SynergyID::VoidCatalyst)) return 0.015f;
    return 0.01f; // Default
}

float RelicSynergy::getPhoenixShardRetain(const RelicComponent& relics) {
    if (isActive(relics, SynergyID::LuckyPhoenix)) return 0.5f;
    return 0;
}

float RelicSynergy::getGlassHeartDamageMult(const RelicComponent& relics) {
    // FortifiedSoul: GlassHeart penalty 2x -> 1.3x
    if (isActive(relics, SynergyID::FortifiedSoul)) return 1.3f;
    return 0; // 0 = no override, use default
}

float RelicSynergy::getKillEntropyReduction(const RelicComponent& relics) {
    // EntropyVortex: kills reduce entropy by 5%
    if (isActive(relics, SynergyID::EntropyVortex)) return 0.05f;
    return 0;
}

float RelicSynergy::getDashSpeedBonus(const RelicComponent& relics) {
    // Quicksilver: +20% dash speed
    if (isActive(relics, SynergyID::Quicksilver)) return 0.20f;
    return 0;
}

float RelicSynergy::getShardDropBonus(const RelicComponent& relics) {
    // CursedFortune: +30% shard drop instead of normal 10%
    if (isActive(relics, SynergyID::CursedFortune)) return 0.30f;
    return 0; // 0 = no override
}

bool RelicSynergy::isRiftMasterActive(const RelicComponent& relics) {
    return isActive(relics, SynergyID::RiftMaster);
}

float RelicSynergy::getStabilityDmgPerSec(const RelicComponent& relics) {
    // RiftMaster: +4% DMG/s instead of 3%
    if (isActive(relics, SynergyID::RiftMaster)) return 0.04f;
    return 0.03f; // Default
}

bool RelicSynergy::isDualNatureActive(const RelicComponent& relics) {
    return isActive(relics, SynergyID::DualNature);
}

float RelicSynergy::getResidueDuration(const RelicComponent& relics) {
    // VoidEcho: 4s instead of 2s
    if (isActive(relics, SynergyID::VoidEcho)) return 4.0f;
    return 2.0f;
}

float RelicSynergy::getResidueDamage(const RelicComponent& relics) {
    // VoidEcho: 20 DMG/s instead of 15
    if (isActive(relics, SynergyID::VoidEcho)) return 20.0f;
    return 15.0f;
}

// --- Weapon-Relic Synergy Queries ---

float RelicSynergy::getBloodRiftHeal(const RelicComponent& relics, WeaponID melee, float hpPercent) {
    // BloodRift: RiftBlade + BloodFrenzy — melee kills under 50% HP heal 5
    if (melee == WeaponID::RiftBlade && isActive(relics, SynergyID::BloodRift) && hpPercent < 0.5f) {
        return 5.0f;
    }
    return 0;
}

float RelicSynergy::getBerserkerSmashRadiusMult(const RelicComponent& relics, WeaponID melee) {
    if (melee == WeaponID::VoidHammer && isActive(relics, SynergyID::BerserkerSmash)) {
        return 1.5f;
    }
    return 1.0f;
}

float RelicSynergy::getBerserkerSmashStunBonus(const RelicComponent& relics, WeaponID melee) {
    if (melee == WeaponID::VoidHammer && isActive(relics, SynergyID::BerserkerSmash)) {
        return 0.5f;
    }
    return 0;
}

int RelicSynergy::getPhantomRushCritThreshold(const RelicComponent& relics, WeaponID melee) {
    if (melee == WeaponID::PhaseDaggers && isActive(relics, SynergyID::PhantomRush)) {
        return 4; // Every 4th hit instead of 5th
    }
    return 5;
}

float RelicSynergy::getPhantomRushSpeedBonus(const RelicComponent& relics, WeaponID melee) {
    if (melee == WeaponID::PhaseDaggers && isActive(relics, SynergyID::PhantomRush)) {
        return 0.10f; // +10% extra movespeed on top of PhaseDaggers base
    }
    return 0;
}

float RelicSynergy::getRapidShardsSpeedMult(const RelicComponent& relics, WeaponID ranged) {
    if (ranged == WeaponID::ShardPistol && isActive(relics, SynergyID::RapidShards)) {
        return 1.3f; // +30% projectile speed
    }
    return 1.0f;
}

bool RelicSynergy::rollRapidShardsDoubleShot(const RelicComponent& relics, WeaponID ranged) {
    if (ranged == WeaponID::ShardPistol && isActive(relics, SynergyID::RapidShards)) {
        float roll = static_cast<float>(std::rand()) / RAND_MAX;
        return roll < 0.25f; // 25% chance
    }
    return false;
}

bool RelicSynergy::isStormScatterActive(const RelicComponent& relics, WeaponID ranged) {
    return ranged == WeaponID::RiftShotgun && isActive(relics, SynergyID::StormScatter);
}

bool RelicSynergy::isEntropyBeamActive(const RelicComponent& relics, WeaponID ranged) {
    return ranged == WeaponID::VoidBeam && isActive(relics, SynergyID::EntropyBeam);
}
