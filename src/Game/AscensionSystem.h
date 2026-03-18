#pragma once
#include <string>

struct AscensionLevel {
    const char* bonusDesc = "";
    const char* malusDesc = "";
    float enemyHPMult = 1.0f;
    float enemyDMGMult = 1.0f;
    float enemySpeedMult = 1.0f;
    float eliteSpawnMult = 1.0f;
    float trapDensityMult = 1.0f;
    float startShardBonus = 0;
    float shardGainMult = 1.0f;
    float shopDiscountMult = 1.0f;
    int extraRelicChoices = 0;
    int startRelicTier = -1;  // -1=none, 0=common, 2=legendary
    bool keepWeaponUpgrade = false;
    bool extraShopSlot = false;
    bool abilitiesStartHalfCD = false;
    bool bossExtraPhase = false;     // Bosses get a 4th phase at 15% HP
};

class AscensionSystem {
public:
    static void init();
    static const AscensionLevel& getLevel(int level);
    static int getMaxLevel();

    // Persistent state
    static int currentLevel;
    static int riftCores;       // Prestige currency

    static void save(const std::string& filepath);
    static void load(const std::string& filepath);
};
