#include "UpgradeSystem.h"
#include "ClassSystem.h"
#include "WeaponSystem.h"
#include <sstream>
#include <algorithm>

UpgradeSystem::UpgradeSystem() {
    init();
}

void UpgradeSystem::init() {
    // BALANCE: All upgrade costs reduced ~25-30% so players can buy 1-2 per level
    m_upgrades = {
        {UpgradeID::MaxHP, "Reinforced Suit", "+20 Max HP", 35, 5},            // was 50
        {UpgradeID::MoveSpeed, "Quantum Boots", "+10% Move Speed", 30, 5},     // was 40
        {UpgradeID::DashCooldown, "Phase Thrusters", "-15% Dash Cooldown", 45, 4}, // was 60
        {UpgradeID::MeleeDamage, "Rift Blade", "+15% Melee Damage", 40, 5},    // was 55
        {UpgradeID::RangedDamage, "Void Projector", "+15% Ranged Damage", 40, 5}, // was 55
        {UpgradeID::EntropyResistance, "Entropy Shield", "-20% Entropy Gain", 55, 5}, // was 80
        {UpgradeID::EntropyDecay, "Auto-Stabilizer", "+Passive Entropy Decay", 70, 3}, // was 100
        {UpgradeID::JumpHeight, "Gravity Modulator", "+10% Jump Height", 35, 4}, // was 45
        {UpgradeID::DoubleJump, "Air Dash Module", "+1 Extra Jump", 85, 2},     // was 120
        {UpgradeID::SwitchCooldown, "Rift Capacitor", "-20% Switch Cooldown", 50, 4}, // was 70
        {UpgradeID::Armor, "Dimensional Plating", "+10% Damage Reduction", 45, 5}, // was 65
        {UpgradeID::ComboMaster, "Combo Amplifier", "+Combo Damage Bonus", 65, 3}, // was 90
        {UpgradeID::WallSlide, "Grip Enhancer", "Better Wall Slide Control", 35, 2}, // was 50
        {UpgradeID::CritChance, "Rift Resonance", "+10% Critical Chance", 55, 3}, // was 75
        {UpgradeID::ShardMagnet, "Shard Magnet", "+Pickup Range", 25, 3},       // was 35
        {UpgradeID::AbilityCooldown, "Temporal Flux", "-15% Ability Cooldowns", 55, 3}, // was 80
        {UpgradeID::AbilityPower, "Rift Amplifier", "+20% Ability Damage", 65, 3}, // was 90
        {UpgradeID::ShieldCapacity, "Barrier Core", "+1 Shield Capacity", 70, 2}, // was 100
    };
}

bool UpgradeSystem::purchaseUpgrade(UpgradeID id) {
    for (auto& u : m_upgrades) {
        if (u.id == id && u.canPurchase(m_riftShards)) {
            m_riftShards -= u.getNextCost();
            u.currentLevel++;
            return true;
        }
    }
    return false;
}

int UpgradeSystem::getUpgradeLevel(UpgradeID id) const {
    for (auto& u : m_upgrades) {
        if (u.id == id) return u.currentLevel;
    }
    return 0;
}

void UpgradeSystem::resetAll() {
    for (auto& u : m_upgrades) u.currentLevel = 0;
    m_riftShards = 0;
    totalRuns = 0;
    bestRoomReached = 0;
    totalEnemiesKilled = 0;
    totalRiftsRepaired = 0;
}

float UpgradeSystem::getMaxHPBonus() const { return getUpgradeLevel(UpgradeID::MaxHP) * 20.0f; }
float UpgradeSystem::getMoveSpeedMultiplier() const { return 1.0f + getUpgradeLevel(UpgradeID::MoveSpeed) * 0.05f; }
float UpgradeSystem::getDashCooldownMultiplier() const { return 1.0f - getUpgradeLevel(UpgradeID::DashCooldown) * 0.15f; }
float UpgradeSystem::getMeleeDamageMultiplier() const { return 1.0f + getUpgradeLevel(UpgradeID::MeleeDamage) * 0.15f; }
float UpgradeSystem::getRangedDamageMultiplier() const { return 1.0f + getUpgradeLevel(UpgradeID::RangedDamage) * 0.15f; }
float UpgradeSystem::getEntropyResistance() const { return getUpgradeLevel(UpgradeID::EntropyResistance) * 0.2f; }
float UpgradeSystem::getEntropyDecay() const { return getUpgradeLevel(UpgradeID::EntropyDecay) * 2.0f; }
float UpgradeSystem::getJumpMultiplier() const { return 1.0f + getUpgradeLevel(UpgradeID::JumpHeight) * 0.1f; }
int UpgradeSystem::getExtraJumps() const { return getUpgradeLevel(UpgradeID::DoubleJump); }
float UpgradeSystem::getSwitchCooldownMultiplier() const { return 1.0f - getUpgradeLevel(UpgradeID::SwitchCooldown) * 0.2f; }
float UpgradeSystem::getArmorBonus() const { return getUpgradeLevel(UpgradeID::Armor) * 0.1f; }
float UpgradeSystem::getComboBonus() const { return getUpgradeLevel(UpgradeID::ComboMaster) * 0.1f; }
// FIX: WallSlide upgrade had no getter — players could buy it but it did nothing
float UpgradeSystem::getWallSlideSpeedMultiplier() const { return 1.0f - getUpgradeLevel(UpgradeID::WallSlide) * 0.25f; }
float UpgradeSystem::getCritChance() const { return getUpgradeLevel(UpgradeID::CritChance) * 0.1f; }
float UpgradeSystem::getShardMagnetRange() const { return 14.0f + getUpgradeLevel(UpgradeID::ShardMagnet) * 30.0f; }
float UpgradeSystem::getAbilityCooldownMultiplier() const { return 1.0f - getUpgradeLevel(UpgradeID::AbilityCooldown) * 0.15f; }
float UpgradeSystem::getAbilityPowerMultiplier() const { return 1.0f + getUpgradeLevel(UpgradeID::AbilityPower) * 0.2f; }
int UpgradeSystem::getShieldCapacityBonus() const { return getUpgradeLevel(UpgradeID::ShieldCapacity); }

void UpgradeSystem::addRunRecord(int rooms, int enemies, int rifts, int shards, int difficulty,
                                  int bestCombo, float runTime, int playerClass, int deathCause) {
    RunRecord record{rooms, enemies, rifts, shards, difficulty, bestCombo, runTime, playerClass, deathCause};
    m_runHistory.push_back(record);
    // Sort by rooms descending, keep top 10
    std::sort(m_runHistory.begin(), m_runHistory.end(),
        [](const RunRecord& a, const RunRecord& b) { return a.rooms > b.rooms; });
    if (m_runHistory.size() > 10) m_runHistory.resize(10);
}

const char* UpgradeSystem::getDeathCauseName(int cause) {
    switch (cause) {
        case 1: return "HP Depleted";
        case 2: return "Entropy Overload";
        case 3: return "Rift Collapse";
        case 4: return "Time Expired";
        case 5: return "Victory!";
        default: return "Unknown";
    }
}

UpgradeSystem::MilestoneBonus UpgradeSystem::checkMilestones() {
    MilestoneBonus bonus;
    int oldCount = milestonesUnlocked;
    int newCount = 0;

    // Milestone 1: 10 runs = +10 max HP
    if (totalRuns >= 10) newCount = 1;
    // Milestone 2: 50 runs = +5% damage
    if (totalRuns >= 50) newCount = 2;
    // Milestone 3: 100 kills = +50 shards
    if (totalEnemiesKilled >= 100) newCount = std::max(newCount, 3);
    // Milestone 4: 500 kills = +5% speed
    if (totalEnemiesKilled >= 500) newCount = std::max(newCount, 4);
    // Milestone 5: 1000 kills = +10% damage
    if (totalEnemiesKilled >= 1000) newCount = std::max(newCount, 5);
    // Milestone 6: reach floor 5 = +15 HP
    if (bestRoomReached >= 5) newCount = std::max(newCount, 6);
    // Milestone 7: reach floor 10 = +100 shards
    if (bestRoomReached >= 10) newCount = std::max(newCount, 7);
    // Milestone 8: 100 runs = +10% damage
    if (totalRuns >= 100) newCount = std::max(newCount, 8);

    // Only grant new milestones
    if (newCount > oldCount) {
        for (int i = oldCount + 1; i <= newCount; i++) {
            switch (i) {
                case 1: bonus.bonusHP += 10; break;
                case 2: bonus.bonusDamageMult *= 1.05f; break;
                case 3: bonus.bonusShards += 50; break;
                case 4: bonus.bonusSpeed += 0.05f; break;
                case 5: bonus.bonusDamageMult *= 1.10f; break;
                case 6: bonus.bonusHP += 15; break;
                case 7: bonus.bonusShards += 100; break;
                case 8: bonus.bonusDamageMult *= 1.10f; break;
            }
        }
        milestonesUnlocked = newCount;
    }

    return bonus;
}

std::string UpgradeSystem::serialize() const {
    std::ostringstream ss;
    ss << m_riftShards << " " << totalRuns << " " << bestRoomReached << " "
       << totalEnemiesKilled << " " << totalRiftsRepaired << " ";
    for (auto& u : m_upgrades) {
        ss << u.currentLevel << " ";
    }
    // Serialize run history (extended format)
    ss << static_cast<int>(m_runHistory.size()) << " ";
    for (auto& r : m_runHistory) {
        ss << r.rooms << " " << r.enemies << " " << r.rifts << " "
           << r.shards << " " << r.difficulty << " "
           << r.bestCombo << " " << r.runTime << " "
           << r.playerClass << " " << r.deathCause << " ";
    }
    // Serialize class unlocks (all CLASS_COUNT slots)
    for (int i = 0; i < ClassSystem::CLASS_COUNT; i++) {
        ss << (ClassSystem::s_classUnlocked[i] ? 1 : 0) << " ";
    }
    // Serialize milestones
    ss << milestonesUnlocked << " ";
    // Serialize weapon unlocks
    int weaponCount = static_cast<int>(WeaponID::COUNT);
    ss << weaponCount << " ";
    for (int i = 0; i < weaponCount; i++) {
        ss << (WeaponSystem::isUnlocked(static_cast<WeaponID>(i)) ? 1 : 0) << " ";
    }
    // NG+ progress (appended for backward compatibility)
    ss << highestNGPlusCompleted << " ";
    return ss.str();
}

void UpgradeSystem::deserialize(const std::string& data) {
    std::istringstream ss(data);
    if (!(ss >> m_riftShards >> totalRuns >> bestRoomReached
           >> totalEnemiesKilled >> totalRiftsRepaired)) {
        // Corrupted or empty save — keep defaults
        m_riftShards = 0;
        totalRuns = 0;
        bestRoomReached = 0;
        totalEnemiesKilled = 0;
        totalRiftsRepaired = 0;
        return;
    }
    if (m_riftShards < 0) m_riftShards = 0;
    if (totalRuns < 0) totalRuns = 0;
    if (bestRoomReached < 0) bestRoomReached = 0;
    if (bestRoomReached > 99) bestRoomReached = 99;
    if (totalEnemiesKilled < 0) totalEnemiesKilled = 0;
    if (totalRiftsRepaired < 0) totalRiftsRepaired = 0;
    for (auto& u : m_upgrades) {
        if (!(ss >> u.currentLevel)) break;
        if (u.currentLevel < 0) u.currentLevel = 0;
        if (u.currentLevel > u.maxLevel) u.currentLevel = u.maxLevel;
    }
    // Deserialize run history (extended format, backwards-compatible)
    int histCount = 0;
    if (ss >> histCount) {
        m_runHistory.clear();
        for (int i = 0; i < histCount && i < 10; i++) {
            RunRecord r{};
            if (ss >> r.rooms >> r.enemies >> r.rifts >> r.shards >> r.difficulty) {
                // Extended fields may not exist in old saves
                ss >> r.bestCombo >> r.runTime >> r.playerClass >> r.deathCause;
                m_runHistory.push_back(r);
            }
        }
    }
    // Deserialize class unlocks (CLASS_COUNT slots, backwards-compatible with old 3-slot saves)
    ClassSystem::initUnlocks();
    for (int i = 0; i < ClassSystem::CLASS_COUNT; i++) {
        int val = 0;
        if (ss >> val) {
            ClassSystem::s_classUnlocked[i] = (val != 0);
        }
    }
    ClassSystem::s_classUnlocked[0] = true; // Voidwalker always unlocked
    // Deserialize milestones
    int ms = 0;
    if (ss >> ms) milestonesUnlocked = std::max(0, std::min(ms, 8));
    // Deserialize weapon unlocks (always apply defaults first, then override from save)
    WeaponSystem::resetUnlocks();
    int weaponCount = 0;
    if (ss >> weaponCount) {
        int maxWeapons = static_cast<int>(WeaponID::COUNT);
        for (int i = 0; i < weaponCount && i < maxWeapons; i++) {
            int val = 0;
            if (ss >> val && val != 0) {
                WeaponSystem::unlock(static_cast<WeaponID>(i));
            }
        }
    }
    // NG+ progress (optional for backward compatibility with old saves)
    int ngp = 0;
    if (ss >> ngp) {
        highestNGPlusCompleted = std::max(0, std::min(ngp, 10));
    }
}
