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
        "Rift Walkers are rudimentary constructs formed when residual dimensional energy "
        "crystallizes around a humanoid imprint left in the rift fabric. They have no "
        "true intelligence, only a territorial directive burned into their core. Explorers "
        "who enter rift corridors frequently report being followed by these silent sentinels "
        "for hours before they finally strike.",
        40, 10, 90, EnemyElement::None,
        "Slow movement, predictable patrol",
        "Melee attack, patrols platforms",
        "Any weapon. Melee combo finishers stun."};
    e[static_cast<int>(EnemyType::Flyer)]    = {EnemyType::Flyer,    "Rift Flyer",
        "When rift energy surges upward through dimensional tears, it sometimes coalesces "
        "into airborne predators known as Rift Flyers. Born in the thermals between "
        "dimensions, they claim vertical space as their territory and dive-bomb anything "
        "that intrudes from below. Their hollow bodies are nearly transparent at rest, "
        "making them nearly invisible until the moment they strike.",
        25, 10, 120, EnemyElement::None,
        "Low HP, vulnerable while swooping",
        "Swoop attack, aerial patrol, 250px dive speed",
        "Ranged weapons. Ice slows swoop."};
    e[static_cast<int>(EnemyType::Turret)]   = {EnemyType::Turret,   "Void Turret",
        "Void Turrets are ancient automated sentinels left behind by an unknown civilization "
        "that once colonized the space between dimensions. Their crystalline cores fire "
        "compressed rift bolts with mechanical precision, never tiring, never missing within "
        "their detection radius. Researchers theorize that whoever built them feared something "
        "far worse than what the turrets guard against.",
        40, 8, 0, EnemyElement::None,
        "Cannot move, approach from close range",
        "Ranged fire every 1.8s, 300px detection",
        "Close-range melee. Dash past projectiles."};
    e[static_cast<int>(EnemyType::Charger)]  = {EnemyType::Charger,  "Rift Charger",
        "Rift Chargers were once slow-moving dimensional grazers, feeding harmlessly on "
        "ambient rift energy. Prolonged exposure to dimensional instability drove them "
        "permanently into a maddened frenzy, their bodies warping into low-slung battering "
        "rams of pure aggression. The horn-like protrusion on their snout is not a natural "
        "growth but crystallized dimensional fury that builds with each charge.",
        45, 18, 50, EnemyElement::None,
        "Windup before charge, vulnerable during stop",
        "Charge dash at 350px/s, knockback 350",
        "Dodge sideways, counter after charge ends."};
    e[static_cast<int>(EnemyType::Phaser)]   = {EnemyType::Phaser,   "Phase Stalker",
        "Phase Stalkers exist in an unstable superposition between dimensions, never fully "
        "belonging to either. The dimensional shift they perform every few seconds is not "
        "a choice but an involuntary convulsion of their fractured existence. Survivors "
        "describe them as predators that feel genuinely confused between attacks, as if "
        "they keep forgetting which reality they are hunting in.",
        30, 14, 110, EnemyElement::None,
        "Vulnerable right after phasing",
        "Dimension phase every 3s, melee on approach",
        "Match its dimension. Time attacks post-phase."};
    e[static_cast<int>(EnemyType::Exploder)] = {EnemyType::Exploder, "Void Exploder",
        "Void Exploders are the most volatile form of dimensional energy given shape. They "
        "are essentially living pressure vessels, their bodies constantly accumulating rift "
        "energy far beyond what their fragile forms can contain. Contact with any solid "
        "object triggers catastrophic decompression, and the fire elemental energy released "
        "is hot enough to melt standard dimensional suit plating.",
        20, 30, 180, EnemyElement::Fire,
        "Very low HP, one melee hit kills",
        "80px explosion radius, fire elemental, fast rush",
        "Shoot from distance. Ice freezes in place."};
    e[static_cast<int>(EnemyType::Shielder)] = {EnemyType::Shielder, "Rift Shielder",
        "Rift Shielders are constructed entities that project a dimensional barrier from "
        "one arm, creating an impenetrable wall of compressed rift energy on their guarded "
        "side. The shield is not merely physical protection but a localized dimensional "
        "fold that absorbs and redirects incoming force. They are slow precisely because "
        "maintaining the barrier requires constant concentration from their energy core.",
        65, 14, 40, EnemyElement::None,
        "Unshielded from behind, armor 0.35",
        "Frontal shield, 35% armor, slow but tanky",
        "Attack from behind. Electric chains bypass shield."};
    e[static_cast<int>(EnemyType::Crawler)]  = {EnemyType::Crawler,  "Void Crawler",
        "Void Crawlers evolved in the zero-gravity spaces where rift corridors intersect "
        "overhead terrain, developing specialized claws that grip any surface regardless "
        "of orientation. They are patient ambush predators, capable of clinging motionless "
        "to a ceiling for days while waiting for prey to pass beneath. The drop is so fast "
        "that victims rarely have time to look up before impact.",
        20, 14, 50, EnemyElement::None,
        "Watch the ceiling, low HP",
        "Ceiling cling, drop ambush at 400px/s",
        "Ranged weapons work well. Fire clears ceilings."};
    e[static_cast<int>(EnemyType::Summoner)] = {EnemyType::Summoner, "Rift Summoner",
        "Rift Summoners are conduit entities that have learned to reach back through the "
        "dimensional fabric and pull lesser constructs into existence. The minions they "
        "create are not truly alive but temporary extensions of the Summoner's own energy "
        "given crude physical form. Killing the Summoner causes all active minions to "
        "instantly dissipate, suggesting they share a single distributed consciousness.",
        60, 8, 30, EnemyElement::Electric,
        "Fragile when alone, kill minions first",
        "Summons 3 minions every 6s, electric affinity",
        "Burst damage to summoner. Ignore minions if possible."};
    e[static_cast<int>(EnemyType::Sniper)]   = {EnemyType::Sniper,   "Void Sniper",
        "Void Snipers are precision-evolved dimensional constructs that developed a unique "
        "targeting organ capable of perceiving rift distortions across vast distances. "
        "The red laser dot they project before firing is not a simple sight but a "
        "concentrated beam of pre-collapsed dimensional energy that destabilizes the "
        "target's local space, ensuring the follow-up shot cannot miss.",
        35, 16, 40, EnemyElement::None,
        "Long telegraph time, low HP",
        "400px range, 0.8s telegraph, retreats on approach",
        "Dash between shots. Close the gap quickly."};
    e[static_cast<int>(EnemyType::Teleporter)] = {EnemyType::Teleporter, "Phase Blinker",
        "Phase Blinkers are dimensional anomalies that learned to fold space around "
        "themselves, blinking short distances through the rift fabric as naturally as "
        "breathing. They favor ambush tactics, materializing behind unsuspecting prey "
        "for a quick strike before vanishing again. The afterimage they leave behind "
        "is not an illusion but a brief echo of their presence lingering in displaced space.",
        35, 14, 100, EnemyElement::None,
        "Predictable 3s teleport cycle, fragile",
        "Teleports behind player every 3s, surprise melee attack",
        "Time your attacks to catch after teleport. Ranged weapons."};
    e[static_cast<int>(EnemyType::Reflector)] = {EnemyType::Reflector, "Mirror Sentinel",
        "Mirror Sentinels are armored constructs whose frontal plating is composed of "
        "crystallized dimensional mirrors. These mirrors do not merely deflect physical "
        "attacks but redirect the kinetic energy back along its original vector. The "
        "shield cycles through phases of coherence, becoming briefly vulnerable when "
        "the mirror lattice needs to realign its dimensional frequency.",
        55, 12, 70, EnemyElement::None,
        "Shield cycles down for 1s every 4s, attack from behind",
        "Frontal shield blocks attacks (4s up / 1s down cycle), 0.2 armor",
        "Wait for shield down window. Attack from behind."};
    e[static_cast<int>(EnemyType::Leech)] = {EnemyType::Leech, "Void Leech",
        "Void Leeches are parasitic organisms that evolved in the nutrient-poor void "
        "between dimensions. They developed the ability to siphon life energy directly "
        "through dimensional barriers, converting stolen vitality into their own "
        "biomass. When wounded, they become frantic, their drain rate increasing as "
        "their survival instinct overrides all other behavior.",
        80, 5, 55, EnemyElement::None,
        "Slow, low direct damage, kill quickly before it heals",
        "Drains 2HP/0.5s at close range, heals equal amount. Faster below 50% HP",
        "Burst damage to kill fast. Keep distance."};

    e[static_cast<int>(EnemyType::Swarmer)] = {EnemyType::Swarmer, "Rift Swarmer",
        "Rift Swarmers are tiny interdimensional insects that travel in aggressive packs. "
        "Individually fragile, they overwhelm prey through sheer numbers and relentless "
        "zigzag pursuit patterns. Their amber exoskeletons vibrate at frequencies that "
        "disrupt spatial awareness, making them difficult to track in groups.",
        15, 6, 160, EnemyElement::None,
        "Low HP, easy to one-shot individually",
        "Fast zigzag movement, spawns in groups, jumps at player",
        "AoE attacks or wide melee sweeps. Don't let them surround you."};

    e[static_cast<int>(EnemyType::GravityWell)] = {EnemyType::GravityWell, "Gravity Well",
        "Gravity Wells are anomalous entities formed at points where dimensional boundaries "
        "are weakest. They exist as living singularities, drawing all matter toward their "
        "cores with an irresistible pull. Ancient texts describe them as 'the void's hunger "
        "given form' — patient predators that need only wait for prey to be dragged in.",
        50, 8, 40, EnemyElement::None,
        "Slow movement, can be outranged",
        "Gravitational pull drags player within 120px, floating, pulsing aura",
        "Ranged attacks from outside pull radius. Dash away when pulled in."};

    e[static_cast<int>(EnemyType::Mimic)] = {EnemyType::Mimic, "Mimic",
        "Mimics are shapeshifting predators that disguise themselves as mundane objects — "
        "most commonly supply crates. They wait with infinite patience for unsuspecting "
        "travelers to approach, then burst open with devastating lunge attacks. Some rift "
        "scholars theorize they were once normal creatures corrupted by prolonged exposure "
        "to unstable dimensional energies.",
        55, 18, 140, EnemyElement::None,
        "Predictable once revealed, high damage but slow attacks",
        "Disguised as crate, ambush lunge (18 DMG, 300 knockback), aggressive chase",
        "Approach crates carefully. Once revealed, parry the lunge."};

    s_bossEntries.resize(6);
    // Boss entries: {type, name, lore, HP, DMG, speed, element, weakness, abilities, effectiveWeapons}
    s_bossEntries[0] = {EnemyType::Walker, "Rift Guardian",
        "The Rift Guardian predates every civilization that has ever navigated the "
        "dimensional corridors. It was not created but emerged spontaneously when the "
        "first rift was torn open, as if the dimension itself generated an immune response. "
        "Ancient logs recovered from failed expeditions describe it as patient, "
        "methodical, and utterly without mercy toward anything that disrupts dimensional "
        "stability. It does not hate intruders. It simply removes them.",
        230, 23, 100, EnemyElement::None,
        "Phase 2 enrage leaves brief opening",
        "Shield burst, multi-shot, phase leap, 3 phases",
        "Learn attack patterns. Dash during shield burst."};
    s_bossEntries[1] = {EnemyType::Walker, "Void Wyrm",
        "The Void Wyrm inhabits the spaces between dimensions where no light from either "
        "side reaches, growing to immense size on a diet of raw dimensional energy. "
        "Its serpentine body can phase partially between realities, making conventional "
        "weapons pass harmlessly through sections of its form. The poison it spews is "
        "dimensionally corrupted venom that disrupts the molecular cohesion of anything "
        "it touches, slowly dissolving matter across both dimensions simultaneously.",
        190, 21, 145, EnemyElement::None,
        "Vulnerable after divebomb lands",
        "Poison clouds, dive bomb, bullet barrage, 3 phases",
        "Avoid green floor. Counter after divebomb."};
    s_bossEntries[2] = {EnemyType::Walker, "Dimensional Architect",
        "The Dimensional Architect is the only known entity that actively constructs "
        "within the rift rather than merely inhabiting it. It rearranges tiles and "
        "terrain instinctively, like a creature building a nest, though the structures "
        "it creates serve aggressive rather than defensive purposes. Physicists who "
        "studied its remains concluded that its core contains a miniature dimensional "
        "engine, granting it the power to rewrite local reality as casually as a human "
        "moves furniture.",
        220, 18, 90, EnemyElement::Electric,
        "Weak while constructing beam, destroy constructs",
        "Tile swap, rift zones, construct beam, arena collapse",
        "Destroy constructs fast. Use dim-switch to dodge rift zones."};
    s_bossEntries[3] = {EnemyType::Walker, "Temporal Weaver",
        "The Temporal Weaver exists at a fixed point in time, perceiving past and future "
        "simultaneously while remaining anchored to a single moment of dimensional "
        "convergence. This omniscience of time within its local rift makes it nearly "
        "impossible to surprise. The clock-like mechanisms visible through its translucent "
        "body are not decorative but functional temporal regulators that it can project "
        "outward to freeze, rewind, or stop time entirely for anyone caught in their radius.",
        280, 22, 100, EnemyElement::Ice,
        "Vulnerable in time-stop wind-up",
        "Time slow zones, clock sweep, time rewind, time stop",
        "Dash through slow zones. Learn sweep timing."};
    s_bossEntries[4] = {EnemyType::Walker, "Void Sovereign",
        "The Void Sovereign rules from the deepest layer of the dimensional void, a region "
        "so far from any natural reality that the laws of physics themselves begin to "
        "unravel. It does not fight out of territorial instinct or programmed defense but "
        "out of genuine contempt for anything that still belongs to a stable dimension. "
        "Survivors of its assault describe a palpable psychic pressure, as if the Sovereign "
        "is personally offended by the concept of matter having a fixed location.",
        400, 25, 120, EnemyElement::None,
        "Watch for phase transitions, avoid void storm",
        "Void orbs, rift slam, dim lock, laser sweep, void storm",
        "Stay mobile. Use dim-switch to counter dim-lock."};
    s_bossEntries[5] = {EnemyType::Walker, "Entropy Incarnate",
        "Entropy Incarnate is what happens when a dimensional suit begins to fail at a "
        "catastrophic rate and the escaping entropy energy gains enough coherence to "
        "develop awareness. It is drawn specifically to functioning suits, attracted by "
        "the same dimensional energy it was born from. Fighting it accelerates suit "
        "degradation because proximity to Entropy Incarnate warps the fabric that suit "
        "integrity depends on. In a cruel irony, the better shape your suit is in, the "
        "more ferociously this entity hunts you.",
        350, 24, 110, EnemyElement::None,
        "Entropy bursts predictable, brief immunity windows",
        "Entropy drain, entropy storm, dim lock, phase corruption",
        "Manage suit entropy carefully. Purge DoTs immediately."};
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
