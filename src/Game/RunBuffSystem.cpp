#include "RunBuffSystem.h"
#include <algorithm>

RunBuffSystem::RunBuffSystem() {
    m_buffs = {
        {RunBuffID::MaxHPBoost,        "HP Surge",          "+30 Max HP",                   30,  BuffTier::Common},
        {RunBuffID::CooldownReduction, "Temporal Haste",    "-25% Ability Cooldowns",       35,  BuffTier::Common},
        {RunBuffID::EntropyShield,     "Entropy Damper",    "-30% Entropy Gain",            25,  BuffTier::Common},
        {RunBuffID::CritSurge,         "Critical Edge",     "+20% Crit Chance",             50,  BuffTier::Rare},
        {RunBuffID::DoubleShardGain,   "Shard Doubler",     "2x Shard Gain",                60,  BuffTier::Rare},
        {RunBuffID::DashRefresh,       "Kill Rush",         "Kill = Dash CD Reset",         55,  BuffTier::Rare},
        {RunBuffID::FireWeapon,        "Ember Blade",       "Attacks Burn Enemies",         65,  BuffTier::Rare},
        {RunBuffID::IceWeapon,         "Frost Edge",        "Attacks Slow Enemies",         65,  BuffTier::Rare},
        {RunBuffID::ElectricWeapon,    "Storm Striker",     "Attacks Chain Lightning",      65,  BuffTier::Rare},
        {RunBuffID::VampireBlade,      "Vampire Blade",     "5% Lifesteal on Hit",          90,  BuffTier::Legendary},
        {RunBuffID::ExtraLife,         "Soul Anchor",       "Revive Once at 50% HP",        100, BuffTier::Legendary},
        {RunBuffID::PhantomStep,       "Phantom Step",      "Invincible After Dim-Switch",  80,  BuffTier::Legendary},
    };
    reset();
}

void RunBuffSystem::reset() {
    for (int i = 0; i < static_cast<int>(RunBuffID::COUNT); i++) {
        m_active[i] = false;
    }
    for (auto& b : m_buffs) b.purchased = false;
    m_extraLifeAvailable = false;
}

std::vector<RunBuff> RunBuffSystem::generateShopOffering(int difficulty) {
    std::vector<RunBuff> available;
    for (auto& b : m_buffs) {
        if (b.purchased) continue;
        // Legendary buffs only appear at difficulty 3+
        if (b.tier == BuffTier::Legendary && difficulty < 3) continue;
        // Only one element weapon at a time
        if ((b.id == RunBuffID::FireWeapon || b.id == RunBuffID::IceWeapon || b.id == RunBuffID::ElectricWeapon)
            && hasElementWeapon()) continue;
        available.push_back(b);
    }

    // Shuffle and pick 3-4 items
    for (int i = static_cast<int>(available.size()) - 1; i > 0; i--) {
        int j = std::rand() % (i + 1);
        std::swap(available[i], available[j]);
    }

    int count = std::min(static_cast<int>(available.size()), 3 + (difficulty >= 4 ? 1 : 0));
    available.resize(count);

    // Sort by tier for display
    std::sort(available.begin(), available.end(), [](const RunBuff& a, const RunBuff& b) {
        return static_cast<int>(a.tier) < static_cast<int>(b.tier);
    });

    return available;
}

bool RunBuffSystem::purchase(RunBuffID id, int& shards) {
    for (auto& b : m_buffs) {
        if (b.id == id && !b.purchased && shards >= b.cost) {
            shards -= b.cost;
            b.purchased = true;
            m_active[static_cast<int>(id)] = true;
            if (id == RunBuffID::ExtraLife) m_extraLifeAvailable = true;
            return true;
        }
    }
    return false;
}

SDL_Color RunBuffSystem::getWeaponTint() const {
    if (hasFireWeapon()) return {255, 120, 30, 255};
    if (hasIceWeapon()) return {80, 180, 255, 255};
    if (hasElectricWeapon()) return {255, 255, 80, 255};
    return {255, 255, 255, 255};
}
