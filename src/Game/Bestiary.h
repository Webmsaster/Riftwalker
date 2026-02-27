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
    const char* weakness = "";
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

    // Boss entries use negative indices: -1=Guardian, -2=Wyrm, -3=Architect, -4=TemporalWeaver
    static const BestiaryEntry& getBossEntry(int bossType);

    static void save(const std::string& filepath);
    static void load(const std::string& filepath);
};
