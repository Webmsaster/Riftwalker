#include "AchievementSystem.h"
#include "Core/SaveUtils.h"
#include "Core/SteamShim.h"
#include "Core/Localization.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstring>

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
        // New content achievements
        {"entropy_master",  "Entropy Master",     "Complete a floor using only Entropy Scythe",     "+3% Entropy Decay"},
        {"quest_helper",    "Helpful Traveler",   "Complete 5 NPC quests",                          "+5% Shard Drop"},
        {"speed_demon",     "Speed Demon",        "Complete the game in under 10 minutes",          "+5% Move Speed"},
        {"weapon_collector","Arsenal Master",     "Unlock all 12 weapons",                          "+3% All DMG"},
        {"dimension_dancer","Dimension Dancer",   "Switch dimensions 50 times in one run",          "-5% Switch CD"},
        // Class mastery achievements
        {"void_mastery",    "Void Master",        "Win a run as Voidwalker",                        "+5% Phase DMG"},
        {"berserk_mastery", "Berserker Lord",     "Win a run as Berserker",                         "+5% Melee DMG"},
        {"phantom_mastery", "Ghost Protocol",     "Win a run as Phantom",                           "+3% Dodge Chance"},
        {"tech_mastery",    "Chief Engineer",     "Win a run as Technomancer",                      "+5% Construct DMG"},
        {"all_classes",     "Jack of All Trades", "Win a run with each class",                      "+10 Max HP"},
        // Relic achievements
        {"relic_collector", "Relic Hoarder",      "Hold 6 relics in a single run",                  "+5% Relic Drop"},
        {"synergy_hunter",  "Synergist",          "Activate 3 relic synergies in one run",          "+3% All DMG"},
        {"cursed_survivor", "Curse Bearer",       "Win a run with 3 cursed relics",                 "+5% All DMG"},
        // Combat achievements
        {"parry_master",    "Parry King",         "Parry 20 attacks in a single run",               "+5% Counter DMG"},
        {"no_damage_boss",  "Untouchable",        "Defeat a boss without taking damage",            "+5 Max HP"},
        {"kill_streak_50",  "Massacre",           "Kill 50 enemies in a single floor",              "+3% All DMG"},
        {"ranged_only",     "Sharpshooter",       "Complete 5 floors using only ranged weapons",    "+5% Ranged DMG"},
        // Exploration achievements
        {"lore_hunter",     "Lore Keeper",        "Discover 12 lore fragments",                     "+10% Shard Drop"},
        {"ascension_1",     "New Game Plus",      "Complete Ascension Tier 1",                      "+5 Max HP"},
        {"ascension_5",     "True Riftwalker",    "Complete Ascension Tier 5",                      "+10% All DMG"},
    };
}

bool AchievementSystem::unlock(const std::string& id) {
    for (auto& a : m_achievements) {
        if (a.id == id && !a.unlocked) {
            a.unlocked = true;
            // Pick localized name (falls back to hardcoded EN if key missing)
            char nameKey[64];
            std::snprintf(nameKey, sizeof(nameKey), "ach.%s.name", a.id.c_str());
            const char* locName = LOC(nameKey);
            m_notification.name = (std::strcmp(locName, nameKey) == 0) ? a.name : locName;
            m_notification.rewardText = a.rewardDesc;
            m_notification.timer = m_notification.duration;
            // Mirror to Steam (no-op without ENABLE_STEAMWORKS). Achievement
            // API name = the same id we use locally (e.g. "boss_slayer").
            Steam::setAchievement(id);
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
    return atomicSave(filepath, [&](std::ofstream& file) {
        for (auto& a : m_achievements) {
            if (a.unlocked) {
                file << a.id << "\n";
            }
        }
    });
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
        // New content achievements
        else if (a.id == "entropy_master")   b.entropyDecayMult *= 1.03f;
        else if (a.id == "quest_helper")     b.shardDropMult *= 1.05f;
        else if (a.id == "speed_demon")      b.moveSpeedMult *= 1.05f;
        else if (a.id == "weapon_collector") b.allDamageMult *= 1.03f;
        else if (a.id == "dimension_dancer") b.switchCooldownMult *= 0.95f;
        // Class mastery achievements
        else if (a.id == "void_mastery")     b.allDamageMult *= 1.05f;
        else if (a.id == "berserk_mastery")  b.meleeDamageMult *= 1.05f;
        else if (a.id == "phantom_mastery")  b.moveSpeedMult *= 1.03f;
        else if (a.id == "tech_mastery")     b.rangedDamageMult *= 1.05f;
        else if (a.id == "all_classes")      b.maxHPBonus += 10.0f;
        // Relic achievements
        else if (a.id == "relic_collector")  b.shardDropMult *= 1.05f;
        else if (a.id == "synergy_hunter")   b.allDamageMult *= 1.03f;
        else if (a.id == "cursed_survivor")  b.allDamageMult *= 1.05f;
        // Combat achievements
        else if (a.id == "parry_master")     b.meleeDamageMult *= 1.05f;
        else if (a.id == "no_damage_boss")   b.maxHPBonus += 5.0f;
        else if (a.id == "kill_streak_50")   b.allDamageMult *= 1.03f;
        else if (a.id == "ranged_only")      b.rangedDamageMult *= 1.05f;
        // Exploration achievements
        else if (a.id == "lore_hunter")      b.shardDropMult *= 1.10f;
        else if (a.id == "ascension_1")      b.maxHPBonus += 5.0f;
        else if (a.id == "ascension_5")      b.allDamageMult *= 1.10f;
    }
    return b;
}
