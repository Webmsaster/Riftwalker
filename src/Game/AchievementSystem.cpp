#include "AchievementSystem.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <SDL2/SDL.h>

static void backupFile(const std::string& path) {
    std::ifstream src(path, std::ios::binary);
    if (!src) return;
    std::ofstream dst(path + ".bak", std::ios::binary);
    if (dst) dst << src.rdbuf();
}

static std::ifstream openWithBackupFallback(const std::string& path) {
    std::ifstream file(path);
    if (file.is_open() && file.peek() != std::ifstream::traits_type::eof()) return file;
    file.close();
    file.open(path + ".bak");
    if (file.is_open()) SDL_Log("Using backup save: %s.bak", path.c_str());
    return file;
}

void AchievementSystem::init() {
    m_achievements = {
        {"first_blood",     "First Blood",      "Kill your first enemy",           "+5 Max HP"},
        {"rift_walker",     "Rift Walker",       "Repair your first rift",          "-5% Switch CD"},
        {"room_clearer",    "Room Clearer",      "Clear 10 rooms in a single run",  "+3% Move Speed"},
        {"boss_slayer",     "Boss Slayer",       "Defeat a boss",                   "+5% Melee DMG"},
        {"wyrm_hunter",     "Wyrm Hunter",       "Defeat the Void Wyrm",            "+5% Ranged DMG"},
        {"combo_king",      "Combo King",        "Reach a 10-hit combo",            "+5% Combo DMG"},
        {"unstoppable",     "Unstoppable",       "Reach difficulty 5",              "+10 Max HP"},
        {"shard_hoarder",   "Shard Hoarder",     "Collect 1000 shards total",       "+10% Shard Drop"},
        {"full_upgrade",    "Fully Upgraded",    "Max out any upgrade",             "+5% All DMG"},
        {"mini_boss_hunter","Mini-Boss Hunter",  "Defeat a mini-boss",              "+1 Armor"},
        {"elemental_slayer","Elemental Slayer",  "Kill an elemental enemy",         "-15% DOT Duration"},
        {"dash_master",     "Dash Master",       "Dash 100 times in a single run",  "-8% Dash CD"},
        {"dimension_hopper","Dimension Hopper",  "Switch dimensions 50 times total","-5% Switch CD"},
        {"grade_s",         "S-Rank",            "Get an S grade on a run",         "+3% Crit Chance"},
        {"survivor",        "Survivor",          "Survive 20 rooms in a single run","+15 Max HP"},
        // Skill achievements
        {"aerial_ace",      "Aerial Ace",        "Kill 5 enemies while airborne in one run",    "+5% All DMG"},
        {"flawless_floor",  "Flawless Floor",    "Complete a level without taking damage",       "+3% Move Speed"},
        {"resonance_master","Resonance Master",  "Reach maximum resonance tier",                "+5% All DMG"},
        {"combo_legend",    "Combo Legend",       "Reach a 15-hit combo",                        "+5% Combo DMG"},
        {"dash_slayer",     "Dash Slayer",        "Kill 10 enemies with dash in one run",        "-5% Dash CD"},
        {"charged_fury",    "Charged Fury",       "Kill 3 enemies with charged attacks in one run","+5% Melee DMG"},
    };
}

bool AchievementSystem::unlock(const std::string& id) {
    for (auto& a : m_achievements) {
        if (a.id == id && !a.unlocked) {
            a.unlocked = true;
            m_notification.name = a.name;
            m_notification.rewardText = a.rewardDesc;
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
    backupFile(filepath);
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
    std::ifstream file = openWithBackupFallback(filepath);
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

AchievementBonuses AchievementSystem::getUnlockedBonuses() const {
    AchievementBonuses b;
    for (auto& a : m_achievements) {
        if (!a.unlocked) continue;
        if (a.id == "first_blood")      b.maxHPBonus += 5.0f;
        else if (a.id == "rift_walker")      b.switchCooldownMult *= 0.95f;
        else if (a.id == "room_clearer")     b.moveSpeedMult *= 1.03f;
        else if (a.id == "boss_slayer")      b.meleeDamageMult *= 1.05f;
        else if (a.id == "wyrm_hunter")      b.rangedDamageMult *= 1.05f;
        else if (a.id == "combo_king")       b.comboDamageBonus += 0.05f;
        else if (a.id == "unstoppable")      b.maxHPBonus += 10.0f;
        else if (a.id == "shard_hoarder")    b.shardDropMult *= 1.10f;
        else if (a.id == "full_upgrade")     b.allDamageMult *= 1.05f;
        else if (a.id == "mini_boss_hunter") b.armorBonus += 1.0f;
        else if (a.id == "elemental_slayer") b.dotDurationMult *= 0.85f;
        else if (a.id == "dash_master")      b.dashCooldownMult *= 0.92f;
        else if (a.id == "dimension_hopper") b.switchCooldownMult *= 0.95f;
        else if (a.id == "grade_s")          b.critChanceBonus += 0.03f;
        else if (a.id == "survivor")         b.maxHPBonus += 15.0f;
        // Skill achievements
        else if (a.id == "aerial_ace")       b.allDamageMult *= 1.05f;
        else if (a.id == "flawless_floor")   b.moveSpeedMult *= 1.03f;
        else if (a.id == "resonance_master") b.allDamageMult *= 1.05f;
        else if (a.id == "combo_legend")     b.comboDamageBonus += 0.05f;
        else if (a.id == "dash_slayer")      b.dashCooldownMult *= 0.95f;
        else if (a.id == "charged_fury")     b.meleeDamageMult *= 1.05f;
    }
    return b;
}
