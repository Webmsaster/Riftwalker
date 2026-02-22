#include "UpgradeSystem.h"
#include <sstream>
#include <algorithm>

UpgradeSystem::UpgradeSystem() {
    init();
}

void UpgradeSystem::init() {
    m_upgrades = {
        {UpgradeID::MaxHP, "Reinforced Suit", "+20 Max HP", 50, 5},
        {UpgradeID::MoveSpeed, "Quantum Boots", "+10% Move Speed", 40, 5},
        {UpgradeID::DashCooldown, "Phase Thrusters", "-15% Dash Cooldown", 60, 4},
        {UpgradeID::MeleeDamage, "Rift Blade", "+15% Melee Damage", 55, 5},
        {UpgradeID::RangedDamage, "Void Projector", "+15% Ranged Damage", 55, 5},
        {UpgradeID::EntropyResistance, "Entropy Shield", "-20% Entropy Gain", 80, 5},
        {UpgradeID::EntropyDecay, "Auto-Stabilizer", "+Passive Entropy Decay", 100, 3},
        {UpgradeID::JumpHeight, "Gravity Modulator", "+10% Jump Height", 45, 4},
        {UpgradeID::DoubleJump, "Air Dash Module", "+1 Extra Jump", 120, 2},
        {UpgradeID::SwitchCooldown, "Rift Capacitor", "-20% Switch Cooldown", 70, 4},
        {UpgradeID::Armor, "Dimensional Plating", "+10% Damage Reduction", 65, 5},
        {UpgradeID::ComboMaster, "Combo Amplifier", "+Combo Damage Bonus", 90, 3},
        {UpgradeID::WallSlide, "Grip Enhancer", "Better Wall Slide Control", 50, 2},
        {UpgradeID::CritChance, "Rift Resonance", "+10% Critical Chance", 75, 3},
        {UpgradeID::ShardMagnet, "Shard Magnet", "+Pickup Range", 35, 3},
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
float UpgradeSystem::getMoveSpeedMultiplier() const { return 1.0f + getUpgradeLevel(UpgradeID::MoveSpeed) * 0.1f; }
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
float UpgradeSystem::getCritChance() const { return getUpgradeLevel(UpgradeID::CritChance) * 0.1f; }
float UpgradeSystem::getShardMagnetRange() const { return 14.0f + getUpgradeLevel(UpgradeID::ShardMagnet) * 30.0f; }

void UpgradeSystem::addRunRecord(int rooms, int enemies, int rifts, int shards, int difficulty) {
    RunRecord record{rooms, enemies, rifts, shards, difficulty};
    m_runHistory.push_back(record);
    // Sort by rooms descending, keep top 10
    std::sort(m_runHistory.begin(), m_runHistory.end(),
        [](const RunRecord& a, const RunRecord& b) { return a.rooms > b.rooms; });
    if (m_runHistory.size() > 10) m_runHistory.resize(10);
}

std::string UpgradeSystem::serialize() const {
    std::ostringstream ss;
    ss << m_riftShards << " " << totalRuns << " " << bestRoomReached << " "
       << totalEnemiesKilled << " " << totalRiftsRepaired << " ";
    for (auto& u : m_upgrades) {
        ss << u.currentLevel << " ";
    }
    // Serialize run history
    ss << static_cast<int>(m_runHistory.size()) << " ";
    for (auto& r : m_runHistory) {
        ss << r.rooms << " " << r.enemies << " " << r.rifts << " "
           << r.shards << " " << r.difficulty << " ";
    }
    return ss.str();
}

void UpgradeSystem::deserialize(const std::string& data) {
    std::istringstream ss(data);
    ss >> m_riftShards >> totalRuns >> bestRoomReached
       >> totalEnemiesKilled >> totalRiftsRepaired;
    for (auto& u : m_upgrades) {
        ss >> u.currentLevel;
    }
    // Deserialize run history
    int histCount = 0;
    if (ss >> histCount) {
        m_runHistory.clear();
        for (int i = 0; i < histCount && i < 10; i++) {
            RunRecord r{};
            if (ss >> r.rooms >> r.enemies >> r.rifts >> r.shards >> r.difficulty) {
                m_runHistory.push_back(r);
            }
        }
    }
}
