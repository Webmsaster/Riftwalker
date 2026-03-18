#pragma once
#include "Components/AIComponent.h"
#include <string>
#include <unordered_map>

struct BestiaryEntry {
    EnemyType type = EnemyType::Walker;
    const char* name = "";
    const char* lore = "";
    float baseHP = 0;
    float baseDMG = 0;
    float baseSpeed = 0;       // patrol/chase speed reference
    EnemyElement element = EnemyElement::None;
    const char* weakness = ""; // primary weakness description
    const char* abilities = ""; // special abilities summary
    const char* effectiveWeapons = ""; // what weapons/tactics work best
    int killCount = 0;
    bool discovered = false;
};

class Bestiary {
public:
    static void init();
    static void onEnemyKill(EnemyType type);
    static void onBossKill(int bossType);
    static const BestiaryEntry& getEntry(EnemyType type);
    static int getDiscoveredCount();
    static int getTotalCount();

    // Boss entries: 0=Guardian, 1=Wyrm, 2=Architect, 3=TemporalWeaver, 4=VoidSovereign, 5=EntropyIncarnate
    static const BestiaryEntry& getBossEntry(int bossType);

    static void save(const std::string& filepath);
    static void load(const std::string& filepath);
};
