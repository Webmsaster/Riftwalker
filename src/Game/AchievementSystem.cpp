#include "AchievementSystem.h"
#include <fstream>
#include <sstream>
#include <algorithm>

void AchievementSystem::init() {
    m_achievements = {
        {"first_blood",     "First Blood",      "Kill your first enemy"},
        {"rift_walker",     "Rift Walker",       "Repair your first rift"},
        {"room_clearer",    "Room Clearer",      "Clear 10 rooms in a single run"},
        {"boss_slayer",     "Boss Slayer",       "Defeat a boss"},
        {"wyrm_hunter",     "Wyrm Hunter",       "Defeat the Void Wyrm"},
        {"combo_king",      "Combo King",        "Reach a 10-hit combo"},
        {"unstoppable",     "Unstoppable",       "Reach difficulty 5"},
        {"shard_hoarder",   "Shard Hoarder",     "Collect 1000 shards total"},
        {"full_upgrade",    "Fully Upgraded",    "Max out any upgrade"},
        {"mini_boss_hunter","Mini-Boss Hunter",  "Defeat a mini-boss"},
        {"elemental_slayer","Elemental Slayer",  "Kill an elemental enemy"},
        {"dash_master",     "Dash Master",       "Dash 100 times in a single run"},
        {"dimension_hopper","Dimension Hopper",  "Switch dimensions 50 times total"},
        {"grade_s",         "S-Rank",            "Get an S grade on a run"},
        {"survivor",        "Survivor",          "Survive 20 rooms in a single run"},
    };
}

bool AchievementSystem::unlock(const std::string& id) {
    for (auto& a : m_achievements) {
        if (a.id == id && !a.unlocked) {
            a.unlocked = true;
            m_notification.name = a.name;
            m_notification.timer = m_notification.duration;
            return true;
        }
    }
    return false;
}

bool AchievementSystem::isUnlocked(const std::string& id) const {
    for (auto& a : m_achievements) {
        if (a.id == id) return a.unlocked;
    }
    return false;
}

void AchievementSystem::update(float dt) {
    if (m_notification.timer > 0) {
        m_notification.timer -= dt;
    }
}

int AchievementSystem::getUnlockedCount() const {
    int count = 0;
    for (auto& a : m_achievements) {
        if (a.unlocked) count++;
    }
    return count;
}

const AchievementNotification* AchievementSystem::getActiveNotification() const {
    if (m_notification.timer > 0) return &m_notification;
    return nullptr;
}

bool AchievementSystem::save(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    for (auto& a : m_achievements) {
        if (a.unlocked) {
            file << a.id << "\n";
        }
    }
    return true;
}

bool AchievementSystem::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        for (auto& a : m_achievements) {
            if (a.id == line) {
                a.unlocked = true;
                break;
            }
        }
    }
    return true;
}
