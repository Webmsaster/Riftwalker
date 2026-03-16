#pragma once
#include <string>

enum class WeaponID {
    // Melee
    RiftBlade = 0,   // Default melee: 15 DMG, normal speed, standard combo
    VoidHammer,       // 25 DMG, slow, charged: 3x3 AoE shockwave
    PhaseDaggers,     // 8 DMG, very fast, every 5th hit = crit, +20% movespeed
    // Ranged
    ShardPistol,      // Default ranged: 10 DMG, normal
    RiftShotgun,      // 5x4 DMG, slow, 5 projectiles in fan, short range
    VoidBeam,         // 3 DMG/tick, continuous, pierces enemies, high entropy gain
    GrapplingHook,    // 20 DMG, hooks to walls (swing) or enemies (pull), 300px range
    COUNT
};

enum class MasteryTier { None = 0, Familiar, Proficient, Mastered };

struct MasteryBonus {
    float damageMult = 1.0f;
    float cooldownMult = 1.0f;
};

struct WeaponData {
    WeaponID id = WeaponID::RiftBlade;
    const char* name = "";
    const char* description = "";
    float damage = 10.0f;
    float cooldown = 0.4f;
    float range = 48.0f;
    float knockback = 200.0f;
    float duration = 0.2f;
    float speedModifier = 0;  // Added to player movespeed (%)
    bool isMelee = true;
    bool unlocked = false;    // Meta-progression: stays unlocked across runs
};

class WeaponSystem {
public:
    static void init();
    static const WeaponData& getWeaponData(WeaponID id);
    static const char* getWeaponName(WeaponID id);
    static bool isMelee(WeaponID id);

    // Cycle through melee/ranged weapons
    static WeaponID nextMelee(WeaponID current);
    static WeaponID nextRanged(WeaponID current);

    // Unlock tracking
    static bool isUnlocked(WeaponID id);
    static void unlock(WeaponID id);
    static void resetUnlocks(); // For new save
    static const char* getUnlockRequirement(WeaponID id);

    // Weapon Mastery (per-run progression)
    static MasteryTier getMasteryTier(int kills);
    static MasteryBonus getMasteryBonus(int kills);
    static const char* getMasteryTierName(MasteryTier tier);
    static int getNextTierThreshold(int kills);
};
