// PlayStatePlaytest.cpp -- Balance playtest bot with zone-aware tracking
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
    "Fractured Threshold",
    "Shifting Depths",
    "Resonant Core",
    "Entropy Cascade",
    "Sovereign's Domain"
};
const char* kProfileNames[] = {"balanced", "aggressive", "defensive", "speedrun"};

// Profile tuning constants
struct PlaytestProfile {
    float meleeRange;        // Max distance for melee engagement
    float rangedRange;       // Max distance for ranged engagement
    int dashAttackChance;    // 1/N chance to dash-attack per frame
    int chargeChance;        // 1/N chance to charge on boss/elite
    float dodgeDimHPThresh;  // HP% threshold for defensive dim-switch
    float reactionDelay;     // Base reaction time for combat
    int dashFrequency;       // 1/N chance to dash while moving
    float exploreFrequency;  // Base dim-explore interval
    bool prefersMelee;       // Biases toward melee range
};

constexpr PlaytestProfile kProfiles[] = {
    // Balanced: moderate everything
    {80.0f, 300.0f, 4, 6, 0.25f, 0.25f, 20, 5.0f, false},
    // Aggressive: closes distance fast, melees a lot, dashes through enemies
    {100.0f, 200.0f, 2, 3, 0.15f, 0.18f, 10, 3.0f, true},
    // Defensive: stays at range, dodges often, switches dim at higher HP
    {60.0f, 400.0f, 8, 8, 0.40f, 0.35f, 25, 6.0f, false},
    // Speedrun: dashes constantly, skips fights when possible, low reaction
    {70.0f, 250.0f, 3, 5, 0.20f, 0.15f, 5, 2.5f, false},
};
}

// ============================================================================
// PLAYTEST MODE: Human-like auto-play for balance testing
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

    // Class selection: locked to one, or rotate
    int classIdx;
    if (g_playtestClassLock >= 0 && g_playtestClassLock <= 2) {
        classIdx = g_playtestClassLock;
    } else {
        classIdx = ((m_playtestRun - 1) / 3) % 3; // Rotate: 3 runs each
    }
    g_selectedClass = static_cast<PlayerClass>(classIdx);
    const char* classNames[] = {"Voidwalker", "Berserker", "Phantom"};

    playtestLog("");
    playtestLog("=== Run %d / %s / %s ===", m_playtestRun, classNames[classIdx], kProfileNames[g_playtestProfile]);

    startNewRun();

    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    m_playtestLastHP = hp.currentHP;

    // Reset all per-run tracking state
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

    playtestLog("  Class: %s | HP: %.0f | Speed: %.0f | Zone: %s",
        classNames[classIdx], hp.maxHP, m_player->moveSpeed, kZoneNames[0]);
}

void PlayState::playtestOnDeath() {
    // Record death floor
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
    playtestLog("  --- Run %d End: Floor %d (Zone %d: %s) | %.0fs | %d Kills | %d Rifts | %s ---",
            m_playtestRun, m_currentDifficulty, zone + 1, kZoneNames[zone],
            m_playtestRunTimer, enemiesKilled, riftsRepaired,
            success ? "SUCCESS" : "DEATH");

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
    playtestLog("  PLAYTEST BALANCE REPORT");
    playtestLog("================================================================");
    playtestLog("  Runs: %d | Deaths: %d | Best Floor: %d", m_playtestRun, m_playtestDeaths, m_playtestBestLevel);
    playtestLog("");

    // Per-zone summary
    playtestLog("  --- ZONE SURVIVAL ---");
    for (int z = 0; z < 5; z++) {
        int startFloor = z * 6;
        int endFloor = startFloor + 5;
        int zoneDeaths = 0;
        float zoneDmg = 0;
        float zoneTime = 0;
        int zoneKills = 0;
        int floorsReached = 0;
        for (int f = startFloor; f <= endFloor && f < kMaxTrackedFloors; f++) {
            zoneDeaths += m_ptFloorDeathCount[f];
            zoneDmg += m_ptFloorStats[f].damageTaken;
            zoneTime += m_ptFloorStats[f].timeSpent;
            zoneKills += m_ptFloorStats[f].enemiesKilled;
            if (m_ptFloorStats[f].timeSpent > 0) floorsReached++;
        }
        if (floorsReached == 0) continue;
        playtestLog("  Zone %d (%s): %d deaths | %.0f dmg taken | %d kills | %.0fs | %d floors played",
            z + 1, kZoneNames[z], zoneDeaths, zoneDmg, zoneKills, zoneTime, floorsReached);
    }

    // Per-floor death heatmap
    playtestLog("");
    playtestLog("  --- DEATH HEATMAP (floor: deaths) ---");
    char heatBuf[512] = "  ";
    int heatLen = 2;
    for (int f = 0; f < kMaxTrackedFloors; f++) {
        if (m_ptFloorDeathCount[f] > 0) {
            int written = snprintf(heatBuf + heatLen, sizeof(heatBuf) - heatLen,
                "F%d:%d  ", f + 1, m_ptFloorDeathCount[f]);
            if (written > 0) heatLen += written;
        }
    }
    playtestLog("%s", heatBuf);

    // Per-floor stats table
    playtestLog("");
    playtestLog("  --- PER-FLOOR STATS ---");
    playtestLog("  %-6s %-5s %-8s %-8s %-8s %-8s %-6s %-5s",
        "Floor", "Zone", "DmgTaken", "Kills", "Time(s)", "HP@End", "Entr%", "Boss");
    for (int f = 0; f < kMaxTrackedFloors; f++) {
        auto& fs = m_ptFloorStats[f];
        if (fs.timeSpent <= 0) continue;
        playtestLog("  %-6d %-5d %-8.0f %-8d %-8.0f %-8.0f %-6.0f %-5s",
            f + 1, getZone(f + 1) + 1,
            fs.damageTaken, fs.enemiesKilled, fs.timeSpent,
            fs.hpAtEnd, fs.entropyAtEnd * 100.0f,
            fs.bossFloor ? "YES" : "");
    }

    playtestLog("");
    playtestLog("================================================================");

    if (g_playtestFile) {
        fclose(g_playtestFile);
        g_playtestFile = nullptr;
    }
}

void PlayState::updatePlaytest(float dt) {
    if (!m_player || !m_level || !m_playtestRunActive) return;

    m_playtestRunTimer += dt;
    m_playtestReactionTimer -= dt;
    m_playtestThinkTimer -= dt;
    m_ptDimSwitchCD -= dt;
    m_ptDimExploreTimer -= dt;
    m_ptAbilityCD -= dt;

    // Track HP changes for per-floor damage tracking
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpDiff = hp.currentHP - m_playtestLastHP;
    if (hpDiff < -1.0f) {
        m_ptFloorDmgTaken += (-hpDiff);
        // Only log big hits (>10% HP) to reduce spam
        if (-hpDiff > hp.maxHP * 0.1f) {
            playtestLog("  [%.0fs] F%d: BIG HIT %.0f dmg (HP: %.0f/%.0f = %.0f%%)",
                    m_playtestRunTimer, m_currentDifficulty, -hpDiff,
                    hp.currentHP, hp.maxHP, hp.currentHP / hp.maxHP * 100.0f);
        }
    }
    m_playtestLastHP = hp.currentHP;

    // Log entropy pressure (only when critical)
    float entropyPct = m_entropy.getPercent();
    if (entropyPct > 0.8f && m_playtestRunTimer - m_ptLastEntropyLog > 10.0f) {
        playtestLog("  [%.0fs] F%d: ENTROPY CRITICAL %.0f%%",
                m_playtestRunTimer, m_currentDifficulty, entropyPct * 100.0f);
        m_ptLastEntropyLog = m_playtestRunTimer;
    }

    // Log level completion with per-floor stats
    if (m_currentDifficulty != m_ptLastLoggedLevel && m_ptLastLoggedLevel > 0) {
        writeBalanceSnapshot();

        // Save per-floor stats
        int prevFloor = std::clamp(m_ptLastLoggedLevel - 1, 0, kMaxTrackedFloors - 1);
        auto& fs = m_ptFloorStats[prevFloor];
        fs.damageTaken += m_ptFloorDmgTaken;
        fs.enemiesKilled += (enemiesKilled - m_ptFloorKillsStart);
        fs.timeSpent += (m_playtestRunTimer - m_ptFloorTimeStart);
        fs.hpAtEnd = hp.currentHP;
        fs.entropyAtEnd = entropyPct;
        fs.bossFloor = isBossFloor(m_ptLastLoggedLevel) || isMidBossFloor(m_ptLastLoggedLevel);

        int zone = getZone(m_ptLastLoggedLevel);
        int newZone = getZone(m_currentDifficulty);
        playtestLog("  [%.0fs] FLOOR %d CLEAR (Zone %d) | Dmg:%.0f Kills:%d Time:%.0fs HP:%.0f/%.0f",
                m_playtestRunTimer, m_ptLastLoggedLevel, zone + 1,
                m_ptFloorDmgTaken, enemiesKilled - m_ptFloorKillsStart,
                m_playtestRunTimer - m_ptFloorTimeStart,
                hp.currentHP, hp.maxHP);

        // Zone transition announcement
        if (newZone != zone) {
            playtestLog("  >>> ENTERING ZONE %d: %s <<<", newZone + 1, kZoneNames[newZone]);
        }

        // Reset per-floor tracking
        m_ptFloorDmgTaken = 0;
        m_ptFloorTimeStart = m_playtestRunTimer;
        m_ptFloorKillsStart = enemiesKilled;
    }

    // Log level start
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
        const char* floorType = "normal";
        if (isBossFloor(m_currentDifficulty)) floorType = "ZONE BOSS";
        else if (isMidBossFloor(m_currentDifficulty)) floorType = "MID-BOSS";
        else if (isBreatherFloor(m_currentDifficulty)) floorType = "breather";

        // Record HP at start
        int floorIdx = std::clamp(m_currentDifficulty - 1, 0, kMaxTrackedFloors - 1);
        m_ptFloorStats[floorIdx].hpAtStart = hp.currentHP;

        playtestLog("  Floor %d (Z%d.%d %s) | %d rifts | ~%d enemies | %s",
                m_currentDifficulty, zone + 1, fiz, kZoneNames[zone],
                static_cast<int>(rifts.size()), enemyCount, floorType);
        m_ptLastEntropyLog = 0;
        m_ptFloorDmgTaken = 0;
        m_ptFloorTimeStart = m_playtestRunTimer;
        m_ptFloorKillsStart = enemiesKilled;
    }

    // Let level transition handle itself
    if (m_levelComplete) return;

    // Run timeout scales with zone: Z1=300s, Z2=450s, Z3=600s, Z4=750s, Z5=900s
    float timeoutSec = 300.0f + getZone(m_currentDifficulty) * 150.0f;
    if (m_playtestRunTimer > timeoutSec) {
        playtestLog("  [%.0fs] TIMEOUT at Floor %d (%.0fs limit)", m_playtestRunTimer, m_currentDifficulty, timeoutSec);
        playtestOnDeath();
        return;
    }

    // Auto-select relic when choice appears
    if (m_showRelicChoice && !m_relicChoices.empty()) {
        if (m_playtestThinkTimer <= 0) {
            int pick = std::rand() % static_cast<int>(m_relicChoices.size());
            selectRelic(pick);
            m_playtestThinkTimer = 0.5f;
        }
        return;
    }

    // Auto-dismiss NPC dialog
    if (m_showNPCDialog) {
        if (m_playtestThinkTimer <= 0) {
            m_showNPCDialog = false;
            m_playtestThinkTimer = 0.3f;
        }
        return;
    }

    // Auto-solve puzzles (with human-like delay per step)
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        if (m_playtestThinkTimer <= 0) {
            switch (m_activePuzzle->getType()) {
                case PuzzleType::Timing:
                    if (m_activePuzzle->isInSweetSpot()) {
                        m_activePuzzle->handleInput(4);
                        m_playtestThinkTimer = 0.3f;
                    }
                    break;
                case PuzzleType::Sequence:
                    if (!m_activePuzzle->isShowingSequence()) {
                        int idx = static_cast<int>(m_activePuzzle->getPlayerInput().size());
                        if (idx < static_cast<int>(m_activePuzzle->getSequence().size())) {
                            m_activePuzzle->handleInput(m_activePuzzle->getSequence()[idx]);
                            m_playtestThinkTimer = 0.5f;
                        }
                    }
                    break;
                case PuzzleType::Alignment:
                    if (m_activePuzzle->getCurrentRotation() != m_activePuzzle->getTargetRotation()) {
                        m_activePuzzle->handleInput(1);
                        m_playtestThinkTimer = 0.35f;
                    }
                    break;
                case PuzzleType::Pattern:
                    if (!m_activePuzzle->isPatternShowing()) {
                        for (int py = 0; py < 3; py++) {
                            for (int px = 0; px < 3; px++) {
                                if (m_activePuzzle->getPatternTarget(px, py) &&
                                    !m_activePuzzle->getPatternPlayer(px, py)) {
                                    int pcx = m_activePuzzle->getPatternCursorX();
                                    int pcy = m_activePuzzle->getPatternCursorY();
                                    if (pcx < px) { m_activePuzzle->handleInput(1); }
                                    else if (pcx > px) { m_activePuzzle->handleInput(3); }
                                    else if (pcy < py) { m_activePuzzle->handleInput(2); }
                                    else if (pcy > py) { m_activePuzzle->handleInput(0); }
                                    else { m_activePuzzle->handleInput(4); }
                                    m_playtestThinkTimer = 0.4f;
                                    goto ptPatternDone;
                                }
                            }
                        }
                        ptPatternDone:;
                    }
                    break;
            }
        }
        return;
    }

    // === NAVIGATION ===
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
    auto& inputMut = game->getInputMutable();
    int ptx = static_cast<int>(playerPos.x) / 32;
    int pty = static_cast<int>(playerPos.y) / 32;
    int dim = m_dimManager.getCurrentDimension();
    int otherDim = (dim == 1) ? 2 : 1;
    static float ptRecoveryGraceTimer = 0;
    if (ptRecoveryGraceTimer > 0) ptRecoveryGraceTimer -= dt;

    // Auto-rescue: fell below map (level gen issue, free teleport)
    {
        Vec2 spawn = m_level->getSpawnPoint();
        float maxDropBelow = std::max(400.0f, m_level->getPixelHeight() * 0.4f);
        float mapBottom = m_level->getPixelHeight() * 0.85f;
        if ((playerPos.y > spawn.y + maxDropBelow || playerPos.y > mapBottom) && ptRecoveryGraceTimer <= 0) {
            auto& transform = m_player->getEntity()->getComponent<TransformComponent>();
            transform.position = spawn;
            phys.velocity = {0, 0};
            m_ptFallCount++;
            if (m_ptFallCount % 3 == 0) {
                inputMut.injectActionPress(Action::DimensionSwitch);
            }
            return;
        }
    }

    // Find target: nearest unrepaired rift, or exit
    Vec2 target = m_level->getExitPoint();
    auto rifts = m_level->getRiftPositions();
    float nearestRiftDist = 99999.0f;
    int targetRiftIdx = -1;
    int targetRequiredDim = 0;

    if (!m_collapsing) {
        for (int ri = 0; ri < static_cast<int>(rifts.size()); ri++) {
            if (m_repairedRiftIndices.count(ri)) continue;
            if (m_ptSkipRiftMask & (1 << ri)) continue;
            float dx = rifts[ri].x - playerPos.x;
            float dy = rifts[ri].y - playerPos.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < nearestRiftDist) {
                nearestRiftDist = d;
                target = rifts[ri];
                targetRiftIdx = ri;
                targetRequiredDim = m_level->getRiftRequiredDimension(ri);
            }
        }
        if (nearestRiftDist > 99998.0f && m_ptSkipRiftMask != 0) {
            m_ptSkipRiftMask = 0;
            for (int ri = 0; ri < static_cast<int>(rifts.size()); ri++) {
                if (m_repairedRiftIndices.count(ri)) continue;
                float dx = rifts[ri].x - playerPos.x;
                float dy = rifts[ri].y - playerPos.y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < nearestRiftDist) {
                    nearestRiftDist = d;
                    target = rifts[ri];
                    targetRiftIdx = ri;
                    targetRequiredDim = m_level->getRiftRequiredDimension(ri);
                }
            }
        }
    }

    float dirX = target.x - playerPos.x;
    float dirY = target.y - playerPos.y;
    float distToTarget = std::sqrt(dirX * dirX + dirY * dirY);
    int moveDir = (dirX >= 0) ? 1 : -1;

    if (!m_collapsing && targetRequiredDim > 0 && targetRequiredDim != dim && m_ptDimSwitchCD <= 0) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptDimSwitchCD = 1.0f;
    }

    // Progress tracking
    if (distToTarget < m_ptBestDistToTarget - 20.0f) {
        m_ptBestDistToTarget = distToTarget;
        m_ptNoProgressTimer = 0;
    } else {
        m_ptNoProgressTimer += dt;
    }

    float noProgThreshold = m_collapsing ? 4.0f : 7.0f;
    if (m_ptNoProgressTimer > noProgThreshold) {
        m_ptNoProgressTimer = 0;
        m_ptBestDistToTarget = 99999.0f;
        m_ptNoProgressSkips++;
        auto& transform = m_player->getEntity()->getComponent<TransformComponent>();

        if (m_ptNoProgressSkips <= 1) {
            inputMut.injectActionPress(Action::DimensionSwitch);
            transform.position = m_level->getSpawnPoint();
            phys.velocity = {0, 0};
            m_ptLastCheckPos = m_level->getSpawnPoint();
            return;
        } else {
            transform.position = {target.x - 16.0f, target.y - 32.0f};
            phys.velocity = {0, 0};
            ptRecoveryGraceTimer = 1.5f;
            if (!m_collapsing && targetRiftIdx >= 0 &&
                m_level->isRiftActiveInDimension(targetRiftIdx, dim)) {
                inputMut.injectActionPress(Action::Interact);
            }
            m_ptLastCheckPos = target;
            m_ptNoProgressSkips = 0;
            return;
        }
    }

    // Stuck detection
    float moved = std::sqrt((playerPos.x - m_ptLastCheckPos.x) * (playerPos.x - m_ptLastCheckPos.x)
                          + (playerPos.y - m_ptLastCheckPos.y) * (playerPos.y - m_ptLastCheckPos.y));
    if (moved < 30.0f) {
        m_ptStuckTimer += dt;
    } else {
        m_ptStuckTimer = 0;
        m_ptLastCheckPos = playerPos;
    }

    if (m_ptStuckTimer > 3.0f && m_ptStuckTimer < 3.0f + dt * 3) {
        inputMut.injectActionPress(Action::DimensionSwitch);
    }
    if (m_ptStuckTimer > 5.0f && m_ptStuckTimer < 5.0f + dt * 3) {
        inputMut.injectActionPress(Action::Jump);
        if (dirX >= 0) inputMut.injectActionPress(Action::MoveLeft);
        else inputMut.injectActionPress(Action::MoveRight);
    }
    if (m_ptStuckTimer > 8.0f) {
        auto& transform = m_player->getEntity()->getComponent<TransformComponent>();
        transform.position = m_level->getSpawnPoint();
        phys.velocity = {0, 0};
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptStuckTimer = 0;
        m_ptLastCheckPos = m_level->getSpawnPoint();
        return;
    }

    // Floor/wall scanning ahead
    bool hasFloorAhead = false;
    bool wallAhead = false;
    bool hasFloorBelow = false;
    for (int dx = 1; dx <= 3; dx++) {
        int cx = ptx + moveDir * dx;
        if (dx == 1 && m_level->inBounds(cx, pty) && m_level->isSolid(cx, pty, dim))
            wallAhead = true;
        for (int dy = 0; dy <= 3; dy++) {
            if (m_level->inBounds(cx, pty + 1 + dy) &&
                (m_level->isSolid(cx, pty + 1 + dy, dim) || m_level->isOneWay(cx, pty + 1 + dy, dim))) {
                hasFloorAhead = true;
                break;
            }
        }
        if (hasFloorAhead) break;
    }
    for (int dy = 1; dy <= 2; dy++) {
        if (m_level->inBounds(ptx, pty + dy) &&
            (m_level->isSolid(ptx, pty + dy, dim) || m_level->isOneWay(ptx, pty + dy, dim))) {
            hasFloorBelow = true;
            break;
        }
    }

    // Dimension switch when blocked or no floor
    if ((wallAhead || (!hasFloorAhead && phys.onGround) || (!hasFloorBelow && phys.onGround)) && m_ptDimSwitchCD <= 0) {
        bool otherBetter = false;
        if (wallAhead) {
            int cx = ptx + moveDir;
            otherBetter = m_level->inBounds(cx, pty) && !m_level->isSolid(cx, pty, otherDim);
        }
        if (!hasFloorAhead || !hasFloorBelow) {
            for (int dx = 0; dx <= 3; dx++) {
                int cx = ptx + moveDir * dx;
                for (int dy = 0; dy <= 3; dy++) {
                    if (m_level->inBounds(cx, pty + 1 + dy) &&
                        (m_level->isSolid(cx, pty + 1 + dy, otherDim) || m_level->isOneWay(cx, pty + 1 + dy, otherDim))) {
                        otherBetter = true;
                        break;
                    }
                }
                if (otherBetter) break;
            }
        }
        if (otherBetter) {
            inputMut.injectActionPress(Action::DimensionSwitch);
            m_ptDimSwitchCD = 1.0f;
        }
    }

    // Horizontal movement toward target
    {
        if (dirX > 30.0f) {
            inputMut.injectActionPress(Action::MoveRight);
        } else if (dirX < -30.0f) {
            inputMut.injectActionPress(Action::MoveLeft);
        } else if (std::abs(dirY) > 200.0f) {
            if (static_cast<int>(m_playtestRunTimer * 2) % 8 < 4)
                inputMut.injectActionPress(Action::MoveRight);
            else
                inputMut.injectActionPress(Action::MoveLeft);
        }
    }

    // Jumping
    bool blocked = (phys.onWallLeft || phys.onWallRight) ||
                   (std::abs(phys.velocity.x) < 10.0f && std::abs(dirX) > 50.0f);
    bool targetAbove = dirY < -40.0f;

    if (phys.onGround) {
        bool shouldJump = blocked || targetAbove || !hasFloorAhead;
        if (shouldJump && std::rand() % 100 < 90) {
            inputMut.injectActionPress(Action::Jump);
        }
        if (std::rand() % 40 == 0) {
            inputMut.injectActionPress(Action::Jump);
        }
    }

    // Wall-jump
    if (m_player->isWallSliding) {
        if (std::rand() % 100 < 85) {
            inputMut.injectActionPress(Action::Jump);
        }
    }

    // Double-jump
    if (!phys.onGround && !m_player->isWallSliding && m_player->jumpsRemaining > 0) {
        bool falling = phys.velocity.y > 150.0f;
        bool noPlatformBelow = true;
        for (int dy = 1; dy <= 5; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (m_level->inBounds(ptx + dx, pty + dy) &&
                    (m_level->isSolid(ptx + dx, pty + dy, dim) || m_level->isOneWay(ptx + dx, pty + dy, dim))) {
                    noPlatformBelow = false;
                    break;
                }
            }
            if (!noPlatformBelow) break;
        }
        if ((targetAbove && std::rand() % 8 == 0) || (falling && noPlatformBelow)) {
            inputMut.injectActionPress(Action::Jump);
        }
    }

    // Profile-based dash frequency
    const auto& prof = kProfiles[g_playtestProfile];
    if (m_player->dashCooldownTimer <= 0) {
        if (phys.onGround && hasFloorAhead && (distToTarget > 300.0f || std::rand() % prof.dashFrequency == 0)) {
            inputMut.injectActionPress(Action::Dash);
        }
    }

    // Interact with rift when near
    if (nearestRiftDist < 60.0f && !m_activePuzzle && m_nearRiftIndex >= 0 &&
        m_level->isRiftActiveInDimension(m_nearRiftIndex, dim)) {
        inputMut.injectActionPress(Action::Interact);
    }

    // === COMBAT (improved: charged attacks, abilities, dash-attacks) ===
    if (m_playtestReactionTimer <= 0) {
        float nearestEnemy = 99999.0f;
        Vec2 enemyDir = {1.0f, 0.0f};
        bool nearestIsBoss = false;
        float nearestEnemyHP = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos || !e.isAlive()) return;
            if (!e.hasComponent<TransformComponent>()) return;
            auto& et = e.getComponent<TransformComponent>();
            float dx2 = et.getCenter().x - playerPos.x;
            float dy2 = et.getCenter().y - playerPos.y;
            float d = std::sqrt(dx2 * dx2 + dy2 * dy2);
            if (d < nearestEnemy) {
                nearestEnemy = d;
                enemyDir = (d > 0) ? Vec2{dx2 / d, dy2 / d} : Vec2{1.0f, 0.0f};
                if (e.hasComponent<AIComponent>()) {
                    nearestIsBoss = (e.getComponent<AIComponent>().enemyType == EnemyType::Boss);
                }
                if (e.hasComponent<HealthComponent>()) {
                    nearestEnemyHP = e.getComponent<HealthComponent>().currentHP;
                }
            }
        });

        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        float hpPct = hp.currentHP / hp.maxHP;

        // Dash-attack: dash through enemies (profile controls frequency)
        if (nearestEnemy > prof.meleeRange && nearestEnemy < 200.0f && m_player->dashCooldownTimer <= 0
            && std::rand() % static_cast<int>(prof.dashAttackChance) == 0) {
            if (enemyDir.x > 0) inputMut.injectActionPress(Action::MoveRight);
            else inputMut.injectActionPress(Action::MoveLeft);
            inputMut.injectActionPress(Action::Dash);
            m_playtestReactionTimer = prof.reactionDelay * 1.5f;
        }
        // Charged attack on bosses/elites (profile controls frequency)
        else if (nearestEnemy < prof.meleeRange * 1.25f && (nearestIsBoss || nearestEnemyHP > 80.0f)
                 && !m_ptCharging && std::rand() % static_cast<int>(prof.chargeChance) == 0) {
            combat.startAttack(AttackType::Charged, enemyDir);
            m_ptCharging = true;
            m_ptChargeTimer = 0.4f;
            m_playtestReactionTimer = prof.reactionDelay * 2.0f;
        }
        // Normal melee
        else if (nearestEnemy < prof.meleeRange) {
            combat.startAttack(AttackType::Melee, enemyDir);
            m_playtestReactionTimer = prof.reactionDelay;
        }
        // Ranged attack (aggressive profile closes distance instead)
        else if (nearestEnemy < prof.rangedRange && (!prof.prefersMelee || nearestEnemy > 150.0f)) {
            combat.startAttack(AttackType::Ranged, enemyDir);
            m_playtestReactionTimer = prof.reactionDelay * 1.6f;
        }
        // Speedrun: ignore distant enemies, keep moving
        else if (g_playtestProfile == 3 && nearestEnemy > 200.0f) {
            m_playtestReactionTimer = 0.05f; // Skip combat, keep running
        }
        else {
            m_playtestReactionTimer = 0.1f;
        }

        // Use ability when available and enemies nearby
        if (nearestEnemy < 200.0f && m_ptAbilityCD <= 0 &&
            m_player->getEntity()->hasComponent<AbilityComponent>()) {
            auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
            for (int a = 0; a < 3; a++) {
                if (abil.abilities[a].isReady()) {
                    abil.activate(a);
                    m_ptAbilityCD = 3.0f;
                    break;
                }
            }
        }

        // Defensive: dimension switch when low HP (profile controls threshold)
        if (hpPct < prof.dodgeDimHPThresh && nearestEnemy < 120.0f && m_ptDimSwitchCD <= 0
            && std::rand() % 3 == 0) {
            inputMut.injectActionPress(Action::DimensionSwitch);
            m_ptDimSwitchCD = 2.0f;
        }
    }

    // Charge timer decay
    if (m_ptCharging) {
        m_ptChargeTimer -= dt;
        if (m_ptChargeTimer <= 0) m_ptCharging = false;
    }

    // Periodic dimension exploration (profile + zone based)
    float exploreInterval = prof.exploreFrequency + getZone(m_currentDifficulty) * 0.5f;
    if (m_ptDimExploreTimer <= 0 && std::rand() % 5 == 0) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptDimExploreTimer = exploreInterval + static_cast<float>(std::rand() % 30) * 0.1f;
    }

    // Log rift completion
    if (m_levelRiftsRepaired > m_ptLastLoggedRifts) {
        m_ptLastLoggedRifts = m_levelRiftsRepaired;
    }

    // Successful run completion — reaching zone 5 floor 1 (floor 25+)
    if (m_currentDifficulty > 24 && m_playtestRunActive) {
        playtestLog("  [%.0fs] === RUN SUCCESS! Reached Floor %d (Zone 5) ===",
            m_playtestRunTimer, m_currentDifficulty);
        playtestEndRun(true);
    }
}
