// PlayStatePlaytest.cpp -- Split from PlayStateSmokeBot.cpp (balance playtest bot)
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

    // Rotate classes: Run 1-3 = Voidwalker, 4-6 = Berserker, 7-9 = Phantom, 10 = Voidwalker
    int classIdx = ((m_playtestRun - 1) / 3) % 3;
    g_selectedClass = static_cast<PlayerClass>(classIdx);
    const char* classNames[] = {"Voidwalker", "Berserker", "Phantom"};

    playtestLog("");
    playtestLog("--- Run %d / %s ---", m_playtestRun, classNames[classIdx]);

    // Must regenerate level to apply class properly
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

    playtestLog("  Klasse: %s (HP: %.0f, Speed: %.0f)", classNames[classIdx], hp.maxHP, m_player->moveSpeed);
}

void PlayState::playtestOnDeath() {
    playtestEndRun(false);
}

void PlayState::playtestEndRun(bool success) {
    if (!m_playtestRunActive) return; // Prevent double-calling
    m_playtestRunActive = false;

    if (!success) m_playtestDeaths++;
    if (m_currentDifficulty > m_playtestBestLevel)
        m_playtestBestLevel = m_currentDifficulty;

    // Snapshot balance data at end of each run
    writeBalanceSnapshot();

    playtestLog("  --- Run %d Ende: Level %d erreicht, %.0fs, %d Kills, %d Rifts ---",
            m_playtestRun, m_currentDifficulty, m_playtestRunTimer,
            enemiesKilled, riftsRepaired);

    if (m_playtestRun >= m_playtestMaxRuns) {
        playtestWriteReport();
        game->quit();
        return;
    }

    // Start next run
    playtestStartRun();
}

void PlayState::playtestWriteReport() {
    playtestLog("");
    playtestLog("========================================");
    playtestLog("  PLAYTEST ZUSAMMENFASSUNG");
    playtestLog("========================================");
    playtestLog("  Runs gesamt:     %d", m_playtestRun);
    playtestLog("  Tode gesamt:     %d", m_playtestDeaths);
    playtestLog("  Bestes Level:    %d", m_playtestBestLevel);
    playtestLog("========================================");

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

    // Track HP changes and log damage taken
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpDiff = hp.currentHP - m_playtestLastHP;
    if (hpDiff < -1.0f) {
        playtestLog("  [%.0fs] Schaden: %.0f HP verloren (HP: %.0f/%.0f, %.0f%%)",
                m_playtestRunTimer, -hpDiff, hp.currentHP, hp.maxHP,
                hp.currentHP / hp.maxHP * 100.0f);
    } else if (hpDiff > 5.0f) {
        playtestLog("  [%.0fs] Heilung: +%.0f HP (HP: %.0f/%.0f)",
                m_playtestRunTimer, hpDiff, hp.currentHP, hp.maxHP);
    }
    m_playtestLastHP = hp.currentHP;

    // Log entropy pressure
    float entropyPct = m_entropy.getPercent();
    if (entropyPct > 0.7f && m_playtestRunTimer - m_ptLastEntropyLog > 5.0f) {
        playtestLog("  [%.0fs] WARNUNG: Entropy bei %.0f%% - Anzug instabil!",
                m_playtestRunTimer, entropyPct * 100.0f);
        m_ptLastEntropyLog = m_playtestRunTimer;
    }

    // Log level completion (detected when level number increases)
    if (m_currentDifficulty != m_ptLastLoggedLevel && m_ptLastLoggedLevel > 0) {
        writeBalanceSnapshot(); // Snapshot balance at each level transition
        playtestLog("  [%.0fs] LEVEL %d GESCHAFFT! (HP: %.0f/%.0f, Entropy: %.0f%%)",
                m_playtestRunTimer, m_ptLastLoggedLevel,
                hp.currentHP, hp.maxHP, entropyPct * 100.0f);
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
        playtestLog("  Level %d gestartet (%d Rifts, ~%d Gegner, %s)",
                m_currentDifficulty,
                static_cast<int>(rifts.size()), enemyCount,
                m_isBossLevel ? "BOSS-LEVEL" : "normal");
        m_ptLastEntropyLog = 0;
    }

    // Let level transition handle itself
    if (m_levelComplete) return;

    // Run timeout: 600s max per run (Level 4+ has 6-8 rifts in huge maps)
    if (m_playtestRunTimer > 600.0f) {
        playtestLog("  [%.0fs] AUFGEGEBEN: Run dauert zu lange", m_playtestRunTimer);
        playtestLog("  GESTORBEN: Timeout (600s)");
        playtestOnDeath();
        return;
    }

    // Decrement reaction timer (used only for combat decisions)
    // Movement runs EVERY FRAME - like a real human holding buttons

    // Auto-select relic when choice appears
    if (m_showRelicChoice && !m_relicChoices.empty()) {
        if (m_playtestThinkTimer <= 0) {
            int pick = std::rand() % static_cast<int>(m_relicChoices.size());
            playtestLog("  [%.0fs] Relic gewaehlt: #%d", m_playtestRunTimer, static_cast<int>(m_relicChoices[pick]));
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
            // After 3 falls, try other dimension at spawn
            if (m_ptFallCount % 3 == 0) {
                inputMut.injectActionPress(Action::DimensionSwitch);
            }
            return; // Skip rest of frame after rescue
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
        // All skipped? Clear mask and retry
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

    // Progress tracking: detect when bot isn't getting closer to target
    if (distToTarget < m_ptBestDistToTarget - 20.0f) {
        m_ptBestDistToTarget = distToTarget;
        m_ptNoProgressTimer = 0;
    } else {
        m_ptNoProgressTimer += dt;
    }

    // No-progress handling: dim-switch at 5s, teleport at 10s
    float noProgThreshold = m_collapsing ? 4.0f : 7.0f;
    if (m_ptNoProgressTimer > noProgThreshold) {
        m_ptNoProgressTimer = 0;
        m_ptBestDistToTarget = 99999.0f;
        m_ptNoProgressSkips++;
        auto& transform = m_player->getEntity()->getComponent<TransformComponent>();

        if (m_ptNoProgressSkips <= 1) {
            // First: dim-switch + teleport to spawn
            inputMut.injectActionPress(Action::DimensionSwitch);
            transform.position = m_level->getSpawnPoint();
            phys.velocity = {0, 0};
            m_ptLastCheckPos = m_level->getSpawnPoint();
            return;
        } else {
            // Second: teleport near target (costs HP as penalty)
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

    // Stuck detection: position barely changed
    float moved = std::sqrt((playerPos.x - m_ptLastCheckPos.x) * (playerPos.x - m_ptLastCheckPos.x)
                          + (playerPos.y - m_ptLastCheckPos.y) * (playerPos.y - m_ptLastCheckPos.y));
    if (moved < 30.0f) {
        m_ptStuckTimer += dt;
    } else {
        m_ptStuckTimer = 0;
        m_ptLastCheckPos = playerPos;
    }

    // Stuck: dim-switch at 3s, reverse at 5s, teleport-spawn at 8s
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
    // Check floor directly below (am I standing on something?)
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

    // Horizontal movement toward target (runs every frame for smooth control)
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

    // Dash
    if (phys.onGround && m_player->dashCooldownTimer <= 0 && hasFloorAhead &&
        (distToTarget > 300.0f || std::rand() % 30 == 0)) {
        inputMut.injectActionPress(Action::Dash);
    }

    // Interact with rift when near
    if (nearestRiftDist < 60.0f && !m_activePuzzle && m_nearRiftIndex >= 0 &&
        m_level->isRiftActiveInDimension(m_nearRiftIndex, dim)) {
        inputMut.injectActionPress(Action::Interact);
        playtestLog("  [%.0fs] Rift %d erreicht", m_playtestRunTimer, m_nearRiftIndex);
    }

    // Combat (uses reaction timer for human-like delay)
    if (m_playtestReactionTimer <= 0) {
        float nearestEnemy = 99999.0f;
        Vec2 enemyDir = {1.0f, 0.0f};
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
            }
        });

        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        if (nearestEnemy < 80.0f) {
            combat.startAttack(AttackType::Melee, enemyDir);
            m_playtestReactionTimer = 0.3f;
        } else if (nearestEnemy < 250.0f) {
            combat.startAttack(AttackType::Ranged, enemyDir);
            m_playtestReactionTimer = 0.45f;
        } else {
            m_playtestReactionTimer = 0.1f;
        }
    }

    // Periodic dimension exploration
    if (m_ptDimExploreTimer <= 0 && std::rand() % 4 == 0) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        m_ptDimExploreTimer = 4.0f + static_cast<float>(std::rand() % 30) * 0.1f;
    }

    // Log rift completion
    if (m_levelRiftsRepaired > m_ptLastLoggedRifts) {
        playtestLog("  [%.0fs] Rift repariert (%d/%d)", m_playtestRunTimer,
                m_levelRiftsRepaired, static_cast<int>(rifts.size()));
        m_ptLastLoggedRifts = m_levelRiftsRepaired;
    }

    // (Level completion is logged above when level number changes)

    // Successful run completion (level 5+) — treat as run end, start next run
    if (m_currentDifficulty > 5 && m_playtestRunActive) {
        playtestLog("  [%.0fs] === RUN ERFOLGREICH! Level %d erreicht ===", m_playtestRunTimer, m_currentDifficulty);
        playtestEndRun(true);
    }
}
