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
    PlayerClass id = PlayerClass::Voidwalker;
    const char* name = "";
    const char* description = "";
    const char* passiveName = "";
    const char* passiveDesc = "";
    const char* abilityMod = "";
    SDL_Color color{255, 255, 255, 255};
    float baseHP = 100.0f;
    float baseSpeed = 250.0f;
    // Passive params
    float switchHeal = 0;         // Voidwalker: HP healed on dim-switch
    float switchCDReduction = 1.0f; // Voidwalker: switch cooldown multiplier
    float lowHPThreshold = 0;     // Berserker: HP% threshold for Blood Rage
    float rageDmgBonus = 1.0f;    // Berserker: DMG multiplier when Blood Rage active
    float rageAtkSpeedBonus = 1.0f; // Berserker: attack speed bonus in Blood Rage
    float dashLengthMult = 1.0f;  // Phantom: dash duration multiplier
    float postDashInvisTime = 0;  // Phantom: invisibility after dash
    // Technomancer params
    float turretDamageMult = 1.0f; // Technomancer: turret/trap damage multiplier (Construct Mastery)
    float turretDurationMult = 1.0f; // Technomancer: turret/trap duration multiplier
    float rangedDmgBonus = 1.0f;  // Technomancer: +10% ranged damage
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
