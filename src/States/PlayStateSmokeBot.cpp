// PlayStateSmokeBot.cpp — Smoke test bot implementation
// Split from PlayState.cpp — all member functions unchanged, just moved.
// Playtest bot moved to PlayStatePlaytest.cpp
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
#include <string_view>

extern bool g_autoSmokeTest;
extern int g_forcedRunSeed;
extern bool g_smokeRegression;
extern int g_smokeTargetFloor;
extern float g_smokeMaxRuntime;
extern int g_smokeCompletedFloor;
extern int g_smokeFailureCode;
extern char g_smokeFailureReason[256];

FILE* g_smokeFile = nullptr;

void smokeLog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (!g_smokeFile) {
        g_smokeFile = fopen("smoke_test.log", "a");
    }
    if (g_smokeFile) {
        fprintf(g_smokeFile, "%s\n", buf);
        fflush(g_smokeFile);
    }
}

enum SmokeRegressionFailureCode {
    SmokeRegressionFailure_None = 0,
    SmokeRegressionFailure_Topology = 2,
    SmokeRegressionFailure_SpawnFallback = 3,
    SmokeRegressionFailure_LevelTimeout = 4,
    SmokeRegressionFailure_TotalTimeout = 5,
    SmokeRegressionFailure_Progress = 6
};

static void smokeSetRegressionFailure(int code, const char* fmt, ...) {
    if (!g_smokeRegression || g_smokeFailureCode != 0) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(g_smokeFailureReason, sizeof(g_smokeFailureReason), fmt, args);
    va_end(args);

    g_smokeFailureCode = code;
    smokeLog("[SMOKE] RESULT status=FAIL code=%d completedFloor=%d targetFloor=%d reason=%s",
             code,
             g_smokeCompletedFloor,
             g_smokeTargetFloor,
             g_smokeFailureReason);
}

static void smokeLogRegressionResult(const char* status, float runtimeSeconds) {
    if (!g_smokeRegression) return;
    smokeLog("[SMOKE] RESULT status=%s completedFloor=%d targetFloor=%d runtime=%.1f failureCode=%d reason=%s",
             status,
             g_smokeCompletedFloor,
             g_smokeTargetFloor,
             runtimeSeconds,
             g_smokeFailureCode,
             g_smokeFailureReason[0] ? g_smokeFailureReason : "none");
}

static std::string smokeFailureMaskToString(int failureMask) {
    if (failureMask == LevelValidationFailure_None) {
        return "none";
    }

    struct FailureName {
        int bit;
        const char* name;
    };

    static const FailureName kNames[] = {
        {LevelValidationFailure_Spawn, "spawn"},
        {LevelValidationFailure_Exit, "exit"},
        {LevelValidationFailure_RiftAnchor, "rift_anchor"},
        {LevelValidationFailure_RiftReachability, "rift_reach"},
        {LevelValidationFailure_ExitReachability, "exit_reach"},
        {LevelValidationFailure_MainPath, "main_path"},
        {LevelValidationFailure_DimSwitch, "dim_switch"},
        {LevelValidationFailure_DimPuzzle, "dim_puzzle"}
    };

    std::string result;
    for (const auto& entry : kNames) {
        if ((failureMask & entry.bit) == 0) continue;
        if (!result.empty()) result += ",";
        result += entry.name;
    }
    return result;
}

static int smokeFindTopologyRoomIndex(const LevelTopology& topology, const Vec2& worldPos) {
    int tileX = static_cast<int>(worldPos.x) / 32;
    int tileY = static_cast<int>(worldPos.y) / 32;
    for (int i = 0; i < static_cast<int>(topology.rooms.size()); i++) {
        const auto& room = topology.rooms[i];
        if (tileX >= room.x && tileX < room.x + room.w &&
            tileY >= room.y && tileY < room.y + room.h) {
            return i;
        }
    }
    return -1;
}

static int smokeFindClosestTopologyRoomIndex(const LevelTopology& topology, const Vec2& worldPos) {
    int tileX = static_cast<int>(worldPos.x) / 32;
    int tileY = static_cast<int>(worldPos.y) / 32;
    int bestRoom = -1;
    int bestDist = 0;
    for (int i = 0; i < static_cast<int>(topology.rooms.size()); i++) {
        const auto& room = topology.rooms[i];
        int dx = 0;
        int dy = 0;
        if (tileX < room.x) dx = room.x - tileX;
        else if (tileX >= room.x + room.w) dx = tileX - (room.x + room.w - 1);
        if (tileY < room.y) dy = room.y - tileY;
        else if (tileY >= room.y + room.h) dy = tileY - (room.y + room.h - 1);
        int dist = dx * dx + dy * dy;
        if (bestRoom < 0 || dist < bestDist) {
            bestRoom = i;
            bestDist = dist;
        }
    }
    return bestRoom;
}

struct SmokeTargetDebugInfo {
    const char* type = "exit";
    int index = -1;
    int roomIndex = -1;
    int tileX = -1;
    int tileY = -1;
    int requiredDimension = 0;
    std::array<bool, 3> validDims{};
    Vec2 position{};
    float distance = 0.0f;
    bool validated = false;
};

struct SmokeRecoveryAnchor {
    bool valid = false;
    bool targetsObjective = false;
    bool spawnFallback = false;
    const char* reason = "none";
    int roomIndex = -1;
    int tileX = -1;
    int tileY = -1;
    int dimension = 1;
    Vec2 position{};
};

static int smokePickPreferredTargetDimension(const SmokeTargetDebugInfo& target, int currentDim) {
    if (target.requiredDimension == 1 || target.requiredDimension == 2) {
        return target.requiredDimension;
    }
    if (currentDim >= 1 && currentDim <= 2 && target.validDims[currentDim]) {
        return currentDim;
    }
    if (target.validDims[1]) return 1;
    if (target.validDims[2]) return 2;
    return currentDim;
}

static void smokeLogLevelValidation(const Level& level) {
    const auto& topology = level.getTopology();
    smokeLog("[SMOKE]   validation: generatedValid=%d requestedSeed=%d usedSeed=%d attempt=%d mask=%s rooms=%d edges=%d switches=%d spawnRoom=%d exitRoom=%d",
             topology.validation.passed ? 1 : 0,
             topology.validation.requestedSeed,
             topology.validation.usedSeed,
             topology.validation.attempt,
             smokeFailureMaskToString(topology.validation.failureMask).c_str(),
             static_cast<int>(topology.rooms.size()),
             static_cast<int>(topology.edges.size()),
             static_cast<int>(topology.switches.size()),
             topology.spawnRoomIndex,
             topology.exitRoomIndex);

    for (int i = 0; i < static_cast<int>(topology.rifts.size()); i++) {
        const auto& rift = topology.rifts[i];
        smokeLog("[SMOKE]   topo_rift[%d]: room=%d tile=(%d,%d) dims=%d%d requiredDim=%d validated=%d",
                 i,
                 rift.roomIndex,
                 rift.tileX,
                 rift.tileY,
                 rift.accessibleDimensions[1] ? 1 : 0,
                 rift.accessibleDimensions[2] ? 1 : 0,
                 rift.requiredDimension,
                 rift.validated ? 1 : 0);
    }
}

static bool smokeIsValidRecoveryTile(const Level& level, int tileX, int tileY, int dimension) {
    if (dimension < 1 || dimension > 2) return false;
    if (!level.inBounds(tileX, tileY) ||
        !level.inBounds(tileX, tileY + 1)) {
        return false;
    }

    const auto& tile = level.getTile(tileX, tileY, dimension);
    const auto& supportTile = level.getTile(tileX, tileY + 1, dimension);

    if (tile.isSolid() || tile.isOneWay() || tile.isDangerous()) return false;
    if (supportTile.isDangerous()) return false;
    return supportTile.isSolid() || supportTile.isOneWay();
}

static bool smokeTryMakeRecoveryAnchor(const Level& level,
                                       int roomIndex,
                                       int tileX,
                                       int tileY,
                                       int dimension,
                                       const char* reason,
                                       bool targetsObjective,
                                       bool spawnFallback,
                                       SmokeRecoveryAnchor& out) {
    if (!smokeIsValidRecoveryTile(level, tileX, tileY, dimension)) {
        return false;
    }

    int tileSize = level.getTileSize();
    out.valid = true;
    out.targetsObjective = targetsObjective;
    out.spawnFallback = spawnFallback;
    out.reason = reason;
    out.roomIndex = roomIndex;
    out.tileX = tileX;
    out.tileY = tileY;
    out.dimension = dimension;
    out.position = {
        static_cast<float>(tileX * tileSize),
        static_cast<float>(tileY * tileSize)
    };
    return true;
}

static bool smokeTryFindStandingTileInRoom(const Level& level,
                                           const LevelGraphRoom& room,
                                           int dimension,
                                           int preferredTileX,
                                           int preferredTileY,
                                           int& outTileX,
                                           int& outTileY) {
    if (dimension < 1 || dimension > 2) return false;

    int minX = std::max(room.x + 1, 0);
    int maxX = std::min(room.x + room.w - 2, level.getWidth() - 1);
    int minY = std::max(room.y + 1, 1);
    int maxY = std::min(room.y + room.h - 3, level.getHeight() - 2);
    if (minX > maxX || minY > maxY) return false;

    preferredTileX = std::clamp(preferredTileX, minX, maxX);
    preferredTileY = std::clamp(preferredTileY, minY, maxY);

    bool found = false;
    int bestScore = 0;
    int bestX = preferredTileX;
    int bestY = preferredTileY;
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            if (!smokeIsValidRecoveryTile(level, x, y, dimension)) continue;
            int score = std::abs(x - preferredTileX) * 3 + std::abs(y - preferredTileY) * 5;
            if (!found || score < bestScore) {
                found = true;
                bestScore = score;
                bestX = x;
                bestY = y;
            }
        }
    }

    if (!found) return false;
    outTileX = bestX;
    outTileY = bestY;
    return true;
}

static bool smokeTryFindRoomRecoveryAnchor(const Level& level,
                                           const LevelTopology& topology,
                                           int roomIndex,
                                           int primaryDimension,
                                           int preferredTileX,
                                           int preferredTileY,
                                           const char* reason,
                                           bool targetsObjective,
                                           bool spawnFallback,
                                           SmokeRecoveryAnchor& out) {
    if (roomIndex < 0 || roomIndex >= static_cast<int>(topology.rooms.size())) {
        return false;
    }

    const auto& room = topology.rooms[roomIndex];
    bool triedDim[3] = {};
    int dimOrder[2] = {
        primaryDimension,
        primaryDimension == 1 ? 2 : 1
    };

    for (int dim : dimOrder) {
        if (dim < 1 || dim > 2 || triedDim[dim]) continue;
        triedDim[dim] = true;
        int anchorTileX = preferredTileX;
        int anchorTileY = preferredTileY;
        if (!smokeTryFindStandingTileInRoom(level, room, dim, preferredTileX, preferredTileY, anchorTileX, anchorTileY)) {
            continue;
        }
        if (smokeTryMakeRecoveryAnchor(level,
                                       roomIndex,
                                       anchorTileX,
                                       anchorTileY,
                                       dim,
                                       reason,
                                       targetsObjective,
                                       spawnFallback,
                                       out)) {
            return true;
        }
    }

    return false;
}

static const LevelGraphEdge* smokeFindValidatedTopologyEdge(const LevelTopology& topology,
                                                            int roomA,
                                                            int roomB,
                                                            int currentDim,
                                                            int preferredDim) {
    const LevelGraphEdge* fallback = nullptr;
    const int preferredOrder[] = {currentDim, preferredDim};

    for (int desiredDim : preferredOrder) {
        if (desiredDim < 1 || desiredDim > 2) continue;
        for (const auto& edge : topology.edges) {
            bool samePair = (edge.fromRoom == roomA && edge.toRoom == roomB) ||
                            (edge.fromRoom == roomB && edge.toRoom == roomA);
            if (!samePair || !edge.validated || edge.dimension != desiredDim) continue;
            return &edge;
        }
    }

    for (const auto& edge : topology.edges) {
        bool samePair = (edge.fromRoom == roomA && edge.toRoom == roomB) ||
                        (edge.fromRoom == roomB && edge.toRoom == roomA);
        if (!samePair || !edge.validated) continue;
        if (!fallback) fallback = &edge;
    }

    return fallback;
}

static SmokeRecoveryAnchor smokeBuildRecoveryAnchor(const Level& level,
                                                    const SmokeTargetDebugInfo& target,
                                                    const Vec2& playerPos,
                                                    int currentDim,
                                                    int preferredTargetDim,
                                                    bool collapsing) {
    const auto& topology = level.getTopology();
    SmokeRecoveryAnchor anchor;

    auto buildSpawnFallback = [&]() {
        SmokeRecoveryAnchor fallback;
        int spawnDim = topology.spawnValid[currentDim] ? currentDim
            : (topology.spawnValid[1] ? 1
               : (topology.spawnValid[2] ? 2 : std::clamp(currentDim, 1, 2)));
        fallback.valid = true;
        fallback.spawnFallback = true;
        fallback.reason = "spawn_fallback";
        fallback.roomIndex = topology.spawnRoomIndex;
        fallback.tileX = topology.spawnTileX;
        fallback.tileY = topology.spawnTileY;
        fallback.dimension = spawnDim;
        fallback.position = level.getSpawnPoint();
        return fallback;
    };

    if (topology.rooms.empty()) {
        return buildSpawnFallback();
    }

    int playerRoom = smokeFindTopologyRoomIndex(topology, playerPos);
    int referenceRoom = playerRoom >= 0 ? playerRoom : smokeFindClosestTopologyRoomIndex(topology, playerPos);
    bool targetIsExit = target.type == "exit";
    bool targetObjective = target.validated &&
                           target.roomIndex >= 0 &&
                           target.tileX >= 0 &&
                           target.tileY >= 0;

    if (targetObjective && referenceRoom == target.roomIndex) {
        bool triedDim[3] = {};
        int dimOrder[2] = {preferredTargetDim, currentDim};
        for (int dim : dimOrder) {
            if (dim < 1 || dim > 2 || triedDim[dim]) continue;
            triedDim[dim] = true;
            if (((dim >= 1 && dim <= 2) && !target.validDims[dim]) && (target.validDims[1] || target.validDims[2])) {
                continue;
            }
            if (smokeTryMakeRecoveryAnchor(level,
                                           target.roomIndex,
                                           target.tileX,
                                           target.tileY,
                                           dim,
                                           targetIsExit ? "exit_anchor" : "target_anchor",
                                           true,
                                           false,
                                           anchor)) {
                return anchor;
            }
        }
    }

    if (targetObjective && referenceRoom >= 0 && referenceRoom != target.roomIndex) {
        int step = (target.roomIndex > referenceRoom) ? 1 : -1;
        int approachRoom = referenceRoom + step;
        if (approachRoom >= 0 && approachRoom < static_cast<int>(topology.rooms.size())) {
            const auto* edge = smokeFindValidatedTopologyEdge(topology, referenceRoom, approachRoom, currentDim, preferredTargetDim);
            if (edge) {
                const auto& room = topology.rooms[approachRoom];
                int preferredTileX = (step > 0) ? room.x + 1 : room.x + room.w - 2;
                int preferredTileY = room.y + room.h / 2;
                const char* reason = edge->type == LevelGraphEdgeType::VerticalShaft
                    ? "main_path_landing"
                    : "main_path_anchor";
                if (smokeTryFindRoomRecoveryAnchor(level,
                                                   topology,
                                                   approachRoom,
                                                   edge->dimension,
                                                   preferredTileX,
                                                   preferredTileY,
                                                   reason,
                                                   false,
                                                   false,
                                                   anchor)) {
                    return anchor;
                }
            }
        }
    }

    if (targetObjective) {
        if (smokeTryFindRoomRecoveryAnchor(level,
                                           topology,
                                           target.roomIndex,
                                           preferredTargetDim,
                                           target.tileX,
                                           target.tileY,
                                           targetIsExit && collapsing ? "exit_room_anchor" : "target_room_anchor",
                                           true,
                                           false,
                                           anchor)) {
            return anchor;
        }
    }

    if (referenceRoom >= 0) {
        const auto& room = topology.rooms[referenceRoom];
        if (smokeTryFindRoomRecoveryAnchor(level,
                                           topology,
                                           referenceRoom,
                                           currentDim,
                                           room.x + room.w / 2,
                                           room.y + room.h / 2,
                                           "current_room_anchor",
                                           false,
                                           false,
                                           anchor)) {
            return anchor;
        }
    }

    if (topology.spawnRoomIndex >= 0 && topology.spawnTileX >= 0 && topology.spawnTileY >= 0) {
        bool triedDim[3] = {};
        int dimOrder[2] = {currentDim, currentDim == 1 ? 2 : 1};
        for (int dim : dimOrder) {
            if (dim < 1 || dim > 2 || triedDim[dim] || !topology.spawnValid[dim]) continue;
            triedDim[dim] = true;
            if (smokeTryMakeRecoveryAnchor(level,
                                           topology.spawnRoomIndex,
                                           topology.spawnTileX,
                                           topology.spawnTileY,
                                           dim,
                                           "spawn_anchor",
                                           false,
                                           true,
                                           anchor)) {
                return anchor;
            }
        }
        if (smokeTryFindRoomRecoveryAnchor(level,
                                           topology,
                                           topology.spawnRoomIndex,
                                           currentDim,
                                           topology.spawnTileX,
                                           topology.spawnTileY,
                                           "spawn_room_anchor",
                                           false,
                                           true,
                                           anchor)) {
            return anchor;
        }
    }

    return buildSpawnFallback();
}

static void smokeLogRecoveryAnchor(const char* label,
                                   const Level& level,
                                   int difficulty,
                                   const Vec2& playerPos,
                                   const SmokeTargetDebugInfo& target,
                                   const SmokeRecoveryAnchor& anchor,
                                   int currentDim) {
    const auto& topology = level.getTopology();
    int playerRoom = smokeFindTopologyRoomIndex(topology, playerPos);
    smokeLog("[SMOKE] %s_ANCHOR floor=%d seed=%d pos=(%.0f,%.0f) room=%d dim=%d target=%s idx=%d targetRoom=%d reason=%s anchorRoom=%d anchorTile=(%d,%d) anchorPos=(%.0f,%.0f) anchorDim=%d objective=%d spawnFallback=%d genValidated=%d",
             label,
             difficulty,
             level.getGenerationSeed(),
             playerPos.x,
             playerPos.y,
             playerRoom,
             currentDim,
             target.type,
             target.index,
             target.roomIndex,
             anchor.reason,
             anchor.roomIndex,
             anchor.tileX,
             anchor.tileY,
             anchor.position.x,
             anchor.position.y,
             anchor.dimension,
             anchor.targetsObjective ? 1 : 0,
             anchor.spawnFallback ? 1 : 0,
             topology.validation.passed ? 1 : 0);
}

static void smokeLogRuntimeContext(const char* label,
                                   const Level& level,
                                   int difficulty,
                                   const Vec2& playerPos,
                                   int currentDim,
                                   const SmokeTargetDebugInfo& target,
                                   bool collapsing,
                                   int repairedRifts) {
    const auto& topology = level.getTopology();
    int playerRoom = smokeFindTopologyRoomIndex(topology, playerPos);
    smokeLog("[SMOKE] %s floor=%d seed=%d pos=(%.0f,%.0f) room=%d dim=%d target=%s idx=%d targetPos=(%.0f,%.0f) targetRoom=%d targetTile=(%d,%d) targetValid=%d dims=%d%d requiredDim=%d repaired=%d/%d collapsing=%d genValidated=%d genMask=%s",
             label,
             difficulty,
             level.getGenerationSeed(),
             playerPos.x,
             playerPos.y,
             playerRoom,
             currentDim,
             target.type,
             target.index,
             target.position.x,
             target.position.y,
             target.roomIndex,
             target.tileX,
             target.tileY,
             target.validated ? 1 : 0,
             target.validDims[1] ? 1 : 0,
             target.validDims[2] ? 1 : 0,
             target.requiredDimension,
             repairedRifts,
             static_cast<int>(level.getRiftPositions().size()),
             collapsing ? 1 : 0,
             topology.validation.passed ? 1 : 0,
             smokeFailureMaskToString(topology.validation.failureMask).c_str());
}

void PlayState::updateSmokeTest(float dt) {
    if (!m_player || !m_level) return;

    static float smokeRecoveryGraceTimer = 0;
    static bool smokeRecoverySuppressedLogged = false;

    m_smokeRunTime += dt;
    m_smokeActionTimer -= dt;
    m_smokeDimTimer -= dt;

    // God mode: keep HP full + prevent entropy death
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    hp.currentHP = hp.maxHP;
    if (m_entropy.getPercent() > 0.5f) {
        m_entropy.reduceEntropy(m_entropy.getEntropy() * 0.5f);
    }

    // All static navigation state (declared here, used throughout)
    static float levelTimer = 0;
    static int smokeLastLevel = 0;
    static float noProgressTimer = 0;
    static float bestDistToTarget = 99999.0f;
    static int noProgressSkips = 0;
    static Vec2 lastStuckCheckPos = {0, 0};
    static float stuckTimer = 0;
    static bool stuckLogged = false;
    static int smokeSkipRiftMask = 0;
    static int smokeCurrentTarget = -1;
    static int smokeLastTargetIndex = -2;
    static int smokeLastTargetRoom = -2;
    static bool smokeLastTargetWasExit = false;
    if (smokeRecoveryGraceTimer > 0) smokeRecoveryGraceTimer -= dt;

    const auto& topology = m_level->getTopology();
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();

    auto buildSmokeTargetInfo = [&]() {
        SmokeTargetDebugInfo info;
        info.type = "exit";
        info.position = m_level->getExitPoint();
        info.roomIndex = topology.exitRoomIndex;
        info.tileX = topology.exitTileX;
        info.tileY = topology.exitTileY;
        info.validDims = topology.exitValid;
        info.validated = topology.validation.passed &&
                         topology.exitRoomIndex >= 0 &&
                         (topology.exitValid[1] || topology.exitValid[2]);
        info.distance = 99999.0f;

        smokeCurrentTarget = -1;
        auto rifts = m_level->getRiftPositions();
        if (!m_collapsing) {
            float nearestRiftDist = 99999.0f;
            for (int ri = 0; ri < static_cast<int>(rifts.size()); ri++) {
                if (m_repairedRiftIndices.count(ri)) continue;
                if (smokeSkipRiftMask & (1 << ri)) continue;
                float dx = rifts[ri].x - playerPos.x;
                float dy = rifts[ri].y - playerPos.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < nearestRiftDist) {
                    nearestRiftDist = dist;
                    info.type = "rift";
                    info.index = ri;
                    info.position = rifts[ri];
                    info.distance = dist;
                    smokeCurrentTarget = ri;
                    if (ri < static_cast<int>(topology.rifts.size())) {
                        const auto& topoRift = topology.rifts[ri];
                        info.roomIndex = topoRift.roomIndex;
                        info.tileX = topoRift.tileX;
                        info.tileY = topoRift.tileY;
                        info.requiredDimension = topoRift.requiredDimension;
                        info.validDims = topoRift.accessibleDimensions;
                        info.validated = topoRift.validated;
                    } else {
                        info.roomIndex = -1;
                        info.tileX = static_cast<int>(rifts[ri].x) / 32;
                        info.tileY = static_cast<int>(rifts[ri].y) / 32;
                        info.requiredDimension = 0;
                        info.validDims = {};
                        info.validated = false;
                    }
                }
            }

            if (nearestRiftDist > 99998.0f && smokeSkipRiftMask != 0) {
                smokeSkipRiftMask = 0;
                smokeLog("[SMOKE] Cleared skip mask - retrying all rifts");
                for (int ri = 0; ri < static_cast<int>(rifts.size()); ri++) {
                    if (m_repairedRiftIndices.count(ri)) continue;
                    float dx = rifts[ri].x - playerPos.x;
                    float dy = rifts[ri].y - playerPos.y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < nearestRiftDist) {
                        nearestRiftDist = dist;
                        info.type = "rift";
                        info.index = ri;
                        info.position = rifts[ri];
                        info.distance = dist;
                        smokeCurrentTarget = ri;
                        if (ri < static_cast<int>(topology.rifts.size())) {
                            const auto& topoRift = topology.rifts[ri];
                            info.roomIndex = topoRift.roomIndex;
                            info.tileX = topoRift.tileX;
                            info.tileY = topoRift.tileY;
                            info.requiredDimension = topoRift.requiredDimension;
                            info.validDims = topoRift.accessibleDimensions;
                            info.validated = topoRift.validated;
                        }
                    }
                }
            }
        }

        return info;
    };

    SmokeTargetDebugInfo targetInfo = buildSmokeTargetInfo();
    int currentDim = m_dimManager.getCurrentDimension();
    int preferredTargetDim = smokePickPreferredTargetDimension(targetInfo, currentDim);

    // Heartbeat frame counter — proves game loop is alive
    static int smokeFrameCount = 0;
    smokeFrameCount++;

    // Log level start once + reset per-level timer
    if (m_currentDifficulty != smokeLastLevel) {
        levelTimer = 0;
        smokeLastLevel = m_currentDifficulty;
        // Reset navigation state for new level
        noProgressTimer = 0;
        bestDistToTarget = 99999.0f;
        noProgressSkips = 0;
        stuckTimer = 0;
        stuckLogged = false;
        smokeSkipRiftMask = 0;
        smokeCurrentTarget = -1;
        smokeLastTargetIndex = -2;
        smokeLastTargetRoom = -2;
        smokeLastTargetWasExit = false;
        smokeRecoveryGraceTimer = 0;
        smokeRecoverySuppressedLogged = false;
        auto rifts = m_level->getRiftPositions();
        Vec2 spawn = m_level->getSpawnPoint();
        Vec2 exit = m_level->getExitPoint();
        int levelSeed = m_level->getGenerationSeed() != 0
            ? m_level->getGenerationSeed()
            : (m_runSeed + m_currentDifficulty);
        smokeLog("[SMOKE] === LEVEL %d START === seed=%d rooms=%dx%d rifts=%d",
                m_currentDifficulty, levelSeed,
                m_level->getWidth(), m_level->getHeight(),
                static_cast<int>(rifts.size()));
        {
            const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
            smokeLog("[SMOKE]   dim_shift: dimB=%d/%d ratio=%.2f dimBEntropy=%.2f dimBShardBonus=%d dimBRepairEntropy=%.1f",
                     getDimensionShiftDimBRiftCount(m_currentDifficulty, static_cast<int>(rifts.size())),
                     static_cast<int>(rifts.size()),
                     shiftBalance.dimBRatio,
                     shiftBalance.dimBEntropyPerSecond,
                     getDimensionShiftDimBShardBonusPercent(m_currentDifficulty),
                     shiftBalance.dimBEntropyRepairBonus);
        }
        smokeLog("[SMOKE]   spawn=(%.0f,%.0f) exit=(%.0f,%.0f)",
                spawn.x, spawn.y, exit.x, exit.y);
        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            smokeLog("[SMOKE]   rift[%d]=(%.0f,%.0f) requiredDim=%d",
                     i, rifts[i].x, rifts[i].y, m_level->getRiftRequiredDimension(i));
        }
        smokeLogLevelValidation(*m_level);
        if (g_smokeRegression &&
            (!topology.validation.passed || topology.validation.failureMask != LevelValidationFailure_None)) {
            smokeSetRegressionFailure(SmokeRegressionFailure_Topology,
                                      "level=%d seed=%d topology invalid mask=%s",
                                      m_currentDifficulty,
                                      levelSeed,
                                      smokeFailureMaskToString(topology.validation.failureMask).c_str());
            game->quit();
            return;
        }

        // Validate: check exit has floor below in both dimensions
        int exitTX = static_cast<int>(exit.x) / 32;
        int exitTY = static_cast<int>(exit.y) / 32;
        bool exitFloorA = m_level->isSolid(exitTX, exitTY + 1, 1);
        bool exitFloorB = m_level->isSolid(exitTX, exitTY + 1, 2);
        if (!exitFloorA && !exitFloorB) {
            smokeLog("[SMOKE] WARNING: Exit has no floor in either dimension!");
        }
        // Validate: check spawn has floor
        int spawnTX = static_cast<int>(spawn.x) / 32;
        int spawnTY = static_cast<int>(spawn.y) / 32;
        bool spawnFloorA = m_level->isSolid(spawnTX, spawnTY + 1, 1);
        bool spawnFloorB = m_level->isSolid(spawnTX, spawnTY + 1, 2);
        if (!spawnFloorA && !spawnFloorB) {
            smokeLog("[SMOKE] WARNING: Spawn has no floor in either dimension!");
        }
        // Validate: check each rift has floor within 5 tiles below
        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            int rtx = static_cast<int>(rifts[i].x) / 32;
            int rty = static_cast<int>(rifts[i].y) / 32;
            bool hasFloor = false;
            for (int b = 1; b <= 5; b++) {
                bool supportA = m_level->isSolid(rtx, rty + b, 1) || m_level->isOneWay(rtx, rty + b, 1);
                bool supportB = m_level->isSolid(rtx, rty + b, 2) || m_level->isOneWay(rtx, rty + b, 2);
                if (supportA || supportB) {
                    hasFloor = true; break;
                }
            }
            if (!hasFloor) {
                smokeLog("[SMOKE] WARNING: Rift[%d] at (%d,%d) has no floor within 5 tiles!", i, rtx, rty);
            }
            // Check rift isn't inside a wall
            if (m_level->isSolid(rtx, rty, 1) && m_level->isSolid(rtx, rty, 2)) {
                smokeLog("[SMOKE] ERROR: Rift[%d] at (%d,%d) is inside solid in BOTH dimensions!", i, rtx, rty);
            }
        }
    }
    levelTimer += dt;

    // Periodic status log every 5 seconds
    static float lastStatusLog = 0;
    if (m_smokeRunTime - lastStatusLog >= 5.0f) {
        lastStatusLog = m_smokeRunTime;
        int playerRoom = smokeFindTopologyRoomIndex(topology, playerPos);
        smokeLog("[SMOKE] t=%.0fs lvl=%d pos=(%.0f,%.0f) room=%d vel=(%.0f,%.0f) rifts=%d/%d ground=%d dim=%d target=%s:%d targetRoom=%d targetDim=%d collapsing=%d frames=%d genValidated=%d",
                m_smokeRunTime, m_currentDifficulty, playerPos.x, playerPos.y, playerRoom,
                phys.velocity.x, phys.velocity.y,
                m_levelRiftsRepaired, static_cast<int>(m_level->getRiftPositions().size()),
                phys.onGround ? 1 : 0, currentDim,
                targetInfo.type, targetInfo.index, targetInfo.roomIndex, preferredTargetDim,
                m_collapsing ? 1 : 0, smokeFrameCount,
                topology.validation.passed ? 1 : 0);
    }

    bool targetIsExit = targetInfo.type == "exit";
    if (targetIsExit != smokeLastTargetWasExit ||
        targetInfo.index != smokeLastTargetIndex ||
        targetInfo.roomIndex != smokeLastTargetRoom) {
        smokeLastTargetWasExit = targetIsExit;
        smokeLastTargetIndex = targetInfo.index;
        smokeLastTargetRoom = targetInfo.roomIndex;
        smokeLogRuntimeContext("TARGET", *m_level, m_currentDifficulty, playerPos, currentDim,
                               targetInfo, m_collapsing, m_levelRiftsRepaired);
    }

    auto& inputMut = game->getInputMutable();
    auto applyRecoveryAnchor = [&](const char* label,
                                   const SmokeRecoveryAnchor& anchor,
                                   bool armInteract,
                                   bool resetNoProgressSkips) {
        auto& transform = m_player->getEntity()->getComponent<TransformComponent>();
        smokeLogRecoveryAnchor(label, *m_level, m_currentDifficulty, playerPos, targetInfo, anchor, currentDim);
        if (g_smokeRegression && anchor.spawnFallback) {
            smokeSetRegressionFailure(SmokeRegressionFailure_SpawnFallback,
                                      "%s seed=%d level=%d target=%s idx=%d room=%d reason=%s",
                                      label,
                                      m_level->getGenerationSeed(),
                                      m_currentDifficulty,
                                      targetInfo.type,
                                      targetInfo.index,
                                      targetInfo.roomIndex,
                                      anchor.reason);
            game->quit();
            return;
        }
        transform.position = anchor.position;
        phys.velocity = {0, 0};
        smokeRecoveryGraceTimer = 1.5f;
        smokeRecoverySuppressedLogged = false;
        if (anchor.dimension != currentDim) {
            inputMut.injectActionPress(Action::DimensionSwitch);
            smokeLog("[SMOKE]   recovery align dim=%d -> %d via %s", currentDim, anchor.dimension, anchor.reason);
        }
        if (armInteract && !m_collapsing && smokeCurrentTarget >= 0 && anchor.roomIndex == targetInfo.roomIndex) {
            inputMut.injectActionPress(Action::Interact);
            smokeLog("[SMOKE]   recovery anchor is target room %d, arming interact for rift %d",
                     anchor.roomIndex, smokeCurrentTarget);
        }
        lastStuckCheckPos = anchor.position;
        stuckTimer = 0;
        stuckLogged = false;
        if (resetNoProgressSkips) {
            noProgressSkips = 0;
        }
    };

    // Auto-rescue: only below true map bottom or far below spawn.
    {
        Vec2 spawn = m_level->getSpawnPoint();
        float maxDropBelow = std::max(500.0f, m_level->getPixelHeight() * 0.4f);
        float mapBottom = m_level->getPixelHeight() + 96.0f;
        if (playerPos.y > spawn.y + maxDropBelow || playerPos.y > mapBottom) {
            if (smokeRecoveryGraceTimer > 0) {
                if (!smokeRecoverySuppressedLogged) {
                    smokeLog("[SMOKE] Rescue suppressed for %.1fs after recovery teleport", smokeRecoveryGraceTimer);
                    smokeRecoverySuppressedLogged = true;
                }
            } else {
                SmokeRecoveryAnchor recoveryAnchor = smokeBuildRecoveryAnchor(*m_level,
                                                                              targetInfo,
                                                                              playerPos,
                                                                              currentDim,
                                                                              preferredTargetDim,
                                                                              m_collapsing);
                const char* rescueLabel = recoveryAnchor.spawnFallback ? "AUTO_RESCUE_SPAWN" : "AUTO_RESCUE_PATH";
                smokeLogRuntimeContext(rescueLabel, *m_level, m_currentDifficulty, playerPos, currentDim,
                                       targetInfo, m_collapsing, m_levelRiftsRepaired);
                applyRecoveryAnchor("AUTO_RESCUE", recoveryAnchor, false, false);
                smokeLog("[SMOKE] Auto-rescue: fell too far (y=%.0f, spawn=%.0f, mapBottom=%.0f), teleport via %s room=%d",
                         playerPos.y, spawn.y, mapBottom, recoveryAnchor.reason, recoveryAnchor.roomIndex);
                return;
            }
        }
    }

    // Target: default 5 levels; regression mode can stop earlier with a fixed target floor.
    int smokeTargetLevel = g_smokeRegression ? std::max(1, g_smokeTargetFloor) : 5;
    if (m_currentDifficulty > smokeTargetLevel) {
        static bool successLogged = false;
        if (!successLogged) {
            successLogged = true;
            if (g_smokeRegression) {
                smokeLogRegressionResult("PASS", m_smokeRunTime);
            }
            smokeLog("[SMOKE] === SUCCESS === Completed %d levels in %.1fs!", smokeTargetLevel, m_smokeRunTime);
            writeBalanceSnapshot();
        }
        game->quit();
        return;
    }

    // Per-level timeout: 120s per level
    if (levelTimer > 120.0f) {
        smokeLog("[SMOKE] LEVEL TIMEOUT: Level %d not completed in 120s (rifts=%d/%d collapsing=%d)",
                m_currentDifficulty, m_levelRiftsRepaired,
                static_cast<int>(m_level->getRiftPositions().size()),
                m_collapsing ? 1 : 0);
        smokeLogRuntimeContext("TIMEOUT", *m_level, m_currentDifficulty, playerPos, currentDim,
                               targetInfo, m_collapsing, m_levelRiftsRepaired);
        if (g_smokeRegression) {
            smokeSetRegressionFailure(SmokeRegressionFailure_LevelTimeout,
                                      "seed=%d level=%d timed out at %.1fs",
                                      m_level->getGenerationSeed(),
                                      m_currentDifficulty,
                                      m_smokeRunTime);
            game->quit();
            return;
        }
        // Force-advance to next level to continue testing
        smokeLog("[SMOKE] Force-advancing to next level...");
        m_currentDifficulty++;
        m_runSeed += 100;
        m_levelComplete = false;
        m_levelCompleteTimer = 0;
        m_pendingLevelGen = false;
        m_collapsing = false;
        generateLevel();
        m_combatSystem.setPlayer(m_player.get());
        m_aiSystem.setLevel(m_level.get());
        m_aiSystem.setPlayer(m_player.get());
        levelTimer = 0;
        return;
    }

    float smokeMaxRuntime = g_smokeRegression ? g_smokeMaxRuntime : 600.0f;
    if (m_smokeRunTime > smokeMaxRuntime) {
        smokeLog("[SMOKE] TOTAL TIMEOUT after %.0fs on level %d", smokeMaxRuntime, m_currentDifficulty);
        smokeLogRuntimeContext("TOTAL_TIMEOUT", *m_level, m_currentDifficulty, playerPos, currentDim,
                               targetInfo, m_collapsing, m_levelRiftsRepaired);
        if (g_smokeRegression) {
            smokeSetRegressionFailure(SmokeRegressionFailure_TotalTimeout,
                                      "seed=%d completedFloor=%d targetFloor=%d runtime=%.1fs",
                                      m_level->getGenerationSeed(),
                                      g_smokeCompletedFloor,
                                      g_smokeTargetFloor,
                                      m_smokeRunTime);
        }
        writeBalanceSnapshot();
        game->quit();
        return;
    }

    // Auto-select relic when choice appears
    if (m_showRelicChoice && !m_relicChoices.empty()) {
        int pick = std::rand() % static_cast<int>(m_relicChoices.size());
        smokeLog("[SMOKE] Picked relic: %d", static_cast<int>(m_relicChoices[pick]));
        selectRelic(pick);
        return;
    }

    // Auto-dismiss NPC dialog
    if (m_showNPCDialog) {
        m_showNPCDialog = false;
        return;
    }

    // Auto-solve active puzzle
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        switch (m_activePuzzle->getType()) {
            case PuzzleType::Timing:
                // Only press when cursor is in sweet spot (misses reduce hit count!)
                if (m_activePuzzle->isInSweetSpot()) {
                    m_activePuzzle->handleInput(4);
                }
                break;
            case PuzzleType::Sequence:
                // Feed the correct sequence directly
                if (!m_activePuzzle->isShowingSequence()) {
                    int idx = static_cast<int>(m_activePuzzle->getPlayerInput().size());
                    if (idx < static_cast<int>(m_activePuzzle->getSequence().size())) {
                        m_activePuzzle->handleInput(m_activePuzzle->getSequence()[idx]);
                    }
                }
                break;
            case PuzzleType::Alignment:
                // Rotate until aligned
                if (m_activePuzzle->getCurrentRotation() != m_activePuzzle->getTargetRotation()) {
                    m_activePuzzle->handleInput(1); // rotate right
                }
                break;
            case PuzzleType::Pattern:
                // Wait for reveal phase, then select correct cells
                if (!m_activePuzzle->isPatternShowing()) {
                    // Find next target cell not yet selected
                    for (int py = 0; py < 3; py++) {
                        for (int px = 0; px < 3; px++) {
                            if (m_activePuzzle->getPatternTarget(px, py) &&
                                !m_activePuzzle->getPatternPlayer(px, py)) {
                                // Navigate cursor to this cell
                                int cx = m_activePuzzle->getPatternCursorX();
                                int cy = m_activePuzzle->getPatternCursorY();
                                if (cx < px) { m_activePuzzle->handleInput(1); goto patternDone; }
                                if (cx > px) { m_activePuzzle->handleInput(3); goto patternDone; }
                                if (cy < py) { m_activePuzzle->handleInput(2); goto patternDone; }
                                if (cy > py) { m_activePuzzle->handleInput(0); goto patternDone; }
                                // Cursor is on target cell, confirm
                                m_activePuzzle->handleInput(4);
                                goto patternDone;
                            }
                        }
                    }
                    patternDone:;
                }
                break;
        }
        return; // Don't move while solving puzzle
    }

    // Navigate toward exit or nearest unrepaired rift
    Vec2 target = targetInfo.position;

    float dirX = target.x - playerPos.x;
    float dirY = target.y - playerPos.y;
    float distToTarget = std::sqrt(dirX * dirX + dirY * dirY);

    // Progress tracking: detect no-progress situations (bouncing, circling)
    if (distToTarget < bestDistToTarget - 20.0f) {
        bestDistToTarget = distToTarget;
        noProgressTimer = 0;
    } else {
        noProgressTimer += dt;
    }
    // After 5s without getting closer (3s during collapse — exit is urgent)
    float noProgressThreshold = m_collapsing ? 3.0f : 5.0f;
    bool noProgress = noProgressTimer > noProgressThreshold;

    // Stuck detection: position barely changed
    float movedDist = std::sqrt((playerPos.x - lastStuckCheckPos.x) * (playerPos.x - lastStuckCheckPos.x)
                              + (playerPos.y - lastStuckCheckPos.y) * (playerPos.y - lastStuckCheckPos.y));
    if (movedDist < 40.0f) {
        stuckTimer += dt;
    } else {
        stuckTimer = 0;
        stuckLogged = false;
        lastStuckCheckPos = playerPos;
    }
    bool isStuck = stuckTimer > 4.0f;

    // Diagnostic logging when stuck
    if ((isStuck || noProgress) && !stuckLogged) {
        stuckLogged = true;
        int ptx = static_cast<int>(playerPos.x) / 32;
        int pty = static_cast<int>(playerPos.y) / 32;
        int dim = m_dimManager.getCurrentDimension();
        smokeLog("[SMOKE] STUCK at tile (%d,%d) dim=%d ground=%d wallL=%d wallR=%d jumps=%d noProg=%.0f",
                ptx, pty, dim, phys.onGround ? 1 : 0,
                phys.onWallLeft ? 1 : 0, phys.onWallRight ? 1 : 0,
                m_player->jumpsRemaining, noProgressTimer);
        for (int dy = -2; dy <= 2; dy++) {
            char row[32];
            for (int dx = -2; dx <= 2; dx++) {
                int tx = ptx + dx, ty = pty + dy;
                if (m_level->inBounds(tx, ty) && m_level->isSolid(tx, ty, dim))
                    row[dx + 2] = '#';
                else if (m_level->inBounds(tx, ty) && m_level->isOneWay(tx, ty, dim))
                    row[dx + 2] = '-';
                else
                    row[dx + 2] = '.';
            }
            row[5] = '\0';
            smokeLog("[SMOKE]   tiles[y%+d]: %s", dy, row);
        }
    }

    // No-progress escape: recover toward a validated main-path or target anchor.
    if (noProgress) {
        noProgressTimer = 0;
        bestDistToTarget = 99999.0f;
        noProgressSkips++;
        int playerRoom = smokeFindTopologyRoomIndex(topology, playerPos);
        bool sameRoomTarget = targetInfo.validated &&
                              targetInfo.roomIndex >= 0 &&
                              playerRoom == targetInfo.roomIndex;
        SmokeRecoveryAnchor recoveryAnchor = smokeBuildRecoveryAnchor(*m_level,
                                                                      targetInfo,
                                                                      playerPos,
                                                                      currentDim,
                                                                      preferredTargetDim,
                                                                      m_collapsing);
        const char* contextLabel = recoveryAnchor.spawnFallback
            ? "NO_PROGRESS_SPAWN"
            : (recoveryAnchor.targetsObjective
               ? (m_collapsing
                  ? "NO_PROGRESS_EXIT"
                  : (sameRoomTarget ? "NO_PROGRESS_TARGET_ROOM" : "NO_PROGRESS_TARGET"))
               : "NO_PROGRESS_PATH");
        smokeLog("[SMOKE] No progress (%d): recovery via %s room=%d tile=(%d,%d)",
                 noProgressSkips,
                 recoveryAnchor.reason,
                 recoveryAnchor.roomIndex,
                 recoveryAnchor.tileX,
                 recoveryAnchor.tileY);
        smokeLogRuntimeContext(contextLabel, *m_level, m_currentDifficulty, playerPos, currentDim,
                               targetInfo, m_collapsing, m_levelRiftsRepaired);
        applyRecoveryAnchor("NO_PROGRESS", recoveryAnchor, recoveryAnchor.targetsObjective, true);
        return;
    }

    // Stuck escape (position locked) — dimension switch then teleport
    if (stuckTimer > 3.0f && stuckTimer < 3.0f + dt * 2) {
        inputMut.injectActionPress(Action::DimensionSwitch);
    }
    if (stuckTimer > 5.0f) {
        int playerRoom = smokeFindTopologyRoomIndex(topology, playerPos);
        bool sameRoomTarget = targetInfo.validated &&
                              targetInfo.roomIndex >= 0 &&
                              playerRoom == targetInfo.roomIndex;
        SmokeRecoveryAnchor recoveryAnchor = smokeBuildRecoveryAnchor(*m_level,
                                                                      targetInfo,
                                                                      playerPos,
                                                                      currentDim,
                                                                      preferredTargetDim,
                                                                      m_collapsing);
        const char* contextLabel = recoveryAnchor.spawnFallback
            ? "STUCK_SPAWN_RECOVERY"
            : (recoveryAnchor.targetsObjective
               ? (m_collapsing ? "STUCK_EXIT_RECOVERY"
                               : (sameRoomTarget ? "STUCK_TARGET_RECOVERY" : "STUCK_OBJECTIVE_RECOVERY"))
               : "STUCK_PATH_RECOVERY");
        smokeLogRuntimeContext(contextLabel, *m_level, m_currentDifficulty, playerPos, currentDim,
                               targetInfo, m_collapsing, m_levelRiftsRepaired);
        applyRecoveryAnchor("STUCK", recoveryAnchor, recoveryAnchor.targetsObjective, false);
        return;
    }

    // Navigation: floor-following with smart gap/wall handling
    int ptx = static_cast<int>(playerPos.x) / 32;
    int pty = static_cast<int>(playerPos.y) / 32;
    int dim = m_dimManager.getCurrentDimension();
    int otherDim = (dim == 1) ? 2 : 1;
    int moveDir = (dirX >= 0) ? 1 : -1;
    bool targetAbove = dirY < -40.0f;

    // Scan ahead: check for floor and walls in movement direction
    bool hasFloorAhead = false;
    bool wallAhead = false;
    for (int dx = 1; dx <= 3; dx++) {
        int cx = ptx + moveDir * dx;
        // Check wall at player height
        if (dx == 1 && m_level->inBounds(cx, pty) && m_level->isSolid(cx, pty, dim))
            wallAhead = true;
        // Check for any floor within 3 tiles below
        for (int dy = 0; dy <= 3; dy++) {
            if (m_level->inBounds(cx, pty + 1 + dy) &&
                (m_level->isSolid(cx, pty + 1 + dy, dim) || m_level->isOneWay(cx, pty + 1 + dy, dim))) {
                hasFloorAhead = true;
                break;
            }
        }
        if (hasFloorAhead) break;
    }

    // Proactive dimension switch: wall ahead or no floor, check other dim
    static float dimSwitchCooldown = 0;
    dimSwitchCooldown -= dt;
    if ((wallAhead || (!hasFloorAhead && phys.onGround)) && dimSwitchCooldown <= 0) {
        bool otherBetter = false;
        if (wallAhead) {
            int cx = ptx + moveDir;
            otherBetter = m_level->inBounds(cx, pty) && !m_level->isSolid(cx, pty, otherDim);
        }
        if (!hasFloorAhead) {
            for (int dx = 1; dx <= 3; dx++) {
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
            dimSwitchCooldown = 1.0f;
        }
    }

    // Horizontal: always move toward target X
    if (dirX > 30.0f) {
        inputMut.injectActionPress(Action::MoveRight);
    } else if (dirX < -30.0f) {
        inputMut.injectActionPress(Action::MoveLeft);
    } else if (std::abs(dirY) > 200.0f) {
        // At target X but wrong Y: explore to find corridors
        if (static_cast<int>(m_smokeRunTime * 2) % 6 < 3)
            inputMut.injectActionPress(Action::MoveRight);
        else
            inputMut.injectActionPress(Action::MoveLeft);
    }

    // Jumping logic
    bool blocked = (phys.onWallLeft || phys.onWallRight) ||
                   (std::abs(phys.velocity.x) < 10.0f && std::abs(dirX) > 50.0f);

    if (phys.onGround) {
        // Jump: when blocked, target above, gap ahead, or random exploration
        if (blocked || targetAbove || !hasFloorAhead || (std::rand() % 30 == 0)) {
            inputMut.injectActionPress(Action::Jump);
        }
    }
    // Wall-jump: always (key for climbing shafts)
    if (m_player->isWallSliding) {
        inputMut.injectActionPress(Action::Jump);
    }
    // Double-jump: use when target is above or when falling into a pit
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
        if ((targetAbove && (std::rand() % 8 == 0)) || (falling && noPlatformBelow)) {
            inputMut.injectActionPress(Action::Jump);
        }
    }
    // Dash: for speed when on ground with clear path
    if (phys.onGround && m_player->dashCooldownTimer <= 0 && hasFloorAhead &&
        (distToTarget > 300.0f || (std::rand() % 30 == 0))) {
        inputMut.injectActionPress(Action::Dash);
    }

    // Interact with rift when near (m_nearRiftIndex >= 0 implies dist < 60)
    if (!m_activePuzzle && m_nearRiftIndex >= 0 &&
        m_level->isRiftActiveInDimension(m_nearRiftIndex, currentDim)) {
        game->getInputMutable().injectActionPress(Action::Interact);
        smokeLog("[SMOKE] Interacting with rift %d", m_nearRiftIndex);
    }

    // Attack nearby enemies
    if (m_smokeActionTimer <= 0) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        float nearestEnemy = 99999.0f;
        Vec2 enemyDir = {1.0f, 0.0f};
        m_entities.forEach([&](Entity& e) {
            if (!e.isEnemy || !e.isAlive()) return;
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

        if (nearestEnemy < 80.0f) {
            combat.startAttack(AttackType::Melee, enemyDir);
            m_smokeActionTimer = 0.25f;
        } else if (nearestEnemy < 300.0f) {
            combat.startAttack(AttackType::Ranged, enemyDir);
            m_smokeActionTimer = 0.4f;
        } else {
            m_smokeActionTimer = 0.1f;
        }
    }

    // Topology-aware dimension alignment: only switch when the active target prefers the other dimension.
    if (m_smokeDimTimer <= 0 && preferredTargetDim != currentDim) {
        inputMut.injectActionPress(Action::DimensionSwitch);
        smokeLog("[SMOKE] Aligning dimension %d -> %d for %s target", currentDim, preferredTargetDim, targetInfo.type);
        m_smokeDimTimer = 1.5f;
    }

    // Random dash for mobility
    if (std::rand() % 80 == 0 && phys.onGround && m_player->dashCooldownTimer <= 0) {
        inputMut.injectActionPress(Action::Dash);
    }
}

