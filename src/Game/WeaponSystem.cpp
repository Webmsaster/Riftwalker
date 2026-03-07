#include "WeaponSystem.h"
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
        case MasteryTier::Familiar:   return "Familiar";
        case MasteryTier::Proficient: return "Proficient";
        case MasteryTier::Mastered:   return "Mastered";
        default: return "";
    }
}

int WeaponSystem::getNextTierThreshold(int kills) {
    if (kills < 10) return 10;
    if (kills < 25) return 25;
    if (kills < 50) return 50;
    return 0; // Already mastered
}
