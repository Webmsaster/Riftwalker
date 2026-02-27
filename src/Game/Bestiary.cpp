#include "Bestiary.h"
#include <vector>
#include <fstream>

static std::vector<BestiaryEntry> s_entries;
static std::vector<BestiaryEntry> s_bossEntries;
static bool s_init = false;

void Bestiary::init() {
    if (s_init) return;
    s_init = true;

    s_entries.resize(static_cast<int>(EnemyType::COUNT));
    auto& e = s_entries;
    e[static_cast<int>(EnemyType::Walker)]   = {EnemyType::Walker, "Rift Walker", "Basic dimensional drifter. Patrols rift corridors.", 30, 10, "Slow, easy to dodge"};
    e[static_cast<int>(EnemyType::Flyer)]    = {EnemyType::Flyer, "Rift Flyer", "Hovers above, swoops to attack.", 20, 8, "Low HP, stunnable"};
    e[static_cast<int>(EnemyType::Turret)]   = {EnemyType::Turret, "Void Turret", "Stationary. Fires at intruders.", 40, 12, "Cannot move, approach from sides"};
    e[static_cast<int>(EnemyType::Charger)]  = {EnemyType::Charger, "Rift Charger", "Charges at high speed when it spots you.", 35, 18, "Dodge then counter"};
    e[static_cast<int>(EnemyType::Phaser)]   = {EnemyType::Phaser, "Phase Stalker", "Phases between dimensions.", 25, 14, "Switch dim to hit"};
    e[static_cast<int>(EnemyType::Exploder)] = {EnemyType::Exploder, "Void Exploder", "Rushes in and detonates.", 15, 25, "Kill from range"};
    e[static_cast<int>(EnemyType::Shielder)] = {EnemyType::Shielder, "Rift Shielder", "Blocks attacks from one side.", 45, 12, "Attack from behind"};
    e[static_cast<int>(EnemyType::Crawler)]  = {EnemyType::Crawler, "Void Crawler", "Climbs walls and drops on prey.", 30, 15, "Watch ceilings"};
    e[static_cast<int>(EnemyType::Summoner)] = {EnemyType::Summoner, "Rift Summoner", "Calls minions to fight for it.", 35, 8, "Kill summoner first"};
    e[static_cast<int>(EnemyType::Sniper)]   = {EnemyType::Sniper, "Void Sniper", "Long-range precision shots.", 25, 20, "Close the gap fast"};

    s_bossEntries.resize(4);
    s_bossEntries[0] = {EnemyType::Walker, "Rift Guardian", "First guardian of the rift. Massive and powerful.", 200, 15, "Learn phase patterns"};
    s_bossEntries[1] = {EnemyType::Walker, "Void Wyrm", "Serpentine beast of the void. Poison and dive attacks.", 240, 18, "Avoid poison pools"};
    s_bossEntries[2] = {EnemyType::Walker, "Dimensional Architect", "Builder of realities. Constructs barriers and beams.", 220, 16, "Destroy constructs"};
    s_bossEntries[3] = {EnemyType::Walker, "Temporal Weaver", "Master of time. Slows, stops, and rewinds.", 280, 20, "Dash through time zones"};
}

void Bestiary::onEnemyKill(EnemyType type) {
    init();
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(s_entries.size())) {
        s_entries[idx].discovered = true;
        s_entries[idx].killCount++;
    }
}

void Bestiary::onBossKill(int bossType) {
    init();
    if (bossType >= 0 && bossType < static_cast<int>(s_bossEntries.size())) {
        s_bossEntries[bossType].discovered = true;
        s_bossEntries[bossType].killCount++;
    }
}

const BestiaryEntry& Bestiary::getEntry(EnemyType type) {
    init();
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(s_entries.size())) return s_entries[idx];
    return s_entries[0];
}

const BestiaryEntry& Bestiary::getBossEntry(int bossType) {
    init();
    if (bossType >= 0 && bossType < static_cast<int>(s_bossEntries.size())) return s_bossEntries[bossType];
    return s_bossEntries[0];
}

int Bestiary::getDiscoveredCount() {
    init();
    int count = 0;
    for (auto& e : s_entries) if (e.discovered) count++;
    for (auto& b : s_bossEntries) if (b.discovered) count++;
    return count;
}

int Bestiary::getTotalCount() {
    init();
    return static_cast<int>(s_entries.size() + s_bossEntries.size());
}

void Bestiary::save(const std::string& filepath) {
    init();
    std::ofstream f(filepath);
    if (!f) return;
    for (auto& e : s_entries) {
        f << static_cast<int>(e.type) << " " << e.killCount << " " << (e.discovered ? 1 : 0) << "\n";
    }
    f << "BOSS\n";
    for (int i = 0; i < static_cast<int>(s_bossEntries.size()); i++) {
        f << i << " " << s_bossEntries[i].killCount << " " << (s_bossEntries[i].discovered ? 1 : 0) << "\n";
    }
}

void Bestiary::load(const std::string& filepath) {
    init();
    std::ifstream f(filepath);
    if (!f) return;
    std::string line;
    bool readingBoss = false;
    while (std::getline(f, line)) {
        if (line == "BOSS") { readingBoss = true; continue; }
        int id, kills, disc;
        if (sscanf(line.c_str(), "%d %d %d", &id, &kills, &disc) == 3) {
            if (!readingBoss && id >= 0 && id < static_cast<int>(s_entries.size())) {
                s_entries[id].killCount = kills;
                s_entries[id].discovered = (disc != 0);
            } else if (readingBoss && id >= 0 && id < static_cast<int>(s_bossEntries.size())) {
                s_bossEntries[id].killCount = kills;
                s_bossEntries[id].discovered = (disc != 0);
            }
        }
    }
}
