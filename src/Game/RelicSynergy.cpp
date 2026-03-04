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
     "Void Hunger +2%/kill instead of +1%",
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
     RelicID::CursedBlade, RelicID::LuckyCoin}
};

const SynergyData& RelicSynergy::getData(SynergyID id) {
    return s_synergyData[static_cast<int>(id)];
}

bool RelicSynergy::isActive(const RelicComponent& relics, SynergyID id) {
    const auto& data = s_synergyData[static_cast<int>(id)];
    return relics.hasRelic(data.relicA) && relics.hasRelic(data.relicB);
}

int RelicSynergy::getActiveSynergyCount(const RelicComponent& relics) {
    int count = 0;
    for (int i = 0; i < static_cast<int>(SynergyID::COUNT); i++) {
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
    if (isActive(relics, SynergyID::VoidCatalyst)) return 0.02f;
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
