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
    // {type, name, lore, HP, DMG, speed, element, weakness, abilities, effectiveWeapons}
    e[static_cast<int>(EnemyType::Walker)]   = {EnemyType::Walker,   "Rift Walker",
        "Basic dimensional drifter. Patrols rift corridors seeking intruders.",
        40, 10, 90, EnemyElement::None,
        "Slow movement, predictable patrol",
        "Melee attack, patrols platforms",
        "Any weapon. Melee combo finishers stun."};
    e[static_cast<int>(EnemyType::Flyer)]    = {EnemyType::Flyer,    "Rift Flyer",
        "Aerial predator that hovers overhead and swoops down to strike.",
        25, 10, 120, EnemyElement::None,
        "Low HP, vulnerable while swooping",
        "Swoop attack, aerial patrol, 250px dive speed",
        "Ranged weapons. Ice slows swoop."};
    e[static_cast<int>(EnemyType::Turret)]   = {EnemyType::Turret,   "Void Turret",
        "Stationary automated defense. Fires precision bolts at any intruder in range.",
        40, 8, 0, EnemyElement::None,
        "Cannot move, approach from close range",
        "Ranged fire every 1.8s, 300px detection",
        "Close-range melee. Dash past projectiles."};
    e[static_cast<int>(EnemyType::Charger)]  = {EnemyType::Charger,  "Rift Charger",
        "Heavy bruiser that telegraphs a devastating charge attack.",
        45, 18, 50, EnemyElement::None,
        "Windup before charge, vulnerable during stop",
        "Charge dash at 350px/s, knockback 350",
        "Dodge sideways, counter after charge ends."};
    e[static_cast<int>(EnemyType::Phaser)]   = {EnemyType::Phaser,   "Phase Stalker",
        "Blinks between dimensions every 3 seconds to avoid attacks.",
        30, 14, 110, EnemyElement::None,
        "Vulnerable right after phasing",
        "Dimension phase every 3s, melee on approach",
        "Match its dimension. Time attacks post-phase."};
    e[static_cast<int>(EnemyType::Exploder)] = {EnemyType::Exploder, "Void Exploder",
        "Rushes at full speed and detonates on contact. Handle from range.",
        20, 30, 180, EnemyElement::Fire,
        "Very low HP, one melee hit kills",
        "80px explosion radius, fire elemental, fast rush",
        "Shoot from distance. Ice freezes in place."};
    e[static_cast<int>(EnemyType::Shielder)] = {EnemyType::Shielder, "Rift Shielder",
        "Armored guardian with a frontal shield that deflects all attacks.",
        65, 14, 40, EnemyElement::None,
        "Unshielded from behind, armor 0.35",
        "Frontal shield, 35% armor, slow but tanky",
        "Attack from behind. Electric chains bypass shield."};
    e[static_cast<int>(EnemyType::Crawler)]  = {EnemyType::Crawler,  "Void Crawler",
        "Clings to ceilings and drops on unsuspecting prey from above.",
        20, 12, 50, EnemyElement::None,
        "Watch the ceiling, low HP",
        "Ceiling cling, drop ambush at 400px/s",
        "Ranged weapons work well. Fire clears ceilings."};
    e[static_cast<int>(EnemyType::Summoner)] = {EnemyType::Summoner, "Rift Summoner",
        "Calls up to 3 minions and hides while they overwhelm you.",
        60, 8, 30, EnemyElement::Electric,
        "Fragile when alone, kill minions first",
        "Summons 3 minions every 6s, electric affinity",
        "Burst damage to summoner. Ignore minions if possible."};
    e[static_cast<int>(EnemyType::Sniper)]   = {EnemyType::Sniper,   "Void Sniper",
        "Keeps maximum distance and telegraphs lethal precision shots.",
        30, 16, 40, EnemyElement::None,
        "Long telegraph time, low HP",
        "400px range, 0.8s telegraph, retreats on approach",
        "Dash between shots. Close the gap quickly."};

    s_bossEntries.resize(5);
    // Boss entries: {type, name, lore, HP, DMG, speed, element, weakness, abilities, effectiveWeapons}
    s_bossEntries[0] = {EnemyType::Walker, "Rift Guardian",
        "First guardian of the rift. Massive and powerful with shield bursts and phase leaps.",
        230, 23, 100, EnemyElement::None,
        "Phase 2 enrage leaves brief opening",
        "Shield burst, multi-shot, phase leap, 3 phases",
        "Learn attack patterns. Dash during shield burst."};
    s_bossEntries[1] = {EnemyType::Walker, "Void Wyrm",
        "Serpentine void beast with poison dive attacks and orbital barrages.",
        190, 21, 145, EnemyElement::None,
        "Vulnerable after divebomb lands",
        "Poison clouds, dive bomb, bullet barrage, 3 phases",
        "Avoid green floor. Counter after divebomb."};
    s_bossEntries[2] = {EnemyType::Walker, "Dimensional Architect",
        "Builder of realities. Swaps tiles, constructs energy beams and collapses arenas.",
        220, 18, 90, EnemyElement::Electric,
        "Weak while constructing beam, destroy constructs",
        "Tile swap, rift zones, construct beam, arena collapse",
        "Destroy constructs fast. Use dim-switch to dodge rift zones."};
    s_bossEntries[3] = {EnemyType::Walker, "Temporal Weaver",
        "Master of time itself. Creates slow zones, clock sweeps and full time stops.",
        280, 22, 100, EnemyElement::Ice,
        "Vulnerable in time-stop wind-up",
        "Time slow zones, clock sweep, time rewind, time stop",
        "Dash through slow zones. Learn sweep timing."};
    s_bossEntries[4] = {EnemyType::Walker, "Void Sovereign",
        "The final lord of the Rift. Commands dimensions, void storms and reality-tear lasers.",
        400, 25, 120, EnemyElement::None,
        "Watch for phase transitions, avoid void storm",
        "Void orbs, rift slam, dim lock, laser sweep, void storm",
        "Stay mobile. Use dim-switch to counter dim-lock."};
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
            if (kills < 0) kills = 0;
            if (kills > 999999) kills = 999999;
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
