#include "WeaponSystem.h"
#include "Core/Localization.h"
#include <vector>

static std::vector<WeaponData> s_weapons;
static bool s_initialized = false;

void WeaponSystem::init() {
    if (s_initialized) return;
    s_initialized = true;
    s_weapons.resize(static_cast<int>(WeaponID::COUNT));

    // Melee weapons
    s_weapons[static_cast<int>(WeaponID::RiftBlade)] = {
        WeaponID::RiftBlade, "Rift Blade", "Standard combo blade",
        15.0f, 0.35f, 48.0f, 200.0f, 0.2f, 0, true, true
    };
    s_weapons[static_cast<int>(WeaponID::VoidHammer)] = {
        WeaponID::VoidHammer, "Void Hammer", "Slow but massive AoE",
        25.0f, 0.7f, 56.0f, 350.0f, 0.35f, 0, true, false
    };
    s_weapons[static_cast<int>(WeaponID::PhaseDaggers)] = {
        WeaponID::PhaseDaggers, "Phase Daggers", "Fast + crit every 5th hit",
        8.0f, 0.18f, 36.0f, 120.0f, 0.12f, 0.2f, true, false
    };
    s_weapons[static_cast<int>(WeaponID::EntropyScythe)] = {
        WeaponID::EntropyScythe, "Entropy Scythe", "Wide arc, reduces entropy per hit",
        18.0f, 0.45f, 56.0f, 250.0f, 0.3f, 0, true, false
    };
    s_weapons[static_cast<int>(WeaponID::ChainWhip)] = {
        WeaponID::ChainWhip, "Chain Whip", "Piercing line attack, rapid strikes",
        6.0f, 0.25f, 72.0f, 100.0f, 0.15f, 0, true, false
    };

    s_weapons[static_cast<int>(WeaponID::GravityGauntlet)] = {
        WeaponID::GravityGauntlet, "Gravity Gauntlet", "Pulls enemies toward you on hit",
        12.0f, 0.4f, 44.0f, -150.0f, 0.25f, 0, true, false  // Negative knockback = pull
    };

    // Ranged weapons
    s_weapons[static_cast<int>(WeaponID::ShardPistol)] = {
        WeaponID::ShardPistol, "Shard Pistol", "Standard projectile",
        10.0f, 0.4f, 300.0f, 80.0f, 0.15f, 0, false, true
    };
    s_weapons[static_cast<int>(WeaponID::RiftShotgun)] = {
        WeaponID::RiftShotgun, "Rift Shotgun", "5 pellets, short range",
        4.0f, 0.65f, 150.0f, 180.0f, 0.2f, 0, false, false
    };
    s_weapons[static_cast<int>(WeaponID::VoidBeam)] = {
        WeaponID::VoidBeam, "Void Beam", "Continuous beam, pierces",
        3.0f, 0.05f, 200.0f, 20.0f, 0.05f, 0, false, false
    };
    s_weapons[static_cast<int>(WeaponID::GrapplingHook)] = {
        WeaponID::GrapplingHook, "Grappling Hook", "Hook to walls or pull enemies",
        20.0f, 1.5f, 300.0f, 150.0f, 0.1f, 0, false, false
    };
    s_weapons[static_cast<int>(WeaponID::DimLauncher)] = {
        WeaponID::DimLauncher, "Dimensional Launcher", "Slow projectile hits both dimensions",
        30.0f, 1.2f, 350.0f, 120.0f, 0.25f, 0, false, false
    };
    s_weapons[static_cast<int>(WeaponID::RiftCrossbow)] = {
        WeaponID::RiftCrossbow, "Rift Crossbow", "Piercing bolts through multiple enemies",
        22.0f, 0.8f, 280.0f, 100.0f, 0.2f, 0, false, false
    };
}

const WeaponData& WeaponSystem::getWeaponData(WeaponID id) {
    init();
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(s_weapons.size())) return s_weapons[idx];
    return s_weapons[0];
}

const char* WeaponSystem::getWeaponName(WeaponID id) {
    return getWeaponData(id).name;
}

bool WeaponSystem::isMelee(WeaponID id) {
    return getWeaponData(id).isMelee;
}

WeaponID WeaponSystem::nextMelee(WeaponID current) {
    init();
    int start = static_cast<int>(current);
    for (int i = 1; i < static_cast<int>(WeaponID::COUNT); i++) {
        int idx = (start + i) % static_cast<int>(WeaponID::COUNT);
        if (s_weapons[idx].isMelee && s_weapons[idx].unlocked) {
            return static_cast<WeaponID>(idx);
        }
    }
    return current;
}

WeaponID WeaponSystem::nextRanged(WeaponID current) {
    init();
    int start = static_cast<int>(current);
    for (int i = 1; i < static_cast<int>(WeaponID::COUNT); i++) {
        int idx = (start + i) % static_cast<int>(WeaponID::COUNT);
        if (!s_weapons[idx].isMelee && s_weapons[idx].unlocked) {
            return static_cast<WeaponID>(idx);
        }
    }
    return current;
}

bool WeaponSystem::isUnlocked(WeaponID id) {
    init();
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(s_weapons.size())) return false;
    return s_weapons[idx].unlocked;
}

void WeaponSystem::unlock(WeaponID id) {
    init();
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(s_weapons.size())) return;
    s_weapons[idx].unlocked = true;
}

void WeaponSystem::resetUnlocks() {
    init();
    for (auto& w : s_weapons) w.unlocked = false;
    // Defaults always unlocked
    s_weapons[static_cast<int>(WeaponID::RiftBlade)].unlocked = true;
    s_weapons[static_cast<int>(WeaponID::ShardPistol)].unlocked = true;
}

const char* WeaponSystem::getUnlockRequirement(WeaponID id) {
    switch (id) {
        case WeaponID::RiftBlade:       return LOC("weapon.unlock.riftblade");
        case WeaponID::ShardPistol:     return LOC("weapon.unlock.shardpistol");
        case WeaponID::PhaseDaggers:    return LOC("weapon.unlock.phasedaggers");
        case WeaponID::RiftShotgun:     return LOC("weapon.unlock.riftshotgun");
        case WeaponID::VoidHammer:      return LOC("weapon.unlock.voidhammer");
        case WeaponID::VoidBeam:        return LOC("weapon.unlock.voidbeam");
        case WeaponID::GrapplingHook:   return LOC("weapon.unlock.grapplinghook");
        case WeaponID::EntropyScythe:   return LOC("weapon.unlock.entropyscythe");
        case WeaponID::ChainWhip:       return LOC("weapon.unlock.chainwhip");
        case WeaponID::DimLauncher:     return LOC("weapon.unlock.dimlauncher");
        case WeaponID::GravityGauntlet: return LOC("weapon.unlock.gravitygauntlet");
        case WeaponID::RiftCrossbow:    return LOC("weapon.unlock.riftcrossbow");
        default: return LOC("weapon.unlock.unknown");
    }
}

MasteryTier WeaponSystem::getMasteryTier(int kills) {
    if (kills >= 50) return MasteryTier::Mastered;
    if (kills >= 25) return MasteryTier::Proficient;
    if (kills >= 10) return MasteryTier::Familiar;
    return MasteryTier::None;
}

MasteryBonus WeaponSystem::getMasteryBonus(int kills) {
    MasteryBonus bonus;
    switch (getMasteryTier(kills)) {
        case MasteryTier::Mastered:   bonus.damageMult = 1.30f; bonus.cooldownMult = 0.85f; break;
        case MasteryTier::Proficient: bonus.damageMult = 1.20f; bonus.cooldownMult = 0.90f; break;
        case MasteryTier::Familiar:   bonus.damageMult = 1.10f; bonus.cooldownMult = 1.00f; break;
        default: break;
    }
    return bonus;
}

const char* WeaponSystem::getMasteryTierName(MasteryTier tier) {
    switch (tier) {
        case MasteryTier::Familiar:   return LOC("mastery.familiar");
        case MasteryTier::Proficient: return LOC("mastery.proficient");
        case MasteryTier::Mastered:   return LOC("mastery.mastered");
        default: return "";
    }
}

int WeaponSystem::getNextTierThreshold(int kills) {
    if (kills < 10) return 10;
    if (kills < 25) return 25;
    if (kills < 50) return 50;
    return 0; // Already mastered
}
