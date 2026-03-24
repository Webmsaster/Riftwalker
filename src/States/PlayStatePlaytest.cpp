// PlayStatePlaytest.cpp -- Advanced balance playtest bot with combat AI, boss strategies, and zone tracking
#include "PlayState.h"
#include "Core/Game.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AbilityComponent.h"
#include "Core/AudioManager.h"
#include "Game/AchievementSystem.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/LoreSystem.h"
#include "Game/DailyRun.h"
#include "Game/Bestiary.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <algorithm>
#include <string>

FILE* g_playtestFile = nullptr;
extern int g_playtestClassLock;
extern int g_playtestProfile;

namespace {
const char* kZoneNames[] = {
    "Fractured Threshold", "Shifting Depths", "Resonant Core",
    "Entropy Cascade", "Sovereign's Domain"
};
const char* kProfileNames[] = {"balanced", "aggressive", "defensive", "speedrun"};

struct PlaytestProfile {
    float meleeRange;
    float rangedRange;
    int dashAttackChance;   // 1/N
    int chargeChance;       // 1/N
    float dodgeDimHPThresh;
    float reactionDelay;
    int dashFrequency;      // 1/N
    float exploreFrequency;
    bool prefersMelee;
};

constexpr PlaytestProfile kProfiles[] = {
    {80.0f, 300.0f, 4, 6, 0.25f, 0.25f, 20, 5.0f, false},   // balanced
    {100.0f, 200.0f, 2, 3, 0.15f, 0.18f, 10, 3.0f, true},    // aggressive
    {60.0f, 400.0f, 8, 8, 0.40f, 0.35f, 25, 6.0f, false},    // defensive
    {70.0f, 250.0f, 3, 5, 0.20f, 0.15f, 5, 2.5f, false},     // speedrun
};

// Relic scoring: positive = good for this class, negative = avoid
int scoreRelic(RelicID id, PlayerClass cls, const std::vector<ActiveRelic>& owned) {
    int score = 0;
    // Universal good picks
    if (id == RelicID::SoulSiphon) score += 5;
    if (id == RelicID::PhoenixFeather) score += 8;
    if (id == RelicID::VoidHunger) score += 7;
    if (id == RelicID::IronHeart) score += 3;
    if (id == RelicID::TimeDilator) score += 5;
    if (id == RelicID::EchoStrike) score += 4;
    if (id == RelicID::ChainLightning) score += 4;

    // Class synergies
    if (cls == PlayerClass::Voidwalker) {
        if (id == RelicID::RiftConduit) score += 6;
        if (id == RelicID::DualityGem) score += 5;
        if (id == RelicID::PhaseCloak) score += 4;
        if (id == RelicID::EntropyAnchor) score += 4;
    } else if (cls == PlayerClass::Berserker) {
        if (id == RelicID::BloodFrenzy) score += 7;
        if (id == RelicID::BerserkerCore) score += 6;
        if (id == RelicID::QuickHands) score += 4;
        if (id == RelicID::ThornMail) score += 3;
    } else if (cls == PlayerClass::Phantom) {
        if (id == RelicID::SwiftBoots) score += 5;
        if (id == RelicID::QuickHands) score += 5;
        if (id == RelicID::DimResidue) score += 3;
    }

    // Avoid cursed relics unless aggressive profile
    auto& data = RelicSystem::getRelicData(id);
    // Cursed relics have IDs >= CursedBlade
    if (id >= RelicID::CursedBlade) {
        score -= (g_playtestProfile == 1) ? 1 : 5; // aggressive tolerates cursed
    }

    // Check weapon synergies
    for (auto& r : owned) {
        // BloodFrenzy + RiftBlade synergy
        if (r.id == RelicID::BloodFrenzy && id == RelicID::SoulSiphon) score += 3;
        if (r.id == RelicID::ChainLightning && id == RelicID::EchoStrike) score += 2;
    }

    return score;
}

// Human reaction time: base + variance + fatigue, never frame-perfect
inline float humanReaction(float base, float multiplier, float fatigue) {
    // Gaussian-like variance: ±30% of base
    float variance = base * 0.3f * (static_cast<float>(std::rand() % 100 - 50) * 0.01f);
    float reaction = (base + variance + fatigue) * multiplier;
    return std::max(0.05f, reaction); // Never below 50ms
}

} // namespace

// ============================================================================
// LIFECYCLE
// ============================================================================

void PlayState::playtestLog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (!g_playtestFile) {
        g_playtestFile = fopen("playtest_report.log", "a");
    }
    if (g_playtestFile) {
        fprintf(g_playtestFile, "%s\n", buf);
        fflush(g_playtestFile);
    }
}

void PlayState::playtestStartRun() {
    m_playtestRun++;
    m_playtestRunTimer = 0;
    m_playtestRunActive = true;
    m_playtestReactionTimer = 0;
    m_playtestThinkTimer = 0;

    int classIdx = (g_playtestClassLock >= 0 && g_playtestClassLock <= 2)
        ? g_playtestClassLock : ((m_playtestRun - 1) / 3) % 3;
    g_selectedClass = static_cast<PlayerClass>(classIdx);
    const char* classNames[] = {"Voidwalker", "Berserker", "Phantom"};

    playtestLog("");
    playtestLog("=== Run %d / %s / %s ===", m_playtestRun, classNames[classIdx], kProfileNames[g_playtestProfile]);

    startNewRun();

    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    m_playtestLastHP = hp.currentHP;
    m_ptLastLoggedLevel = 0;
    m_ptLastLoggedRifts = 0;
    m_ptLastCompletedLevel = 0;
    m_ptStuckTimer = 0;
    m_ptLastCheckPos = {0, 0};
    m_ptDimSwitchCD = 0;
    m_ptDimExploreTimer = 0;
    m_ptLastEntropyLog = 0;
    m_ptFallCount = 0;
    m_ptNoProgressTimer = 0;
    m_ptBestDistToTarget = 99999.0f;
    m_ptNoProgressSkips = 0;
    m_ptSkipRiftMask = 0;
    m_ptAbilityCD = 0;
    m_ptChargeTimer = 0;
    m_ptCharging = false;
    m_ptFloorDmgTaken = 0;
    m_ptFloorTimeStart = 0;
    m_ptFloorKillsStart = 0;
    m_ptLastComboCount = 0;
    m_ptComboAttackTimer = 0;
    m_ptWavePrepTimer = 0;
    m_ptDefensiveFloor = false;
    // Human input variance: each run has slightly different base reaction time
    m_ptHumanReactionBase = 0.18f + static_cast<float>(std::rand() % 10) * 0.01f; // 0.18-0.27s
    m_ptHumanHesitation = 0;
    m_ptHumanDimDisorient = 0;
    m_ptHumanIdleTimer = 3.0f + static_cast<float>(std::rand() % 50) * 0.1f; // First idle in 3-8s
    m_ptHumanAimJitter = 0;
    m_ptHumanLastMoveDir = 0;
    m_ptHumanDirChangeDelay = 0;
    m_ptHumanFatigue = 0;

    playtestLog("  Class: %s | HP: %.0f | Speed: %.0f | Zone: %s",
        classNames[classIdx], hp.maxHP, m_player->moveSpeed, kZoneNames[0]);
}

void PlayState::playtestOnDeath() {
    int deathFloor = std::clamp(m_currentDifficulty - 1, 0, kMaxTrackedFloors - 1);
    m_ptFloorDeathCount[deathFloor]++;
    playtestEndRun(false);
}

void PlayState::playtestEndRun(bool success) {
    if (!m_playtestRunActive) return;
    m_playtestRunActive = false;
    if (!success) m_playtestDeaths++;
    if (m_currentDifficulty > m_playtestBestLevel)
        m_playtestBestLevel = m_currentDifficulty;
    writeBalanceSnapshot();

    int zone = getZone(m_currentDifficulty);
    playtestLog("  --- Run %d End: Floor %d (Zone %d: %s) | %.0fs | %d Kills | %s ---",
            m_playtestRun, m_currentDifficulty, zone + 1, kZoneNames[zone],
            m_playtestRunTimer, enemiesKilled, success ? "SUCCESS" : "DEATH");

    if (m_playtestRun >= m_playtestMaxRuns) {
        playtestWriteReport();
        game->quit();
        return;
    }
    playtestStartRun();
}

void PlayState::playtestWriteReport() {
    playtestLog("");
    playtestLog("================================================================");
    const char* classLabel = "all";
    if (g_playtestClassLock == 0) classLabel = "Voidwalker";
    else if (g_playtestClassLock == 1) classLabel = "Berserker";
    else if (g_playtestClassLock == 2) classLabel = "Phantom";
    playtestLog("  PLAYTEST BALANCE REPORT (%s / %s)", classLabel, kProfileNames[g_playtestProfile]);
    playtestLog("================================================================");
    playtestLog("  Runs: %d | Deaths: %d | Best Floor: %d", m_playtestRun, m_playtestDeaths, m_playtestBestLevel);
    playtestLog("");

    // Per-zone summary
    playtestLog("  --- ZONE SURVIVAL ---");
    for (int z = 0; z < 5; z++) {
        int startFloor = z * 6, endFloor = startFloor + 5;
        int zoneDeaths = 0; float zoneDmg = 0, zoneTime = 0; int zoneKills = 0, floorsReached = 0;
        for (int f = startFloor; f <= endFloor && f < kMaxTrackedFloors; f++) {
            zoneDeaths += m_ptFloorDeathCount[f];
            zoneDmg += m_ptFloorStats[f].damageTaken;
            zoneTime += m_ptFloorStats[f].timeSpent;
            zoneKills += m_ptFloorStats[f].enemiesKilled;
            if (m_ptFloorStats[f].timeSpent > 0) floorsReached++;
        }
        if (floorsReached == 0) continue;
        playtestLog("  Zone %d (%s): %d deaths | %.0f dmg | %d kills | %.0fs | %d floors",
            z + 1, kZoneNames[z], zoneDeaths, zoneDmg, zoneKills, zoneTime, floorsReached);
    }

    // Death heatmap
    playtestLog("");
    playtestLog("  --- DEATH HEATMAP ---");
    for (int f = 0; f < kMaxTrackedFloors; f++) {
        if (m_ptFloorDeathCount[f] > 0) {
            char bar[41] = {};
            int len = std::min(m_ptFloorDeathCount[f], 40);
            for (int i = 0; i < len; i++) bar[i] = '#';
            playtestLog("  F%-2d (Z%d): %2d %s", f + 1, getZone(f + 1) + 1, m_ptFloorDeathCount[f], bar);
        }
    }

    // Per-floor table
    playtestLog("");
    playtestLog("  --- PER-FLOOR STATS ---");
    playtestLog("  %-5s %-4s %-7s %-6s %-7s %-7s %-5s %-5s",
        "Floor", "Zone", "Dmg", "Kills", "Time", "HP", "Entr", "Boss");
    for (int f = 0; f < kMaxTrackedFloors; f++) {
        auto& fs = m_ptFloorStats[f];
        if (fs.timeSpent <= 0) continue;
        playtestLog("  %-5d %-4d %-7.0f %-6d %-7.0f %-7.0f %-5.0f %-5s",
            f + 1, getZone(f + 1) + 1, fs.damageTaken, fs.enemiesKilled,
            fs.timeSpent, fs.hpAtEnd, fs.entropyAtEnd * 100.0f,
            fs.bossFloor ? "YES" : "");
    }

    playtestLog("");
    playtestLog("================================================================");
    if (g_playtestFile) { fclose(g_playtestFile); g_playtestFile = nullptr; }
}

// ============================================================================
// SHOP: Auto-buy best upgrades with available shards
// ============================================================================

void PlayState::playtestBuyUpgrades() {
    auto& upgrades = game->getUpgradeSystem();
    // Priority order: survivability first, then damage, then utility
    static const UpgradeID priority[] = {
        UpgradeID::MaxHP,           // Most important: survive longer
        UpgradeID::MeleeDamage,     // Kill faster
        UpgradeID::Armor,           // Take less damage
        UpgradeID::RangedDamage,    // Ranged DPS
        UpgradeID::DashCooldown,    // Mobility = survival
        UpgradeID::CritChance,      // DPS spike
        UpgradeID::AbilityCooldown, // More abilities
        UpgradeID::EntropyResistance,// Entropy management
        UpgradeID::MoveSpeed,       // Navigation speed
        UpgradeID::SwitchCooldown,  // Dim-switch utility
        UpgradeID::ComboMaster,     // Combo damage
        UpgradeID::AbilityPower,    // Ability damage
        UpgradeID::JumpHeight,      // Platforming
        UpgradeID::ShieldCapacity,  // Shield hits
    };
    // Buy in priority order until out of shards
    bool bought = true;
    while (bought) {
        bought = false;
        for (auto id : priority) {
            if (upgrades.purchaseUpgrade(id)) {
                bought = true;
                break; // Re-check priorities after each buy (cheaper ones first)
            }
        }
    }
}

// ============================================================================
// MAIN UPDATE
// ============================================================================

void PlayState::updatePlaytest(float dt) {
    if (!m_player || !m_level || !m_playtestRunActive) return;

    m_playtestRunTimer += dt;
    m_playtestReactionTimer -= dt;
    m_playtestThinkTimer -= dt;
    m_ptDimSwitchCD -= dt;
    m_ptDimExploreTimer -= dt;
    m_ptAbilityCD -= dt;

    const auto& prof = kProfiles[g_playtestProfile];
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    float hpPct = hp.currentHP / std::max(hp.maxHP, 1.0f);

    // --- HP TRACKING ---
    float hpDiff = hp.currentHP - m_playtestLastHP;
    if (hpDiff < -1.0f) {
        m_ptFloorDmgTaken += (-hpDiff);
        if (-hpDiff > hp.maxHP * 0.15f) {
            playtestLog("  [%.0fs] F%d: HIT %.0f dmg (HP: %.0f/%.0f = %.0f%%)",
                m_playtestRunTimer, m_currentDifficulty, -hpDiff,
                hp.currentHP, hp.maxHP, hpPct * 100.0f);
        }
    }
    m_playtestLastHP = hp.currentHP;

    // --- ENTROPY LOGGING ---
    float entropyPct = m_entropy.getPercent();
    if (entropyPct > 0.8f && m_playtestRunTimer - m_ptLastEntropyLog > 10.0f) {
        playtestLog("  [%.0fs] F%d: ENTROPY %.0f%%", m_playtestRunTimer, m_currentDifficulty, entropyPct * 100.0f);
        m_ptLastEntropyLog = m_playtestRunTimer;
    }

    // --- LEVEL TRANSITION TRACKING ---
    if (m_currentDifficulty != m_ptLastLoggedLevel && m_ptLastLoggedLevel > 0) {
        writeBalanceSnapshot();
        int prevFloor = std::clamp(m_ptLastLoggedLevel - 1, 0, kMaxTrackedFloors - 1);
        auto& fs = m_ptFloorStats[prevFloor];
        fs.damageTaken += m_ptFloorDmgTaken;
        fs.enemiesKilled += (enemiesKilled - m_ptFloorKillsStart);
        fs.timeSpent += (m_playtestRunTimer - m_ptFloorTimeStart);
        fs.hpAtEnd = hp.currentHP;
        fs.entropyAtEnd = entropyPct;
        fs.bossFloor = isBossFloor(m_ptLastLoggedLevel) || isMidBossFloor(m_ptLastLoggedLevel);

        int prevZone = getZone(m_ptLastLoggedLevel);
        int newZone = getZone(m_currentDifficulty);
        playtestLog("  [%.0fs] F%d CLEAR (Z%d) | Dmg:%.0f Kills:%d HP:%.0f/%.0f",
            m_playtestRunTimer, m_ptLastLoggedLevel, prevZone + 1,
            m_ptFloorDmgTaken, enemiesKilled - m_ptFloorKillsStart, hp.currentHP, hp.maxHP);
        if (newZone != prevZone)
            playtestLog("  >>> ZONE %d: %s <<<", newZone + 1, kZoneNames[newZone]);

        // Buy upgrades with accumulated shards between levels
        int shardsBefore = game->getUpgradeSystem().getRiftShards();
        playtestBuyUpgrades();
        int shardsSpent = shardsBefore - game->getUpgradeSystem().getRiftShards();
        if (shardsSpent > 0)
            playtestLog("  [SHOP] Spent %d shards on upgrades (%d remaining)",
                shardsSpent, game->getUpgradeSystem().getRiftShards());

        m_ptFloorDmgTaken = 0;
        m_ptFloorTimeStart = m_playtestRunTimer;
        m_ptFloorKillsStart = enemiesKilled;
    }

    if (m_currentDifficulty != m_ptLastLoggedLevel) {
        m_ptLastLoggedLevel = m_currentDifficulty;
        m_ptLastLoggedRifts = 0;
        auto rifts = m_level->getRiftPositions();
        int enemyCount = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") != std::string::npos && e.isAlive()) enemyCount++;
        });
        int zone = getZone(m_currentDifficulty);
        int fiz = getFloorInZone(m_currentDifficulty);
        const char* floorType = isBossFloor(m_currentDifficulty) ? "ZONE BOSS"
            : isMidBossFloor(m_currentDifficulty) ? "MID-BOSS"
            : isBreatherFloor(m_currentDifficulty) ? "breather" : "normal";
        int fi = std::clamp(m_currentDifficulty - 1, 0, kMaxTrackedFloors - 1);
        m_ptFloorStats[fi].hpAtStart = hp.currentHP;
        // Run-to-run learning: check if this floor killed us before
        int fi2 = std::clamp(m_currentDifficulty - 1, 0, kMaxTrackedFloors - 1);
        m_ptDefensiveFloor = (m_ptFloorDeathCount[fi2] >= 2);
        const char* learnTag = m_ptDefensiveFloor ? " [DEFENSIVE - died here before]" : "";

        playtestLog("  F%d (Z%d.%d) | %d rifts | ~%d enemies | %s%s",
            m_currentDifficulty, zone + 1, fiz, (int)rifts.size(), enemyCount, floorType, learnTag);
        m_ptFloorDmgTaken = 0;
        m_ptFloorTimeStart = m_playtestRunTimer;
        m_ptFloorKillsStart = enemiesKilled;
    }

    if (m_levelComplete) return;

    // --- TIMEOUT ---
    float timeoutSec = 300.0f + getZone(m_currentDifficulty) * 150.0f;
    if (m_playtestRunTimer > timeoutSec) {
        playtestLog("  [%.0fs] TIMEOUT at F%d", m_playtestRunTimer, m_currentDifficulty);
        playtestOnDeath();
        return;
    }

    // --- COMBO TIMER ---
    m_ptComboAttackTimer -= dt;

    // --- RELIC CHOICE (scored, not random) ---
    if (m_showRelicChoice && !m_relicChoices.empty()) {
        if (m_playtestThinkTimer <= 0) {
            std::vector<ActiveRelic> owned;
            if (m_player->getEntity()->hasComponent<RelicComponent>())
                owned = m_player->getEntity()->getComponent<RelicComponent>().relics;
            int bestIdx = 0, bestScore = -999;
            for (int i = 0; i < static_cast<int>(m_relicChoices.size()); i++) {
                int s = scoreRelic(m_relicChoices[i], g_selectedClass, owned);
                if (s > bestScore) { bestScore = s; bestIdx = i; }
            }
            playtestLog("  [%.0fs] Relic: %s (score:%d)",
                m_playtestRunTimer, RelicSystem::getRelicData(m_relicChoices[bestIdx]).name, bestScore);
            selectRelic(bestIdx);
            m_playtestThinkTimer = 0.5f;
        }
        return;
    }

    // --- NPC: pick first dialog option (usually best reward) then dismiss ---
    if (m_showNPCDialog) {
        if (m_playtestThinkTimer <= 0) {
            if (m_nearNPCIndex >= 0) {
                handleNPCDialogChoice(m_nearNPCIndex, 0); // Pick first option (heal/buff)
            }
            m_showNPCDialog = false;
            m_playtestThinkTimer = 0.5f;
        }
        return;
    }

    // --- PUZZLES ---
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        if (m_playtestThinkTimer <= 0) {
            switch (m_activePuzzle->getType()) {
                case PuzzleType::Timing:
                    if (m_activePuzzle->isInSweetSpot()) { m_activePuzzle->handleInput(4); m_playtestThinkTimer = 0.3f; }
                    break;
                case PuzzleType::Sequence:
                    if (!m_activePuzzle->isShowingSequence()) {
                        int idx = (int)m_activePuzzle->getPlayerInput().size();
                        if (idx < (int)m_activePuzzle->getSequence().size()) {
                            m_activePuzzle->handleInput(m_activePuzzle->getSequence()[idx]);
                            m_playtestThinkTimer = 0.5f;
                        }
                    }
                    break;
                case PuzzleType::Alignment:
                    if (m_activePuzzle->getCurrentRotation() != m_activePuzzle->getTargetRotation()) {
                        m_activePuzzle->handleInput(1); m_playtestThinkTimer = 0.35f;
                    }
                    break;
                case PuzzleType::Pattern:
                    if (!m_activePuzzle->isPatternShowing()) {
                        for (int py = 0; py < 3; py++) for (int px = 0; px < 3; px++) {
                            if (m_activePuzzle->getPatternTarget(px, py) && !m_activePuzzle->getPatternPlayer(px, py)) {
                                int pcx = m_activePuzzle->getPatternCursorX(), pcy = m_activePuzzle->getPatternCursorY();
                                if (pcx < px) m_activePuzzle->handleInput(1);
                                else if (pcx > px) m_activePuzzle->handleInput(3);
                                else if (pcy < py) m_activePuzzle->handleInput(2);
                                else if (pcy > py) m_activePuzzle->handleInput(0);
                                else m_activePuzzle->handleInput(4);
                                m_playtestThinkTimer = 0.4f;
                                goto ptPatternDone;
                            }
                        }
                        ptPatternDone:;
                    }
                    break;
            }
        }
        return;
    }

    // ========================================================================
    // NAVIGATION
    // ========================================================================
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
    auto& inputMut = game->getInputMutable();
    int ptx = (int)(playerPos.x) / 32;
    int pty = (int)(playerPos.y) / 32;
    int dim = m_dimManager.getCurrentDimension();
    int otherDim = (dim == 1) ? 2 : 1;
    static float ptRecoveryGraceTimer = 0;
    if (ptRecoveryGraceTimer > 0) ptRecoveryGraceTimer -= dt;

    // ========================================================================
    // HUMAN INPUT SIMULATION
    // ========================================================================

    // Fatigue: reactions slow down over time (0.5% per minute)
    m_ptHumanFatigue = std::min(m_playtestRunTimer * 0.00008f, 0.12f); // Max +120ms at 25 min

    // Hesitation: brief freeze after taking a big hit (simulates "oh shit" moment)
    if (m_ptHumanHesitation > 0) {
        m_ptHumanHesitation -= dt;
        // During hesitation: no inputs at all (human freezes briefly)
        if (m_ptHumanHesitation > 0) return; // Skip entire frame — frozen in shock
    }
    if (hpDiff < -hp.maxHP * 0.2f) {
        // Big hit! Human hesitates 0.15-0.35s
        m_ptHumanHesitation = 0.15f + static_cast<float>(std::rand() % 20) * 0.01f;
    }

    // Disorientation after dimension switch (0.1-0.2s of impaired movement)
    if (m_ptHumanDimDisorient > 0) {
        m_ptHumanDimDisorient -= dt;
        // During disorientation: movement is sluggish (50% chance to not press move)
        if (m_ptHumanDimDisorient > 0 && std::rand() % 2 == 0) {
            // Skip movement input this frame (disoriented)
        }
    }

    // Occasional idle: human takes a breath, assesses situation (0.3-0.6s every 8-15s)
    m_ptHumanIdleTimer -= dt;
    if (m_ptHumanIdleTimer <= 0) {
        m_ptHumanIdleTimer = 8.0f + static_cast<float>(std::rand() % 70) * 0.1f;
        // Brief pause — no combat or movement for a moment
        m_playtestReactionTimer = 0.3f + static_cast<float>(std::rand() % 30) * 0.01f;
    }

    // Direction change smoothing: don't instantly reverse
    m_ptHumanDirChangeDelay -= dt;

    // Aim jitter: recalculate every 0.3s
    if (std::rand() % 18 == 0) {
        m_ptHumanAimJitter = (static_cast<float>(std::rand() % 20) - 10.0f) * 0.02f; // ±0.2 radians
    }

    // --- WAVE PREPARATION: position defensively before wave spawns ---
    if (m_waveTimer > 0 && m_waveTimer < 1.5f && m_currentWave < (int)m_spawnWaves.size()) {
        if (phys.onGround || phys.canCoyoteJump())
            inputMut.injectActionPress(Action::Jump);
        float levelCenterX = m_level->getPixelWidth() * 0.5f;
        if (playerPos.x < levelCenterX) inputMut.injectActionPress(Action::MoveRight);
        else inputMut.injectActionPress(Action::MoveLeft);
        m_ptWavePrepTimer += dt;
    }

    // Auto-rescue: fell below map
    {
        Vec2 spawn = m_level->getSpawnPoint();
        float mapBottom = m_level->getPixelHeight() * 0.85f;
        if ((playerPos.y > spawn.y + std::max(400.0f, m_level->getPixelHeight() * 0.4f)
             || playerPos.y > mapBottom) && ptRecoveryGraceTimer <= 0) {
            auto& transform = m_player->getEntity()->getComponent<TransformComponent>();
            transform.position = spawn;
            phys.velocity = {0, 0};
            m_ptFallCount++;
            if (m_ptFallCount % 3 == 0) inputMut.injectActionPress(Action::DimensionSwitch);
            return;
        }
    }

    // Detect actual dimension switch (for disorientation)
    static int ptLastDim = 0;
    if (dim != ptLastDim && ptLastDim != 0) {
        m_ptHumanDimDisorient = 0.1f + static_cast<float>(std::rand() % 10) * 0.01f; // 0.1-0.2s
    }
    ptLastDim = dim;

    // Preliminary move direction (toward exit, refined later with actual target)
    int moveDir = (m_level->getExitPoint().x >= playerPos.x) ? 1 : -1;

    // Hazard awareness: scan 3 tiles ahead in move direction + underfoot
    bool hazardNearby = false;
    bool hazardAhead = false;
    for (int scanDist = -1; scanDist <= 4; scanDist++) {
        for (int scanDy = -1; scanDy <= 2; scanDy++) {
            int hx = ptx + moveDir * scanDist, hy = pty + scanDy;
            if (m_level->inBounds(hx, hy)) {
                auto& ht = m_level->getTile(hx, hy, dim);
                if (ht.type == TileType::Spike || ht.type == TileType::Fire || ht.type == TileType::LaserEmitter) {
                    hazardNearby = true;
                    if (scanDist >= 1 && scanDist <= 3) hazardAhead = true;
                }
            }
        }
    }

    // Pickup routing: prioritize health pickups when low HP, grab any nearby pickups on path
    Vec2 nearestPickupPos = {0, 0};
    float nearestPickupDist = 99999.0f;
    bool seekingPickup = false;
    {
        float searchRadius = (hpPct < 0.35f) ? 500.0f : 150.0f; // Desperate = wider search
        float bestScore = -1.0f;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("pickup") == std::string::npos || !e.isAlive()) return;
            if (!e.hasComponent<TransformComponent>()) return;
            auto& et = e.getComponent<TransformComponent>();
            float dx2 = et.getCenter().x - playerPos.x, dy2 = et.getCenter().y - playerPos.y;
            float d = std::sqrt(dx2 * dx2 + dy2 * dy2);
            if (d > searchRadius) return;

            // Score pickups: health = very high when low HP, shards = moderate, buffs = low
            float score = 0;
            bool isHealth = (e.getTag().find("health") != std::string::npos);
            bool isShard = (e.getTag().find("shard") != std::string::npos);
            if (isHealth) score = (hpPct < 0.35f) ? 100.0f : 20.0f;
            else if (isShard) score = 10.0f;
            else score = 15.0f; // shield/speed/damage boost

            // Closer = better (distance penalty)
            score -= d * 0.1f;

            if (score > bestScore) {
                bestScore = score;
                nearestPickupDist = d;
                nearestPickupPos = et.getCenter();
            }
        });
        if (bestScore > 0 && nearestPickupDist < searchRadius) seekingPickup = true;
    }

    // Breakable wall detection: dash/attack nearby breakable walls for secret rooms
    bool breakableWallNearby = false;
    for (int scanDx = -2; scanDx <= 3; scanDx++) {
        for (int scanDy = -2; scanDy <= 2; scanDy++) {
            if (m_level->inBounds(ptx + scanDx, pty + scanDy)) {
                auto& bwt = m_level->getTile(ptx + scanDx, pty + scanDy, dim);
                if (bwt.type == TileType::Breakable) breakableWallNearby = true;
            }
        }
    }
    // Attack breakable walls when near (dash or melee)
    if (breakableWallNearby && m_playtestReactionTimer <= 0) {
        if (m_player->dashCooldownTimer <= 0) {
            inputMut.injectActionPress(Action::Dash);
        } else if (combat.canAttack()) {
            combat.startAttack(AttackType::Melee, {static_cast<float>(moveDir), 0});
        }
    }

    // Shrine-seeking: find beneficial shrines and evaluate risk
    Vec2 nearestShrinePos = {0, 0};
    float nearestShrineDist = 99999.0f;
    bool seekingShrine = false;
    if (!seekingPickup) {
        auto& events = m_level->getRandomEvents();
        for (auto& ev : events) {
            if (ev.type != RandomEventType::Shrine || ev.used) continue;
            float sdx = ev.position.x - playerPos.x, sdy = ev.position.y - playerPos.y;
            float sd = std::sqrt(sdx * sdx + sdy * sdy);
            if (sd > 500.0f) continue; // Only seek nearby shrines

            // Evaluate: is this shrine safe to use?
            bool shouldUse = false;
            switch (ev.shrineType) {
                case ShrineType::Vitality: shouldUse = true; break; // Free HP, always good
                case ShrineType::Renewal:  shouldUse = (hpPct < 0.6f); break; // Heal when needed
                case ShrineType::Power:    shouldUse = (hp.currentHP > 50.0f); break; // HP cost
                case ShrineType::Speed:    shouldUse = (hp.currentHP > 40.0f); break;
                case ShrineType::Entropy:  shouldUse = (entropyPct > 0.5f && hp.currentHP > 30.0f); break;
                case ShrineType::Shards:   shouldUse = (entropyPct < 0.4f); break; // Entropy cost
                default: break;
            }
            if (shouldUse && sd < nearestShrineDist) {
                nearestShrineDist = sd;
                nearestShrinePos = ev.position;
                seekingShrine = true;
            }
        }
    }

    // Find target: nearest unrepaired rift or exit
    Vec2 target = seekingPickup ? nearestPickupPos : (seekingShrine ? nearestShrinePos : m_level->getExitPoint());
    auto rifts = m_level->getRiftPositions();
    float nearestRiftDist = 99999.0f;
    int targetRiftIdx = -1;
    int targetRequiredDim = 0;

    if (!m_collapsing) {
        for (int ri = 0; ri < (int)rifts.size(); ri++) {
            if (m_repairedRiftIndices.count(ri)) continue;
            if (m_ptSkipRiftMask & (1 << ri)) continue;
            float dx = rifts[ri].x - playerPos.x, dy = rifts[ri].y - playerPos.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < nearestRiftDist) {
                nearestRiftDist = d; target = rifts[ri];
                targetRiftIdx = ri; targetRequiredDim = m_level->getRiftRequiredDimension(ri);
            }
        }
        if (nearestRiftDist > 99998.0f && m_ptSkipRiftMask != 0) {
            m_ptSkipRiftMask = 0;
            for (int ri = 0; ri < (int)rifts.size(); ri++) {
                if (m_repairedRiftIndices.count(ri)) continue;
                float dx = rifts[ri].x - playerPos.x, dy = rifts[ri].y - playerPos.y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < nearestRiftDist) {
                    nearestRiftDist = d; target = rifts[ri];
                    targetRiftIdx = ri; targetRequiredDim = m_level->getRiftRequiredDimension(ri);
                }
            }
        }
    }

    float dirX = target.x - playerPos.x, dirY = target.y - playerPos.y;
    float distToTarget = std::sqrt(dirX * dirX + dirY * dirY);
    moveDir = (dirX >= 0) ? 1 : -1; // Refine with actual target

    // Dimension alignment for rift target
    if (!m_collapsing && targetRequiredDim > 0 && targetRequiredDim != dim && m_ptDimSwitchCD <= 0) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptDimSwitchCD = 1.0f;
    }

    // Entropy-aware dim switching: avoid Dim B if entropy is high
    if (entropyPct > 0.6f && dim == 2 && m_ptDimSwitchCD <= 0 && targetRequiredDim != 2) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptDimSwitchCD = 2.0f;
    }

    // Progress tracking
    if (distToTarget < m_ptBestDistToTarget - 20.0f) {
        m_ptBestDistToTarget = distToTarget; m_ptNoProgressTimer = 0;
    } else {
        m_ptNoProgressTimer += dt;
    }
    float noProgThreshold = m_collapsing ? 4.0f : 7.0f;
    if (m_ptNoProgressTimer > noProgThreshold) {
        m_ptNoProgressTimer = 0; m_ptBestDistToTarget = 99999.0f; m_ptNoProgressSkips++;
        auto& transform = m_player->getEntity()->getComponent<TransformComponent>();
        if (m_ptNoProgressSkips <= 1) {
            inputMut.injectActionPress(Action::DimensionSwitch);
            transform.position = m_level->getSpawnPoint();
            phys.velocity = {0, 0}; m_ptLastCheckPos = m_level->getSpawnPoint();
            return;
        } else {
            transform.position = {target.x - 16.0f, target.y - 32.0f};
            phys.velocity = {0, 0}; ptRecoveryGraceTimer = 1.5f;
            if (!m_collapsing && targetRiftIdx >= 0 && m_level->isRiftActiveInDimension(targetRiftIdx, dim))
                inputMut.injectActionPress(Action::Interact);
            m_ptLastCheckPos = target; m_ptNoProgressSkips = 0;
            return;
        }
    }

    // Stuck detection
    float moved = std::sqrt((playerPos.x - m_ptLastCheckPos.x) * (playerPos.x - m_ptLastCheckPos.x)
                          + (playerPos.y - m_ptLastCheckPos.y) * (playerPos.y - m_ptLastCheckPos.y));
    if (moved < 30.0f) m_ptStuckTimer += dt;
    else { m_ptStuckTimer = 0; m_ptLastCheckPos = playerPos; }

    if (m_ptStuckTimer > 3.0f && m_ptStuckTimer < 3.0f + dt * 3) inputMut.injectActionPress(Action::DimensionSwitch);
    if (m_ptStuckTimer > 5.0f && m_ptStuckTimer < 5.0f + dt * 3) {
        inputMut.injectActionPress(Action::Jump);
        inputMut.injectActionPress(dirX >= 0 ? Action::MoveLeft : Action::MoveRight);
    }
    if (m_ptStuckTimer > 8.0f) {
        auto& transform = m_player->getEntity()->getComponent<TransformComponent>();
        transform.position = m_level->getSpawnPoint();
        phys.velocity = {0, 0}; inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptStuckTimer = 0; m_ptLastCheckPos = m_level->getSpawnPoint();
        return;
    }

    // Floor/wall scanning
    bool hasFloorAhead = false, wallAhead = false, hasFloorBelow = false;
    for (int dx = 1; dx <= 3; dx++) {
        int cx = ptx + moveDir * dx;
        if (dx == 1 && m_level->inBounds(cx, pty) && m_level->isSolid(cx, pty, dim)) wallAhead = true;
        for (int dy = 0; dy <= 3; dy++) {
            if (m_level->inBounds(cx, pty + 1 + dy) &&
                (m_level->isSolid(cx, pty + 1 + dy, dim) || m_level->isOneWay(cx, pty + 1 + dy, dim))) {
                hasFloorAhead = true; break;
            }
        }
        if (hasFloorAhead) break;
    }
    for (int dy = 1; dy <= 2; dy++) {
        if (m_level->inBounds(ptx, pty + dy) &&
            (m_level->isSolid(ptx, pty + dy, dim) || m_level->isOneWay(ptx, pty + dy, dim))) {
            hasFloorBelow = true; break;
        }
    }

    // Dim switch when blocked
    if ((wallAhead || (!hasFloorAhead && phys.onGround) || (!hasFloorBelow && phys.onGround)) && m_ptDimSwitchCD <= 0) {
        bool otherBetter = false;
        if (wallAhead) {
            int cx = ptx + moveDir;
            otherBetter = m_level->inBounds(cx, pty) && !m_level->isSolid(cx, pty, otherDim);
        }
        if (!hasFloorAhead || !hasFloorBelow) {
            for (int dx2 = 0; dx2 <= 3; dx2++) {
                int cx = ptx + moveDir * dx2;
                for (int dy2 = 0; dy2 <= 3; dy2++) {
                    if (m_level->inBounds(cx, pty + 1 + dy2) &&
                        (m_level->isSolid(cx, pty + 1 + dy2, otherDim) || m_level->isOneWay(cx, pty + 1 + dy2, otherDim))) {
                        otherBetter = true; break;
                    }
                }
                if (otherBetter) break;
            }
        }
        // Also check if other dim has fewer hazards
        if (hazardNearby && !otherBetter) {
            bool otherSafe = true;
            for (int dx2 = -1; dx2 <= 1; dx2++) for (int dy2 = -1; dy2 <= 1; dy2++) {
                if (m_level->inBounds(ptx + dx2, pty + dy2)) {
                    auto& t = m_level->getTile(ptx + dx2, pty + dy2, otherDim);
                    if (t.type == TileType::Spike || t.type == TileType::Fire || t.type == TileType::LaserEmitter)
                        otherSafe = false;
                }
            }
            if (otherSafe) otherBetter = true;
        }
        if (otherBetter) { inputMut.injectActionPress(Action::DimensionSwitch); m_ptDimSwitchCD = 1.0f; }
    }

    // Horizontal movement with human-like direction change delay
    {
        int wantDir = 0;
        if (dirX > 30.0f) wantDir = 1;
        else if (dirX < -30.0f) wantDir = -1;
        else if (std::abs(dirY) > 200.0f)
            wantDir = ((int)(m_playtestRunTimer * 2) % 8 < 4) ? 1 : -1;

        // Direction change smoothing: 80ms delay when reversing (human momentum)
        if (wantDir != 0 && wantDir != m_ptHumanLastMoveDir && m_ptHumanDirChangeDelay > 0) {
            // Still moving in old direction briefly (human can't instantly reverse)
            if (m_ptHumanLastMoveDir > 0) inputMut.injectActionPress(Action::MoveRight);
            else if (m_ptHumanLastMoveDir < 0) inputMut.injectActionPress(Action::MoveLeft);
        } else {
            if (wantDir > 0) inputMut.injectActionPress(Action::MoveRight);
            else if (wantDir < 0) inputMut.injectActionPress(Action::MoveLeft);
            if (wantDir != m_ptHumanLastMoveDir) {
                m_ptHumanDirChangeDelay = 0.06f + static_cast<float>(std::rand() % 4) * 0.01f;
                m_ptHumanLastMoveDir = wantDir;
            }
        }
    }

    // Human input error: occasional wrong button press (2% chance, 5% at low HP = panic)
    float errorChance = (hpPct < 0.2f) ? 0.05f : 0.02f;
    if (static_cast<float>(std::rand() % 1000) * 0.001f < errorChance) {
        // Panic press: random action
        int randomAction = std::rand() % 4;
        if (randomAction == 0) inputMut.injectActionPress(Action::Jump);
        else if (randomAction == 1) inputMut.injectActionPress(Action::Dash);
        else if (randomAction == 2) inputMut.injectActionPress(Action::MoveLeft);
        else inputMut.injectActionPress(Action::MoveRight);
    }

    // Jump logic with variable height: hold jump for tall jumps, tap for short hops
    bool blocked = (phys.onWallLeft || phys.onWallRight) || (std::abs(phys.velocity.x) < 10.0f && std::abs(dirX) > 50.0f);
    bool targetAbove = dirY < -40.0f;
    bool targetFarAbove = dirY < -120.0f;

    // Coyote time: can still jump for 0.1s after leaving ground
    bool canJump = phys.onGround || phys.canCoyoteJump();

    if (canJump) {
        bool shouldJump = blocked || targetAbove || !hasFloorAhead || hazardNearby || hazardAhead;
        if (shouldJump && std::rand() % 100 < 90)
            inputMut.injectActionPress(Action::Jump);
        if (std::rand() % 40 == 0) inputMut.injectActionPress(Action::Jump);
    }
    // Variable jump height: keep holding jump for tall jumps (target far above),
    // let go for short hops (target near or same height).
    // injectActionPress lasts 1 frame; by NOT re-injecting, the jump becomes short.
    if (!phys.onGround && phys.velocity.y < 0 && targetFarAbove) {
        // Hold jump: re-inject to prevent early release velocity cut
        inputMut.injectActionPress(Action::Jump);
    }

    // Wall-jump + chain: immediately double-jump after wall-jump if target is above
    if (m_player->isWallSliding) {
        inputMut.injectActionPress(Action::Jump); // Always wall-jump
    }

    // Double-jump: smarter usage
    if (!phys.onGround && !m_player->isWallSliding && m_player->jumpsRemaining > 0) {
        bool falling = phys.velocity.y > 150.0f;
        bool noPlatformBelow = true;
        for (int dy = 1; dy <= 5; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (m_level->inBounds(ptx + dx, pty + dy) &&
                    (m_level->isSolid(ptx + dx, pty + dy, dim) || m_level->isOneWay(ptx + dx, pty + dy, dim))) {
                    noPlatformBelow = false; break;
                }
            }
            if (!noPlatformBelow) break;
        }
        if ((targetAbove && std::rand() % 6 == 0) || (falling && noPlatformBelow))
            inputMut.injectActionPress(Action::Jump);
    }

    // Dash
    if (m_player->dashCooldownTimer <= 0) {
        if (phys.onGround && hasFloorAhead && (distToTarget > 300.0f || std::rand() % prof.dashFrequency == 0))
            inputMut.injectActionPress(Action::Dash);
    }

    // Interact with rift
    if (nearestRiftDist < 60.0f && !m_activePuzzle && m_nearRiftIndex >= 0 &&
        m_level->isRiftActiveInDimension(m_nearRiftIndex, dim))
        inputMut.injectActionPress(Action::Interact);

    // Interact with shrine/event/NPC when nearby (within 50px of target)
    if ((seekingShrine && nearestShrineDist < 50.0f) || (m_nearNPCIndex >= 0 && !m_showNPCDialog)) {
        inputMut.injectActionPress(Action::Interact);
    }

    // Log rift repair
    if (m_levelRiftsRepaired > m_ptLastLoggedRifts) m_ptLastLoggedRifts = m_levelRiftsRepaired;

    // ========================================================================
    // COMBAT AI
    // ========================================================================

    // Pre-scan: count threats for retreat decision
    int preScanMeleeThreats = 0;
    float preScanNearestDist = 99999.0f;
    Vec2 preScanThreatCenter = {0, 0};
    m_entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") == std::string::npos || !e.isAlive()) return;
        if (!e.hasComponent<TransformComponent>()) return;
        auto& et = e.getComponent<TransformComponent>();
        float dx2 = et.getCenter().x - playerPos.x, dy2 = et.getCenter().y - playerPos.y;
        float d = std::sqrt(dx2 * dx2 + dy2 * dy2);
        if (d < 120.0f) {
            preScanMeleeThreats++;
            preScanThreatCenter.x += et.getCenter().x;
            preScanThreatCenter.y += et.getCenter().y;
        }
        if (d < preScanNearestDist) preScanNearestDist = d;
    });

    // RETREAT: kite backward when overwhelmed at low HP
    // Defensive floors (died here before): trigger retreat at higher HP
    float retreatThreshold = m_ptDefensiveFloor ? 0.45f : 0.30f;
    int retreatEnemyCount = m_ptDefensiveFloor ? 1 : 2;
    if (hpPct < retreatThreshold && preScanMeleeThreats >= retreatEnemyCount && preScanNearestDist < 100.0f) {
        // Move AWAY from threat center
        if (preScanMeleeThreats > 0) {
            preScanThreatCenter.x /= preScanMeleeThreats;
            preScanThreatCenter.y /= preScanMeleeThreats;
        }
        float retreatDirX = playerPos.x - preScanThreatCenter.x;
        if (retreatDirX >= 0) inputMut.injectActionPress(Action::MoveRight);
        else inputMut.injectActionPress(Action::MoveLeft);
        // Jump to gain height advantage
        if (canJump) inputMut.injectActionPress(Action::Jump);
        // Dash away if possible
        if (m_player->dashCooldownTimer <= 0) inputMut.injectActionPress(Action::Dash);
        // Switch to ranged while kiting
        if (m_playtestReactionTimer <= 0 && combat.canAttack() && preScanNearestDist < 300.0f) {
            Vec2 rd = {retreatDirX > 0 ? -1.0f : 1.0f, 0};
            combat.startAttack(AttackType::Ranged, rd);
            m_playtestReactionTimer = humanReaction(m_ptHumanReactionBase, 1.5f, m_ptHumanFatigue);
        }
        goto ptPostCombat; // Skip normal combat while retreating
    }

    if (m_playtestReactionTimer > 0) goto ptPostCombat;
    {
        // Scan all enemies: find nearest, count nearby, detect boss
        float nearestEnemy = 99999.0f;
        Vec2 enemyDir = {1.0f, 0.0f};
        bool nearestIsBoss = false;
        float nearestEnemyHP = 0;
        float nearestEnemyMaxHP = 0;
        int enemiesInMeleeRange = 0;
        int enemiesInRangedRange = 0;
        bool bossIsTelegraphing = false;
        int bossPhase = 1;

        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos || !e.isAlive()) return;
            if (!e.hasComponent<TransformComponent>()) return;
            auto& et = e.getComponent<TransformComponent>();
            float dx2 = et.getCenter().x - playerPos.x, dy2 = et.getCenter().y - playerPos.y;
            float d = std::sqrt(dx2 * dx2 + dy2 * dy2);
            if (d < 120.0f) enemiesInMeleeRange++;
            if (d < 350.0f) enemiesInRangedRange++;
            if (d < nearestEnemy) {
                nearestEnemy = d;
                // Human aim: slightly off-target (jitter)
                float jx = dx2, jy = dy2;
                if (d > 0) {
                    float angle = std::atan2(jy, jx) + m_ptHumanAimJitter;
                    jx = std::cos(angle) * d;
                    jy = std::sin(angle) * d;
                }
                enemyDir = (d > 0) ? Vec2{jx / d, jy / d} : Vec2{1.0f, 0.0f};
                if (e.hasComponent<AIComponent>()) {
                    auto& ai = e.getComponent<AIComponent>();
                    nearestIsBoss = (ai.enemyType == EnemyType::Boss);
                    bossIsTelegraphing = (ai.bossTelegraphTimer > 0);
                    if (nearestIsBoss) bossPhase = ai.bossPhase;
                }
                if (e.hasComponent<HealthComponent>()) {
                    nearestEnemyHP = e.getComponent<HealthComponent>().currentHP;
                    nearestEnemyMaxHP = e.getComponent<HealthComponent>().maxHP;
                }
            }
        });

        // --- BOSS TELEGRAPH DODGE ---
        if (bossIsTelegraphing && nearestEnemy < 250.0f) {
            // Boss is winding up: dodge via dimension switch or dash away
            if (m_ptDimSwitchCD <= 0 && std::rand() % 2 == 0) {
                inputMut.injectActionPress(Action::DimensionSwitch);
                m_ptDimSwitchCD = 1.5f;
            } else if (m_player->dashCooldownTimer <= 0) {
                // Dash perpendicular to boss
                inputMut.injectActionPress(enemyDir.x > 0 ? Action::MoveLeft : Action::MoveRight);
                inputMut.injectActionPress(Action::Dash);
            }
            // Activate Rift Shield preemptively
            if (m_player->getEntity()->hasComponent<AbilityComponent>()) {
                auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
                if (abil.abilities[1].isReady()) { // Rift Shield
                    abil.activate(1);
                    m_ptAbilityCD = 2.0f;
                }
            }
            m_playtestReactionTimer = 0.3f;
            goto ptPostCombat;
        }

        // --- BOSS PHASE + DEFENSIVE FLOOR ADAPTATION ---
        float effectiveMeleeRange = prof.meleeRange;
        float effectiveAbilityCD = 3.0f;
        // Boss phase adaptation
        if (nearestIsBoss && bossPhase >= 2) {
            effectiveMeleeRange *= 0.7f;
            effectiveAbilityCD = 2.0f;
        }
        if (nearestIsBoss && bossPhase >= 3) {
            effectiveMeleeRange *= 0.6f;
            effectiveAbilityCD = 1.0f;
        }
        // Defensive floor: play cautiously (learned from previous deaths)
        if (m_ptDefensiveFloor) {
            effectiveMeleeRange *= 0.8f;  // Stay at range more
            effectiveAbilityCD *= 0.7f;   // Use abilities more freely
        }

        // --- CHARGED ATTACK (hold and release) ---
        if (m_ptCharging) {
            m_ptChargeTimer -= dt;
            if (m_ptChargeTimer <= 0) {
                combat.releaseCharged(enemyDir);
                m_ptCharging = false;
            }
            m_playtestReactionTimer = 0.05f;
            goto ptPostCombat;
        }

        // --- PARRY attempt on Boss melee (when close and boss not telegraphing ranged) ---
        if (nearestIsBoss && nearestEnemy < 70.0f && !bossIsTelegraphing
            && combat.parryCooldown <= 0 && std::rand() % 8 == 0) {
            combat.startParry();
            m_playtestReactionTimer = 0.2f;
            goto ptPostCombat;
        }

        // --- COUNTER after successful parry ---
        if (combat.counterReady && nearestEnemy < 100.0f) {
            combat.startCounterAttack(0.3f);
            m_playtestReactionTimer = 0.35f;
            goto ptPostCombat;
        }

        // --- GROUND SLAM: use when 3+ enemies in melee range ---
        if (enemiesInMeleeRange >= 3 && m_ptAbilityCD <= 0 &&
            m_player->getEntity()->hasComponent<AbilityComponent>()) {
            auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
            if (abil.abilities[0].isReady()) { // Ground Slam
                abil.activate(0);
                m_ptAbilityCD = effectiveAbilityCD + 1.0f;
                m_playtestReactionTimer = 0.5f;
                goto ptPostCombat;
            }
        }

        // --- PHASE STRIKE: teleport-nuke on isolated target or boss ---
        if ((nearestIsBoss || (nearestEnemy > 80.0f && nearestEnemy < 200.0f && nearestEnemyHP > 40.0f))
            && m_ptAbilityCD <= 0 && m_player->getEntity()->hasComponent<AbilityComponent>()) {
            auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
            if (abil.abilities[2].isReady()) { // Phase Strike
                abil.activate(2);
                m_ptAbilityCD = effectiveAbilityCD;
                m_playtestReactionTimer = 0.4f;
                goto ptPostCombat;
            }
        }

        // --- RIFT SHIELD: use when low HP and enemies close ---
        if (hpPct < 0.35f && nearestEnemy < 150.0f && m_ptAbilityCD <= 0
            && m_player->getEntity()->hasComponent<AbilityComponent>()) {
            auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
            if (abil.abilities[1].isReady()) { // Rift Shield
                abil.activate(1);
                m_ptAbilityCD = effectiveAbilityCD;
                m_playtestReactionTimer = 0.3f;
                goto ptPostCombat;
            }
        }

        // --- DASH-ATTACK toward mid-range enemy ---
        if (nearestEnemy > effectiveMeleeRange && nearestEnemy < 200.0f && m_player->dashCooldownTimer <= 0
            && std::rand() % prof.dashAttackChance == 0) {
            inputMut.injectActionPress(enemyDir.x > 0 ? Action::MoveRight : Action::MoveLeft);
            inputMut.injectActionPress(Action::Dash);
            m_playtestReactionTimer = humanReaction(m_ptHumanReactionBase, 1.5f, m_ptHumanFatigue);
            goto ptPostCombat;
        }

        // --- CHARGED ATTACK: start charging on boss or heavy enemy ---
        if (nearestEnemy < effectiveMeleeRange * 1.25f && (nearestIsBoss || nearestEnemyHP > 60.0f)
            && !m_ptCharging && combat.canAttack() && std::rand() % prof.chargeChance == 0) {
            combat.startCharging();
            m_ptCharging = true;
            m_ptChargeTimer = 0.35f + static_cast<float>(std::rand() % 20) * 0.01f;
            m_playtestReactionTimer = 0.05f;
            goto ptPostCombat;
        }

        // --- AERIAL ATTACK: melee/ranged while airborne ---
        if (!phys.onGround && nearestEnemy < effectiveMeleeRange * 1.5f && combat.canAttack()) {
            combat.startAttack(AttackType::Melee, enemyDir);
            m_playtestReactionTimer = humanReaction(m_ptHumanReactionBase, 1.0f, m_ptHumanFatigue);
            goto ptPostCombat;
        }

        // --- MELEE WITH COMBO TRACKING ---
        // Pace attacks to maintain combo: hit within 0.5s window for combo buildup
        // After 3rd hit, CombatSystem auto-triggers finisher
        if (nearestEnemy < effectiveMeleeRange && combat.canAttack()) {
            // If in combo, time next hit to stay within window
            if (combat.comboCount > 0 && combat.comboTimer > 0 && m_ptComboAttackTimer > 0) {
                // Wait for combo timer to optimize spacing (don't mash too fast)
                m_playtestReactionTimer = 0.05f;
            } else {
                combat.startAttack(AttackType::Melee, enemyDir);
                m_ptComboAttackTimer = 0.15f; // Brief pause between combo hits (human-like)
                m_playtestReactionTimer = humanReaction(m_ptHumanReactionBase, 0.8f, m_ptHumanFatigue);
            }
        }
        // --- RANGED: prefer when multiple enemies or not melee-preferred ---
        else if (nearestEnemy < prof.rangedRange && combat.canAttack()
                 && (!prof.prefersMelee || enemiesInMeleeRange == 0 || enemiesInRangedRange >= 3)) {
            combat.startAttack(AttackType::Ranged, enemyDir);
            m_playtestReactionTimer = humanReaction(m_ptHumanReactionBase, 1.5f, m_ptHumanFatigue);
        }
        // --- SPEEDRUN: skip distant enemies ---
        else if (g_playtestProfile == 3 && nearestEnemy > 200.0f) {
            m_playtestReactionTimer = 0.05f;
        }
        else {
            m_playtestReactionTimer = 0.1f;
        }

        // --- DEFENSIVE DIM SWITCH at low HP ---
        if (hpPct < prof.dodgeDimHPThresh && nearestEnemy < 120.0f && m_ptDimSwitchCD <= 0
            && std::rand() % 3 == 0) {
            inputMut.injectActionPress(Action::DimensionSwitch);
            m_ptDimSwitchCD = 2.0f;
        }

        // --- VOIDWALKER: aggro burst after dim-switch (Rift Charge active) ---
        if (g_selectedClass == PlayerClass::Voidwalker && m_player->riftChargeTimer > 0
            && nearestEnemy < 150.0f && combat.canAttack()) {
            // Rift Charge gives +30% DMG — go aggressive
            combat.startAttack(AttackType::Melee, enemyDir);
            m_playtestReactionTimer = humanReaction(m_ptHumanReactionBase, 0.7f, m_ptHumanFatigue);
        }

        // --- BERSERKER: maintain kill momentum (dash to next enemy) ---
        if (g_selectedClass == PlayerClass::Berserker && m_player->momentumStacks > 0
            && nearestEnemy > 100.0f && nearestEnemy < 250.0f && m_player->dashCooldownTimer <= 0) {
            inputMut.injectActionPress(enemyDir.x > 0 ? Action::MoveRight : Action::MoveLeft);
            inputMut.injectActionPress(Action::Dash);
        }

        // --- RESONANCE BUILD: quick dim-switches before boss engagement ---
        if (nearestIsBoss && nearestEnemy > 200.0f && nearestEnemy < 400.0f
            && m_dimManager.getResonance() < 0.5f && m_ptDimSwitchCD <= 0) {
            inputMut.injectActionPress(Action::DimensionSwitch);
            m_ptDimSwitchCD = 0.6f; // Fast switches to build resonance
        }
    }

ptPostCombat:
    // Periodic dimension exploration
    float exploreInterval = prof.exploreFrequency + getZone(m_currentDifficulty) * 0.5f;
    if (m_ptDimExploreTimer <= 0 && std::rand() % 5 == 0) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptDimExploreTimer = exploreInterval + static_cast<float>(std::rand() % 30) * 0.1f;
    }

    // Run success at floor 25+
    if (m_currentDifficulty > 24 && m_playtestRunActive) {
        playtestLog("  [%.0fs] === SUCCESS Floor %d ===", m_playtestRunTimer, m_currentDifficulty);
        playtestEndRun(true);
    }
}
