#include "ClassSystem.h"

static const ClassData s_classData[] = {
    // Voidwalker - balanced, dimension-focused
    {
        PlayerClass::Voidwalker,
        "Voidwalker",
        "Master of dimensional rifts. Switching dimensions heals and recharges faster.",
        "Dimensional Affinity",
        "Dim-switch heals 3 HP, -30% switch cooldown, +30% DMG for 2.5s",
        "Phase Strike: +50% range",
        {60, 120, 220, 255},  // Blue
        100.0f,   // HP
        250.0f,   // Speed
        3.0f,     // switchHeal
        0.7f,     // switchCDReduction (30% less)
        0.0f, 0.0f, 0.0f,  // no berserker
        0.0f, 0.0f          // no phantom
    },
    // Berserker - tanky, high damage at low HP
    {
        PlayerClass::Berserker,
        "Berserker",
        "Raging warrior. Grows stronger as health drops. Ground Slam devastates.",
        "Blood Rage",
        "Under 40% HP: +30% DMG, +20% attack speed",
        "Ground Slam: +40% radius, +0.4s stun",
        {220, 60, 60, 255},  // Red
        120.0f,   // HP
        212.0f,   // Speed
        0.0f, 0.0f,         // no voidwalker
        0.4f,     // lowHPThreshold (40%)
        1.3f,     // rageDmgBonus (+30%)
        1.2f,     // rageAtkSpeedBonus (+20%)
        0.0f, 0.0f          // no phantom
    },
    // Phantom - fast, evasive
    {
        PlayerClass::Phantom,
        "Phantom",
        "Swift shadow. Phases through enemies during dash. Rift Shield enhanced.",
        "Shadow Step",
        "Dash 50% longer, phase through enemies, 0.5s invisible after",
        "Rift Shield: +1 max hit, speed boost on absorb",
        {60, 220, 200, 255}, // Cyan
        90.0f,    // HP
        300.0f,   // Speed
        0.0f, 0.0f,         // no voidwalker
        0.0f, 0.0f, 0.0f,   // no berserker
        1.5f,     // dashLengthMult (50% longer)
        0.5f      // postDashInvisTime
    }
};

const ClassData& ClassSystem::getData(PlayerClass pc) {
    int idx = static_cast<int>(pc);
    if (idx < 0 || idx >= CLASS_COUNT) idx = 0;
    return s_classData[idx];
}

const ClassData& ClassSystem::getData(int index) {
    if (index < 0 || index >= CLASS_COUNT) index = 0;
    return s_classData[index];
}
