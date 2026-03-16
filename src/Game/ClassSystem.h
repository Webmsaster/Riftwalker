#pragma once
#include <SDL2/SDL.h>

enum class PlayerClass {
    Voidwalker = 0,
    Berserker,
    Phantom,
    Technomancer,
    COUNT
};

struct ClassData {
    PlayerClass id;
    const char* name;
    const char* description;
    const char* passiveName;
    const char* passiveDesc;
    const char* abilityMod;
    SDL_Color color;
    float baseHP;
    float baseSpeed;
    // Passive params
    float switchHeal;         // Voidwalker: HP healed on dim-switch
    float switchCDReduction;  // Voidwalker: switch cooldown multiplier
    float lowHPThreshold;     // Berserker: HP% threshold for Blood Rage
    float rageDmgBonus;       // Berserker: DMG multiplier when Blood Rage active
    float rageAtkSpeedBonus;  // Berserker: attack speed bonus in Blood Rage
    float dashLengthMult;     // Phantom: dash duration multiplier
    float postDashInvisTime;  // Phantom: invisibility after dash
    // Technomancer params
    float turretDamageMult;   // Technomancer: turret/trap damage multiplier (Construct Mastery)
    float turretDurationMult; // Technomancer: turret/trap duration multiplier
    float rangedDmgBonus;     // Technomancer: +10% ranged damage
};

class ClassSystem {
public:
    static const ClassData& getData(PlayerClass pc);
    static const ClassData& getData(int index);
    static constexpr int CLASS_COUNT = static_cast<int>(PlayerClass::COUNT);

    // Unlock system
    static bool isUnlocked(PlayerClass pc);
    static void unlock(PlayerClass pc);
    static void initUnlocks(); // Call at startup, sets Voidwalker unlocked
    static const char* getUnlockRequirement(PlayerClass pc);

    // Persist via UpgradeSystem serialize
    static bool s_classUnlocked[CLASS_COUNT];
};

// Global selected class (like g_selectedDifficulty)
inline PlayerClass g_selectedClass = PlayerClass::Voidwalker;
