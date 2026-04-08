#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <random>
#include <cstdint>

enum class BuffTier { Common = 0, Rare = 1, Legendary = 2 };

enum class RunBuffID {
    MaxHPBoost,        // +30 max HP
    CooldownReduction, // -25% ability cooldowns
    EntropyShield,     // -30% entropy gain
    CritSurge,         // +20% crit chance
    DoubleShardGain,   // 2x shard gain
    DashRefresh,       // Kill resets dash CD
    FireWeapon,        // Attacks burn enemies
    IceWeapon,         // Attacks slow enemies
    ElectricWeapon,    // Attacks chain lightning
    VampireBlade,      // 5% lifesteal
    ExtraLife,         // Revive once at 50% HP
    PhantomStep,       // Invincible after dim-switch
    COUNT
};

struct RunBuff {
    RunBuffID id = RunBuffID::MaxHPBoost;
    std::string name;
    std::string description;
    int cost = 0;
    BuffTier tier = BuffTier::Common;
    bool purchased = false;
};

class RunBuffSystem {
public:
    RunBuffSystem();

    void reset(); // Clear all buffs for new run

    // Seed the RNG for reproducible runs (e.g. daily run seed)
    void seed(uint32_t s);

    // Shop: generate random selection
    std::vector<RunBuff> generateShopOffering(int difficulty);

    // Purchase
    bool purchase(RunBuffID id, int& shards);

    // Query active buffs
    bool hasBuff(RunBuffID id) const { return m_active[static_cast<int>(id)]; }

    // Stat modifiers
    float getMaxHPBoost() const { return hasBuff(RunBuffID::MaxHPBoost) ? 30.0f : 0.0f; }
    float getAbilityCDMultiplier() const { return hasBuff(RunBuffID::CooldownReduction) ? 0.75f : 1.0f; }
    float getEntropyGainMultiplier() const { return hasBuff(RunBuffID::EntropyShield) ? 0.7f : 1.0f; }
    float getCritBonus() const { return hasBuff(RunBuffID::CritSurge) ? 0.2f : 0.0f; }
    float getShardMultiplier() const { return hasBuff(RunBuffID::DoubleShardGain) ? 2.0f : 1.0f; }
    bool hasDashRefresh() const { return hasBuff(RunBuffID::DashRefresh); }
    bool hasFireWeapon() const { return hasBuff(RunBuffID::FireWeapon); }
    bool hasIceWeapon() const { return hasBuff(RunBuffID::IceWeapon); }
    bool hasElectricWeapon() const { return hasBuff(RunBuffID::ElectricWeapon); }
    float getLifesteal() const { return hasBuff(RunBuffID::VampireBlade) ? 0.05f : 0.0f; }
    bool hasExtraLife() const { return m_extraLifeAvailable; }
    void consumeExtraLife() { m_extraLifeAvailable = false; }
    float getPhantomStepDuration() const { return hasBuff(RunBuffID::PhantomStep) ? 0.5f : 0.0f; }

    // Element weapon color
    SDL_Color getWeaponTint() const;
    bool hasElementWeapon() const { return hasFireWeapon() || hasIceWeapon() || hasElectricWeapon(); }

    const std::vector<RunBuff>& getAllBuffs() const { return m_buffs; }

private:
    std::vector<RunBuff> m_buffs;
    bool m_active[static_cast<int>(RunBuffID::COUNT)] = {};
    bool m_extraLifeAvailable = false;
    std::mt19937 m_rng{std::random_device{}()};
};
