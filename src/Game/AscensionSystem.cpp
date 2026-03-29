#include "AscensionSystem.h"
#include <vector>
#include <fstream>
#include <SDL.h>

static std::vector<AscensionLevel> s_levels;
static bool s_init = false;

int AscensionSystem::currentLevel = 0;
int AscensionSystem::riftCores = 0;

void AscensionSystem::init() {
    if (s_init) return;
    s_init = true;
    s_levels.resize(11); // 0 = base, 1-10 = ascension levels

    // Level 0: no modifiers (base game)
    s_levels[0] = {"Normal", "Normal", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0, 1.0f, 1.0f, 0, -1, false, false, false, false};

    // Level 1: +10% start shards, enemies +10% HP
    s_levels[1] = {"+10% Start Shards", "Enemies +10% HP",
        1.1f, 1.0f, 1.0f, 1.0f, 1.0f, 0.1f, 1.0f, 1.0f, 0, -1, false, false, false, false};

    // Level 2: +1 relic choice at boss, enemies +15% DMG
    s_levels[2] = {"+1 Relic Choice", "Enemies +15% DMG",
        1.1f, 1.15f, 1.0f, 1.0f, 1.0f, 0.1f, 1.0f, 1.0f, 1, -1, false, false, false, false};

    // Level 3: keep weapon upgrade, elites +20% spawn
    s_levels[3] = {"Keep Weapon Upgrade", "Elites +20% Spawn",
        1.1f, 1.15f, 1.0f, 1.2f, 1.0f, 0.1f, 1.0f, 1.0f, 1, -1, true, false, false, false};

    // Level 4: extra shop slot, boss +20% HP
    s_levels[4] = {"Extra Shop Slot", "Boss +20% HP",
        1.3f, 1.15f, 1.0f, 1.2f, 1.0f, 0.1f, 1.0f, 1.0f, 1, -1, true, true, false, false};

    // Level 5: start with random common relic, new enemy patterns
    s_levels[5] = {"Start: Common Relic", "Harder Patterns",
        1.3f, 1.2f, 1.1f, 1.2f, 1.0f, 0.1f, 1.0f, 1.0f, 1, 0, true, true, false, false};

    // Level 6: +15% shard gain, traps +30%
    s_levels[6] = {"+15% Shard Gain", "Traps +30%",
        1.3f, 1.2f, 1.1f, 1.2f, 1.3f, 0.1f, 1.15f, 1.0f, 1, 0, true, true, false, false};

    // Level 7: abilities start at 50% CD, boss extra phase
    s_levels[7] = {"Abilities: 50% CD Start", "Boss Extra Phase",
        1.4f, 1.25f, 1.1f, 1.2f, 1.3f, 0.1f, 1.15f, 1.0f, 1, 0, true, true, true, true};

    // Level 8: start with 2 relics, elites have 2 modifiers
    s_levels[8] = {"Start: 2 Relics", "Elites: 2 Modifiers",
        1.4f, 1.25f, 1.2f, 1.4f, 1.3f, 0.1f, 1.15f, 1.0f, 1, 0, true, true, true, true};

    // Level 9: shop prices -20%, all enemies +30% speed
    s_levels[9] = {"Shop: -20% Prices", "Enemies +30% Speed",
        1.4f, 1.25f, 1.3f, 1.4f, 1.3f, 0.1f, 1.15f, 0.8f, 1, 0, true, true, true, true};

    // Level 10: start with legendary relic, chaos mode
    s_levels[10] = {"Start: Legendary Relic", "CHAOS MODE",
        1.6f, 1.4f, 1.4f, 1.6f, 1.5f, 0.2f, 1.2f, 0.8f, 2, 2, true, true, true, true};
}

const AscensionLevel& AscensionSystem::getLevel(int level) {
    init();
    if (level < 0) level = 0;
    if (level >= static_cast<int>(s_levels.size())) level = static_cast<int>(s_levels.size()) - 1;
    return s_levels[level];
}

int AscensionSystem::getMaxLevel() { return 10; }

void AscensionSystem::save(const std::string& filepath) {
    // Backup existing file
    {
        std::ifstream src(filepath, std::ios::binary);
        if (src) {
            std::ofstream dst(filepath + ".bak", std::ios::binary);
            if (dst) dst << src.rdbuf();
        }
    }
    std::ofstream f(filepath);
    if (!f) return;
    f << currentLevel << "\n" << riftCores << "\n";
}

void AscensionSystem::load(const std::string& filepath) {
    init();
    std::ifstream f(filepath);
    if (!f.is_open() || f.peek() == std::ifstream::traits_type::eof()) {
        f.close();
        f.open(filepath + ".bak");
        if (f.is_open()) SDL_Log("Using backup ascension save: %s.bak", filepath.c_str());
    }
    if (!f) return;
    f >> currentLevel >> riftCores;
    if (currentLevel < 0) currentLevel = 0;
    if (currentLevel > 10) currentLevel = 10;
    if (riftCores < 0) riftCores = 0;
}
