#pragma once
#include <string>
#include <vector>
#include <functional>

enum class UpgradeID {
    MaxHP,              // +20 max HP
    MoveSpeed,          // +10% movement speed
    DashCooldown,       // -15% dash cooldown
    MeleeDamage,        // +15% melee damage
    RangedDamage,       // +15% ranged damage
    EntropyResistance,  // -20% passive entropy gain
    EntropyDecay,       // +passive entropy decay
    JumpHeight,         // +10% jump height
    DoubleJump,         // +1 extra jump
    SwitchCooldown,     // -20% dimension switch cooldown
    Armor,              // +10% damage reduction
    ComboMaster,        // +combo damage bonus
    WallSlide,          // Improved wall slide
    CritChance,         // +10% crit chance
    ShardMagnet,        // Increased shard pickup range
    AbilityCooldown,    // -15% ability cooldowns
    AbilityPower,       // +20% ability damage
    ShieldCapacity      // +1 shield hit capacity
};

struct Upgrade {
    UpgradeID id;
    std::string name;
    std::string description;
    int cost;
    int maxLevel;
    int currentLevel = 0;

    bool canPurchase(int shards) const {
        return currentLevel < maxLevel && shards >= getNextCost();
    }

    int getNextCost() const {
        return cost * (currentLevel + 1);
    }
};

class UpgradeSystem {
public:
    UpgradeSystem();

    void init();
    const std::vector<Upgrade>& getUpgrades() const { return m_upgrades; }
    bool purchaseUpgrade(UpgradeID id);
    int getUpgradeLevel(UpgradeID id) const;
    void resetAll();

    // Currency
    int getRiftShards() const { return m_riftShards; }
    void addRiftShards(int amount) { m_riftShards += amount; }
    void setRiftShards(int amount) { m_riftShards = amount; }

    // Apply upgrades to game stats
    float getMaxHPBonus() const;
    float getMoveSpeedMultiplier() const;
    float getDashCooldownMultiplier() const;
    float getMeleeDamageMultiplier() const;
    float getRangedDamageMultiplier() const;
    float getEntropyResistance() const;
    float getEntropyDecay() const;
    float getJumpMultiplier() const;
    int getExtraJumps() const;
    float getSwitchCooldownMultiplier() const;
    float getArmorBonus() const;
    float getComboBonus() const;
    // FIX: WallSlide upgrade was missing getter — upgrade had no gameplay effect
    float getWallSlideSpeedMultiplier() const;
    float getCritChance() const;
    float getShardMagnetRange() const;
    float getAbilityCooldownMultiplier() const;
    float getAbilityPowerMultiplier() const;
    int getShieldCapacityBonus() const;

    // Persistence
    std::string serialize() const;
    void deserialize(const std::string& data);

    // Run stats
    int totalRuns = 0;
    int bestRoomReached = 0;
    int totalEnemiesKilled = 0;
    int totalRiftsRepaired = 0;

    // Milestone rewards (cumulative unlock bonuses)
    struct MilestoneBonus {
        float bonusHP = 0;
        float bonusDamageMult = 1.0f;
        float bonusSpeed = 0;
        int bonusShards = 0; // one-time shard grant per milestone
    };
    MilestoneBonus checkMilestones(); // returns newly unlocked bonuses
    int milestonesUnlocked = 0; // persisted milestone count

    // NG+ tracking (persisted)
    int highestNGPlusCompleted = 0;  // Highest NG+ tier beaten (0 = never won, 1-5 = beaten that tier)
    int getMaxUnlockedNGPlus() const { return highestNGPlusCompleted; }
    void unlockNGPlus(int tier) {
        if (tier > highestNGPlusCompleted && tier <= 5) highestNGPlusCompleted = tier;
    }

    // Run history (leaderboard)
    enum class DeathCause { Unknown = 0, HP, Entropy, Collapse, Speedrun, Victory };
    struct RunRecord {
        int rooms;
        int enemies;
        int rifts;
        int shards;
        int difficulty;
        int bestCombo = 0;
        float runTime = 0;       // seconds
        int playerClass = 0;     // PlayerClass as int
        int deathCause = 0;      // DeathCause as int
    };
    void addRunRecord(int rooms, int enemies, int rifts, int shards, int difficulty,
                      int bestCombo = 0, float runTime = 0, int playerClass = 0, int deathCause = 0);
    static const char* getDeathCauseName(int cause);
    const std::vector<RunRecord>& getRunHistory() const { return m_runHistory; }

private:
    std::vector<Upgrade> m_upgrades;
    int m_riftShards = 0;
    std::vector<RunRecord> m_runHistory; // top 10 runs sorted by rooms
};
