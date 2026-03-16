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
#include "States/EndingState.h"
#include "States/RunSummaryState.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <algorithm>
#include <string>

extern bool g_autoSmokeTest;
extern bool g_autoPlaytest;
extern int g_forcedRunSeed;
extern bool g_smokeRegression;
extern int g_smokeTargetFloor;
extern float g_smokeMaxRuntime;
extern int g_smokeCompletedFloor;
extern int g_smokeFailureCode;
extern char g_smokeFailureReason[256];

static FILE* g_smokeFile = nullptr;

namespace {
constexpr bool kUseAiFinalBackgroundArtTest = true;
constexpr const char* kDimAFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dima_run01_s1103_fin.png";
constexpr const char* kDimBFinalBackgroundPath =
    "assets/ai/finals/backgrounds/run01/rw_bg_dimb_run01_s2103_fin.png";
constexpr const char* kBossFinalBackgroundPath =
    "assets/ai/finals/boss/run01/rw_boss_run01_f03_s3120_fin.png";
constexpr float kArtTestBackgroundParallaxX = 0.24f;

void renderWholeArtBackground(SDL_Renderer* renderer,
                              SDL_Texture* texture,
                              const Vec2& camPos,
                              int screenW,
                              int screenH,
                              int worldPixelWidth,
                              float parallaxX,
                              Uint8 colorR,
                              Uint8 colorG,
                              Uint8 colorB,
                              Uint8 alpha) {
    if (!texture) return;

    int texW = 0;
    int texH = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH) != 0 || texW <= 0 || texH <= 0) {
        return;
    }

    const float scaleX = static_cast<float>(screenW) / static_cast<float>(texW);
    const float scaleY = static_cast<float>(screenH) / static_cast<float>(texH);
    const float coverScale = std::max(scaleX, scaleY);
    const float minCameraX = screenW * 0.5f;
    const float maxCameraX = std::max(minCameraX, static_cast<float>(worldPixelWidth) - screenW * 0.5f);
    const float cameraTravel = std::max(0.0f, maxCameraX - minCameraX);
    const float requiredWidth = static_cast<float>(screenW) + cameraTravel * parallaxX;
    const float scale = std::max(coverScale, requiredWidth / static_cast<float>(texW));
    const int drawW = static_cast<int>(std::ceil(static_cast<float>(texW) * scale));
    const int drawH = static_cast<int>(std::ceil(static_cast<float>(texH) * scale));

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(texture, colorR, colorG, colorB);
    SDL_SetTextureAlphaMod(texture, alpha);

    float cameraT = 0.5f;
    if (cameraTravel > 0.001f) {
        cameraT = std::clamp((camPos.x - minCameraX) / cameraTravel, 0.0f, 1.0f);
    }

    const int travelPixels = std::max(0, drawW - screenW);
    const int dstX = -static_cast<int>(std::round(static_cast<float>(travelPixels) * cameraT));
    const int dstY = (screenH - drawH) / 2;
    SDL_Rect dst{dstX, dstY, drawW, drawH};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);

    SDL_SetTextureColorMod(texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(texture, 255);
}
}

static void smokeLog(const char* fmt, ...) {
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
    bool targetIsExit = std::strcmp(target.type, "exit") == 0;
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

static FILE* g_playtestFile = nullptr;

void PlayState::enter() {
    if (g_autoSmokeTest) {
        m_smokeTest = true;
        m_showDebugOverlay = true;
        // Open persistent log file
        if (g_smokeFile) fclose(g_smokeFile);
        g_smokeFile = fopen("smoke_test.log", "w");
        if (g_smokeFile) {
            fprintf(g_smokeFile, "=== RIFTWALKER SMOKE TEST ===\n");
            fflush(g_smokeFile);
        }
    }
    if (g_autoPlaytest) {
        m_playtest = true;
        m_showDebugOverlay = true;
        if (g_playtestFile) fclose(g_playtestFile);
        g_playtestFile = fopen("playtest_report.log", "w");
        m_playtestRun = 0;
        m_playtestDeaths = 0;
        m_playtestBestLevel = 0;
        playtestLog("========================================");
        playtestLog("  RIFTWALKER BALANCE PLAYTEST");
        playtestLog("  Modus: Menschliche Simulation");
        playtestLog("  Max Versuche: %d", m_playtestMaxRuns);
        playtestLog("========================================");
    }
    startNewRun();
    if (m_playtest) {
        playtestStartRun();
    }
}

void PlayState::exit() {
    AudioManager::instance().stopAmbient();
    AudioManager::instance().stopMusicLayers();
    AudioManager::instance().stopThemeAmbient();
    m_dimATestBackground = nullptr;
    m_dimBTestBackground = nullptr;
    m_bossTestBackground = nullptr;
    m_entities.clear();
    m_particles.clear();
    m_player.reset();
    m_level.reset();
    m_activePuzzle.reset();
}

void PlayState::startNewRun() {
    m_entities.clear();
    m_particles.clear();

    if (g_smokeRegression) {
        g_smokeCompletedFloor = 0;
        g_smokeFailureCode = 0;
        g_smokeFailureReason[0] = '\0';
    }

    m_currentDifficulty = 1;
    m_isDailyRun = g_dailyRunActive;
    m_runSeed = (g_forcedRunSeed >= 0)
        ? g_forcedRunSeed
        : (m_isDailyRun ? DailyRun::getTodaySeed() : std::rand());
    if (g_smokeRegression) {
        std::srand(static_cast<unsigned int>(m_runSeed));
    }
    g_dailyRunActive = false; // Reset flag
    enemiesKilled = 0;
    riftsRepaired = 0;
    roomsCleared = 0;
    shardsCollected = 0;
    m_collapsing = false;
    m_riftDimensionHintTimer = 0;
    m_riftDimensionHintRequiredDim = 0;
    m_tutorialTimer = 0;
    m_tutorialHintIndex = 0;
    m_tutorialHintShowTimer = 0;
    std::memset(m_tutorialHintDone, 0, sizeof(m_tutorialHintDone));
    m_hasMovedThisRun = false;
    m_hasJumpedThisRun = false;
    m_hasDashedThisRun = false;
    m_hasAttackedThisRun = false;
    m_hasRangedThisRun = false;
    m_hasUsedAbilityThisRun = false;
    m_shownEntropyWarning = false;
    m_shownConveyorHint = false;
    m_shownDimPlatformHint = false;
    m_shownRelicHint = false;
    m_shownWallSlideHint = false;
    m_dashCount = 0;
    m_aerialKillsThisRun = 0;
    m_dashKillsThisRun = 0;
    m_chargedKillsThisRun = 0;
    m_tookDamageThisLevel = false;
    m_pendingLevelGen = false;
    m_showRelicChoice = false;
    m_relicChoices.clear();
    m_savedRelics.clear();
    m_savedHP = 0;
    m_savedMaxHP = 0;
    m_savedVoidHungerBonus = 0;
    m_relicChoiceSelected = 0;
    m_voidSovereignDefeated = false;
    m_trails.clear();
    m_moveTrailTimer = 0.0f;
    m_runTime = 0.0f;
    m_bestCombo = 0;
    m_nearNPCIndex = -1;
    m_showNPCDialog = false;
    m_npcDialogChoice = 0;
    m_echoSpawned = false;
    m_echoRewarded = false;
    std::memset(m_npcStoryProgress, 0, sizeof(m_npcStoryProgress));
    m_combatChallenge = {};
    m_challengeCompleteTimer = 0;
    m_challengesCompleted = 0;
    m_tookDamageThisWave = false;
    m_balanceStats = {};
    m_combatSystem.voidResonanceProcs = 0;

    // Reset per-run unlock tracking
    m_unlockedBossWeaponThisRun = false;
    m_aoeKillCountThisRun = 0;
    m_dashKillsThisRunForWeapon = 0;
    std::memset(m_unlockNotifs, 0, sizeof(m_unlockNotifs));
    m_unlockNotifHead = 0;

    // Event chain: 30% chance to start a chain per run
    m_eventChain = {};
    m_chainEventSpawned = false;
    m_chainEventIndex = -1;
    m_chainNotifyTimer = 0;
    m_chainRewardTimer = 0;
    m_chainRewardShards = 0;
    if (std::rand() % 100 < 30) {
        m_eventChain.type = static_cast<EventChainType>(std::rand() % static_cast<int>(EventChainType::COUNT));
        m_eventChain.stage = 1;
        m_eventChain.startLevel = 1;
        m_chainNotifyTimer = 4.0f; // Show intro notification
    }

    // Reset run buffs
    game->getRunBuffSystem().reset();

    // Random theme pair
    auto themes = WorldTheme::getRandomPair(m_runSeed);
    m_themeA = themes.first;
    m_themeB = themes.second;
    m_dimATestBackground = nullptr;
    m_dimBTestBackground = nullptr;
    m_bossTestBackground = nullptr;
    if (kUseAiFinalBackgroundArtTest) {
        m_dimATestBackground = ResourceManager::instance().getTexture(kDimAFinalBackgroundPath);
        m_dimBTestBackground = ResourceManager::instance().getTexture(kDimBFinalBackgroundPath);
        m_bossTestBackground = ResourceManager::instance().getTexture(kBossFinalBackgroundPath);
        if (!m_dimATestBackground) {
            SDL_Log("AI test background missing for DIM-A: %s", kDimAFinalBackgroundPath);
        }
        if (!m_dimBTestBackground) {
            SDL_Log("AI test background missing for DIM-B: %s", kDimBFinalBackgroundPath);
        }
        if (!m_bossTestBackground) {
            SDL_Log("AI test background missing for boss floors: %s", kBossFinalBackgroundPath);
        }
    }

    m_dimManager = DimensionManager();
    m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
    m_dimManager.particles = &m_particles;
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
    AudioManager::instance().startMusicLayers();
    AudioManager::instance().playThemeAmbient(static_cast<int>(m_themeA.id));

    m_entropy = SuitEntropy();
    m_combatSystem.resetRunState();
    m_combatSystem.setParticleSystem(&m_particles);
    m_combatSystem.setCamera(&m_camera);
    m_aiSystem.setParticleSystem(&m_particles);
    m_aiSystem.setCamera(&m_camera);
    m_aiSystem.setCombatSystem(&m_combatSystem);
    m_hitFreezeTimer = 0;
    m_spikeDmgCooldown = 0;
    m_waveClearTriggered = false;
    m_waveClearTimer = 0;
    m_playerDying = false;
    m_deathSequenceTimer = 0;
    m_deathCause = 0;

    generateLevel();
    m_combatSystem.setPlayer(m_player.get());
    m_combatSystem.setDimensionManager(&m_dimManager);
    m_hud.setCombatSystem(&m_combatSystem);
    m_aiSystem.setLevel(m_level.get());
    m_aiSystem.setPlayer(m_player.get());
}

void PlayState::generateLevel() {
    m_entities.clear();
    m_particles.clear();

    m_levelGen.setThemes(m_themeA, m_themeB);
    m_level = std::make_unique<Level>(m_levelGen.generate(m_currentDifficulty, m_runSeed + m_currentDifficulty));

    // Try to load tileset for sprite-based tile rendering
    m_level->loadTileset();

    // Create player with selected class
    m_player = std::make_unique<Player>(m_entities);
    m_player->particles = &m_particles;
    m_player->entityManager = &m_entities;
    m_player->combatSystemRef = &m_combatSystem;
    m_player->dimensionManager = &m_dimManager;
    m_player->playerClass = g_selectedClass;
    m_player->applyClassStats();
    applyUpgrades();
    applyAscensionModifiers();

    // Register heal callback for floating green numbers
    {
        auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
        hp.onHeal = [this](float amount) {
            if (amount < 1.0f) return; // Skip tiny heals to avoid spam
            if (!m_player) return;
            auto& t = m_player->getEntity()->getComponent<TransformComponent>();
            FloatingDamageNumber num;
            num.position = t.getCenter();
            num.position.x += static_cast<float>((std::rand() % 20) - 10); // Slight random offset
            num.value = amount;
            num.isPlayerDamage = false;
            num.isHeal = true;
            num.lifetime = 0.9f;
            num.maxLifetime = num.lifetime;
            m_damageNumbers.push_back(num);
        };
    }

    // Voidwalker: reduced switch cooldown
    if (g_selectedClass == PlayerClass::Voidwalker) {
        m_dimManager.switchCooldown = 0.5f * ClassSystem::getData(PlayerClass::Voidwalker).switchCDReduction;
    }

    auto& playerTransform = m_player->getEntity()->getComponent<TransformComponent>();
    playerTransform.position = m_level->getSpawnPoint();

    m_camera.setPosition(playerTransform.getCenter());
    m_camera.setBounds(0, 0, m_level->getPixelWidth(), m_level->getPixelHeight());

    spawnEnemies();

    // Boss every 3 levels
    m_isBossLevel = (m_currentDifficulty % 3 == 0);
    m_bossDefeated = false;
    if (m_isBossLevel) {
        spawnBoss();
    }

    m_levelComplete = false;
    m_levelCompleteTimer = 0;
    m_activePuzzle.reset();
    m_nearRiftIndex = -1;
    m_riftDimensionHintTimer = 0;
    m_riftDimensionHintRequiredDim = 0;
    // FIX: Reset per-level rift tracking
    m_repairedRiftIndices.clear();
    m_levelRiftsRepaired = 0;
    // FIX: Reset collapse state for new level
    m_collapsing = false;
    m_collapseTimer = 0;
    m_teleportCooldown = 0;
    m_tookDamageThisLevel = false;

    // Event chain: spawn chain-specific event this level
    m_chainEventSpawned = false;
    m_chainEventIndex = -1;
    if (m_eventChain.stage > 0 && !m_eventChain.completed) {
        spawnChainEvent();
    }

    // Start ambient music
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
}

void PlayState::spawnEnemies() {
    auto spawns = m_level->getEnemySpawns();

    // Difficulty modifier: Easy removes 30% of spawns, Hard adds 40% extra
    if (g_selectedDifficulty == GameDifficulty::Easy) {
        int removeCount = static_cast<int>(spawns.size() * 0.3f);
        for (int i = 0; i < removeCount && !spawns.empty(); i++) {
            spawns.pop_back();
        }
    } else if (g_selectedDifficulty == GameDifficulty::Hard) {
        int extraCount = static_cast<int>(spawns.size() * 0.4f);
        int origSize = static_cast<int>(spawns.size());
        spawns.reserve(spawns.size() + extraCount);
        for (int i = 0; i < extraCount; i++) {
            auto base = spawns[std::rand() % origSize]; // copy to avoid dangling ref
            Vec2 offset = {static_cast<float>((std::rand() % 100) - 50), 0};
            spawns.push_back({Vec2{base.position.x + offset.x, base.position.y}, base.enemyType, base.dimension});
        }
    }

    // Divide spawns into waves of 3-5 enemies
    m_spawnWaves.clear();
    m_currentWave = 0;
    m_waveTimer = 0;
    m_waveActive = false;
    m_waveClearTriggered = false;
    m_waveClearTimer = 0;

    int waveSize = 3 + (m_currentDifficulty > 3 ? 2 : m_currentDifficulty > 1 ? 1 : 0);
    std::vector<Level::SpawnPoint> wave;
    for (auto& sp : spawns) {
        wave.push_back(sp);
        if (static_cast<int>(wave.size()) >= waveSize) {
            m_spawnWaves.push_back(wave);
            wave.clear();
        }
    }
    if (!wave.empty()) m_spawnWaves.push_back(wave);

    // Spawn first wave immediately
    // Mini-boss: one per level at difficulty 2+, 15% chance (not on boss levels)
    bool miniBossSpawned = false;

    if (!m_spawnWaves.empty()) {
        for (auto& sp : m_spawnWaves[0]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            // Initial wave: no spawn flicker (enemies already present when level starts)
            if (e.hasComponent<AIComponent>()) e.getComponent<AIComponent>().spawnTimer = 0;
            // Theme-specific variant: stat mods, element affinity, color tint
            applyThemeVariant(e, sp.dimension);
            // Elemental variant chance: 25% at difficulty 3+, only if theme didn't set element
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && e.getComponent<AIComponent>().element == EnemyElement::None
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Elite modifier: 15% chance per enemy at difficulty 3+
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < 15) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 9);
                Enemy::makeElite(e, mod);
            }
            // Mini-boss: promote first eligible enemy
            if (!miniBossSpawned && !m_isBossLevel && m_currentDifficulty >= 2
                && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && std::rand() % 100 < 15) {
                Enemy::makeMiniBoss(e);
                miniBossSpawned = true;
            }
        }
        m_currentWave = 1;
        m_waveActive = true;
        m_waveTimer = m_waveDelay;
    }

    // Start first combat challenge for this level
    startCombatChallenge();
}

void PlayState::applyThemeVariant(Entity& e, int dimension) {
    // Apply theme-specific stat modifications and element affinity to enemies
    if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>() ||
        !e.hasComponent<CombatComponent>() || !e.hasComponent<SpriteComponent>()) return;

    const auto& theme = (dimension == 1) ? m_themeA : m_themeB;
    auto config = ThemeEnemyConfig::getConfig(theme.id);

    auto& ai = e.getComponent<AIComponent>();
    if (ai.enemyType == EnemyType::Boss) return; // Don't modify bosses

    // Apply theme stat modifiers
    auto& hp = e.getComponent<HealthComponent>();
    hp.maxHP *= config.hpMod;
    hp.currentHP = hp.maxHP;

    ai.patrolSpeed *= config.speedMod;
    ai.chaseSpeed *= config.speedMod;

    auto& combat = e.getComponent<CombatComponent>();
    combat.meleeAttack.damage *= config.damageMod;
    combat.rangedAttack.damage *= config.damageMod;

    // Theme element affinity: 40% chance theme element, 10% random other element (at diff 2+)
    if (config.preferredElement > 0 && ai.element == EnemyElement::None && m_currentDifficulty >= 2) {
        if (std::rand() % 100 < 40) {
            Enemy::applyElement(e, static_cast<EnemyElement>(config.preferredElement));
        }
    } else if (config.preferredElement == 0 && ai.element == EnemyElement::None && m_currentDifficulty >= 3) {
        // Themes without element affinity: small chance for random element
        if (std::rand() % 100 < 15) {
            Enemy::applyElement(e, static_cast<EnemyElement>(1 + std::rand() % 3));
        }
    }

    // Tint enemy sprite toward theme accent color for visual cohesion
    auto& sprite = e.getComponent<SpriteComponent>();
    const auto& accent = theme.colors.accent;
    // Blend 20% toward theme accent
    sprite.color.r = static_cast<Uint8>(sprite.color.r * 0.8f + accent.r * 0.2f);
    sprite.color.g = static_cast<Uint8>(sprite.color.g * 0.8f + accent.g * 0.2f);
    sprite.color.b = static_cast<Uint8>(sprite.color.b * 0.8f + accent.b * 0.2f);

    // New Game+ scaling: enemies get stronger per NG+ tier
    if (g_newGamePlusLevel > 0) {
        float ngMult = 1.0f + g_newGamePlusLevel * 0.2f; // +20% per NG+ level
        hp.maxHP *= ngMult;
        hp.currentHP = hp.maxHP;
        combat.meleeAttack.damage *= (1.0f + g_newGamePlusLevel * 0.15f);
        combat.rangedAttack.damage *= (1.0f + g_newGamePlusLevel * 0.15f);
        ai.chaseSpeed *= (1.0f + g_newGamePlusLevel * 0.05f);
        // NG+ visual: slightly purple tint
        sprite.color.r = static_cast<Uint8>(std::min(255, sprite.color.r + g_newGamePlusLevel * 15));
        sprite.color.b = static_cast<Uint8>(std::min(255, sprite.color.b + g_newGamePlusLevel * 20));
    }

    sprite.baseColor = sprite.color; // update base for wind-up restore
}

void PlayState::applyUpgrades() {
    if (!m_player) return;
    auto& upgrades = game->getUpgradeSystem();
    auto achBonus = game->getAchievements().getUnlockedBonuses();

    m_player->moveSpeed = 250.0f * upgrades.getMoveSpeedMultiplier() * achBonus.moveSpeedMult;
    m_player->jumpForce = -420.0f * upgrades.getJumpMultiplier();
    m_player->dashCooldown = 0.5f * upgrades.getDashCooldownMultiplier() * achBonus.dashCooldownMult;
    m_player->maxJumps = 2 + upgrades.getExtraJumps();
    // FIX: Apply WallSlide upgrade (was purchased but had no effect)
    m_player->wallSlideSpeed = 60.0f * upgrades.getWallSlideSpeedMultiplier();

    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    // BALANCE: Base HP 100 -> 120 (matches Player.cpp change)
    hp.maxHP = 120.0f + upgrades.getMaxHPBonus() + achBonus.maxHPBonus;
    hp.currentHP = hp.maxHP;
    hp.armor = upgrades.getArmorBonus() + achBonus.armorBonus;

    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    // BALANCE: Base melee 20 -> 25, ranged 12 -> 15 (matches Player.cpp change)
    combat.meleeAttack.damage = 25.0f * upgrades.getMeleeDamageMultiplier() * achBonus.meleeDamageMult * achBonus.allDamageMult;
    combat.rangedAttack.damage = 15.0f * upgrades.getRangedDamageMultiplier() * achBonus.rangedDamageMult * achBonus.allDamageMult;

    m_dimManager.switchCooldown = 0.5f * upgrades.getSwitchCooldownMultiplier() * achBonus.switchCooldownMult;
    m_entropy.passiveDecay = upgrades.getEntropyDecay();
    // FIX: EntropyResistance upgrade was purchased but never applied (same pattern as WallSlide)
    // Cap at 0.8 so entropy never becomes completely trivial
    m_entropy.upgradeResistance = 1.0f - std::min(upgrades.getEntropyResistance(), 0.8f);
    m_combatSystem.setCritChance(upgrades.getCritChance() + achBonus.critChanceBonus);
    m_combatSystem.setComboBonus(upgrades.getComboBonus() + achBonus.comboDamageBonus);

    // Achievement DOT reduction applied to player
    m_player->dotDurationMult = achBonus.dotDurationMult;

    // Store shard drop multiplier from achievements for use in shard calculations
    m_achievementShardMult = achBonus.shardDropMult;

    // Ability upgrades
    if (m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float cdMult = upgrades.getAbilityCooldownMultiplier();
        float pwrMult = upgrades.getAbilityPowerMultiplier();
        abil.abilities[0].cooldown = 6.0f * cdMult;
        abil.abilities[1].cooldown = 10.0f * cdMult;
        abil.abilities[2].cooldown = 8.0f * cdMult;
        abil.slamDamage = 60.0f * pwrMult * achBonus.allDamageMult;
        abil.shieldBurstDamage = 25.0f * pwrMult * achBonus.allDamageMult;
        abil.shieldMaxHits = 3 + upgrades.getShieldCapacityBonus();
    }
}

void PlayState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.scancode == SDL_SCANCODE_F3) {
            m_showDebugOverlay = !m_showDebugOverlay;
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_F4) {
            m_pendingSnapshot = true;
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_F6) {
            m_smokeTest = !m_smokeTest;
            m_showDebugOverlay = m_smokeTest; // Auto-enable F3 overlay
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_M) {
            m_hud.toggleMinimap();
            return;
        }
        if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            if (m_showRelicChoice) {
                // Can't escape relic choice
                return;
            }
            if (m_activePuzzle && m_activePuzzle->isActive()) {
                m_activePuzzle.reset();
            } else {
                game->pushState(StateID::Pause);
            }
            return;
        }

        // Relic choice input
        if (m_showRelicChoice && !m_relicChoices.empty()) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:
                    m_relicChoiceSelected = (m_relicChoiceSelected - 1 + static_cast<int>(m_relicChoices.size()))
                                            % static_cast<int>(m_relicChoices.size());
                    break;
                case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT:
                    m_relicChoiceSelected = (m_relicChoiceSelected + 1)
                                            % static_cast<int>(m_relicChoices.size());
                    break;
                case SDL_SCANCODE_1: selectRelic(0); break;
                case SDL_SCANCODE_2: if (m_relicChoices.size() > 1) selectRelic(1); break;
                case SDL_SCANCODE_3: if (m_relicChoices.size() > 2) selectRelic(2); break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                    selectRelic(m_relicChoiceSelected);
                    break;
                default: break;
            }
            return;
        }

        // NPC dialog input
        // FIX: Added bounds check for m_nearNPCIndex against npcs.size()
        if (m_showNPCDialog && m_nearNPCIndex >= 0) {
            auto& npcs = m_level->getNPCs();
            if (m_nearNPCIndex >= static_cast<int>(npcs.size())) { m_showNPCDialog = false; return; }
            int stage = getNPCStoryStage(npcs[m_nearNPCIndex].type);
            auto options = NPCSystem::getDialogOptions(npcs[m_nearNPCIndex].type, stage);
            int optCount = static_cast<int>(options.size());
            if (optCount == 0) { m_showNPCDialog = false; return; }
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                    m_npcDialogChoice = (m_npcDialogChoice - 1 + optCount) % optCount;
                    AudioManager::instance().play(SFX::MenuSelect);
                    break;
                case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                    m_npcDialogChoice = (m_npcDialogChoice + 1) % optCount;
                    AudioManager::instance().play(SFX::MenuSelect);
                    break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                    handleNPCDialogChoice(m_nearNPCIndex, m_npcDialogChoice);
                    break;
                case SDL_SCANCODE_ESCAPE:
                    m_showNPCDialog = false;
                    break;
                default: break;
            }
            return;
        }

        // Puzzle input
        if (m_activePuzzle && m_activePuzzle->isActive()) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_W: case SDL_SCANCODE_UP:    handlePuzzleInput(0); break;
                case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT: handlePuzzleInput(1); break;
                case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:  handlePuzzleInput(2); break;
                case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:  handlePuzzleInput(3); break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE: handlePuzzleInput(4); break;
                default: break;
            }
            return;
        }
    }
}

void PlayState::update(float dt) {
    if (!m_player || !m_level) return;

    // Generate level FIRST after returning from shop (before any gameplay logic)
    // This prevents stale state (old collapsing/exit) from triggering false completions
    if (m_pendingLevelGen) {
        m_pendingLevelGen = false;

        // Save player state before level regeneration (relics + HP carry over)
        m_savedRelics.clear();
        m_savedHP = 0;
        m_savedMaxHP = 0;
        m_savedVoidHungerBonus = 0;
        if (m_player) {
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                auto& rc = m_player->getEntity()->getComponent<RelicComponent>();
                m_savedRelics = rc.relics;
                m_savedVoidHungerBonus = rc.voidHungerBonus;
            }
            if (m_player->getEntity()->hasComponent<HealthComponent>()) {
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                m_savedHP = hp.currentHP;
                m_savedMaxHP = hp.maxHP;
            }
        }

        generateLevel();
        // Update system pointers to new Player and Level (prevent dangling after regeneration)
        m_combatSystem.setPlayer(m_player.get());
        m_aiSystem.setLevel(m_level.get());
        m_aiSystem.setPlayer(m_player.get());
        applyRunBuffs();

        // Restore relics and HP to new player entity
        if (m_player && !m_savedRelics.empty()) {
            auto& rc = m_player->getEntity()->getComponent<RelicComponent>();
            rc.relics = m_savedRelics;
            rc.voidHungerBonus = m_savedVoidHungerBonus;

            // Re-apply relic stat effects (IronHeart HP, SwiftBoots speed, etc.)
            auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
            auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
            RelicSystem::applyStatEffects(rc, *m_player, hp, combat);

            // Re-apply relic-specific modifiers
            m_dimManager.switchCooldown = std::max(0.20f,
                0.5f * RelicSystem::getSwitchCooldownMult(rc));

            // TimeDilator: re-apply ability cooldown reduction
            if (rc.hasRelic(RelicID::TimeDilator) &&
                m_player->getEntity()->hasComponent<AbilityComponent>()) {
                auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
                float cdMult = RelicSystem::getAbilityCooldownMultiplier(rc);
                abil.abilities[0].cooldown *= cdMult;
                abil.abilities[1].cooldown *= cdMult;
                abil.abilities[2].cooldown *= cdMult;
            }

            // Carry over HP (clamped to new max)
            hp.currentHP = std::min(m_savedHP, hp.maxHP);

            // SoulLeech: -5 HP per level transition (applied after HP restore)
            float leechCost = RelicSystem::getSoulLeechLevelHPCost(rc);
            if (leechCost > 0 && hp.currentHP > leechCost) {
                hp.currentHP -= leechCost;
            }
        } else if (m_player && m_savedMaxHP > 0) {
            // No relics but still carry over HP
            auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
            hp.currentHP = std::min(m_savedHP, hp.maxHP);
        }
    }

    // Death sequence: dramatic freeze before ending the run
    if (m_playerDying) {
        m_deathSequenceTimer -= dt;
        m_camera.update(dt);       // camera shake continues
        m_particles.update(dt);    // death particles animate
        m_screenEffects.update(dt);

        // Slow-motion particle drip during death
        if (m_player && m_deathSequenceTimer > 0.3f) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 center = pt.getCenter();
            float progress = 1.0f - (m_deathSequenceTimer / m_deathSequenceDuration);
            Uint8 alpha = static_cast<Uint8>(80 + 120 * progress);
            m_particles.burst(
                {center.x + static_cast<float>((std::rand() % 30) - 15),
                 center.y + static_cast<float>((std::rand() % 20) - 10)},
                1 + static_cast<int>(progress * 3),
                {255, static_cast<Uint8>(60 + 100 * (1.0f - progress)), 40, alpha},
                40.0f + progress * 80.0f, 2.0f + progress * 3.0f);
        }

        if (m_deathSequenceTimer <= 0) {
            m_playerDying = false;
            if (m_playtest) {
                playtestOnDeath();
            } else {
                endRun();
            }
        }
        return;
    }

    // Track balance stats every frame
    updateBalanceTracking(dt);

    // Smoke test: auto-play for integration testing
    if (m_smokeTest) updateSmokeTest(dt);
    // Playtest: human-like auto-play for balance testing
    if (m_playtest) updatePlaytest(dt);

    // Hit-freeze: brief pause on melee hit for impact feel
    float freeze = m_combatSystem.consumeHitFreeze();
    if (freeze > 0 && m_hitFreezeTimer <= 0) {
        m_hitFreezeTimer = freeze;
    }
    if (m_hitFreezeTimer > 0) {
        m_hitFreezeTimer -= dt;
        m_camera.update(dt);       // still update camera shake during freeze
        m_particles.update(dt);    // particles keep going
        return;
    }

    // Relic choice popup takes priority
    if (m_showRelicChoice) {
        return; // Wait for player to pick a relic
    }

    // NPC dialog takes priority
    if (m_showNPCDialog) {
        return;
    }

    // Challenge mode: speedrun timer
    if (g_activeChallenge == ChallengeID::Speedrun && m_challengeTimer > 0) {
        m_challengeTimer -= dt;
        if (m_challengeTimer <= 0 && !m_playerDying) {
            m_playerDying = true;
            m_deathCause = 4; // Speedrun
            m_deathSequenceTimer = m_deathSequenceDuration;
            AudioManager::instance().play(SFX::PlayerDeath);
            m_camera.shake(15.0f, 0.6f);
            game->getInputMutable().rumble(1.0f, 500);
            if (m_player) {
                Vec2 pos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(pos, 30, {255, 200, 50, 255}, 250.0f, 5.0f);
            }
            m_hud.triggerDamageFlash();
            return;
        }
    }

    // Mutator: DimensionFlux - auto switch every 15s
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::DimensionFlux) {
            m_mutatorFluxTimer += dt;
            if (m_mutatorFluxTimer >= 15.0f) {
                m_mutatorFluxTimer = 0;
                m_dimManager.switchDimension(true);
                m_camera.shake(6.0f, 0.2f);
                AudioManager::instance().play(SFX::DimensionSwitch);
                AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
                m_screenEffects.triggerDimensionRipple();
            }
        }
    }

    // Active puzzle takes priority
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->update(dt);
        return;
    }
    if (m_activePuzzle && !m_activePuzzle->isActive()) {
        m_activePuzzle.reset();
    }

    auto& input = game->getInput();

    // Dimension switch
    if (input.isActionPressed(Action::DimensionSwitch)) {
        if (m_smokeTest && m_player) {
            Vec2 dimPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            smokeLog("[SMOKE] DIM_REQUEST floor=%d seed=%d pos=(%.0f,%.0f) current=%d switching=%d cooldown=%.2f",
                     m_currentDifficulty,
                     m_level ? m_level->getGenerationSeed() : -1,
                     dimPos.x,
                     dimPos.y,
                     m_dimManager.getCurrentDimension(),
                     m_dimManager.isSwitching() ? 1 : 0,
                     m_dimManager.getCooldownTimer());
        }
        // Check if dimension is locked by Void Sovereign or Entropy Incarnate
        bool dimLocked = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_boss" && e.hasComponent<AIComponent>()) {
                auto& bossAi = e.getComponent<AIComponent>();
                if (bossAi.bossType == 4 && bossAi.vsDimLockActive > 0) dimLocked = true;
                if (bossAi.bossType == 5 && bossAi.eiDimLockActive > 0) dimLocked = true;
            }
        });
        if (dimLocked) {
            m_camera.shake(3.0f, 0.1f); // Feedback: can't switch
        } else if (m_dimManager.switchDimension()) {
            if (m_smokeTest) {
                smokeLog("[SMOKE] DIM_STARTED floor=%d seed=%d current=%d switching=%d cooldown=%.2f",
                         m_currentDifficulty,
                         m_level ? m_level->getGenerationSeed() : -1,
                         m_dimManager.getCurrentDimension(),
                         m_dimManager.isSwitching() ? 1 : 0,
                         m_dimManager.getCooldownTimer());
            }
            m_entropy.onDimensionSwitch();
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            game->getInputMutable().rumble(0.3f, 100);
            m_screenEffects.triggerDimensionRipple();

            // Relic dimension-switch effects
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                auto& relicComp = m_player->getEntity()->getComponent<RelicComponent>();
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                int prevDim = relicComp.lastSwitchDimension;
                relicComp.lastSwitchDimension = m_dimManager.getCurrentDimension();
                RelicSystem::onDimensionSwitch(relicComp, &playerHP);

                // DimResidue: leave damage zone (with spawn ICD + zone cap)
                if (relicComp.hasRelic(RelicID::DimResidue) && prevDim > 0
                    && relicComp.dimResidueSpawnCD <= 0) {
                    // Count existing zones
                    int zoneCount = 0;
                    m_entities.forEach([&](Entity& e) {
                        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
                    });
                    if (zoneCount < RelicSystem::getMaxResidueZones()) {
                        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                        float residueDur = RelicSynergy::getResidueDuration(relicComp);
                        auto& zone = m_entities.addEntity("dim_residue");
                        zone.dimension = prevDim;
                        auto& zt = zone.addComponent<TransformComponent>();
                        zt.position = {pPos.x - 24, pPos.y - 24};
                        zt.width = 48;
                        zt.height = 48;
                        auto& zh = zone.addComponent<HealthComponent>();
                        zh.maxHP = residueDur;
                        zh.currentHP = residueDur;
                        m_particles.burst(pPos, 12, {200, 80, 150, 255}, 80.0f, 2.0f);
                        relicComp.dimResidueSpawnCD = RelicSystem::getDimResidueSpawnICD();
                    }
                }
            }

            // Voidwalker passive: Rift Affinity - heal on dim-switch + Dimensional Affinity
            if (m_player && m_player->playerClass == PlayerClass::Voidwalker) {
                const auto& voidData = ClassSystem::getData(PlayerClass::Voidwalker);
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.heal(voidData.switchHeal);
                if (m_player->particles) {
                    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                    m_player->particles->burst(pPos, 8, {60, 200, 255, 200}, 60.0f, 2.0f);
                }
                // Dimensional Affinity: activate Rift Charge damage buff
                m_player->activateRiftCharge();
            }

            // Lore: Echoes of Origin - first dimension switch
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::DimensionOrigin)) {
                    lore->discover(LoreID::DimensionOrigin);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }

            // Run buff: PhantomStep - invincible after dimension switch
            float phantomDur = game->getRunBuffSystem().getPhantomStepDuration();
            if (phantomDur > 0 && m_player) {
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.invincibilityTimer = std::max(hp.invincibilityTimer, phantomDur);
                if (m_player->particles) {
                    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                    m_player->particles->burst(pPos, 15, {180, 150, 255, 200}, 120.0f, 2.0f);
                }
            }
        } // end else if (switchDimension succeeded)
    }

    // Forced dimension switch from entropy (bypasses cooldown)
    if (m_entropy.shouldForceSwitch()) {
        m_dimManager.switchDimension(true);
        m_entropy.clearForceSwitch();
        m_camera.shake(8.0f, 0.3f);
        AudioManager::instance().play(SFX::DimensionSwitch);
        AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
        m_screenEffects.triggerDimensionRipple();
    }

    m_dimManager.playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_dimManager.update(dt);

    // Resonance particle aura around player
    int resTier = m_dimManager.getResonanceTier();
    if (resTier > 0) {
        Vec2 pCenter = m_dimManager.playerPos;
        float angle = static_cast<float>(std::rand() % 628) / 100.0f;
        float dist = 12.0f + static_cast<float>(std::rand() % 10);
        Vec2 sparkPos = {pCenter.x + std::cos(angle) * dist, pCenter.y + std::sin(angle) * dist};
        SDL_Color auraColor;
        if (resTier >= 3) auraColor = {255, 220, 80, 200};
        else if (resTier >= 2) auraColor = {180, 100, 255, 180};
        else auraColor = {80, 200, 220, 140};
        m_particles.burst(sparkPos, resTier, auraColor, 30.0f, 2.0f);
    }

    // Update input distortion from entropy
    game->getInputMutable().setInputDistortion(m_entropy.getInputDistortion());

    // Trail system
    m_trails.update(dt);

    // Move trail (subtle)
    m_moveTrailTimer -= dt;
    if (m_moveTrailTimer <= 0 && m_player) {
        auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
        auto& classData = ClassSystem::getData(m_player->playerClass);
        m_trails.emit(pt.getCenter().x, pt.getCenter().y + pt.height * 0.3f,
                      2.0f, classData.color.r, classData.color.g, classData.color.b, 50, 0.2f);
        m_moveTrailTimer = 0.05f;
    }

    // Screen effects
    if (m_player && m_player->getEntity()->hasComponent<HealthComponent>()) {
        float hp = m_player->getEntity()->getComponent<HealthComponent>().getPercent();
        m_screenEffects.setHP(hp);

        // Low HP heartbeat SFX
        if (hp > 0 && hp < 0.25f) {
            m_heartbeatTimer -= dt;
            if (m_heartbeatTimer <= 0) {
                AudioManager::instance().play(SFX::Heartbeat);
                // Beat faster as HP gets lower (1.2s at 25% → 0.6s near 0%)
                float urgency = 1.0f - (hp / 0.25f);
                m_heartbeatTimer = 1.2f - urgency * 0.6f;
            }
        } else {
            m_heartbeatTimer = 0; // Reset so it plays immediately when entering low HP
        }
    }
    m_screenEffects.setEntropy(m_entropy.getEntropy());
    // Check if Void Storm or Entropy Storm is active
    bool stormActive = false;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "enemy_boss" && e.hasComponent<AIComponent>()) {
            auto& bossAi = e.getComponent<AIComponent>();
            if (bossAi.bossType == 4 && bossAi.vsStormActive > 0) stormActive = true;
            if (bossAi.bossType == 5 && bossAi.eiStormActive > 0) stormActive = true;
        }
    });
    m_screenEffects.setVoidStorm(stormActive);
    m_screenEffects.update(dt);

    // Music system
    {
        int nearEnemies = 0;
        bool bossActive = false;
        Vec2 pPos = m_player ? m_player->getEntity()->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().substr(0, 5) == "enemy") {
                if (e.getTag() == "enemy_boss") bossActive = true;
                if (!e.hasComponent<TransformComponent>()) return;
                auto& et = e.getComponent<TransformComponent>();
                float dx = et.getCenter().x - pPos.x;
                float dy = et.getCenter().y - pPos.y;
                if (dx * dx + dy * dy < 400.0f * 400.0f) nearEnemies++;
            }
        });
        float hp = m_player ? m_player->getEntity()->getComponent<HealthComponent>().getPercent() : 1.0f;
        m_musicSystem.update(dt, nearEnemies, bossActive, hp, m_entropy.getEntropy());
        AudioManager::instance().updateMusicLayers(m_musicSystem);
    }

    // Run time tracking
    m_runTime += dt;

    // Player update
    m_player->update(dt, input);

    // Update tile timers (crumbling etc.)
    if (m_level) m_level->updateTiles(dt);

    // Physics - pass current dimension so player (dimension=0) collides with correct tiles
    m_physics.update(m_entities, dt, m_level.get(), m_dimManager.getCurrentDimension());

    // Landing effects: screen shake + dust particles proportional to fall speed
    {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        if (phys.onGround && !phys.wasOnGround && phys.landingImpactSpeed > 250.0f) {
            float t = std::min((phys.landingImpactSpeed - 250.0f) / 550.0f, 1.0f);
            m_camera.shake(2.0f + t * 6.0f, 0.08f + t * 0.12f);
            game->getInputMutable().rumble(0.2f + t * 0.4f, 80 + static_cast<int>(t * 120));

            // Landing squash: sprite goes wide+short on impact
            auto& spr = m_player->getEntity()->getComponent<SpriteComponent>();
            spr.landingSquashTimer = 0.15f;
            spr.landingSquashIntensity = 0.1f + t * 0.15f; // 0.1-0.25 based on fall speed

            // Landing dust cloud at feet — bigger for harder landings
            auto& tr = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 feetPos = {tr.getCenter().x, tr.position.y + tr.height};
            int dustCount = 6 + static_cast<int>(t * 10);
            float dustSpeed = 40.0f + t * 80.0f;
            float dustSize = 2.0f + t * 3.0f;
            SDL_Color dustColor = {180, 160, 130, static_cast<Uint8>(150 + t * 80)};
            // Spread left and right from feet
            m_particles.directionalBurst(feetPos, dustCount / 2, dustColor, 180.0f, 60.0f, dustSpeed, dustSize);
            m_particles.directionalBurst(feetPos, dustCount / 2, dustColor, 0.0f, 60.0f, dustSpeed, dustSize);
        }
        if (phys.onGround) phys.landingImpactSpeed = 0;
    }

    // Wall slide dust: small particles at wall contact while sliding
    if (m_player->isWallSliding) {
        m_wallSlideDustTimer -= dt;
        if (m_wallSlideDustTimer <= 0) {
            m_wallSlideDustTimer = 0.12f; // emit every 120ms
            auto& tr = m_player->getEntity()->getComponent<TransformComponent>();
            auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
            // Particles at the wall-contact side, near player's mid-height
            bool onRight = phys.onWallRight;
            float wx = onRight ? (tr.position.x + tr.width) : tr.position.x;
            float wy = tr.position.y + tr.height * 0.4f;
            Vec2 contactPoint = {wx, wy};
            // Dust drifts away from wall (left if wall is right, right if wall is left)
            float dir = onRight ? 180.0f : 0.0f;
            SDL_Color dustColor = {190, 170, 140, 140};
            m_particles.directionalBurst(contactPoint, 2, dustColor, dir, 50.0f, 25.0f, 2.0f);
        }
    } else {
        m_wallSlideDustTimer = 0;
    }

    // Projectile terrain impact: particles where projectiles hit walls
    for (auto& impact : m_physics.getProjectileImpacts()) {
        // Directional burst away from wall (opposite of projectile velocity)
        float speed = Vec2(impact.velocity.x, impact.velocity.y).length();
        if (speed > 0.01f) {
            float dirRad = std::atan2(-impact.velocity.y, -impact.velocity.x);
            float dirDeg = dirRad * 180.0f / 3.14159f;
            m_particles.directionalBurst(impact.position, 8, impact.color, dirDeg, 90.0f, 100.0f, 2.5f);
        } else {
            m_particles.burst(impact.position, 8, impact.color, 100.0f, 2.5f);
        }
    }

    // Enemy wall impact: bounce particles + bonus damage + SFX
    m_entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") == std::string::npos) return;
        if (!e.hasComponent<PhysicsBody>()) return;
        auto& phys = e.getComponent<PhysicsBody>();
        if (phys.wallImpactSpeed <= 0) return;

        float impact = phys.wallImpactSpeed;
        phys.wallImpactSpeed = 0;

        if (!e.hasComponent<TransformComponent>()) return;
        auto& t = e.getComponent<TransformComponent>();
        Vec2 center = t.getCenter();

        // Wall side: left wall = particles go right, right wall = particles go left
        float particleDir = phys.onWallLeft ? 0.0f : 180.0f;
        Vec2 wallPoint = phys.onWallLeft ?
            Vec2{t.position.x, center.y} :
            Vec2{t.position.x + t.width, center.y};

        // Impact intensity scales with speed (150-500+ range)
        float intensity = std::min((impact - 150.0f) / 350.0f, 1.0f);

        // Dust/spark particles at wall contact point
        int particleCount = 4 + static_cast<int>(intensity * 8);
        SDL_Color dustColor = {200, 180, 150, static_cast<Uint8>(180 + intensity * 75)};
        m_particles.directionalBurst(wallPoint, particleCount, dustColor,
                                      particleDir, 50.0f, 60.0f + intensity * 100.0f, 2.0f + intensity * 2.0f);
        // White sparks for harder impacts
        if (intensity > 0.4f) {
            m_particles.burst(wallPoint, 3 + static_cast<int>(intensity * 5),
                              {255, 255, 240, 220}, 80.0f + intensity * 80.0f, 1.5f);
        }

        // Bonus wall slam damage (5-15 based on impact speed)
        if (e.hasComponent<HealthComponent>()) {
            float wallDmg = 5.0f + intensity * 10.0f;
            auto& hp = e.getComponent<HealthComponent>();
            hp.takeDamage(wallDmg);
            m_combatSystem.addDamageEvent(center, wallDmg, false, false);
        }

        // Camera shake + SFX
        m_camera.shake(2.0f + intensity * 4.0f, 0.08f + intensity * 0.1f);
        AudioManager::instance().play(SFX::GroundSlam);
    });

    // AI
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_aiSystem.update(m_entities, dt, playerPos, m_dimManager.getCurrentDimension());

    // Technomancer: update player-owned turrets (lifetime + auto-fire)
    if (m_player->isTechnomancer()) {
        int turretCount = 0;
        int curDim = m_dimManager.getCurrentDimension();
        m_entities.forEach([&](Entity& turret) {
            if (turret.getTag() != "player_turret" || !turret.isAlive()) return;
            if (!turret.hasComponent<HealthComponent>()) return;

            auto& hp = turret.getComponent<HealthComponent>();
            hp.currentHP -= dt; // decay lifetime
            if (hp.currentHP <= 0) {
                // Turret expired
                auto& tt = turret.getComponent<TransformComponent>();
                m_particles.burst(tt.getCenter(), 8, {230, 180, 50, 150}, 60.0f, 2.0f);
                turret.destroy();
                return;
            }
            turretCount++;

            if (!turret.hasComponent<AIComponent>() || !turret.hasComponent<CombatComponent>()) return;
            auto& ai = turret.getComponent<AIComponent>();
            auto& tt = turret.getComponent<TransformComponent>();
            Vec2 turretPos = tt.getCenter();

            // Find nearest enemy in range
            Entity* nearestEnemy = nullptr;
            float nearestDist = ai.detectRange;
            m_entities.forEach([&](Entity& e) {
                if (e.getTag().find("enemy") == std::string::npos) return;
                if (!e.isAlive() || !e.hasComponent<TransformComponent>()) return;
                if (!e.hasComponent<HealthComponent>()) return;
                if (e.dimension != 0 && e.dimension != curDim) return;
                auto& eHP = e.getComponent<HealthComponent>();
                if (eHP.currentHP <= 0) return;
                auto& et = e.getComponent<TransformComponent>();
                float dx = et.getCenter().x - turretPos.x;
                float dy = et.getCenter().y - turretPos.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearestEnemy = &e;
                }
            });

            // Auto-fire at nearest enemy
            ai.attackTimer -= dt;
            if (nearestEnemy && ai.attackTimer <= 0) {
                auto& combat = turret.getComponent<CombatComponent>();
                auto& et = nearestEnemy->getComponent<TransformComponent>();
                Vec2 dir = (et.getCenter() - turretPos).normalized();
                ai.attackTimer = ai.attackCooldown;

                // Fire player-owned projectile
                m_combatSystem.createProjectile(m_entities, turretPos, dir,
                    combat.rangedAttack.damage, 350.0f, 0, false, true);
                AudioManager::instance().play(SFX::RangedShot);

                // Muzzle flash particles
                m_particles.burst({turretPos.x + dir.x * 12, turretPos.y + dir.y * 12},
                    4, {255, 220, 80, 220}, 40.0f, 1.5f);

                // Face toward target
                if (turret.hasComponent<SpriteComponent>()) {
                    turret.getComponent<SpriteComponent>().flipX = (dir.x < 0);
                }
            }
        });
        m_player->activeTurrets = turretCount;

        // Count active traps (lifetime decay)
        int trapCount = 0;
        m_entities.forEach([&](Entity& trap) {
            if (trap.getTag() != "player_trap" || !trap.isAlive()) return;
            if (!trap.hasComponent<HealthComponent>()) return;

            auto& hp = trap.getComponent<HealthComponent>();
            hp.currentHP -= dt; // decay lifetime
            if (hp.currentHP <= 0) {
                auto& trapT = trap.getComponent<TransformComponent>();
                m_particles.burst(trapT.getCenter(), 6, {255, 200, 50, 120}, 40.0f, 1.5f);
                trap.destroy();
                return;
            }
            trapCount++;
        });
        m_player->activeTraps = trapCount;
    }

    // Void Sovereign Phase 3: auto dimension switch
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() != "enemy_boss") return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& bossAi = e.getComponent<AIComponent>();
        if (bossAi.bossType == 4 && bossAi.vsForceDimSwitch) {
            bossAi.vsForceDimSwitch = false;
            m_dimManager.switchDimension(true);
            m_camera.shake(6.0f, 0.2f);
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            m_screenEffects.triggerDimensionRipple();
        }
    });

    // Entropy Incarnate boss effects
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() != "enemy_boss") return;
        if (!e.hasComponent<AIComponent>()) return;
        auto& bossAi = e.getComponent<AIComponent>();
        if (bossAi.bossType != 5) return;

        // Entropy Pulse: add entropy when pulse just fired
        if (bossAi.eiPulseFired) {
            bossAi.eiPulseFired = false;
            float entropyAdd = (bossAi.bossPhase >= 3) ? 20.0f : 15.0f;
            if (m_player && e.hasComponent<TransformComponent>()) {
                auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
                Vec2 bossPos = e.getComponent<TransformComponent>().getCenter();
                Vec2 pPos = playerT.getCenter();
                float d = std::sqrt((pPos.x - bossPos.x) * (pPos.x - bossPos.x) +
                                     (pPos.y - bossPos.y) * (pPos.y - bossPos.y));
                if (d < 200.0f) {
                    m_entropy.addEntropy(entropyAdd);
                }
            }
        }

        // Entropy Storm: continuous entropy drain while player in range
        if (bossAi.eiStormActive > 0 && m_player) {
            auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
            Vec2 bossPos = e.getComponent<TransformComponent>().getCenter();
            Vec2 pPos = playerT.getCenter();
            float d = std::sqrt((pPos.x - bossPos.x) * (pPos.x - bossPos.x) +
                                 (pPos.y - bossPos.y) * (pPos.y - bossPos.y));
            if (d < 150.0f) {
                m_entropy.addEntropy(3.0f * dt); // 3 entropy/s while in range
            }
        }

        // Dimension Shatter: force random dimension switch
        if (bossAi.eiForceDimSwitch) {
            bossAi.eiForceDimSwitch = false;
            m_dimManager.switchDimension(true);
            m_camera.shake(8.0f, 0.3f);
            AudioManager::instance().play(SFX::DimensionSwitch);
            AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
            m_screenEffects.triggerDimensionRipple();
        }

        // Entropy Overload: if player entropy > 70, boss heals 5% max HP/s
        if (bossAi.bossPhase >= 3 && m_entropy.getEntropy() > 70.0f) {
            if (bossAi.eiOverloadHealAccum >= 0.1f) {
                float healPerTick = e.getComponent<HealthComponent>().maxHP * 0.005f; // 0.5% per 0.1s = 5%/s
                auto& bossHP = e.getComponent<HealthComponent>();
                bossHP.heal(healPerTick);
                bossAi.eiOverloadHealAccum -= 0.1f;
                // Visual feedback: green heal particles
                if (bossAi.eiOverloadHealAccum < 0.2f) {
                    Vec2 bPos = e.getComponent<TransformComponent>().getCenter();
                    m_particles.burst(bPos, 3, {50, 200, 80, 200}, 40.0f, 1.5f);
                }
            }
        }

        // Final Stand: massive +30 entropy applied once on trigger
        if (bossAi.eiFinalStandTriggered && !bossAi.eiFinalStandEntropyApplied) {
            bossAi.eiFinalStandEntropyApplied = true;
            m_entropy.addEntropy(30.0f);
        }
    });

    // Entropy minion death: add +10 entropy to player on death
    // (Handled by zombie sweep in CombatSystem — detect dying entropy minions here)
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() != "enemy_entropy_minion") return;
        if (!e.hasComponent<HealthComponent>()) return;
        auto& mhp = e.getComponent<HealthComponent>();
        if (mhp.currentHP <= 0 && e.isAlive()) {
            m_entropy.addEntropy(10.0f);
            if (e.hasComponent<TransformComponent>()) {
                Vec2 mPos = e.getComponent<TransformComponent>().getCenter();
                m_particles.burst(mPos, 20, {140, 0, 200, 255}, 150.0f, 4.0f);
            }
        }
    });

    // Collision
    m_collision.update(m_entities, m_dimManager.getCurrentDimension());

    // Enemy hazard damage: enemies on spikes/fire/laser take damage
    {
        int curDim = m_dimManager.getCurrentDimension();
        int ts = m_level->getTileSize();
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") == std::string::npos) return;
            if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>()) return;
            if (!e.hasComponent<TransformComponent>()) return;
            auto& ai = e.getComponent<AIComponent>();
            if (ai.enemyType == EnemyType::Boss) return; // bosses immune to hazards
            if (ai.spawnTimer > 0) return; // spawning enemies immune
            if (ai.hazardDmgCooldown > 0) { ai.hazardDmgCooldown -= dt; return; }

            auto& t = e.getComponent<TransformComponent>();
            auto& hp = e.getComponent<HealthComponent>();
            Vec2 center = t.getCenter();
            int footX = static_cast<int>(center.x) / ts;
            int footY = static_cast<int>(t.position.y + t.height - 2) / ts;
            if (!m_level->inBounds(footX, footY)) return;

            const auto& tile = m_level->getTile(footX, footY, (e.dimension == 0) ? curDim : e.dimension);

            if (tile.type == TileType::Spike) {
                hp.takeDamage(15.0f);
                m_combatSystem.addDamageEvent(center, 15.0f, false, false, true);
                ai.hazardDmgCooldown = 0.5f;
                m_particles.burst(center, 8, {255, 80, 40, 255}, 100.0f, 2.0f);
                if (e.hasComponent<PhysicsBody>()) {
                    e.getComponent<PhysicsBody>().velocity.y = -200.0f;
                }
            } else if (tile.type == TileType::Fire) {
                hp.takeDamage(12.0f);
                m_combatSystem.addDamageEvent(center, 12.0f, false, false, true);
                ai.hazardDmgCooldown = 0.5f;
                ai.burnTimer = std::max(ai.burnTimer, 1.5f);
                m_particles.burst(center, 6, {255, 150, 30, 255}, 80.0f, 2.0f);
            } else if (m_level->isInLaserBeam(center.x, center.y, (e.dimension == 0) ? curDim : e.dimension)) {
                hp.takeDamage(25.0f);
                m_combatSystem.addDamageEvent(center, 25.0f, false, false, true);
                ai.hazardDmgCooldown = 0.3f;
                m_particles.burst(center, 10, {255, 50, 50, 255}, 120.0f, 2.5f);
            }
        });
    }

    // Combat
    m_combatSystem.update(m_entities, dt, m_dimManager.getCurrentDimension());

    // Consume damage events for floating numbers + achievement tracking
    auto dmgEvents = m_combatSystem.consumeDamageEvents();
    for (auto& evt : dmgEvents) {
        FloatingDamageNumber num;
        num.position = evt.position;
        num.value = evt.damage;
        num.isPlayerDamage = evt.isPlayerDamage;
        num.isCritical = evt.isCritical;
        num.lifetime = evt.isCritical ? 1.2f : 0.8f;
        num.maxLifetime = num.lifetime;
        m_damageNumbers.push_back(num);

        // Track enemy hits from player for achievements
        if (!evt.isPlayerDamage) {
            game->getAchievements().unlock("first_blood");
        }
        // Track player damage for flawless floor achievement + NoDamageWave challenge
        if (evt.isPlayerDamage) {
            m_tookDamageThisLevel = true;
            m_tookDamageThisWave = true;
            // Screen shake + damage flash + hurt SFX on enemy combat hits
            // Skip if source already handled feedback (hazards, DoT)
            if (!evt.feedbackHandled) {
                m_camera.shake(6.0f, 0.15f);
                m_hud.triggerDamageFlash();
                AudioManager::instance().play(SFX::PlayerHurt);
                game->getInputMutable().rumble(0.6f, 150);
            }
        }
    }

    // Wave/area clear celebration: all waves spawned + all enemies dead
    if (m_combatSystem.killCount > 0 && !m_isBossLevel && !m_waveClearTriggered) {
        int aliveAfterCombat = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") != std::string::npos) aliveAfterCombat++;
        });
        if (aliveAfterCombat == 0 && m_currentWave >= static_cast<int>(m_spawnWaves.size())) {
            m_waveClearTriggered = true;
            m_waveClearTimer = 2.0f;
            m_combatSystem.addHitFreeze(0.15f);
            m_camera.shake(10.0f, 0.3f);
            game->getInputMutable().rumble(0.5f, 200);
            AudioManager::instance().play(SFX::ShrineBlessing);
            if (m_player) {
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(pPos, 40, {255, 215, 0, 255}, 300.0f, 5.0f);
                m_particles.burst(pPos, 20, {255, 255, 200, 255}, 200.0f, 4.0f);
            }
        }
    }
    if (m_waveClearTimer > 0) m_waveClearTimer -= dt;

    updateDamageNumbers(dt);

    // Combo milestone check (3x, 5x, 7x, 10x)
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        int combo = combat.comboCount;
        int milestone = 0;
        if (combo >= 10) milestone = 10;
        else if (combo >= 7) milestone = 7;
        else if (combo >= 5) milestone = 5;
        else if (combo >= 3) milestone = 3;

        if (milestone > 0 && milestone != m_lastComboMilestone) {
            m_lastComboMilestone = milestone;
            m_comboMilestoneFlash = 0.4f;
            AudioManager::instance().play(SFX::ComboMilestone);
            if (m_player->particles) {
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                // Golden burst for milestone
                m_player->particles->burst(pPos, 20 + milestone * 2, {255, 215, 0, 255}, 180.0f, 4.0f);
                m_player->particles->burst(pPos, 10, {255, 255, 180, 255}, 120.0f, 3.0f);
            }
        }
        if (combo == 0) m_lastComboMilestone = 0;
    }
    if (m_comboMilestoneFlash > 0) m_comboMilestoneFlash -= dt;

    // Shard magnet: attract pickups toward player
    float magnetRange = game->getUpgradeSystem().getShardMagnetRange();
    // ShardMagnet relic: +50% pickup radius
    if (m_player->getEntity()->hasComponent<RelicComponent>()) {
        magnetRange *= RelicSystem::getShardMagnetMultiplier(
            m_player->getEntity()->getComponent<RelicComponent>());
    }
    if (magnetRange > 14.0f) { // Only if upgrade purchased or relic active
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("pickup") == std::string::npos) return;
            if (!e.hasComponent<TransformComponent>() || !e.hasComponent<PhysicsBody>()) return;

            auto& t = e.getComponent<TransformComponent>();
            Vec2 pickupPos = t.getCenter();
            float dx = pPos.x - pickupPos.x;
            float dy = pPos.y - pickupPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < magnetRange && dist > 5.0f) {
                float strength = 300.0f * (1.0f - dist / magnetRange);
                auto& phys = e.getComponent<PhysicsBody>();
                phys.velocity.x += (dx / dist) * strength * dt;
                phys.velocity.y += (dy / dist) * strength * dt;
            }
        });
    }

    // Entities
    m_entities.update(dt);
    m_entities.refresh();

    // Check boss defeated
    if (m_isBossLevel && !m_bossDefeated) {
        bool bossAlive = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_boss") bossAlive = true;
        });
        if (!bossAlive) {
            m_bossDefeated = true;
            // Boss kill rewards
            float bossShardMult = game->getRunBuffSystem().getShardMultiplier();
            bossShardMult *= m_achievementShardMult;
            if (m_player->getEntity()->hasComponent<RelicComponent>())
                bossShardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
            int bossShards = static_cast<int>((50 + m_currentDifficulty * 20) * bossShardMult);
            shardsCollected += bossShards;
            game->getUpgradeSystem().addRiftShards(bossShards);
            AudioManager::instance().play(SFX::LevelComplete);
            m_camera.shake(15.0f, 0.5f);
            Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(playerPos, 60, {255, 200, 100, 255}, 350.0f, 6.0f);

            // Boss achievements
            game->getAchievements().unlock("boss_slayer");
            int bossIdx = m_currentDifficulty / 3;
            if (bossIdx % 2 == 1) game->getAchievements().unlock("wyrm_hunter");

            // Lore triggers on boss kill
            {
                auto* lore = game->getLoreSystem();
                if (lore) {
                    int bossTypeForLore = bossIdx % 4;
                    if (m_currentDifficulty >= 11 && bossIdx >= 5) {
                        // Entropy Incarnate killed
                        Bestiary::onBossKill(5);
                        AudioManager::instance().play(SFX::LoreDiscover);
                    } else if (m_currentDifficulty >= 10 && bossIdx >= 4) {
                        // Void Sovereign killed
                        lore->discover(LoreID::SovereignTruth);
                        Bestiary::onBossKill(4);
                        AudioManager::instance().play(SFX::LoreDiscover);
                    } else {
                        Bestiary::onBossKill(bossTypeForLore);
                        if (bossTypeForLore == 0) lore->discover(LoreID::BossMemory1);
                        else if (bossTypeForLore == 1) lore->discover(LoreID::BossMemory2);
                        else if (bossTypeForLore == 2) lore->discover(LoreID::BossMemory3);
                        else if (bossTypeForLore == 3) lore->discover(LoreID::BossMemory4);
                    }
                }
            }

            bool finalBossDefeated = (m_currentDifficulty >= 10 && bossIdx >= 4);

            // Track Void Sovereign defeat for ending sequence
            if (finalBossDefeated) {
                m_voidSovereignDefeated = true;
                m_deathCause = 5; // Victory
                m_levelComplete = true;
                m_levelCompleteTimer = 0;
            } else {
                // Boss kill unlocks exit: trigger collapse so player can escape
                if (!m_collapsing) {
                    m_collapsing = true;
                    m_collapseTimer = 0;
                    m_level->setExitActive(true);
                }

                // Boss kill -> Relic choice (3 from pool)
                showRelicChoice();
            }
        }
    }

    // Hazard damage (spikes, fire, laser, conveyor)
    m_spikeDmgCooldown -= dt;
    {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        int tileSize = m_level->getTileSize();
        int footX = static_cast<int>(playerT.getCenter().x) / tileSize;
        int footY = static_cast<int>(playerT.position.y + playerT.height - 1) / tileSize;
        int dim = m_dimManager.getCurrentDimension();

        // Helper: apply defensive relic multiplier to hazard damage
        auto hazardDmg = [&](float base) -> float {
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                return base * RelicSystem::getDamageTakenMult(
                    m_player->getEntity()->getComponent<RelicComponent>(), dim);
            }
            return base;
        };

        // Spike & fire damage (cooldown-based, skip during i-frames)
        if (m_spikeDmgCooldown <= 0 && m_level->inBounds(footX, footY)) {
            const auto& tile = m_level->getTile(footX, footY, dim);
            auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
            if (tile.type == TileType::Spike && !playerHP.isInvincible()) {
                // BALANCE: Spike DMG 15 -> 10, entropy 5 -> 3 (playtest: main death cause in L1)
                float spikeDmg = hazardDmg(10.0f);
                playerHP.takeDamage(spikeDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), spikeDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(3.0f);
                m_spikeDmgCooldown = 0.5f;
                m_camera.shake(6.0f, 0.2f);
                AudioManager::instance().play(SFX::SpikeDamage);
                if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
                    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                    phys.velocity.y = -250.0f;
                }
                m_particles.burst(playerT.getCenter(), 15, {255, 80, 40, 255}, 150.0f, 3.0f);
                m_hud.triggerDamageFlash();
            } else if (tile.type == TileType::Fire && !playerHP.isInvincible()) {
                float fireDmg = hazardDmg(10.0f);
                playerHP.takeDamage(fireDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), fireDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(3.0f);
                m_spikeDmgCooldown = 0.4f;
                m_camera.shake(4.0f, 0.15f);
                AudioManager::instance().play(SFX::FireBurn);
                m_player->applyBurn(1.5f); // Fire tiles also apply burn
                if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
                    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                    phys.velocity.y = -200.0f;
                }
                m_particles.burst(playerT.getCenter(), 12, {255, 150, 30, 255}, 120.0f, 2.5f);
                m_hud.triggerDamageFlash();
            }
        }

        // Laser beam damage (separate cooldown via same timer, skip during i-frames)
        if (m_spikeDmgCooldown <= 0) {
            auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
            if (!playerHP.isInvincible() &&
                m_level->isInLaserBeam(playerT.getCenter().x, playerT.getCenter().y, dim)) {
                float laserDmg = hazardDmg(20.0f);
                playerHP.takeDamage(laserDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), laserDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_entropy.addEntropy(8.0f);
                m_spikeDmgCooldown = 0.3f;
                m_camera.shake(8.0f, 0.25f);
                AudioManager::instance().play(SFX::LaserHit);
                m_particles.burst(playerT.getCenter(), 20, {255, 50, 50, 255}, 200.0f, 3.0f);
                m_hud.triggerDamageFlash();
            }
        }

        // Conveyor belt push (always active, no damage)
        // Check tile at feet AND tile below feet (conveyor is the surface tile)
        if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
            int convDir = 0;
            bool onConveyor = (m_level->inBounds(footX, footY) && m_level->isOnConveyor(footX, footY, dim, convDir))
                || (m_level->inBounds(footX, footY + 1) && m_level->isOnConveyor(footX, footY + 1, dim, convDir));
            if (onConveyor) {
                auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                phys.velocity.x += convDir * 350.0f * dt; // Stronger push (was 120)
                // Subtle wind particles in conveyor direction
                m_conveyorParticleTimer -= dt;
                if (m_conveyorParticleTimer <= 0) {
                    Vec2 feet = {playerT.getCenter().x, playerT.position.y + playerT.height};
                    m_particles.burst(feet, 2, {180, 200, 220, 120}, 50.0f, 1.0f);
                    m_conveyorParticleTimer = 0.25f;
                }
            } else {
                m_conveyorParticleTimer = 0;
            }
        }

        // Teleporter: teleport player to paired tile
        // Cooldown ticks down regardless of position to prevent ping-pong
        if (m_teleportCooldown > 0) m_teleportCooldown -= dt;
        {
            int centerTX = static_cast<int>(playerT.getCenter().x) / m_level->getTileSize();
            int centerTY = static_cast<int>(playerT.getCenter().y) / m_level->getTileSize();
            if (m_level->inBounds(centerTX, centerTY)) {
                const auto& tile = m_level->getTile(centerTX, centerTY, dim);
                if (tile.type == TileType::Teleporter && m_teleportCooldown <= 0) {
                    Vec2 dest = m_level->getTeleporterDestination(centerTX, centerTY, dim);
                    if (dest.x != 0 || dest.y != 0) {
                        playerT.position = dest;
                        m_teleportCooldown = 1.0f; // Prevent instant re-teleport
                        m_particles.burst(playerT.getCenter(), 15, {50, 220, 100, 255}, 200.0f, 3.0f);
                        AudioManager::instance().play(SFX::BossTeleport);
                    }
                }
            }
        }
    }

    // Dimension switch plates: activate when player steps on them
    if (m_player) {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        int dim = m_dimManager.getCurrentDimension();
        int footX = static_cast<int>(playerT.getCenter().x) / m_level->getTileSize();
        int footY = static_cast<int>(playerT.position.y + playerT.height + 1) / m_level->getTileSize();
        if (m_level->isDimSwitchAt(footX, footY, dim)) {
            int pairId = m_level->getTile(footX, footY, dim).variant;
            if (m_level->activateDimSwitch(pairId, dim)) {
                m_camera.shake(6.0f, 0.3f);
                m_particles.burst(playerT.getCenter(), 20, {100, 255, 100, 255}, 150.0f, 3.0f);
                AudioManager::instance().play(SFX::RiftRepair);
            }
        }
    }

    // Status effect DoT damage
    if (m_player) {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
        int dotDim = m_dimManager.getCurrentDimension();

        // Helper: apply defensive relic multiplier to DoT damage
        auto dotDmg = [&](float base) -> float {
            if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                return base * RelicSystem::getDamageTakenMult(
                    m_player->getEntity()->getComponent<RelicComponent>(), dotDim);
            }
            return base;
        };

        if (m_player->isBurning()) {
            m_player->burnDmgTimer -= dt;
            if (m_player->burnDmgTimer <= 0) {
                float burnDmg = dotDmg(5.0f);
                playerHP.takeDamage(burnDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), burnDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_player->burnDmgTimer = 0.3f;
                m_particles.burst(playerT.getCenter(), 4, {255, 120, 30, 200}, 60.0f, 1.5f);
            }
        }
        if (m_player->isPoisoned()) {
            m_player->poisonDmgTimer -= dt;
            if (m_player->poisonDmgTimer <= 0) {
                float poisonDmg = dotDmg(3.0f);
                playerHP.takeDamage(poisonDmg);
                m_combatSystem.addDamageEvent(playerT.getCenter(), poisonDmg, true, false, true);
                m_tookDamageThisLevel = true;
                m_tookDamageThisWave = true;
                m_player->poisonDmgTimer = 0.5f;
                m_particles.burst(playerT.getCenter(), 3, {80, 200, 40, 200}, 40.0f, 1.5f);
            }
        }
    }

    // HUD flash update
    m_hud.updateFlash(dt);

    // Notification timers (achievement + lore)
    game->getAchievements().update(dt);
    if (auto* lore = game->getLoreSystem()) {
        lore->updateNotification(dt);
    }

    // Tutorial timer + action tracking
    m_tutorialTimer += dt;
    if (m_player && m_player->getEntity()->hasComponent<PhysicsBody>()) {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        if (std::abs(phys.velocity.x) > 10.0f) m_hasMovedThisRun = true;
        if (!phys.onGround) m_hasJumpedThisRun = true;
    }
    if (m_player && m_player->isDashing) m_hasDashedThisRun = true;
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        if (combat.isAttacking) m_hasAttackedThisRun = true;
    }
    if (game->getInput().isActionPressed(Action::RangedAttack)) m_hasRangedThisRun = true;
    if (m_player && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        for (int i = 0; i < 3; i++) {
            if (abil.abilities[i].active) m_hasUsedAbilityThisRun = true;
        }
    }

    // Entropy
    m_entropy.update(dt);
    if (m_dimManager.getCurrentDimension() == 2) {
        const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
        m_entropy.addEntropy(shiftBalance.dimBEntropyPerSecond * dt);
    }
    if (m_entropy.isCritical() && !m_playerDying) {
        // Suit crash - run over with death sequence
        m_playerDying = true;
        m_deathCause = 2; // Entropy
        m_deathSequenceTimer = m_deathSequenceDuration;
        AudioManager::instance().play(SFX::SuitEntropyCritical);
        m_camera.shake(15.0f, 0.6f);

        // Entropy overload particles (purple/magenta)
        if (m_player) {
            Vec2 deathPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(deathPos, 35, {200, 50, 180, 255}, 280.0f, 5.0f);
            m_particles.burst(deathPos, 20, {255, 80, 255, 255}, 200.0f, 4.0f);
            m_particles.burst(deathPos, 15, {120, 40, 140, 200}, 350.0f, 6.0f);
        }
        m_hud.triggerDamageFlash();
        return;
    }

    // Relic timed effects update
    // Reset per-frame entropy modifiers before applying relic effects
    m_entropy.passiveDecayModifier = 1.0f;
    if (m_player->getEntity()->hasComponent<RelicComponent>()) {
        auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
        RelicSystem::updateTimedEffects(relics, dt);

        // ChaosRift buff effects (type 0=speed, 3=regen)
        if (relics.chaosRiftBuffTimer > 0) {
            if (relics.chaosRiftBuffType == 0) {
                m_player->speedBoostTimer = std::max(m_player->speedBoostTimer, dt + 0.01f);
                m_player->speedBoostMultiplier = 1.3f;
            } else if (relics.chaosRiftBuffType == 3) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                playerHP.heal(5.0f * dt); // 5 HP/sec regen
            }
        }

        // Phase Cloak: invincibility during cloak
        if (relics.phaseCloakTimer > 0) {
            auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
            playerHP.invincibilityTimer = std::max(playerHP.invincibilityTimer, dt + 0.01f);
        }

        // Entropy Anchor: reduce entropy gain in boss fights
        if (m_isBossLevel && !m_bossDefeated) {
            float entropyMult = RelicSystem::getEntropyMultiplier(relics, true);
            if (entropyMult < 1.0f) {
                // Apply reduction by adjusting passive gain
                m_entropy.passiveGainModifier = entropyMult;
            }
        } else {
            m_entropy.passiveGainModifier = 1.0f;
        }
        // EntropySponge: no passive entropy (kills add entropy instead)
        if (RelicSystem::hasNoPassiveEntropy(relics)) {
            m_entropy.passiveGainModifier = 0.0f;
        }

        // EntropySiphon: +50% entropy passive gain (multiplicative with other modifiers)
        float siphonMult = RelicSystem::getEntropySiphonGainMult(relics);
        if (siphonMult > 1.0f) {
            m_entropy.passiveGainModifier *= siphonMult;
        }

        // TimeDistortion: entropy passive decay is 50% slower
        // passiveDecayModifier is reset to 1.0 each frame before relic effects are applied
        m_entropy.passiveDecayModifier = RelicSystem::getTimeDistortionEntropyDecayMult(relics);

        // ChaosCore: force dimension switch every 20s
        if (relics.chaosCoreTimer <= 0) {
            relics.chaosCoreTimer = 20.0f;
            // Force switch even if on cooldown (force=true bypasses cooldown check)
            if (m_dimManager.switchDimension(true)) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                RelicSystem::onDimensionSwitch(relics, &playerHP);
                relics.lastSwitchDimension = m_dimManager.getCurrentDimension();
            }
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(pPos, 20, {220, 80, 220, 200}, 200.0f, 3.0f);
            AudioManager::instance().play(SFX::DimensionSwitch);
        }

        // VampiricEdge: block natural heal-over-time and pickup-orb healing
        // (healing from kills is still applied in kill-event block below)
        relics.vampiricEdgeActive = relics.hasRelic(RelicID::VampiricEdge);

        // BerserkersCurse: strip shield whenever it would be active
        if (RelicSystem::hasBerserkersCurse(relics) && m_player->hasShield) {
            m_player->hasShield = false;
            m_player->shieldTimer = 0;
        }
    }

    // Player death
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    if (hp.currentHP <= 0) {
        // Check for Phoenix Feather relic first
        if (m_player->getEntity()->hasComponent<RelicComponent>()) {
            auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
            if (RelicSystem::hasPhoenixFeather(relics)) {
                RelicSystem::consumePhoenixFeather(relics);
                hp.currentHP = hp.maxHP;
                hp.invincibilityTimer = 3.0f;
                AudioManager::instance().play(SFX::RiftShieldActivate);
                m_camera.shake(12.0f, 0.5f);
                Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(pPos, 50, {255, 180, 50, 255}, 350.0f, 6.0f);
                m_particles.burst(pPos, 25, {255, 120, 30, 255}, 250.0f, 5.0f);
                // LuckyPhoenix synergy: retain 50% of shards as bonus
                float shardRetain = RelicSynergy::getPhoenixShardRetain(relics);
                if (shardRetain > 0) {
                    int bonusShards = static_cast<int>(shardsCollected * shardRetain);
                    if (bonusShards > 0) {
                        shardsCollected += bonusShards;
                        game->getUpgradeSystem().addRiftShards(bonusShards);
                        m_particles.burst(pPos, 30, {255, 215, 0, 255}, 200.0f, 4.0f);
                    }
                }
                // Skip to buff check
                goto skipDeath;
            }
        }
        // Check for Extra Life run buff
        auto& buffs = game->getRunBuffSystem();
        if (buffs.hasExtraLife()) {
            buffs.consumeExtraLife();
            hp.currentHP = hp.maxHP * 0.5f;
            hp.invincibilityTimer = 2.0f;
            AudioManager::instance().play(SFX::RiftShieldActivate);
            m_camera.shake(10.0f, 0.4f);
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(pPos, 40, {255, 255, 200, 255}, 300.0f, 5.0f);
            m_particles.burst(pPos, 20, {200, 150, 255, 255}, 200.0f, 4.0f);
        } else {
            // Start death sequence: dramatic pause before ending run
            if (!m_playerDying) {
                m_playerDying = true;
                m_deathCause = 1; // HP depleted
                m_deathSequenceTimer = m_deathSequenceDuration;
                AudioManager::instance().play(SFX::PlayerDeath);
                m_camera.shake(20.0f, 0.8f);

                // Death explosion particles
                Vec2 deathPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                const auto& classColor = ClassSystem::getData(m_player->playerClass).color;
                m_particles.burst(deathPos, 40, {classColor.r, classColor.g, classColor.b, 255}, 300.0f, 6.0f);
                m_particles.burst(deathPos, 25, {255, 60, 40, 255}, 200.0f, 5.0f);
                m_particles.burst(deathPos, 15, {255, 255, 200, 255}, 250.0f, 4.0f);
                // Expanding ring
                for (int i = 0; i < 16; i++) {
                    float angle = i * 6.283185f / 16.0f;
                    Vec2 ringPos = {deathPos.x + std::cos(angle) * 8.0f, deathPos.y + std::sin(angle) * 8.0f};
                    m_particles.burst(ringPos, 2, {255, 100, 60, 200}, 180.0f, 3.0f);
                }

                m_hud.triggerDamageFlash();
            }
            return; // Wait for death sequence to finish
        }
    }
    skipDeath:

    // Check breakable walls (dash/charged attack destroys them)
    checkBreakableWalls();

    // Check secret room discovery
    checkSecretRoomDiscovery();

    // Secret room proximity hints (ambient particles near undiscovered rooms)
    updateSecretRoomHints(dt);

    // Check random event interaction
    checkEventInteraction();

    // Check NPC interaction
    checkNPCInteraction();

    // Check rift interaction
    checkRiftInteraction();

    // Check if player reached exit
    checkExitReached();

    // Camera follow player with look-ahead + vertical dead zone
    Vec2 camTarget = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    bool playerGrounded = false;
    Vec2 playerVel = {0, 0};
    if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        playerVel = phys.velocity;
        playerGrounded = phys.onGround;
    }
    m_camera.follow(camTarget, dt, playerVel, playerGrounded);
    m_camera.shakeMultiplier = g_shakeIntensity; // apply settings slider
    m_camera.update(dt);

    // Spawn waves
    updateSpawnWaves(dt);

    // Particles
    m_particles.update(dt);

    // DimResidue damage zones: tick down lifetime and deal AoE damage
    int curDim = m_dimManager.getCurrentDimension();
    float residueDps = 15.0f; // Default DPS
    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
        residueDps = RelicSynergy::getResidueDamage(m_player->getEntity()->getComponent<RelicComponent>());
    }
    m_entities.forEach([&](Entity& zone) {
        if (zone.getTag() != "dim_residue") return;
        if (!zone.hasComponent<HealthComponent>()) return;
        auto& zHP = zone.getComponent<HealthComponent>();
        zHP.currentHP -= dt;
        if (zHP.currentHP <= 0) {
            zone.destroy();
            return;
        }
        // Only deal damage in matching dimension
        if (zone.dimension != 0 && zone.dimension != curDim) return;
        auto& zt = zone.getComponent<TransformComponent>();
        Vec2 zCenter = zt.getCenter();
        // Damage enemies in range
        m_entities.forEach([&](Entity& enemy) {
            if (enemy.getTag().find("enemy") == std::string::npos) return;
            if (enemy.dimension != 0 && enemy.dimension != curDim) return;
            if (!enemy.hasComponent<TransformComponent>() || !enemy.hasComponent<HealthComponent>()) return;
            auto& et = enemy.getComponent<TransformComponent>();
            float dx = et.getCenter().x - zCenter.x;
            float dy = et.getCenter().y - zCenter.y;
            if (std::sqrt(dx * dx + dy * dy) < 48.0f) {
                enemy.getComponent<HealthComponent>().takeDamage(residueDps * dt);
            }
        });
    });

    // Ambient dimension dust (subtle atmospheric particles)
    m_ambientDustTimer += dt;
    if (m_ambientDustTimer >= 0.3f) {
        m_ambientDustTimer = 0;
        if (m_player) {
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            SDL_Color dustColor = (m_dimManager.getCurrentDimension() == 1)
                ? SDL_Color{100, 140, 220, 80}   // Dim A: blue dust
                : SDL_Color{220, 120, 80, 80};   // Dim B: warm dust
            m_particles.ambientDust(pPos, dustColor, 300.0f);
        }
    }

    // Level complete transition
    if (m_levelComplete) {
        m_levelCompleteTimer += dt;
        if (m_levelCompleteTimer >= 2.0f) {
            roomsCleared++;

            if (m_voidSovereignDefeated) {
                endRun();
                return;
            }

            m_currentDifficulty++;
            m_runSeed += 100;

            // Pick new theme pair every 3 levels
            if (m_currentDifficulty % 3 == 0) {
                auto themes = WorldTheme::getRandomPair(m_runSeed);
                m_themeA = themes.first;
                m_themeB = themes.second;
                m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
                AudioManager::instance().playThemeAmbient(static_cast<int>(m_themeA.id));
            }

            // Open shop between levels (every level except first)
            game->setShopDifficulty(m_currentDifficulty);
            m_levelComplete = false;
            m_levelCompleteTimer = 0;
            m_pendingLevelGen = true;
            game->pushState(StateID::Shop);
            return; // Don't process further this frame; resume after shop closes
        }
    }

    // (pendingLevelGen moved to top of update to prevent stale state bugs)

    // Collapse mechanic
    if (m_collapsing) {
        m_collapseTimer += dt;
        float urgency = m_collapseTimer / m_collapseMaxTime;
        if (urgency > 0.5f) {
            m_camera.shake(urgency * 3.0f, 0.1f);
        }
        if (m_collapseTimer >= m_collapseMaxTime && !m_playerDying) {
            m_playerDying = true;
            m_deathCause = 3; // Collapse
            m_deathSequenceTimer = m_deathSequenceDuration;
            AudioManager::instance().play(SFX::PlayerDeath);
            m_camera.shake(20.0f, 0.8f);
            if (m_player) {
                Vec2 deathPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_particles.burst(deathPos, 30, {255, 100, 30, 255}, 250.0f, 5.0f);
                m_particles.burst(deathPos, 20, {255, 50, 20, 255}, 300.0f, 6.0f);
            }
            m_hud.triggerDamageFlash();
            return;
        }
    }

    // FIX: Decay exit locked hint timer
    if (m_exitLockedHintTimer > 0) m_exitLockedHintTimer -= dt;
    if (m_riftDimensionHintTimer > 0) m_riftDimensionHintTimer -= dt;

    // Track dash count for achievement
    if (m_player && m_player->isDashing && m_player->dashTimer >= m_player->dashDuration - 0.02f) {
        m_dashCount++;
    }

    // Consume shard pickups from item drops
    if (m_player && m_player->riftShardsCollected > 0) {
        int shards = m_player->riftShardsCollected;
        float shardMult = game->getRunBuffSystem().getShardMultiplier() * m_achievementShardMult;
        if (m_player->getEntity()->hasComponent<RelicComponent>())
            shardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
        int finalShards = static_cast<int>(shards * shardMult);
        shardsCollected += finalShards;
        game->getUpgradeSystem().addRiftShards(finalShards);
        m_player->riftShardsCollected = 0;

        // Pickup feedback: purple particles + floating shard count
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_particles.burst(pPos, 6, {180, 130, 255, 220}, 60.0f, 2.5f);
        FloatingDamageNumber num;
        num.position = {pPos.x + (std::rand() % 10 - 5.0f), pPos.y - 10.0f};
        num.value = static_cast<float>(finalShards);
        num.isShard = true;
        num.lifetime = 0.9f;
        num.maxLifetime = 0.9f;
        num.isPlayerDamage = false;
        m_damageNumbers.push_back(num);
    }

    // Consume buff pickup effects (shield/speed/damage)
    if (m_player) {
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        auto emitBuffPickup = [&](SDL_Color color, const char* label) {
            m_particles.burst(pPos, 10, color, 80.0f, 3.0f);
            FloatingDamageNumber num;
            num.position = {pPos.x, pPos.y - 16.0f};
            num.value = 0;
            num.isBuff = true;
            num.buffText = label;
            num.lifetime = 1.2f;
            num.maxLifetime = 1.2f;
            num.isPlayerDamage = false;
            m_damageNumbers.push_back(num);
        };
        if (m_player->pickupShieldPending) {
            emitBuffPickup({100, 180, 255, 220}, "SHIELD");
            m_player->pickupShieldPending = false;
        }
        if (m_player->pickupSpeedPending) {
            emitBuffPickup({255, 255, 80, 220}, "SPEED UP");
            m_player->pickupSpeedPending = false;
        }
        if (m_player->pickupDamagePending) {
            emitBuffPickup({255, 80, 80, 220}, "DMG UP");
            m_player->pickupDamagePending = false;
        }
        if (m_player->weaponPickupPending >= 0) {
            auto wid = static_cast<WeaponID>(m_player->weaponPickupPending);
            bool melee = WeaponSystem::isMelee(wid);
            SDL_Color wColor = melee ? SDL_Color{255, 180, 60, 220} : SDL_Color{60, 200, 255, 220};
            emitBuffPickup(wColor, WeaponSystem::getWeaponName(wid));
            m_camera.shake(10.0f, 0.3f);
            m_player->weaponPickupPending = -1;
        }
    }

    // Consume kill tracking from combat system
    if (m_combatSystem.killCount > 0) {
        enemiesKilled += m_combatSystem.killCount;
        game->getUpgradeSystem().totalEnemiesKilled += m_combatSystem.killCount;
        m_screenEffects.triggerKillFlash();

        // Relic on-kill effects
        if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
            auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
            for (int k = 0; k < m_combatSystem.killCount; k++) {
                // SoulSiphon: heal on kill (VampiricFrenzy synergy: 8 HP under 30%)
                // Blocked by VampiricEdge (VampiricEdge is its own kill-heal source)
                float killHeal = RelicSystem::getKillHeal(relics);
                if (killHeal > 0 && !RelicSystem::hasVampiricEdge(relics)) {
                    auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                    float synergyHeal = RelicSynergy::getKillHealBonus(relics, playerHP.getPercent());
                    playerHP.heal(synergyHeal > 0 ? synergyHeal : killHeal);
                }
                // VoidHunger: +1% DMG per kill
                RelicSystem::onEnemyKill(relics);
                // EntropyVortex synergy: kills reduce entropy
                float entropyReduce = RelicSynergy::getKillEntropyReduction(relics);
                if (entropyReduce > 0) {
                    m_entropy.reduceEntropy(m_entropy.getMaxEntropy() * entropyReduce);
                }
                // VoidPact: heal 5 HP on kill (capped at 60% max HP)
                // Blocked by VampiricEdge
                float vpHeal = RelicSystem::getVoidPactHeal(relics);
                if (vpHeal > 0 && !RelicSystem::hasVampiricEdge(relics)) {
                    auto& pHP = m_player->getEntity()->getComponent<HealthComponent>();
                    float hpCap = pHP.maxHP * RelicSystem::getVoidPactMaxHPPercent(relics);
                    if (pHP.currentHP < hpCap) {
                        pHP.heal(std::min(vpHeal, hpCap - pHP.currentHP));
                    }
                }
                // EntropySponge: kills add entropy
                float killEntropy = RelicSystem::getKillEntropyGain(relics);
                if (killEntropy > 0) {
                    m_entropy.addEntropy(killEntropy);
                }

                // BloodPact: -1 HP per kill (can't kill, floor at 1)
                float bloodPactCost = RelicSystem::getBloodPactKillHPCost(relics);
                if (bloodPactCost > 0) {
                    auto& bpHP = m_player->getEntity()->getComponent<HealthComponent>();
                    if (bpHP.currentHP > bloodPactCost) {
                        bpHP.currentHP -= bloodPactCost;
                    }
                }

                // EntropySiphon: kills reduce entropy by 8
                float siphonReduce = RelicSystem::getEntropySiphonKillReduction(relics);
                if (siphonReduce > 0) {
                    m_entropy.reduceEntropy(siphonReduce);
                }

                // VampiricEdge: heal 3 HP per kill (this is kill-heal, allowed even with VampiricEdge)
                float vampHeal = RelicSystem::getVampiricEdgeKillHeal(relics);
                if (vampHeal > 0) {
                    auto& veHP = m_player->getEntity()->getComponent<HealthComponent>();
                    veHP.heal(vampHeal);
                }

                // Weapon-Relic Synergies on kill
                auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
                auto& pHP = m_player->getEntity()->getComponent<HealthComponent>();

                // BloodRift: RiftBlade + BloodFrenzy — melee kills under 50% HP heal 5
                float bloodRiftHeal = RelicSynergy::getBloodRiftHeal(
                    relics, combat.currentMelee, pHP.getPercent());
                if (bloodRiftHeal > 0) {
                    pHP.heal(bloodRiftHeal);
                }

                // EntropyBeam: VoidBeam + EntropyAnchor — kills reduce entropy by 5%
                if (RelicSynergy::isEntropyBeamActive(relics, combat.currentRanged) &&
                    combat.currentAttack == AttackType::Ranged) {
                    m_entropy.reduceEntropy(m_entropy.getMaxEntropy() * 0.05f);
                }

                // StormScatter: RiftShotgun + ChainLightning — kills chain to 2 extra enemies
                if (RelicSynergy::isStormScatterActive(relics, combat.currentRanged) &&
                    combat.currentAttack == AttackType::Ranged) {
                    float chainDmg = 15.0f;
                    int chainsLeft = 2;
                    auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
                    Vec2 killPos = playerT.getCenter();
                    int dim = m_dimManager.getCurrentDimension();
                    m_entities.forEach([&](Entity& nearby) {
                        if (chainsLeft <= 0) return;
                        if (nearby.getTag().find("enemy") == std::string::npos) return;
                        if (!nearby.isAlive()) return;
                        if (!nearby.hasComponent<TransformComponent>() || !nearby.hasComponent<HealthComponent>()) return;
                        if (nearby.dimension != 0 && nearby.dimension != dim) return;
                        auto& nt = nearby.getComponent<TransformComponent>();
                        float dx = nt.getCenter().x - killPos.x;
                        float dy = nt.getCenter().y - killPos.y;
                        if (dx * dx + dy * dy < 120.0f * 120.0f) {
                            nearby.getComponent<HealthComponent>().takeDamage(chainDmg);
                            chainsLeft--;
                            m_particles.burst(nt.getCenter(), 8, {255, 255, 80, 255}, 150.0f, 2.0f);
                        }
                    });
                    if (chainsLeft < 2) {
                        AudioManager::instance().play(SFX::ElectricChain);
                    }
                }
            }
        }
    }
    // Lore: Walker Scourge - 50+ kills
    if (enemiesKilled >= 50) {
        if (auto* lore = game->getLoreSystem()) {
            if (!lore->isDiscovered(LoreID::WalkerScourge)) {
                lore->discover(LoreID::WalkerScourge);
                AudioManager::instance().play(SFX::LoreDiscover);
            }
        }
    }

    // Lore: Void Hunger - survive entropy > 90%
    if (m_entropy.getEntropy() > 90.0f) {
        if (auto* lore = game->getLoreSystem()) {
            if (!lore->isDiscovered(LoreID::VoidHunger)) {
                lore->discover(LoreID::VoidHunger);
                AudioManager::instance().play(SFX::LoreDiscover);
            }
        }
    }

    if (m_combatSystem.killedMiniBoss) {
        game->getAchievements().unlock("mini_boss_hunter");
        m_combatSystem.killedMiniBoss = false;
    }
    if (m_combatSystem.killedElemental) {
        game->getAchievements().unlock("elemental_slayer");
        m_combatSystem.killedElemental = false;
    }

    // Echo of Self: reward when mirror enemy is defeated
    if (m_echoSpawned && !m_echoRewarded) {
        bool echoAlive = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_echo" && e.isAlive()) echoAlive = true;
        });
        if (!echoAlive) {
            m_echoRewarded = true;
            m_echoSpawned = false;
            // Reward scales with story stage (stage already incremented after fight)
            int echoStage = std::min(m_npcStoryProgress[static_cast<int>(NPCType::EchoOfSelf)] - 1, 2);
            echoStage = std::max(0, echoStage);
            int echoShards = 40 + echoStage * 25; // 40, 65, 90
            game->getUpgradeSystem().addRiftShards(echoShards);
            shardsCollected += echoShards;
            m_player->damageBoostTimer = 30.0f + echoStage * 15.0f;
            m_player->damageBoostMultiplier = 1.2f + echoStage * 0.1f;
            if (echoStage >= 2) {
                // Stage 2 bonus: heal to full
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.heal(hp.maxHP);
            }
            AudioManager::instance().play(SFX::LevelComplete);
            m_camera.shake(10.0f + echoStage * 5.0f, 0.4f);
            Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(pPos, 30 + echoStage * 10, {220, 120, 255, 255}, 250.0f + echoStage * 50.0f, 4.0f);
            m_particles.burst(pPos, 15 + echoStage * 8, {255, 215, 0, 255}, 180.0f + echoStage * 40.0f, 3.0f);
        }
    }

    // Skill achievement kill tracking + kill feed (before clear)
    {
        int rangedKillsThisFrame = 0;
        int chainKillsThisFrame = 0;
        for (auto& ke : m_combatSystem.killEvents) {
            if (ke.wasAerial) m_aerialKillsThisRun++;
            if (ke.wasDash) {
                m_dashKillsThisRun++;
                m_dashKillsThisRunForWeapon++;
            }
            if (ke.wasCharged) m_chargedKillsThisRun++;
            if (ke.wasRanged) rangedKillsThisFrame++;
            // AoE kills (slam = AoE)
            if (ke.wasSlam) chainKillsThisFrame++;
            addKillFeedEntry(ke);
        }
        // RiftShotgun unlock: 3+ kills from one ranged attack (e.g. shotgun spread or chain)
        int aoeThisFrame = rangedKillsThisFrame + chainKillsThisFrame;
        if (aoeThisFrame >= 3) {
            m_aoeKillCountThisRun = aoeThisFrame; // triggers unlock check
        }
    }
    updateKillFeed(dt);

    // Combat challenge tracking
    updateCombatChallenge(dt);
    m_combatSystem.killEvents.clear();

    // Event chain tracking
    updateEventChain(dt);

    m_combatSystem.killCount = 0;

    // Achievement checks (update(dt) already called above at line ~1056)
    auto& ach = game->getAchievements();

    if (roomsCleared >= 10) ach.unlock("room_clearer");
    if (roomsCleared >= 20) ach.unlock("survivor");
    if (m_currentDifficulty >= 5) ach.unlock("unstoppable");
    if (m_dashCount >= 100) ach.unlock("dash_master");
    if (m_dimManager.getSwitchCount() >= 50) ach.unlock("dimension_hopper");
    if (game->getUpgradeSystem().getRiftShards() >= 1000) ach.unlock("shard_hoarder");

    // Combo check
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        if (combat.comboCount > m_bestCombo) m_bestCombo = combat.comboCount;
        if (combat.comboCount >= 10) ach.unlock("combo_king");
        if (combat.comboCount >= 15) ach.unlock("combo_legend");
    }

    // Skill achievements
    if (m_aerialKillsThisRun >= 5) ach.unlock("aerial_ace");
    if (m_dashKillsThisRun >= 10) ach.unlock("dash_slayer");
    if (m_chargedKillsThisRun >= 3) ach.unlock("charged_fury");
    if (m_dimManager.getResonanceTier() >= 3) ach.unlock("resonance_master");

    // Full upgrade check
    for (auto& u : game->getUpgradeSystem().getUpgrades()) {
        if (u.currentLevel >= u.maxLevel) {
            ach.unlock("full_upgrade");
            break;
        }
    }

    // Meta-progression unlock checks (class + weapon unlocks)
    checkUnlockConditions();
    updateUnlockNotifications(dt);
}

void PlayState::checkRiftInteraction() {
    if (!m_player || !m_level) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto rifts = m_level->getRiftPositions();
    int currentDim = m_dimManager.getCurrentDimension();

    m_nearRiftIndex = -1;
    float nearestRiftDist = 60.0f;
    for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
        // FIX: Skip already-repaired rifts
        if (m_repairedRiftIndices.count(i)) continue;
        float dx = playerPos.x - rifts[i].x;
        float dy = playerPos.y - rifts[i].y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearestRiftDist) {
            nearestRiftDist = dist;
            m_nearRiftIndex = i;
        }
    }

    if (m_nearRiftIndex >= 0 && game->getInput().isActionPressed(Action::Interact)) {
        if (m_smokeTest) {
            const auto& topology = m_level->getTopology();
            if (m_nearRiftIndex < static_cast<int>(topology.rifts.size())) {
                const auto& topoRift = topology.rifts[m_nearRiftIndex];
                smokeLog("[SMOKE] Rift attempt idx=%d room=%d tile=(%d,%d) dims=%d%d requiredDim=%d validated=%d genValidated=%d pos=(%.0f,%.0f) dim=%d",
                         m_nearRiftIndex,
                         topoRift.roomIndex,
                         topoRift.tileX,
                         topoRift.tileY,
                         topoRift.accessibleDimensions[1] ? 1 : 0,
                         topoRift.accessibleDimensions[2] ? 1 : 0,
                         topoRift.requiredDimension,
                         topoRift.validated ? 1 : 0,
                         topology.validation.passed ? 1 : 0,
                         playerPos.x,
                         playerPos.y,
                         currentDim);
            }
        }

        int requiredDim = m_level->getRiftRequiredDimension(m_nearRiftIndex);
        if (requiredDim != 0 && requiredDim != currentDim) {
            m_riftDimensionHintTimer = 2.0f;
            m_riftDimensionHintRequiredDim = requiredDim;
            SDL_Color hintColor = (requiredDim == 2)
                ? SDL_Color{255, 90, 145, 255}
                : SDL_Color{90, 180, 255, 255};
            m_particles.burst(rifts[m_nearRiftIndex], 10, hintColor, 80.0f, 1.4f);
            if (m_smokeTest) {
                smokeLog("[SMOKE] Rift blocked idx=%d currentDim=%d requiredDim=%d",
                         m_nearRiftIndex,
                         currentDim,
                         requiredDim);
            }
            return;
        }

        // Puzzle type progression: early = Timing (easiest), mid = Sequence, late = Alignment/Pattern
        int puzzleType;
        if (m_currentDifficulty <= 1) {
            puzzleType = 0; // Always Timing for first levels
        } else if (m_currentDifficulty <= 3) {
            puzzleType = (riftsRepaired % 2 == 0) ? 0 : 1; // Mix Timing and Sequence
        } else {
            int roll = std::rand() % 100;
            if (roll < 15) puzzleType = 0;       // 15% Timing
            else if (roll < 40) puzzleType = 1;  // 25% Sequence
            else if (roll < 70) puzzleType = 2;  // 30% Alignment
            else puzzleType = 3;                 // 30% Pattern
        }
        m_activePuzzle = std::make_unique<RiftPuzzle>(
            static_cast<PuzzleType>(puzzleType), m_currentDifficulty);

        int riftIdx = m_nearRiftIndex; // FIX: capture rift index for lambda
        m_activePuzzle->onComplete = [this, riftIdx](bool success) {
            if (success) {
                AudioManager::instance().play(SFX::RiftRepair);
                int requiredDim = m_level ? m_level->getRiftRequiredDimension(riftIdx) : 0;
                const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
                riftsRepaired++;
                // FIX: Mark this rift as repaired and track per-level count
                m_repairedRiftIndices.insert(riftIdx);
                m_levelRiftsRepaired++;
                m_entropy.onRiftRepaired();
                if (requiredDim == 2) {
                    m_entropy.reduceEntropy(shiftBalance.dimBEntropyRepairBonus);
                }
                game->getAchievements().unlock("rift_walker");
                float riftShardMult = game->getRunBuffSystem().getShardMultiplier();
                riftShardMult *= m_achievementShardMult;
                if (m_player && m_player->getEntity()->hasComponent<RelicComponent>())
                    riftShardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
                int riftShards = static_cast<int>((10 + m_currentDifficulty * 5) * riftShardMult);
                if (requiredDim == 2) {
                    riftShards = static_cast<int>(riftShards * shiftBalance.dimBShardMultiplier + 0.5f);
                }
                shardsCollected += riftShards;
                game->getUpgradeSystem().addRiftShards(riftShards);
                game->getUpgradeSystem().totalRiftsRepaired++;

                SDL_Color repairColor = (requiredDim == 2)
                    ? SDL_Color{255, 90, 145, 255}
                    : SDL_Color{90, 180, 255, 255};
                m_particles.burst(m_dimManager.playerPos, 40,
                    repairColor, 250.0f, 6.0f);
                m_camera.shake(5.0f, 0.3f);

                // Start collapse after all rifts in THIS level repaired
                // (skip if already collapsing, e.g. boss kill already triggered it)
                int totalRifts = static_cast<int>(m_level->getRiftPositions().size());
                if (m_smokeTest) {
                    smokeLog("[SMOKE] Rift repaired idx=%d repaired=%d/%d collapsing=%d",
                             riftIdx,
                             m_levelRiftsRepaired,
                             totalRifts,
                             m_collapsing ? 1 : 0);
                }
                if (m_levelRiftsRepaired >= totalRifts && !m_collapsing) {
                    m_collapsing = true;
                    m_collapseTimer = 0;
                    m_level->setExitActive(true);
                }
            } else {
                AudioManager::instance().play(SFX::RiftFail);
                m_entropy.addEntropy(10.0f);
                m_camera.shake(3.0f, 0.2f);
                if (m_smokeTest) {
                    smokeLog("[SMOKE] Rift puzzle failed idx=%d", riftIdx);
                }
            }
        };

        m_activePuzzle->activate();
    }
}

void PlayState::checkExitReached() {
    if (!m_player || !m_level || m_levelComplete) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    Vec2 exitPos = m_level->getExitPoint();
    float dx = playerPos.x - exitPos.x;
    float dy = playerPos.y - exitPos.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 50.0f) {
        // FIX: Exit only works after all rifts are repaired (collapse started).
        // This enforces the core game loop: repair rifts -> collapse -> escape.
        if (!m_collapsing) {
            // Show hint to player that exit is locked
            m_exitLockedHintTimer = 2.0f;
            return;
        }
        m_levelComplete = true;
        m_levelCompleteTimer = 0;
        // Flawless Floor: completed level without taking damage
        if (!m_tookDamageThisLevel) {
            game->getAchievements().unlock("flawless_floor");
        }
        if (m_smokeTest) {
            smokeLog("[SMOKE] LEVEL %d COMPLETE! (%.1fs)", m_currentDifficulty, m_smokeRunTime);
            if (g_smokeRegression) {
                g_smokeCompletedFloor = std::max(g_smokeCompletedFloor, m_currentDifficulty);
            }
        }
        AudioManager::instance().play(SFX::LevelComplete);
        m_camera.shake(10.0f, 0.3f);
        m_combatSystem.addHitFreeze(0.1f);
        // Celebration particles at player position
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_particles.burst(pPos, 40, {140, 255, 180, 255}, 250.0f, 5.0f);
        m_particles.burst(pPos, 20, {255, 215, 100, 255}, 180.0f, 3.0f);
        float shardMult = (g_selectedDifficulty == GameDifficulty::Easy) ? 1.5f :
                          (g_selectedDifficulty == GameDifficulty::Hard) ? 0.75f : 1.0f;
        shardMult *= game->getRunBuffSystem().getShardMultiplier();
        shardMult *= m_achievementShardMult;
        if (m_player->getEntity()->hasComponent<RelicComponent>())
            shardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
        int shards = static_cast<int>((20 + m_currentDifficulty * 10) * shardMult);
        shardsCollected += shards;
        game->getUpgradeSystem().addRiftShards(shards);
    }
}

void PlayState::handlePuzzleInput(int action) {
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->handleInput(action);
    }
}

void PlayState::renderBackground(SDL_Renderer* renderer) {
    auto& bgColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.background : m_themeB.colors.background;
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    Vec2 camPos = m_camera.getPosition();
    Uint32 ticks = SDL_GetTicks();
    SDL_Texture* artTestBackground = nullptr;
    if (kUseAiFinalBackgroundArtTest) {
        if (m_isBossLevel && m_bossTestBackground) {
            artTestBackground = m_bossTestBackground;
        } else {
            artTestBackground = (m_dimManager.getCurrentDimension() == 1) ? m_dimATestBackground : m_dimBTestBackground;
        }
    }

    if (artTestBackground) {
        const int worldPixelWidth = m_level ? m_level->getPixelWidth() : SCREEN_WIDTH;
        renderWholeArtBackground(renderer,
                                 artTestBackground,
                                 camPos,
                                 SCREEN_WIDTH,
                                 SCREEN_HEIGHT,
                                 worldPixelWidth,
                                 kArtTestBackgroundParallaxX,
                                 164, 168, 174, 196);

        // Keep AI backgrounds slightly subdued so gameplay silhouettes stay readable.
        SDL_SetRenderDrawColor(renderer, 7, 9, 14, 92);
        SDL_Rect dimmer{0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &dimmer);

        // Extra calm in the gameplay-heavy center band.
        SDL_SetRenderDrawColor(renderer, 10, 12, 18, 28);
        SDL_Rect outerCenterVeil{
            SCREEN_WIDTH * 14 / 100,
            SCREEN_HEIGHT * 20 / 100,
            SCREEN_WIDTH * 72 / 100,
            SCREEN_HEIGHT * 50 / 100
        };
        SDL_RenderFillRect(renderer, &outerCenterVeil);

        SDL_SetRenderDrawColor(renderer, 12, 14, 22, 34);
        SDL_Rect innerCenterVeil{
            SCREEN_WIDTH * 24 / 100,
            SCREEN_HEIGHT * 28 / 100,
            SCREEN_WIDTH * 52 / 100,
            SCREEN_HEIGHT * 34 / 100
        };
        SDL_RenderFillRect(renderer, &innerCenterVeil);
    } else {
        // Layer 1: Distant stars (slow parallax)
        for (int i = 0; i < 60; i++) {
            // Deterministic pseudo-random positions based on index
            int baseX = ((i * 7919 + 104729) % 2560) - 640;
            int baseY = ((i * 6271 + 73856) % 1440) - 360;

            float parallax = 0.05f;
            int sx = baseX - static_cast<int>(camPos.x * parallax) % 2560;
            int sy = baseY - static_cast<int>(camPos.y * parallax) % 1440;
            // Wrap around screen
            sx = ((sx % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH;
            sy = ((sy % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT;

            float twinkle = 0.5f + 0.5f * std::sin(ticks * 0.002f + i * 1.7f);
            Uint8 brightness = static_cast<Uint8>(30 + 40 * twinkle);
            int size = (i % 3 == 0) ? 2 : 1;
            SDL_SetRenderDrawColor(renderer, brightness, brightness,
                                   static_cast<Uint8>(brightness + 30), 255);
            SDL_Rect star = {sx, sy, size, size};
            SDL_RenderFillRect(renderer, &star);
        }

        // Layer 1.5: Nebula clouds (slow drift, theme-colored)
        {
            float nebulaParallax = 0.08f;
            int nOffX = static_cast<int>(camPos.x * nebulaParallax);
            int nOffY = static_cast<int>(camPos.y * nebulaParallax);
            auto& accent = (m_dimManager.getCurrentDimension() == 1) ?
                m_themeA.colors.accent : m_themeB.colors.accent;
            for (int i = 0; i < 5; i++) {
                int baseX = ((i * 12347 + 5281) % 2200) - 400;
                int baseY = ((i * 8731 + 2917) % 1000) - 200;
                int nx = ((baseX - nOffX) % 2000 + 2000) % 2000 - 400;
                int ny = ((baseY - nOffY / 2) % 900 + 900) % 900 - 100;
                int cloudW = 120 + (i * 37) % 180;
                int cloudH = 40 + (i * 23) % 60;
                float drift = std::sin(ticks * 0.0004f + i * 1.3f) * 15.0f;
                nx += static_cast<int>(drift);
                // Draw as overlapping semi-transparent circles
                Uint8 na = static_cast<Uint8>(8 + 4 * std::sin(ticks * 0.001f + i));
                SDL_SetRenderDrawColor(renderer, accent.r / 3, accent.g / 3, accent.b / 3, na);
                for (int j = 0; j < 4; j++) {
                    int cx = nx + (j * cloudW) / 4;
                    int cy = ny + static_cast<int>(std::sin(j * 1.5f) * cloudH / 3);
                    int rw = cloudW / 3 + (j * 13) % 20;
                    int rh = cloudH / 2 + (j * 7) % 10;
                    SDL_Rect cloud = {cx - rw / 2, cy - rh / 2, rw, rh};
                    SDL_RenderFillRect(renderer, &cloud);
                }
            }
        }

        // Layer 2: Grid lines (medium parallax, gives depth)
        float gridParallax = 0.15f;
        int gridSpacing = 120;
        int gridOffX = static_cast<int>(camPos.x * gridParallax) % gridSpacing;
        int gridOffY = static_cast<int>(camPos.y * gridParallax) % gridSpacing;

        Uint8 gridAlpha = 15;
        SDL_SetRenderDrawColor(renderer,
            static_cast<Uint8>(bgColor.r + 30 > 255 ? 255 : bgColor.r + 30),
            static_cast<Uint8>(bgColor.g + 30 > 255 ? 255 : bgColor.g + 30),
            static_cast<Uint8>(bgColor.b + 30 > 255 ? 255 : bgColor.b + 30),
            gridAlpha);

        for (int x = -gridOffX; x < SCREEN_WIDTH; x += gridSpacing) {
            SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
        }
        for (int y = -gridOffY; y < SCREEN_HEIGHT; y += gridSpacing) {
            SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
        }

        // Layer 2.5: Theme-specific silhouettes (0.1x parallax)
        {
            float silParallax = 0.1f;
            int offX = static_cast<int>(camPos.x * silParallax);
            int offY = static_cast<int>(camPos.y * silParallax);
            const auto& theme = (m_dimManager.getCurrentDimension() == 1) ? m_themeA : m_themeB;
            Uint8 silR = static_cast<Uint8>(std::min(255, bgColor.r + 15));
            Uint8 silG = static_cast<Uint8>(std::min(255, bgColor.g + 15));
            Uint8 silB = static_cast<Uint8>(std::min(255, bgColor.b + 15));
            Uint8 silA = 30;
            SDL_SetRenderDrawColor(renderer, silR, silG, silB, silA);

            int themeId = static_cast<int>(theme.id);
            for (int i = 0; i < 8; i++) {
                int baseX = ((i * 8317 + 29741) % 2000) - 200;
                int baseY = SCREEN_HEIGHT - 50 - ((i * 4729) % 200);
                int sx = ((baseX - offX) % 1800 + 1800) % 1800 - 200;
                int sy = baseY - (offY % 300);

                switch (themeId) {
                case 0: { // Victorian - gear outlines
                    int cx = sx + 30; int cy = sy - 20;
                    int r = 15 + (i % 3) * 8;
                    for (int seg = 0; seg < 8; seg++) {
                        float a1 = seg * 0.785f + ticks * 0.0003f * (i % 2 ? 1 : -1);
                        float a2 = a1 + 0.5f;
                        SDL_RenderDrawLine(renderer, cx + static_cast<int>(std::cos(a1) * r), cy + static_cast<int>(std::sin(a1) * r),
                                           cx + static_cast<int>(std::cos(a2) * r), cy + static_cast<int>(std::sin(a2) * r));
                    }
                    break;
                }
                case 1: // DeepOcean - seaweed columns
                    for (int s = 0; s < 4; s++) {
                        int wx = sx + s * 8;
                        int sway = static_cast<int>(std::sin(ticks * 0.001f + i + s * 0.5f) * 6);
                        SDL_Rect seg = {wx + sway, sy - s * 25, 3, 25};
                        SDL_RenderFillRect(renderer, &seg);
                    }
                    break;
                case 2: { // NeonCity - building skyline
                    int bw = 30 + (i * 17) % 40;
                    int bh = 60 + (i * 31) % 120;
                    SDL_Rect bld = {sx, SCREEN_HEIGHT - bh, bw, bh};
                    SDL_RenderFillRect(renderer, &bld);
                    // Window dots
                    for (int wy = SCREEN_HEIGHT - bh + 8; wy < SCREEN_HEIGHT - 8; wy += 12) {
                        for (int wx = sx + 4; wx < sx + bw - 4; wx += 8) {
                            SDL_SetRenderDrawColor(renderer, silR, silG, static_cast<Uint8>(std::min(255, silB + 40)), 20);
                            SDL_Rect win = {wx, wy, 3, 4};
                            SDL_RenderFillRect(renderer, &win);
                        }
                    }
                    SDL_SetRenderDrawColor(renderer, silR, silG, silB, silA);
                    break;
                }
                case 3: { // AncientRuins - columns
                    int colH = 80 + (i * 23) % 60;
                    SDL_Rect col = {sx, SCREEN_HEIGHT - colH, 12, colH};
                    SDL_RenderFillRect(renderer, &col);
                    SDL_Rect cap = {sx - 4, SCREEN_HEIGHT - colH, 20, 6};
                    SDL_RenderFillRect(renderer, &cap);
                    break;
                }
                case 4: // CrystalCavern - stalactites
                    for (int s = 0; s < 3; s++) {
                        int tip = 30 + (i + s) * 13 % 50;
                        int bx = sx + s * 20;
                        SDL_RenderDrawLine(renderer, bx, 0, bx - 5, tip);
                        SDL_RenderDrawLine(renderer, bx, 0, bx + 5, tip);
                        SDL_RenderDrawLine(renderer, bx - 5, tip, bx + 5, tip);
                    }
                    break;
                case 5: { // BioMechanical - pipes
                    int pipeY = sy - 40;
                    SDL_Rect pipe = {sx, pipeY, 60 + (i * 19) % 80, 6};
                    SDL_RenderFillRect(renderer, &pipe);
                    SDL_Rect joint = {sx + pipe.w, pipeY - 3, 6, 12};
                    SDL_RenderFillRect(renderer, &joint);
                    break;
                }
                case 6: // FrozenWasteland - ice spikes
                    for (int s = 0; s < 3; s++) {
                        int spikeH = 40 + (i + s) * 17 % 60;
                        int bx = sx + s * 18;
                        int by = SCREEN_HEIGHT;
                        SDL_RenderDrawLine(renderer, bx - 6, by, bx, by - spikeH);
                        SDL_RenderDrawLine(renderer, bx + 6, by, bx, by - spikeH);
                    }
                    break;
                case 7: { // VolcanicCore - rock pinnacles
                    int rockH = 50 + (i * 29) % 80;
                    int bx = sx;
                    int by = SCREEN_HEIGHT;
                    SDL_RenderDrawLine(renderer, bx - 12, by, bx - 3, by - rockH);
                    SDL_RenderDrawLine(renderer, bx + 12, by, bx + 3, by - rockH);
                    SDL_Rect base = {bx - 12, by - 8, 24, 8};
                    SDL_RenderFillRect(renderer, &base);
                    break;
                }
                case 8: // FloatingIslands - cloud wisps
                    for (int c = 0; c < 3; c++) {
                        int cx = sx + c * 25;
                        int cy = sy - 80 + static_cast<int>(std::sin(ticks * 0.0005f + i + c) * 10);
                        SDL_Rect cloud = {cx, cy, 20 + c * 5, 6};
                        SDL_RenderFillRect(renderer, &cloud);
                    }
                    break;
                case 9: { // VoidRealm - fractured geometry
                    int fx = sx; int fy = sy - 60;
                    for (int s = 0; s < 4; s++) {
                        float a = s * 1.57f + ticks * 0.0004f;
                        int len = 15 + (i * 7 + s) % 20;
                        SDL_RenderDrawLine(renderer, fx, fy, fx + static_cast<int>(std::cos(a) * len), fy + static_cast<int>(std::sin(a) * len));
                    }
                    break;
                }
                case 10: { // SpaceWestern - mesa silhouettes
                    int mesaW = 50 + (i * 23) % 40;
                    int mesaH = 40 + (i * 13) % 50;
                    SDL_Rect mesa = {sx, SCREEN_HEIGHT - mesaH, mesaW, mesaH};
                    SDL_RenderFillRect(renderer, &mesa);
                    SDL_Rect top = {sx + 5, SCREEN_HEIGHT - mesaH - 8, mesaW - 10, 8};
                    SDL_RenderFillRect(renderer, &top);
                    break;
                }
                case 11: // Biopunk - organic bulbs
                    for (int b = 0; b < 2; b++) {
                        int bx = sx + b * 30;
                        int by = sy - 30;
                        int br = 8 + (i + b) % 6;
                        SDL_Rect bulb = {bx - br, by - br, br * 2, br * 2};
                        SDL_RenderDrawRect(renderer, &bulb);
                        SDL_RenderDrawLine(renderer, bx, by + br, bx, by + br + 20);
                    }
                    break;
                default: break;
                }
            }
        }
    }

    // Layer 3: Floating dimension particles (close parallax)
    auto& dimColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.solid : m_themeB.colors.solid;
    for (int i = 0; i < 20; i++) {
        float speed = 0.3f + (i % 5) * 0.1f;
        int baseX = ((i * 4513 + 23143) % 1600) - 160;
        int baseY = ((i * 3251 + 51749) % 900) - 90;

        float px = baseX - camPos.x * speed;
        float py = baseY - camPos.y * speed;
        // Gentle floating motion
        py += std::sin(ticks * 0.001f + i * 2.3f) * 15.0f;

        int screenX = ((static_cast<int>(px) % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH;
        int screenY = ((static_cast<int>(py) % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT;

        int size = 2 + i % 3;
        Uint8 pa = static_cast<Uint8>(20 + 15 * std::sin(ticks * 0.0015f + i));
        SDL_SetRenderDrawColor(renderer, dimColor.r, dimColor.g, dimColor.b, pa);
        SDL_Rect particle = {screenX, screenY, size, size};
        SDL_RenderFillRect(renderer, &particle);
    }
}

void PlayState::render(SDL_Renderer* renderer) {
    // Parallax background
    renderBackground(renderer);

    if (m_level) {
        m_level->render(renderer, m_camera,
                        m_dimManager.getCurrentDimension(),
                        m_dimManager.getBlendAlpha());
    }

    // Render entities
    m_renderSys.render(renderer, m_entities, m_camera,
                       m_dimManager.getCurrentDimension(),
                       m_dimManager.getBlendAlpha());

    // Grappling hook rope: draw line from player to hook/attach point
    if (m_player) {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        Vec2 playerCenter = playerT.getCenter();
        Vec2 camPos = m_camera.getPosition();

        int px = static_cast<int>(playerCenter.x - camPos.x);
        int py = static_cast<int>(playerCenter.y - camPos.y);

        if (m_player->isGrappling) {
            // Draw rope to attach point
            int ax = static_cast<int>(m_player->hookAttachPoint.x - camPos.x);
            int ay = static_cast<int>(m_player->hookAttachPoint.y - camPos.y);

            // Rope: draw 3 lines for thickness effect
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 180, 140, 60, 200);
            SDL_RenderDrawLine(renderer, px, py, ax, ay);
            SDL_SetRenderDrawColor(renderer, 200, 160, 80, 160);
            SDL_RenderDrawLine(renderer, px - 1, py, ax - 1, ay);
            SDL_RenderDrawLine(renderer, px + 1, py, ax + 1, ay);

            // Hook anchor point indicator (small filled rect)
            SDL_SetRenderDrawColor(renderer, 220, 180, 80, 255);
            SDL_Rect hookRect = {ax - 3, ay - 3, 6, 6};
            SDL_RenderFillRect(renderer, &hookRect);
        } else if (m_player->hookFlying && m_player->hookProjectile &&
                   m_player->hookProjectile->isAlive()) {
            // Draw rope to flying hook projectile
            auto& hookT = m_player->hookProjectile->getComponent<TransformComponent>();
            Vec2 hookCenter = hookT.getCenter();
            int hx = static_cast<int>(hookCenter.x - camPos.x);
            int hy = static_cast<int>(hookCenter.y - camPos.y);

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 180, 140, 60, 150);
            SDL_RenderDrawLine(renderer, px, py, hx, hy);
        }
    }

    // Trails (before particles)
    m_trails.render(renderer);

    // Particles
    m_particles.render(renderer, m_camera);

    // Dimension switch visual effect
    m_dimManager.applyVisualEffect(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Entropy visual effects
    m_entropy.applyVisualEffects(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Collapse warning
    if (m_collapsing) {
        float urgency = m_collapseTimer / m_collapseMaxTime;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint8 a = static_cast<Uint8>(urgency * 100);
        SDL_SetRenderDrawColor(renderer, 255, 30, 0, a);
        SDL_Rect border = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderDrawRect(renderer, &border);

        // Collapse timer text
        TTF_Font* font = game->getFont();
        if (font) {
            float remaining = m_collapseMaxTime - m_collapseTimer;
            if (remaining < 0) remaining = 0;
            int secs = static_cast<int>(remaining);
            int tenths = static_cast<int>((remaining - secs) * 10);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "COLLAPSE: %d.%d", secs, tenths);

            // Pulsing red-white text
            Uint8 pulse = static_cast<Uint8>(200 + 55 * std::sin(SDL_GetTicks() * 0.01f));
            SDL_Color tc = {pulse, static_cast<Uint8>(pulse / 4), 0, 255};
            SDL_Surface* ts = TTF_RenderText_Blended(font, buf, tc);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    SDL_Rect tr = {640 - ts->w / 2, 50, ts->w, ts->h};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
        }
    }

    // FIX: Rift progress indicator (top center, always visible when rifts remain)
    if (m_level && !m_collapsing && !m_levelComplete) {
        int totalRifts = static_cast<int>(m_level->getRiftPositions().size());
        int remaining = totalRifts - m_levelRiftsRepaired;
        if (remaining > 0) {
            int dimARemaining = 0;
            int dimBRemaining = 0;
            for (int i = 0; i < totalRifts; i++) {
                if (m_repairedRiftIndices.count(i)) continue;
                int requiredDim = m_level->getRiftRequiredDimension(i);
                if (requiredDim == 2) dimBRemaining++;
                else dimARemaining++;
            }
            TTF_Font* font = game->getFont();
            if (font) {
                char riftBuf[96];
                std::snprintf(riftBuf, sizeof(riftBuf), "Rifts: %d / %d  [A:%d B:%d]",
                              m_levelRiftsRepaired, totalRifts, dimARemaining, dimBRemaining);
                SDL_Color rc = {180, 130, 255, 200};
                SDL_Surface* rs = TTF_RenderText_Blended(font, riftBuf, rc);
                if (rs) {
                    SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
                    if (rt) {
                        SDL_Rect rr = {640 - rs->w / 2, 30, rs->w, rs->h};
                        SDL_RenderCopy(renderer, rt, nullptr, &rr);
                        SDL_DestroyTexture(rt);
                    }
                    SDL_FreeSurface(rs);
                }
            }
        }
    }

    // FIX: Exit locked hint when player reaches exit before repairing rifts
    if (m_exitLockedHintTimer > 0) {
        TTF_Font* font = game->getFont();
        if (font) {
            Uint8 alpha = static_cast<Uint8>(std::min(1.0f, m_exitLockedHintTimer) * 255);
            SDL_Color hc = {255, 100, 80, alpha};
            const char* hintText = (m_isBossLevel && !m_bossDefeated)
                ? "Defeat the boss to unlock exit!"
                : "Repair all rifts to unlock exit!";
            SDL_Surface* hs = TTF_RenderText_Blended(font, hintText, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_SetTextureAlphaMod(ht, alpha);
                    SDL_Rect hr = {640 - hs->w / 2, 460, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }

    if (m_riftDimensionHintTimer > 0 && m_riftDimensionHintRequiredDim > 0) {
        TTF_Font* font = game->getFont();
        if (font) {
            const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
            int dimBShardBonus = getDimensionShiftDimBShardBonusPercent(m_currentDifficulty);
            Uint8 alpha = static_cast<Uint8>(std::min(1.0f, m_riftDimensionHintTimer) * 255);
            SDL_Color hc = (m_riftDimensionHintRequiredDim == 2)
                ? SDL_Color{255, 90, 145, alpha}
                : SDL_Color{90, 180, 255, alpha};
            char hintText[160];
            if (m_riftDimensionHintRequiredDim == 2) {
                std::snprintf(hintText, sizeof(hintText),
                              "This rift stabilizes in DIM-B. +%d%% shards, -%.0f entropy on repair, +%.2f entropy/s.",
                              dimBShardBonus,
                              shiftBalance.dimBEntropyRepairBonus,
                              shiftBalance.dimBEntropyPerSecond);
            } else {
                std::snprintf(hintText, sizeof(hintText),
                              "This rift stabilizes in DIM-A. Safer route, no DIM-B pressure.");
            }
            SDL_Surface* hs = TTF_RenderText_Blended(font, hintText, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_SetTextureAlphaMod(ht, alpha);
                    SDL_Rect hr = {640 - hs->w / 2, 482, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }

    // Near rift indicator
    if (m_nearRiftIndex >= 0 && !m_activePuzzle) {
        TTF_Font* font = game->getFont();
        if (font) {
            const auto& shiftBalance = getDimensionShiftFloorBalance(m_currentDifficulty);
            int dimBShardBonus = getDimensionShiftDimBShardBonusPercent(m_currentDifficulty);
            int currentDim = m_dimManager.getCurrentDimension();
            int requiredDim = m_level ? m_level->getRiftRequiredDimension(m_nearRiftIndex) : 0;
            bool riftActive = requiredDim == 0 || requiredDim == currentDim;
            SDL_Color c = (requiredDim == 2)
                ? SDL_Color{255, 90, 145, 220}
                : SDL_Color{90, 180, 255, 220};
            char promptText[160];
            std::snprintf(promptText, sizeof(promptText), "Press F to repair rift");
            if (!riftActive) {
                if (requiredDim == 2) {
                    std::snprintf(promptText, sizeof(promptText),
                                  "Shift to DIM-B: +%d%% shards, -%.0f entropy on repair",
                                  dimBShardBonus,
                                  shiftBalance.dimBEntropyRepairBonus);
                } else {
                    std::snprintf(promptText, sizeof(promptText),
                                  "Shift to DIM-A to stabilize this rift");
                }
            } else if (requiredDim == 2) {
                std::snprintf(promptText, sizeof(promptText),
                              "Press F to repair volatile DIM-B rift (+%d%% shards, -%.0f entropy)",
                              dimBShardBonus,
                              shiftBalance.dimBEntropyRepairBonus);
            } else if (requiredDim == 1) {
                std::snprintf(promptText, sizeof(promptText),
                              "Press F to repair stable DIM-A rift");
            }
            SDL_Surface* s = TTF_RenderText_Blended(font, promptText, c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 500, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Off-screen rift direction indicators
    if (m_level && !m_activePuzzle) {
        auto rifts = m_level->getRiftPositions();
        Vec2 camPos = m_camera.getPosition();
        float halfW = 640.0f, halfH = 360.0f;

        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            // Skip already-repaired rifts
            if (m_repairedRiftIndices.count(i)) continue;

            float sx = (rifts[i].x - camPos.x) * m_camera.zoom + halfW;
            float sy = (rifts[i].y - camPos.y) * m_camera.zoom + halfH;

            // Only show if off-screen
            if (sx >= -10 && sx <= 1290 && sy >= -10 && sy <= 730) continue;

            // Clamp to screen edge with margin
            float cx = std::max(25.0f, std::min(1255.0f, sx));
            float cy = std::max(25.0f, std::min(695.0f, sy));

            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.005f + i * 1.5f);
            Uint8 pa = static_cast<Uint8>(120 + 80 * pulse);
            int requiredDim = m_level->getRiftRequiredDimension(i);
            Uint8 rr = (requiredDim == 2) ? 255 : 90;
            Uint8 rg = (requiredDim == 2) ? 90 : 180;
            Uint8 rb = (requiredDim == 2) ? 145 : 255;

            // Diamond indicator
            SDL_SetRenderDrawColor(renderer, rr, rg, rb, pa);
            SDL_Rect ind = {static_cast<int>(cx) - 5, static_cast<int>(cy) - 5, 10, 10};
            SDL_RenderFillRect(renderer, &ind);

            // Arrow pointing toward rift
            float angle = std::atan2(sy - cy, sx - cx);
            int ax = static_cast<int>(cx + std::cos(angle) * 14);
            int ay = static_cast<int>(cy + std::sin(angle) * 14);
            SDL_SetRenderDrawColor(renderer, rr, rg, rb, pa);
            SDL_RenderDrawLine(renderer, static_cast<int>(cx), static_cast<int>(cy), ax, ay);
        }
    }

    // Off-screen exit direction indicator (visible when exit is active OR during collapse)
    if (m_level && (m_level->isExitActive() || m_collapsing) && !m_levelComplete) {
        Vec2 exitPos = m_level->getExitPoint();
        Vec2 camPos = m_camera.getPosition();
        float halfW = 640.0f, halfH = 360.0f;

        float sx = (exitPos.x - camPos.x) * m_camera.zoom + halfW;
        float sy = (exitPos.y - camPos.y) * m_camera.zoom + halfH;

        // Only show if exit is off-screen
        if (sx < -10 || sx > 1290 || sy < -10 || sy > 730) {
            // Clamp to screen edge with margin
            float cx = std::max(30.0f, std::min(1250.0f, sx));
            float cy = std::max(30.0f, std::min(690.0f, sy));

            Uint8 pa, gr, gg;
            if (m_collapsing) {
                // Urgency-based pulsing during collapse
                float urgency = m_collapseTimer / std::max(1.0f, m_collapseMaxTime);
                float pulseSpeed = 0.006f + urgency * 0.012f;
                float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * pulseSpeed);
                pa = static_cast<Uint8>(160 + 95 * pulse);
                gr = static_cast<Uint8>(80 + 175 * urgency);
                gg = static_cast<Uint8>(255 - 80 * urgency);
            } else {
                // Calm green pulsing when exit is simply active
                float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.004f);
                pa = static_cast<Uint8>(120 + 80 * pulse);
                gr = 60;
                gg = 220;
            }
            SDL_SetRenderDrawColor(renderer, gr, gg, 60, pa);

            // Larger diamond for exit (8px vs 5px for rifts)
            int icx = static_cast<int>(cx);
            int icy = static_cast<int>(cy);
            SDL_Rect ind = {icx - 8, icy - 8, 16, 16};
            SDL_RenderFillRect(renderer, &ind);

            // Inner glow
            SDL_SetRenderDrawColor(renderer, 255, 255, 200, static_cast<Uint8>(pa * 0.6f));
            SDL_Rect inner = {icx - 4, icy - 4, 8, 8};
            SDL_RenderFillRect(renderer, &inner);

            // Arrow pointing toward exit
            float angle = std::atan2(sy - cy, sx - cx);
            int ax = static_cast<int>(cx + std::cos(angle) * 18);
            int ay = static_cast<int>(cy + std::sin(angle) * 18);
            SDL_SetRenderDrawColor(renderer, gr, gg, 60, pa);
            SDL_RenderDrawLine(renderer, icx, icy, ax, ay);
            // Thicker arrow: draw offset lines
            SDL_RenderDrawLine(renderer, icx + 1, icy, ax + 1, ay);
            SDL_RenderDrawLine(renderer, icx, icy + 1, ax, ay + 1);

            // "EXIT" label next to indicator
            TTF_Font* font = game->getFont();
            if (font) {
                SDL_Color labelColor = {gr, gg, 60, pa};
                SDL_Surface* ls = TTF_RenderText_Blended(font, "EXIT", labelColor);
                if (ls) {
                    SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                    if (lt) {
                        // Position label offset from indicator, avoid going off-screen
                        int lx = icx - ls->w / 2;
                        int ly = icy - 22;
                        if (ly < 5) ly = icy + 12; // Flip below if at top edge
                        lx = std::max(5, std::min(SCREEN_WIDTH - ls->w - 5, lx));
                        SDL_Rect lr = {lx, ly, ls->w, ls->h};
                        SDL_RenderCopy(renderer, lt, nullptr, &lr);
                        SDL_DestroyTexture(lt);
                    }
                    SDL_FreeSurface(ls);
                }
            }
        }
    }

    // Puzzle overlay
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    // Level complete: rift warp transition effect
    if (m_levelComplete) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        float progress = m_levelCompleteTimer / 2.0f; // 0 to 1 over 2 seconds
        if (progress > 1.0f) progress = 1.0f;
        Uint32 ticks = SDL_GetTicks();

        // Growing dark overlay
        Uint8 overlayA = static_cast<Uint8>(progress * 180);
        SDL_SetRenderDrawColor(renderer, 10, 5, 20, overlayA);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);

        // Expanding rift circle from center
        int maxRadius = 400;
        int radius = static_cast<int>(progress * maxRadius);
        float pulse = 0.7f + 0.3f * std::sin(ticks * 0.01f);
        Uint8 riftA = static_cast<Uint8>((1.0f - progress * 0.5f) * 120 * pulse);

        // Rift ring (multiple concentric rings)
        for (int r = radius - 6; r <= radius + 6; r += 3) {
            if (r < 0) continue;
            for (int angle = 0; angle < 60; angle++) {
                float a = angle * 6.283185f / 60.0f + ticks * 0.002f;
                int px = 640 + static_cast<int>(std::cos(a) * r);
                int py = 360 + static_cast<int>(std::sin(a) * r);
                if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT) continue;
                SDL_SetRenderDrawColor(renderer, 180, 100, 255, riftA);
                SDL_Rect dot = {px - 1, py - 1, 3, 3};
                SDL_RenderFillRect(renderer, &dot);
            }
        }

        // Scanlines converging toward center
        if (progress > 0.3f) {
            int lineCount = static_cast<int>((progress - 0.3f) * 20);
            for (int i = 0; i < lineCount; i++) {
                int y = ((i * 4513 + ticks / 40) % SCREEN_HEIGHT);
                Uint8 la = static_cast<Uint8>(progress * 60);
                SDL_SetRenderDrawColor(renderer, 150, 80, 220, la);
                SDL_Rect line = {0, y, SCREEN_WIDTH, 1};
                SDL_RenderFillRect(renderer, &line);
            }
        }

        // Text banner
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(180 * std::min(1.0f, progress * 3.0f)));
        SDL_Rect banner = {0, 330, SCREEN_WIDTH, 60};
        SDL_RenderFillRect(renderer, &banner);

        TTF_Font* font = game->getFont();
        if (font) {
            float textAlpha = std::min(1.0f, progress * 3.0f);
            Uint8 ta = static_cast<Uint8>(textAlpha * 255);
            SDL_Color c = {140, 255, 180, ta};
            SDL_Surface* s = TTF_RenderText_Blended(font, "RIFT STABILIZED - Warping to next dimension...", c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 348, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Random events (NPCs in world space)
    renderRandomEvents(renderer, game->getFont());

    // NPCs
    renderNPCs(renderer, game->getFont());

    // NPC dialog overlay
    if (m_showNPCDialog) {
        renderNPCDialog(renderer, game->getFont());
    }

    // Tutorial hints (first 3 levels only)
    renderTutorialHints(renderer, game->getFont());

    // Floating damage numbers
    renderDamageNumbers(renderer, game->getFont());

    // Kill feed (bottom-right)
    renderKillFeed(renderer, game->getFont());

    // Boss HP bar at top of screen
    if (m_isBossLevel && !m_bossDefeated) {
        renderBossHealthBar(renderer, game->getFont());
    }

    // Combo milestone golden flash overlay
    if (m_comboMilestoneFlash > 0) {
        float alpha = m_comboMilestoneFlash / 0.4f;
        Uint8 a = static_cast<Uint8>(alpha * 60);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 215, 0, a);
        SDL_Rect fullScreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fullScreen);
    }

    // Screen effects (vignette, low-HP pulse, kill flash, boss intro, ripple, glitch)
    m_screenEffects.render(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Damage flash overlay
    m_hud.renderFlash(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Wave/area clear celebration text
    if (m_waveClearTimer > 0 && game->getFont()) {
        float t = m_waveClearTimer / 2.0f; // 1.0 → 0.0
        // Fade in fast (first 0.3s), then hold, then fade out (last 0.5s)
        float alpha;
        if (t > 0.85f) alpha = (1.0f - t) / 0.15f; // fade in
        else if (t < 0.25f) alpha = t / 0.25f;       // fade out
        else alpha = 1.0f;                             // hold
        Uint8 a = static_cast<Uint8>(alpha * 255);

        // Gold text "AREA CLEARED"
        SDL_Color gold = {255, 215, 0, a};
        SDL_Surface* surf = TTF_RenderText_Blended(game->getFont(), "AREA CLEARED", gold);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                // Scale up with slight pulse
                float pulse = 1.0f + 0.05f * std::sin(m_waveClearTimer * 8.0f);
                float scale = 2.0f * pulse;
                int w = static_cast<int>(surf->w * scale);
                int h = static_cast<int>(surf->h * scale);
                SDL_Rect dst = {SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT / 3 - h / 2, w, h};
                SDL_SetTextureAlphaMod(tex, a);
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        // Subtle gold line underneath
        if (a > 30) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            int lineW = static_cast<int>(200 * alpha);
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, static_cast<Uint8>(a * 0.5f));
            SDL_Rect line = {SCREEN_WIDTH / 2 - lineW / 2, SCREEN_HEIGHT / 3 + 14, lineW, 2};
            SDL_RenderFillRect(renderer, &line);
        }
    }

    // HUD (on top of everything)
    m_hud.setFloor(m_currentDifficulty);
    m_hud.setKillCount(enemiesKilled);
    m_hud.render(renderer, game->getFont(), m_player.get(), &m_entropy, &m_dimManager,
                 SCREEN_WIDTH, SCREEN_HEIGHT, game->getFPS(), game->getUpgradeSystem().getRiftShards());

    // Relic bar (bottom left, below buffs)
    renderRelicBar(renderer, game->getFont());

    // Challenge HUD (left side, below main HUD)
    renderChallengeHUD(renderer, game->getFont());

    // Combat challenge indicator (top center)
    renderCombatChallenge(renderer, game->getFont());

    // Event chain tracker (left side)
    renderEventChain(renderer, game->getFont());

    // Minimap (top right corner, M key to toggle)
    m_hud.renderMinimap(renderer, m_level.get(), m_player.get(), &m_dimManager,
                        SCREEN_WIDTH, SCREEN_HEIGHT, &m_entities, &m_repairedRiftIndices);

    // Achievement notification popup
    auto* notif = game->getAchievements().getActiveNotification();
    TTF_Font* achFont = game->getFont();
    if (notif && achFont) {
        float t = notif->timer / notif->duration;
        float slideIn = std::min(1.0f, (1.0f - t) * 5.0f); // fast slide in
        float fadeOut = std::min(1.0f, t * 3.0f); // fade out at end
        float alpha = slideIn * fadeOut;
        Uint8 a = static_cast<Uint8>(alpha * 220);

        bool hasReward = !notif->rewardText.empty();
        int popW = 300, popH = hasReward ? 56 : 40;
        int popX = 640 - popW / 2;
        int popY = 660 - popH - static_cast<int>(slideIn * 20);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 40, 25, a);
        SDL_Rect bg = {popX, popY, popW, popH};
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 100, 220, 120, a);
        SDL_RenderDrawRect(renderer, &bg);

        // Trophy icon
        SDL_SetRenderDrawColor(renderer, 255, 200, 50, a);
        SDL_Rect trophy = {popX + 10, popY + 8, 16, 16};
        SDL_RenderFillRect(renderer, &trophy);

        // Achievement name
        char achText[128];
        snprintf(achText, sizeof(achText), "Achievement: %s", notif->name.c_str());
        SDL_Color tc = {200, 255, 210, a};
        SDL_Surface* ts = TTF_RenderText_Blended(achFont, achText, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_SetTextureAlphaMod(tt, a);
                SDL_Rect tr = {popX + 34, popY + 4, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }

        // Reward text line (golden)
        if (hasReward) {
            char rewardText[128];
            snprintf(rewardText, sizeof(rewardText), "Reward: %s", notif->rewardText.c_str());
            SDL_Color rc = {255, 220, 80, a};
            SDL_Surface* rs = TTF_RenderText_Blended(achFont, rewardText, rc);
            if (rs) {
                SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
                if (rt) {
                    SDL_SetTextureAlphaMod(rt, a);
                    SDL_Rect rr = {popX + 34, popY + 28, rs->w, rs->h};
                    SDL_RenderCopy(renderer, rt, nullptr, &rr);
                    SDL_DestroyTexture(rt);
                }
                SDL_FreeSurface(rs);
            }
        }
    }

    // Lore discovery notification popup
    if (auto* lore = game->getLoreSystem()) {
        auto* loreNotif = lore->getActiveNotification();
        TTF_Font* loreFont = game->getFont();
        if (loreNotif && loreFont) {
            float t = loreNotif->timer / loreNotif->duration;
            float slideIn = std::min(1.0f, (1.0f - t) * 4.0f);
            float fadeOut = std::min(1.0f, t * 2.5f);
            float alpha = slideIn * fadeOut;
            Uint8 a = static_cast<Uint8>(alpha * 230);

            int popW = 320, popH = 44;
            int popX = SCREEN_WIDTH / 2 - popW / 2;
            int popY = 20 + static_cast<int>((1.0f - slideIn) * 30);

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            // Dark purple background
            SDL_SetRenderDrawColor(renderer, 25, 15, 40, a);
            SDL_Rect bg = {popX, popY, popW, popH};
            SDL_RenderFillRect(renderer, &bg);
            // Purple border with glow
            float pulse = 0.6f + 0.4f * std::sin(SDL_GetTicks() * 0.006f);
            Uint8 borderA = static_cast<Uint8>(a * pulse);
            SDL_SetRenderDrawColor(renderer, 160, 100, 255, borderA);
            SDL_RenderDrawRect(renderer, &bg);
            SDL_Rect innerBorder = {popX + 1, popY + 1, popW - 2, popH - 2};
            SDL_SetRenderDrawColor(renderer, 120, 70, 200, static_cast<Uint8>(borderA * 0.5f));
            SDL_RenderDrawRect(renderer, &innerBorder);

            // Scroll/book icon (small rectangle with lines)
            SDL_SetRenderDrawColor(renderer, 180, 140, 255, a);
            SDL_Rect scrollIcon = {popX + 12, popY + 10, 12, 16};
            SDL_RenderFillRect(renderer, &scrollIcon);
            SDL_SetRenderDrawColor(renderer, 100, 60, 180, a);
            for (int line = 0; line < 3; line++) {
                SDL_RenderDrawLine(renderer, popX + 14, popY + 14 + line * 4,
                                   popX + 22, popY + 14 + line * 4);
            }

            // "Lore Discovered" label
            SDL_Color labelColor = {140, 110, 200, a};
            SDL_Surface* labelSurf = TTF_RenderText_Blended(loreFont, "LORE DISCOVERED", labelColor);
            if (labelSurf) {
                SDL_Texture* labelTex = SDL_CreateTextureFromSurface(renderer, labelSurf);
                if (labelTex) {
                    SDL_SetTextureAlphaMod(labelTex, a);
                    SDL_Rect lr = {popX + 32, popY + 4, labelSurf->w, labelSurf->h};
                    SDL_RenderCopy(renderer, labelTex, nullptr, &lr);
                    SDL_DestroyTexture(labelTex);
                }
                SDL_FreeSurface(labelSurf);
            }

            // Lore title
            SDL_Color titleColor = {220, 200, 255, a};
            SDL_Surface* titleSurf = TTF_RenderText_Blended(loreFont, loreNotif->title.c_str(), titleColor);
            if (titleSurf) {
                SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
                if (titleTex) {
                    SDL_SetTextureAlphaMod(titleTex, a);
                    SDL_Rect tr = {popX + 32, popY + 22, titleSurf->w, titleSurf->h};
                    SDL_RenderCopy(renderer, titleTex, nullptr, &tr);
                    SDL_DestroyTexture(titleTex);
                }
                SDL_FreeSurface(titleSurf);
            }
        }
    }

    // Unlock notification popups (golden, top-center)
    renderUnlockNotifications(renderer, game->getFont());

    // Death sequence overlay: red vignette + desaturation effect
    if (m_playerDying) {
        float progress = 1.0f - (m_deathSequenceTimer / m_deathSequenceDuration);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Full screen red-black fade (intensifies over time)
        Uint8 overlayAlpha = static_cast<Uint8>(std::min(180.0f, progress * 220.0f));
        SDL_SetRenderDrawColor(renderer, 40, 0, 0, overlayAlpha);
        SDL_Rect fullScreen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fullScreen);

        // Vignette border (thick red edges)
        int borderW = static_cast<int>(20 + progress * 60);
        Uint8 borderAlpha = static_cast<Uint8>(std::min(200.0f, progress * 250.0f));
        SDL_SetRenderDrawColor(renderer, 180, 20, 0, borderAlpha);
        SDL_Rect top = {0, 0, SCREEN_WIDTH, borderW};
        SDL_Rect bot = {0, SCREEN_HEIGHT - borderW, SCREEN_WIDTH, borderW};
        SDL_Rect lft = {0, 0, borderW, SCREEN_HEIGHT};
        SDL_Rect rgt = {SCREEN_WIDTH - borderW, 0, borderW, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &lft);
        SDL_RenderFillRect(renderer, &rgt);

        // "SUIT FAILURE" text (fades in)
        TTF_Font* font = game->getFont();
        if (font && progress > 0.2f) {
            float textAlpha = std::min(1.0f, (progress - 0.2f) * 2.0f);
            Uint8 ta = static_cast<Uint8>(textAlpha * 255);
            float pulse = 0.7f + 0.3f * std::sin(SDL_GetTicks() * 0.015f);
            Uint8 tr = static_cast<Uint8>(200 * pulse + 55);
            SDL_Color deathColor = {tr, 30, 20, ta};
            SDL_Surface* ds = TTF_RenderText_Blended(font, "SUIT FAILURE", deathColor);
            if (ds) {
                SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
                if (dt) {
                    SDL_SetTextureAlphaMod(dt, ta);
                    // Center of screen, slight upward drift
                    int yOff = static_cast<int>(progress * 15.0f);
                    SDL_Rect dr = {SCREEN_WIDTH / 2 - ds->w / 2,
                                   SCREEN_HEIGHT / 2 - ds->h / 2 - yOff,
                                   ds->w, ds->h};
                    SDL_RenderCopy(renderer, dt, nullptr, &dr);
                    SDL_DestroyTexture(dt);
                }
                SDL_FreeSurface(ds);
            }
        }
    }

    // Debug overlay (F3)
    if (m_showDebugOverlay) {
        renderDebugOverlay(renderer, game->getFont());
    }

    // Relic choice overlay
    if (m_showRelicChoice) {
        renderRelicChoice(renderer, game->getFont());
    }
}

void PlayState::renderDebugOverlay(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || !m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // Compute raw damage multiplier (before cap)
    float rawDmg = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::BloodFrenzy && hpPct < 0.3f) rawDmg += 0.25f;
        if (r.id == RelicID::BerserkerCore) rawDmg += 0.50f;
        if (r.id == RelicID::VoidHunger) rawDmg += relics.voidHungerBonus;
        if (r.id == RelicID::DualityGem && (curDim == 1 || RelicSynergy::isDualNatureActive(relics)))
            rawDmg += 0.25f;
        if (r.id == RelicID::StabilityMatrix) {
            float rate = RelicSynergy::getStabilityDmgPerSec(relics);
            float maxB = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
            rawDmg += std::min(relics.stabilityTimer * rate, maxB);
        }
    }
    rawDmg *= RelicSynergy::getPhaseHunterDamageMult(relics);
    float clampedDmg = RelicSystem::getDamageMultiplier(relics, hpPct, curDim);

    // Compute raw attack speed multiplier (before cap)
    float rawSpd = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) rawSpd += 0.15f;
    }
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0)
        rawSpd += relics.riftConduitStacks * 0.10f;
    float clampedSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    // Switch CD with floor detection
    float baseCD = 0.5f;
    float cdMult = RelicSystem::getSwitchCooldownMult(relics);
    float finalCD = m_dimManager.switchCooldown;
    bool cdFloored = (baseCD * cdMult) < finalCD + 0.001f && relics.hasRelic(RelicID::RiftMantle);

    // Zone count
    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
    });

    // Gameplay paused detection
    bool gameplayPaused = m_showRelicChoice || m_showNPCDialog
        || (m_activePuzzle && m_activePuzzle->isActive());

    // Stability derived values
    float stabRate = RelicSynergy::getStabilityDmgPerSec(relics);
    float stabMax = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
    float stabPct = std::min(relics.stabilityTimer * stabRate, stabMax) * 100.0f;

    int synergyCount = RelicSynergy::getActiveSynergyCount(relics);
    float dmgTakenMult = RelicSystem::getDamageTakenMult(relics, curDim);

    // Build lines into stack buffer
    struct Line { char text[96]; SDL_Color color; };
    Line lines[15];
    int lineCount = 0;

    auto addLine = [&](SDL_Color c, const char* fmt, ...) {
        if (lineCount >= 15) return;
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(lines[lineCount].text, sizeof(lines[0].text), fmt, args);
        va_end(args);
        lines[lineCount].color = c;
        lineCount++;
    };

    SDL_Color cHeader = {255, 220, 80, 255};
    SDL_Color cNormal = {200, 220, 240, 255};
    SDL_Color cWarn   = {255, 100, 80, 255};
    SDL_Color cGreen  = {100, 255, 140, 255};

    addLine(cHeader, "=== RELIC SAFETY RAILS (F3/F4) ===");
    addLine(gameplayPaused ? cWarn : cGreen, "Paused: %s", gameplayPaused ? "YES" : "NO");
    addLine(rawDmg > clampedDmg + 0.001f ? cWarn : cNormal,
            "DMG Mult: %.2fx raw -> %.2fx clamped", rawDmg, clampedDmg);
    addLine(rawSpd > clampedSpd + 0.001f ? cWarn : cNormal,
            "ATK Speed: %.2fx raw -> %.2fx clamped", rawSpd, clampedSpd);
    addLine(dmgTakenMult > 1.001f ? cWarn : dmgTakenMult < 0.999f ? cGreen : cNormal,
            "Damage Taken Mult: %.2fx", dmgTakenMult);
    if (cdFloored)
        addLine(cWarn, "Switch CD: %.2fs x%.2f = %.2fs FLOOR", baseCD, cdMult, finalCD);
    else
        addLine(cNormal, "Switch CD: %.2fs x%.2f = %.2fs", baseCD, cdMult, finalCD);
    addLine(cNormal, "VoidHunger: %.0f%% / %.0f%% cap",
            relics.voidHungerBonus * 100.0f, 40.0f);
    addLine(cNormal, "VoidRes ICD: %.2fs remain", std::max(0.0f, relics.voidResonanceProcCD));
    addLine(zoneCount > 0 ? cHeader : cNormal,
            "Residue: %d/%d zones, spawn ICD %.2fs",
            zoneCount, RelicSystem::getMaxResidueZones(),
            std::max(0.0f, relics.dimResidueSpawnCD));
    addLine(cNormal, "Stability: %.1fs (%.0f%% DMG)",
            relics.stabilityTimer, stabPct);
    addLine(cNormal, "Conduit: %d stacks, %.1fs left",
            relics.riftConduitStacks, std::max(0.0f, relics.riftConduitTimer));
    addLine(cNormal, "Dim: %d | Synergies: %d | Seed: %d",
            curDim, synergyCount, m_runSeed);
    addLine({140, 140, 160, 255}, "F4: snapshot to balance_snapshots.csv");

    // Render panel
    int panelX = 10, panelY = 190;
    int lineH = 16, padX = 8, padY = 4;
    int panelW = 360;
    int panelH = padY * 2 + lineCount * lineH;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 255, 220, 80, 120);
    SDL_RenderDrawRect(renderer, &bg);

    for (int i = 0; i < lineCount; i++) {
        SDL_Surface* s = TTF_RenderText_Blended(font, lines[i].text, lines[i].color);
        if (!s) continue;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        if (t) {
            SDL_Rect dst = {panelX + padX, panelY + padY + i * lineH, s->w, s->h};
            SDL_RenderCopy(renderer, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
    }

    // Process pending F4 snapshot (only on keypress, not per frame)
    if (m_pendingSnapshot) {
        m_pendingSnapshot = false;
        writeBalanceSnapshot();
    }
}

void PlayState::updateBalanceTracking(float dt) {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    bool gameplayActive = !m_showRelicChoice && !m_showNPCDialog
        && !(m_activePuzzle && m_activePuzzle->isActive());
    if (gameplayActive) m_balanceStats.activePlayTime += dt;

    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // Raw DMG multiplier (mirrors debug overlay logic)
    float rawDmg = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::BloodFrenzy && hpPct < 0.3f) rawDmg += 0.25f;
        if (r.id == RelicID::BerserkerCore) rawDmg += 0.50f;
        if (r.id == RelicID::VoidHunger) rawDmg += relics.voidHungerBonus;
        if (r.id == RelicID::DualityGem && (curDim == 1 || RelicSynergy::isDualNatureActive(relics)))
            rawDmg += 0.25f;
        if (r.id == RelicID::StabilityMatrix) {
            float rate = RelicSynergy::getStabilityDmgPerSec(relics);
            float maxB = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
            rawDmg += std::min(relics.stabilityTimer * rate, maxB);
        }
    }
    rawDmg *= RelicSynergy::getPhaseHunterDamageMult(relics);
    float clampedDmg = RelicSystem::getDamageMultiplier(relics, hpPct, curDim);

    // Raw ATK speed multiplier
    float rawSpd = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) rawSpd += 0.15f;
    }
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0)
        rawSpd += relics.riftConduitStacks * 0.10f;
    float clampedSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    // Track peaks
    m_balanceStats.peakDmgRaw = std::max(m_balanceStats.peakDmgRaw, rawDmg);
    m_balanceStats.peakDmgClamped = std::max(m_balanceStats.peakDmgClamped, clampedDmg);
    m_balanceStats.peakSpdRaw = std::max(m_balanceStats.peakSpdRaw, rawSpd);
    m_balanceStats.peakSpdClamped = std::max(m_balanceStats.peakSpdClamped, clampedSpd);

    // CD floor tracking
    if (gameplayActive && relics.hasRelic(RelicID::RiftMantle)) {
        float baseCD = 0.5f;
        float cdMult = RelicSystem::getSwitchCooldownMult(relics);
        float finalCD = m_dimManager.switchCooldown;
        if ((baseCD * cdMult) < finalCD + 0.001f)
            m_balanceStats.cdFloorTime += dt;
    }

    // Residue zone count
    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
    });
    m_balanceStats.peakResidueZones = std::max(m_balanceStats.peakResidueZones, zoneCount);

    // VoidHunger tracking
    m_balanceStats.peakVoidHunger = std::max(m_balanceStats.peakVoidHunger, relics.voidHungerBonus);
    m_balanceStats.finalVoidHunger = relics.voidHungerBonus;
}

void PlayState::writeBalanceSnapshot() {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    float hpPct = hp.getPercent();
    int curDim = m_dimManager.getCurrentDimension();

    // Raw damage (same calc as overlay)
    float rawDmg = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::BloodFrenzy && hpPct < 0.3f) rawDmg += 0.25f;
        if (r.id == RelicID::BerserkerCore) rawDmg += 0.50f;
        if (r.id == RelicID::VoidHunger) rawDmg += relics.voidHungerBonus;
        if (r.id == RelicID::DualityGem && (curDim == 1 || RelicSynergy::isDualNatureActive(relics)))
            rawDmg += 0.25f;
        if (r.id == RelicID::StabilityMatrix) {
            float rate = RelicSynergy::getStabilityDmgPerSec(relics);
            float maxB = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
            rawDmg += std::min(relics.stabilityTimer * rate, maxB);
        }
    }
    rawDmg *= RelicSynergy::getPhaseHunterDamageMult(relics);
    float clampedDmg = RelicSystem::getDamageMultiplier(relics, hpPct, curDim);

    // Raw attack speed
    float rawSpd = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) rawSpd += 0.15f;
    }
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0)
        rawSpd += relics.riftConduitStacks * 0.10f;
    float clampedSpd = RelicSystem::getAttackSpeedMultiplier(relics);

    float baseCD = 0.5f;
    float cdMult = RelicSystem::getSwitchCooldownMult(relics);
    float finalCD = m_dimManager.switchCooldown;

    int zoneCount = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "dim_residue" && e.isAlive()) zoneCount++;
    });

    float stabRate = RelicSynergy::getStabilityDmgPerSec(relics);
    float stabMax = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
    float stabPct = std::min(relics.stabilityTimer * stabRate, stabMax) * 100.0f;
    int synergyCount = RelicSynergy::getActiveSynergyCount(relics);

    const char* className = "Unknown";
    switch (m_player->playerClass) {
        case PlayerClass::Voidwalker: className = "Voidwalker"; break;
        case PlayerClass::Berserker:  className = "Berserker"; break;
        case PlayerClass::Phantom:    className = "Phantom"; break;
        default: break;
    }

    // Write header if file doesn't exist or is empty
    bool needHeader = false;
    {
        FILE* check = std::fopen("balance_snapshots.csv", "r");
        if (!check) {
            needHeader = true;
        } else {
            std::fseek(check, 0, SEEK_END);
            if (std::ftell(check) == 0) needHeader = true;
            std::fclose(check);
        }
    }

    FILE* f = std::fopen("balance_snapshots.csv", "a");
    if (!f) return;

    if (needHeader) {
        std::fprintf(f, "seed,difficulty,class,time,dim,synergies,"
            "dmg_raw,dmg_clamped,atk_raw,atk_clamped,"
            "switch_base,switch_mult,switch_final,"
            "voidhunger_pct,voidres_icd,"
            "residue_zones,residue_spawn_icd,"
            "stability_timer,stability_pct,"
            "conduit_stacks,conduit_timer\n");
    }

    std::fprintf(f,
        "%d,%d,%s,%.1f,%d,%d,"
        "%.3f,%.3f,%.3f,%.3f,"
        "%.2f,%.2f,%.2f,"
        "%.1f,%.2f,"
        "%d,%.2f,"
        "%.1f,%.1f,"
        "%d,%.1f\n",
        m_runSeed, m_currentDifficulty, className, m_runTime, curDim, synergyCount,
        rawDmg, clampedDmg, rawSpd, clampedSpd,
        baseCD, cdMult, finalCD,
        relics.voidHungerBonus * 100.0f, std::max(0.0f, relics.voidResonanceProcCD),
        zoneCount, std::max(0.0f, relics.dimResidueSpawnCD),
        relics.stabilityTimer, stabPct,
        relics.riftConduitStacks, std::max(0.0f, relics.riftConduitTimer));

    std::fclose(f);
}

void PlayState::showRelicChoice() {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    m_relicChoices = RelicSystem::generateChoice(m_currentDifficulty, relics.relics);
    if (m_relicChoices.empty()) return;
    m_showRelicChoice = true;
    m_relicChoiceSelected = 0;
}

void PlayState::showCursedRelicChoice() {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    m_relicChoices = RelicSystem::generateCursedChoice(m_currentDifficulty, relics.relics);
    if (m_relicChoices.empty()) return; // All cursed relics already owned, skip
    m_showRelicChoice = true;
    m_relicChoiceSelected = 0;
}

void PlayState::selectRelic(int index) {
    if (index < 0 || index >= static_cast<int>(m_relicChoices.size())) return;
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;

    RelicID choice = m_relicChoices[index];
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    relics.addRelic(choice);

    // Apply stat effects
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    RelicSystem::applyStatEffects(relics, *m_player, hp, combat);

    // RiftMantle: reduce dimension switch cooldown (clamped by safety rail)
    m_dimManager.switchCooldown = std::max(0.20f, 0.5f * RelicSystem::getSwitchCooldownMult(relics));

    // TimeDilator: -30% ability cooldowns (applied once on pickup)
    if (choice == RelicID::TimeDilator && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float cdMult = RelicSystem::getAbilityCooldownMultiplier(relics);
        abil.abilities[0].cooldown *= cdMult;
        abil.abilities[1].cooldown *= cdMult;
        abil.abilities[2].cooldown *= cdMult;
    }

    // Visual/audio feedback
    AudioManager::instance().play(SFX::RiftRepair);
    Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto& data = RelicSystem::getRelicData(choice);
    m_particles.burst(pPos, 30, data.glowColor, 200.0f, 4.0f);
    m_particles.burst(pPos, 15, {255, 255, 255, 255}, 150.0f, 3.0f);

    m_showRelicChoice = false;
    m_relicChoices.clear();
}

void PlayState::renderRelicChoice(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_relicChoices.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Dark overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Title
    {
        SDL_Color tc = {255, 215, 100, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "Choose a Relic", tc);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {640 - s->w / 2, 180, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Relic cards
    int cardW = 200;
    int cardH = 260;
    int gap = 30;
    int totalW = static_cast<int>(m_relicChoices.size()) * cardW + (static_cast<int>(m_relicChoices.size()) - 1) * gap;
    int startX = 640 - totalW / 2;
    int cardY = 230;

    for (int i = 0; i < static_cast<int>(m_relicChoices.size()); i++) {
        int cx = startX + i * (cardW + gap);
        bool selected = (i == m_relicChoiceSelected);
        auto& data = RelicSystem::getRelicData(m_relicChoices[i]);

        // Card background
        SDL_SetRenderDrawColor(renderer, 15, 15, 25, 220);
        SDL_Rect cardBg = {cx, cardY, cardW, cardH};
        SDL_RenderFillRect(renderer, &cardBg);

        // Border (glow when selected)
        Uint8 borderA = selected ? 255 : 100;
        SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, borderA);
        SDL_RenderDrawRect(renderer, &cardBg);
        if (selected) {
            SDL_Rect outer = {cx - 2, cardY - 2, cardW + 4, cardH + 4};
            SDL_RenderDrawRect(renderer, &outer);
        }

        // Relic icon (colored orb)
        int orbX = cx + cardW / 2;
        int orbY = cardY + 50;
        int orbR = 24;
        for (int oy = -orbR; oy <= orbR; oy++) {
            for (int ox = -orbR; ox <= orbR; ox++) {
                if (ox * ox + oy * oy <= orbR * orbR) {
                    float dist = std::sqrt(static_cast<float>(ox * ox + oy * oy)) / orbR;
                    Uint8 a = static_cast<Uint8>((1.0f - dist * 0.5f) * 255);
                    SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, a);
                    SDL_RenderDrawPoint(renderer, orbX + ox, orbY + oy);
                }
            }
        }

        // Tier text + CURSED label
        bool isCursedRelic = RelicSystem::isCursed(m_relicChoices[i]);
        const char* tierText = "COMMON";
        SDL_Color tierColor = {180, 180, 180, 255};
        if (data.tier == RelicTier::Rare)      { tierText = "RARE";      tierColor = {80, 180, 255, 255}; }
        else if (data.tier == RelicTier::Legendary) { tierText = "LEGENDARY"; tierColor = {255, 200, 50, 255}; }
        // CURSED override: override tier display with red "CURSED" label
        if (isCursedRelic) { tierText = "CURSED"; tierColor = {255, 50, 50, 255}; }
        {
            SDL_Surface* s = TTF_RenderText_Blended(font, tierText, tierColor);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 90, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
        // Cursed relics: draw a red skull-mark (X lines) in the top-right corner of the card
        if (isCursedRelic) {
            SDL_SetRenderDrawColor(renderer, 200, 20, 20, 200);
            int mx = cx + cardW - 14, my = cardY + 8;
            SDL_RenderDrawLine(renderer, mx, my, mx + 10, my + 10);
            SDL_RenderDrawLine(renderer, mx + 10, my, mx, my + 10);
            SDL_RenderDrawLine(renderer, mx + 1, my, mx + 11, my + 10);
            SDL_RenderDrawLine(renderer, mx + 11, my, mx + 1, my + 10);
            // Dark red card tint overlay
            SDL_SetRenderDrawColor(renderer, 80, 0, 0, 30);
            SDL_RenderFillRect(renderer, &cardBg);
        }

        // Name
        {
            SDL_Color nc = {255, 255, 255, 255};
            SDL_Surface* s = TTF_RenderText_Blended(font, data.name, nc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 120, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Description
        {
            SDL_Color dc = {180, 180, 200, 220};
            SDL_Surface* s = TTF_RenderText_Blended(font, data.description, dc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 160, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Key hint
        char keyBuf[8];
        std::snprintf(keyBuf, sizeof(keyBuf), "[%d]", i + 1);
        {
            SDL_Color kc = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 120, 180};
            SDL_Surface* s = TTF_RenderText_Blended(font, keyBuf, kc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + cardH - 35, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }
}

void PlayState::renderRelicBar(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_player || !m_player->getEntity()->hasComponent<RelicComponent>()) return;
    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
    if (relics.relics.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int iconSize = 20;
    int gap = 4;
    int startX = 15;
    int startY = SCREEN_HEIGHT - 50;

    for (int i = 0; i < static_cast<int>(relics.relics.size()) && i < 12; i++) {
        auto& data = RelicSystem::getRelicData(relics.relics[i].id);
        int ix = startX + i * (iconSize + gap);

        // Background
        SDL_SetRenderDrawColor(renderer, 10, 10, 20, 180);
        SDL_Rect bg = {ix, startY, iconSize, iconSize};
        SDL_RenderFillRect(renderer, &bg);

        // Colored orb
        Uint8 r = data.glowColor.r, g = data.glowColor.g, b = data.glowColor.b;
        if (relics.relics[i].consumed) { r /= 3; g /= 3; b /= 3; }
        SDL_SetRenderDrawColor(renderer, r, g, b, 200);
        SDL_Rect orb = {ix + 3, startY + 3, iconSize - 6, iconSize - 6};
        SDL_RenderFillRect(renderer, &orb);

        // Border: cursed relics get a red border, others get tier-based opacity
        if (RelicSystem::isCursed(relics.relics[i].id)) {
            SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
            SDL_RenderDrawRect(renderer, &bg);
            // Double border for emphasis
            SDL_Rect outerBorder = {ix - 1, startY - 1, iconSize + 2, iconSize + 2};
            SDL_SetRenderDrawColor(renderer, 255, 60, 60, 120);
            SDL_RenderDrawRect(renderer, &outerBorder);
        } else {
            Uint8 borderTierA = 120;
            if (data.tier == RelicTier::Rare) borderTierA = 180;
            if (data.tier == RelicTier::Legendary) borderTierA = 255;
            SDL_SetRenderDrawColor(renderer, r, g, b, borderTierA);
            SDL_RenderDrawRect(renderer, &bg);
        }
    }
}

void PlayState::updateDamageNumbers(float dt) {
    for (auto& dn : m_damageNumbers) {
        dn.lifetime -= dt;
        dn.position.y -= 60.0f * dt; // Float upward
    }
    // Remove expired
    m_damageNumbers.erase(
        std::remove_if(m_damageNumbers.begin(), m_damageNumbers.end(),
            [](const FloatingDamageNumber& d) { return d.lifetime <= 0; }),
        m_damageNumbers.end());
}

void PlayState::updateSpawnWaves(float dt) {
    if (m_currentWave >= static_cast<int>(m_spawnWaves.size())) return;

    // Count alive enemies
    int aliveEnemies = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") != std::string::npos) aliveEnemies++;
    });

    // NoDamageWave challenge: check BEFORE spawning next wave (otherwise new
    // enemies make aliveEnemies > 0 and the later check in updateCombatChallenge fails)
    if (aliveEnemies == 0 && enemiesKilled > 0 && !m_tookDamageThisWave &&
        m_combatChallenge.active && !m_combatChallenge.completed &&
        m_combatChallenge.type == CombatChallengeType::NoDamageWave) {
        m_combatChallenge.currentCount = 1;
    }

    // Spawn next wave when: most enemies dead OR timer expired
    m_waveTimer -= dt;
    if (aliveEnemies <= 1 || m_waveTimer <= 0) {
        // Spawn next wave
        for (auto& sp : m_spawnWaves[m_currentWave]) {
            auto& e = Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
            // Theme-specific variant
            applyThemeVariant(e, sp.dimension);
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && e.getComponent<AIComponent>().element == EnemyElement::None
                && std::rand() % 4 == 0) {
                EnemyElement el = static_cast<EnemyElement>(1 + std::rand() % 3);
                Enemy::applyElement(e, el);
            }
            // Elite modifier in wave spawns
            if (m_currentDifficulty >= 3 && static_cast<EnemyType>(sp.enemyType) != EnemyType::Boss
                && !e.getComponent<AIComponent>().isElite && std::rand() % 100 < 15) {
                EliteModifier mod = static_cast<EliteModifier>(1 + std::rand() % 9);
                Enemy::makeElite(e, mod);
            }
        }
        m_currentWave++;
        m_waveTimer = m_waveDelay;
        m_tookDamageThisWave = false;

        // Start new combat challenge if previous one completed or inactive
        if (!m_combatChallenge.active) {
            startCombatChallenge();
        }

        // Warning SFX for new wave
        AudioManager::instance().play(SFX::CollapseWarning);
    }
}

void PlayState::renderDamageNumbers(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    for (auto& dn : m_damageNumbers) {
        float t = dn.lifetime / dn.maxLifetime; // 1.0 → 0.0
        Uint8 alpha = static_cast<Uint8>(255 * t);

        // Color: green for heals, purple for shards, cyan for buffs, red for player damage, orange for crits, yellow for normal
        SDL_Color color;
        if (dn.isShard) {
            color = {200, 150, 255, alpha};
        } else if (dn.isBuff) {
            color = {100, 220, 255, alpha};
        } else if (dn.isHeal) {
            color = {50, 255, 80, alpha};
        } else if (dn.isPlayerDamage) {
            color = {255, 60, 40, alpha};
        } else if (dn.isCritical) {
            color = {255, 180, 40, alpha};
        } else {
            color = {255, 240, 100, alpha};
        }

        char buf[32];
        if (dn.isShard) {
            std::snprintf(buf, sizeof(buf), "+%.0f", dn.value);
        } else if (dn.isBuff && dn.buffText) {
            std::snprintf(buf, sizeof(buf), "%s", dn.buffText);
        } else if (dn.isHeal) {
            std::snprintf(buf, sizeof(buf), "+%.0f", dn.value);
        } else if (dn.isCritical) {
            std::snprintf(buf, sizeof(buf), "CRIT! %.0f", dn.value);
        } else {
            std::snprintf(buf, sizeof(buf), "%.0f", dn.value);
        }

        // Convert world position to screen position
        SDL_FRect worldRect = {dn.position.x - 10, dn.position.y - 8, 20, 16};
        SDL_Rect screenRect = m_camera.worldToScreen(worldRect);

        SDL_Surface* surface = TTF_RenderText_Blended(font, buf, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                // Scale up for crits and large damage
                float scale = 1.0f;
                if (dn.isCritical) {
                    scale = 1.5f;
                } else if (dn.value > 20) {
                    scale = 1.3f;
                }
                SDL_Rect dst = {screenRect.x - static_cast<int>(surface->w * scale) / 2,
                               screenRect.y,
                               static_cast<int>(surface->w * scale),
                               static_cast<int>(surface->h * scale)};
                SDL_SetTextureAlphaMod(texture, alpha);
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void PlayState::renderKeyBox(SDL_Renderer* renderer, TTF_Font* font,
                              const char* key, int x, int y, Uint8 alpha) {
    // Render a key label as a bordered box
    SDL_Surface* surface = TTF_RenderText_Blended(font, key, {255, 255, 255, alpha});
    if (!surface) return;
    int pad = 4;
    SDL_Rect box = {x - pad, y - 2, surface->w + pad * 2, surface->h + 4};
    SDL_SetRenderDrawColor(renderer, 40, 50, 80, static_cast<Uint8>(alpha * 0.8f));
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 120, 160, 220, alpha);
    SDL_RenderDrawRect(renderer, &box);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void PlayState::renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Context-based hint system: show hints when conditions are met
    const char* hint = nullptr;
    const char* keyLabel = nullptr;
    int hintSlot = -1; // which slot to mark done
    bool conditionMet = false;

    // === LEVEL 1: Basic controls (sequential) ===
    if (m_currentDifficulty == 1) {
        if (!m_tutorialHintDone[0]) {
            if (m_tutorialTimer > 1.5f && !m_hasMovedThisRun) {
                hint = "Move";
                keyLabel = "WASD";
            }
            if (m_hasMovedThisRun) { conditionMet = true; hintSlot = 0; }
        } else if (!m_tutorialHintDone[1]) {
            hint = "Jump";
            keyLabel = "SPACE";
            if (m_hasJumpedThisRun) { conditionMet = true; hintSlot = 1; }
        } else if (!m_tutorialHintDone[2]) {
            hint = "Dash through danger (also in air!)";
            keyLabel = "SHIFT";
            if (m_hasDashedThisRun) { conditionMet = true; hintSlot = 2; }
        } else if (!m_tutorialHintDone[3]) {
            bool enemyNear = false;
            if (m_player) {
                Vec2 pp = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_entities.forEach([&](Entity& e) {
                    if (e.getTag().find("enemy") != std::string::npos &&
                        e.hasComponent<TransformComponent>()) {
                        Vec2 ep = e.getComponent<TransformComponent>().getCenter();
                        float d = std::sqrt((pp.x-ep.x)*(pp.x-ep.x) + (pp.y-ep.y)*(pp.y-ep.y));
                        if (d < 250.0f) enemyNear = true;
                    }
                });
            }
            if (enemyNear) {
                hint = "Melee Attack";
                keyLabel = "J";
            }
            if (m_hasAttackedThisRun) { conditionMet = true; hintSlot = 3; }
        } else if (!m_tutorialHintDone[4]) {
            hint = "Ranged Attack (hold for charged shot!)";
            keyLabel = "K";
            if (m_hasRangedThisRun) { conditionMet = true; hintSlot = 4; }
        } else if (!m_tutorialHintDone[5]) {
            hint = "Switch Dimension - paths differ between worlds!";
            keyLabel = "E";
            if (m_dimManager.getSwitchCount() > 0) { conditionMet = true; hintSlot = 5; }
        }
    }

    // === CONTEXT-BASED HINTS (any level, triggered by situation) ===
    if (!hint) {
        if (!m_tutorialHintDone[16] && m_nearRiftIndex >= 0 && m_level) {
            int requiredDim = m_level->getRiftRequiredDimension(m_nearRiftIndex);
            int currentDim = m_dimManager.getCurrentDimension();
            if (requiredDim > 0 && requiredDim != currentDim) {
                hint = (requiredDim == 2)
                    ? "This Rift only stabilizes in DIM-B. Shift with [E], but entropy rises faster there."
                    : "This Rift only stabilizes in DIM-A. Shift with [E] to secure it safely.";
                keyLabel = "E";
                hintSlot = 16;
            }
        }
        // Near a rift: explain how to repair
        if (!hint && !m_tutorialHintDone[6] && m_nearRiftIndex >= 0) {
            hint = "Repair this Rift! Solve the puzzle to reduce Entropy.";
            keyLabel = "F";
            if (riftsRepaired > 0) { conditionMet = true; hintSlot = 6; }
        }
        // Entropy getting high
        else if (!m_tutorialHintDone[7] && !m_shownEntropyWarning && m_entropy.getPercent() > 0.5f) {
            hint = "Entropy rising! Repair Rifts to lower it. At 100% your suit fails!";
            keyLabel = nullptr;
            m_shownEntropyWarning = true;
            m_tutorialHintShowTimer = 0;
            hintSlot = 7;
            // Auto-dismiss after 6s
            if (m_tutorialHintShowTimer > 6.0f) conditionMet = true;
        }
        // On/near a conveyor
        else if (!m_tutorialHintDone[8] && !m_shownConveyorHint && m_player) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            int ts = m_level ? m_level->getTileSize() : 32;
            int fx = static_cast<int>(pt.getCenter().x) / ts;
            int fy = static_cast<int>(pt.position.y + pt.height) / ts;
            int dim = m_dimManager.getCurrentDimension();
            int cdir = 0;
            if (m_level && (m_level->isOnConveyor(fx, fy, dim, cdir) || m_level->isOnConveyor(fx, fy+1, dim, cdir))) {
                hint = "Conveyor Belt - pushes you along. Use it for speed!";
                m_shownConveyorHint = true;
                hintSlot = 8;
                m_tutorialHintShowTimer = 0;
            }
        }
        // Near a dimension-exclusive platform
        else if (!m_tutorialHintDone[9] && !m_shownDimPlatformHint && m_player && m_level) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            int ts = m_level->getTileSize();
            int px = static_cast<int>(pt.getCenter().x) / ts;
            int py = static_cast<int>(pt.getCenter().y) / ts;
            int dim = m_dimManager.getCurrentDimension();
            // Check nearby tiles for dim-exclusive platforms
            for (int dy = -2; dy <= 2 && !hint; dy++) {
                for (int dx = -3; dx <= 3 && !hint; dx++) {
                    if (m_level->isDimensionExclusive(px+dx, py+dy, dim)) {
                        hint = "Glowing platform - only exists in one dimension! Switch with [E]";
                        m_shownDimPlatformHint = true;
                        hintSlot = 9;
                        m_tutorialHintShowTimer = 0;
                    }
                }
            }
        }
        // Show wall slide hint if next to wall in air
        else if (!m_tutorialHintDone[10] && !m_shownWallSlideHint && m_player) {
            auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
            if ((phys.onWallLeft || phys.onWallRight) && !phys.onGround) {
                hint = "Wall Slide! Jump off walls to reach higher areas.";
                keyLabel = "SPACE";
                m_shownWallSlideHint = true;
                hintSlot = 10;
                m_tutorialHintShowTimer = 0;
            }
        }
        // Show abilities hint (level 1, after combat basics done)
        else if (!m_tutorialHintDone[11] && m_hasAttackedThisRun && !m_hasUsedAbilityThisRun
                 && m_tutorialTimer > 30.0f) {
            hint = "Use special abilities: Slam / Shield / Phase Strike";
            keyLabel = "1 2 3";
            if (m_hasUsedAbilityThisRun) { conditionMet = true; hintSlot = 11; }
        }
        // Relic choice hint
        else if (!m_tutorialHintDone[12] && !m_shownRelicHint && m_showRelicChoice) {
            hint = "Choose a Relic! Each gives a unique passive bonus for this run.";
            m_shownRelicHint = true;
            hintSlot = 12;
            m_tutorialHintShowTimer = 0;
        }
        // Combo hint when first combo reaches 3
        else if (!m_tutorialHintDone[13] && m_player &&
                 m_player->getEntity()->hasComponent<CombatComponent>()) {
            auto& cb = m_player->getEntity()->getComponent<CombatComponent>();
            if (cb.comboCount >= 3) {
                hint = "Combo x3! Chain hits without pause for bonus damage!";
                conditionMet = true;
                hintSlot = 13;
            }
        }
        // Exit hint when all rifts repaired
        else if (!m_tutorialHintDone[14] && m_levelComplete && !m_collapsing) {
            hint = "All Rifts repaired! Find the EXIT before the dimension collapses!";
            conditionMet = false;
            hintSlot = 14;
            m_tutorialHintShowTimer = 0;
        }
        // Weapon switch hint after a few levels
        else if (!m_tutorialHintDone[15] && m_currentDifficulty >= 2 && m_tutorialTimer > 10.0f
                 && m_tutorialHintDone[3]) {
            hint = "Switch weapons: [Q] Melee  [R] Ranged - try different styles!";
            hintSlot = 15;
            m_tutorialHintShowTimer = 0;
        }
    }

    // Auto-dismiss timed hints after showing
    if (hint && hintSlot >= 7 && !conditionMet) {
        if (m_tutorialHintShowTimer > 5.0f) {
            conditionMet = true;
        }
    }

    // Mark completed hints
    if (conditionMet && hintSlot >= 0 && hintSlot < 20) {
        m_tutorialHintDone[hintSlot] = true;
        m_tutorialHintShowTimer = 0;
    }

    if (!hint) return;

    // Fade in over 0.3s
    m_tutorialHintShowTimer += 1.0f / 60.0f;
    float alpha = std::min(1.0f, m_tutorialHintShowTimer / 0.3f);
    Uint8 a = static_cast<Uint8>(alpha * 200);

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(a * 0.4f));
    SDL_Rect bg = {240, 140, 800, 36};
    SDL_RenderFillRect(renderer, &bg);

    if (keyLabel) {
        SDL_Surface* hintSurf = TTF_RenderText_Blended(font, hint, {180, 220, 255, a});
        SDL_Surface* keySurf = TTF_RenderText_Blended(font, keyLabel, {255, 255, 255, 255});
        int totalW = (keySurf ? keySurf->w + 16 : 0) + (hintSurf ? hintSurf->w : 0);
        int startX = 640 - totalW / 2;

        if (keySurf) {
            renderKeyBox(renderer, font, keyLabel, startX, 148, a);
            startX += keySurf->w + 16;
            SDL_FreeSurface(keySurf);
        }
        if (hintSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, hintSurf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect dst = {startX, 148, hintSurf->w, hintSurf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(hintSurf);
        }
    } else {
        SDL_Color color = {180, 220, 255, a};
        SDL_Surface* surface = TTF_RenderText_Blended(font, hint, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, a);
                SDL_Rect dst = {640 - surface->w / 2, 148, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void PlayState::spawnBoss() {
    if (!m_level) return;

    // Spawn boss near the center of the level
    Vec2 exitPos = m_level->getExitPoint();
    Vec2 spawnPos = {exitPos.x - 200.0f, exitPos.y - 64.0f};

    int dim = m_dimManager.getCurrentDimension();
    int bossDiff = m_currentDifficulty;
    if (g_selectedDifficulty == GameDifficulty::Hard) bossDiff += 1;

    // Boss selection based on difficulty
    int bossIndex = m_currentDifficulty / 3; // 0-based boss encounter index
    if (m_currentDifficulty >= 11 && bossIndex >= 5) {
        // Entropy Incarnate at difficulty 11+ (after Void Sovereign rotation)
        Enemy::createEntropyIncarnate(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
        m_screenEffects.triggerBossIntro("ENTROPY INCARNATE");
    } else if (m_currentDifficulty >= 10 && bossIndex >= 4) {
        // Spawn Void Sovereign as final boss at difficulty 10+
        Enemy::createVoidSovereign(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
        m_screenEffects.triggerBossIntro("VOID SOVEREIGN");
    } else {
        // Rotate 4 boss types: Guardian -> Wyrm -> Architect -> Temporal Weaver
        int bossType = (bossIndex - 1) % 4;
        if (bossType == 3) {
            Enemy::createTemporalWeaver(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
            m_screenEffects.triggerBossIntro("TEMPORAL WEAVER");
        } else if (bossType == 2) {
            Enemy::createDimensionalArchitect(m_entities, {spawnPos.x, spawnPos.y - 48.0f}, dim, bossDiff);
            m_screenEffects.triggerBossIntro("DIMENSIONAL ARCHITECT");
        } else if (bossType == 1) {
            Enemy::createVoidWyrm(m_entities, {spawnPos.x, spawnPos.y - 40.0f}, dim, bossDiff);
            m_screenEffects.triggerBossIntro("VOID WYRM");
        } else {
            Enemy::createBoss(m_entities, spawnPos, dim, bossDiff);
            m_screenEffects.triggerBossIntro("RIFT GUARDIAN");
        }
    }
}

void PlayState::renderBossHealthBar(SDL_Renderer* renderer, TTF_Font* font) {
    // Find boss entity
    Entity* boss = nullptr;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "enemy_boss") boss = &e;
    });
    if (!boss || !boss->hasComponent<HealthComponent>()) return;

    auto& hp = boss->getComponent<HealthComponent>();
    int bossPhase = 1;
    if (boss->hasComponent<AIComponent>()) {
        bossPhase = boss->getComponent<AIComponent>().bossPhase;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Bar background
    int barW = 400;
    int barH = 12;
    int barX = 640 - barW / 2;
    int barY = 20;

    // Dark frame
    SDL_SetRenderDrawColor(renderer, 20, 10, 30, 220);
    SDL_Rect frame = {barX - 2, barY - 2, barW + 4, barH + 4};
    SDL_RenderFillRect(renderer, &frame);

    // Background
    SDL_SetRenderDrawColor(renderer, 50, 25, 50, 200);
    SDL_Rect bg = {barX, barY, barW, barH};
    SDL_RenderFillRect(renderer, &bg);

    // HP fill (color changes per phase and boss type)
    float pct = hp.getPercent();
    int fillW = static_cast<int>(barW * pct);
    int bt = 0;
    if (boss->hasComponent<AIComponent>()) bt = boss->getComponent<AIComponent>().bossType;
    Uint8 r, g, b;
    if (bt == 4) {
        // Void Sovereign: dark purple/magenta tones
        switch (bossPhase) {
            case 1: r = 120; g = 0; b = 180; break;
            case 2: r = 180; g = 0; b = 150; break;
            case 3: r = 255; g = 0; b = 120; break;
            default: r = 120; g = 0; b = 180; break;
        }
    } else if (bt == 3) {
        // Temporal Weaver: golden/amber tones
        switch (bossPhase) {
            case 1: r = 200; g = 170; b = 60; break;
            case 2: r = 230; g = 180; b = 40; break;
            case 3: r = 255; g = 200; b = 30; break;
            default: r = 200; g = 170; b = 60; break;
        }
    } else if (bt == 2) {
        // Dimensional Architect: blue/purple tones
        switch (bossPhase) {
            case 1: r = 80; g = 140; b = 255; break;
            case 2: r = 140; g = 100; b = 255; break;
            case 3: r = 200; g = 60; b = 255; break;
            default: r = 80; g = 140; b = 255; break;
        }
    } else if (bt == 1) {
        // Void Wyrm: green tones
        switch (bossPhase) {
            case 1: r = 40; g = 180; b = 120; break;
            case 2: r = 60; g = 220; b = 80; break;
            case 3: r = 120; g = 255; b = 40; break;
            default: r = 40; g = 180; b = 120; break;
        }
    } else {
        // Rift Guardian: purple/orange/red tones
        switch (bossPhase) {
            case 1: r = 200; g = 50; b = 180; break;
            case 2: r = 220; g = 120; b = 40; break;
            case 3: r = 255; g = 40; b = 40; break;
            default: r = 200; g = 50; b = 180; break;
        }
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, 240);
    SDL_Rect fill = {barX, barY, fillW, barH};
    SDL_RenderFillRect(renderer, &fill);

    // Phase markers at 66% and 33%
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
    SDL_RenderDrawLine(renderer, barX + barW * 2 / 3, barY, barX + barW * 2 / 3, barY + barH);
    SDL_RenderDrawLine(renderer, barX + barW / 3, barY, barX + barW / 3, barY + barH);

    // Boss name (dynamic based on boss type)
    if (font) {
        int bt = 0;
        if (boss->hasComponent<AIComponent>()) bt = boss->getComponent<AIComponent>().bossType;
        const char* bossName = (bt == 4) ? "VOID SOVEREIGN" : (bt == 3) ? "TEMPORAL WEAVER" : (bt == 2) ? "DIMENSIONAL ARCHITECT" : (bt == 1) ? "VOID WYRM" : "RIFT GUARDIAN";
        SDL_Color tc = (bt == 4) ? SDL_Color{180, 80, 255, 220} : (bt == 3) ? SDL_Color{220, 190, 100, 220} : (bt == 2) ? SDL_Color{160, 180, 255, 220} : (bt == 1) ? SDL_Color{180, 255, 200, 220} : SDL_Color{220, 180, 255, 220};
        SDL_Surface* ts = TTF_RenderText_Blended(font, bossName, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {640 - ts->w / 2, barY + barH + 4, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }
}

void PlayState::applyRunBuffs() {
    if (!m_player) return;
    auto& buffs = game->getRunBuffSystem();

    // HP boost
    if (buffs.hasBuff(RunBuffID::MaxHPBoost)) {
        auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
        hp.maxHP += buffs.getMaxHPBoost();
        hp.currentHP = hp.maxHP;
    }

    // Crit bonus
    float extraCrit = buffs.getCritBonus();
    if (extraCrit > 0) {
        m_combatSystem.setCritChance(game->getUpgradeSystem().getCritChance() + extraCrit);
    }

    // Ability cooldown reduction
    if (buffs.hasBuff(RunBuffID::CooldownReduction) && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        float mult = buffs.getAbilityCDMultiplier();
        abil.abilities[0].cooldown *= mult;
        abil.abilities[1].cooldown *= mult;
        abil.abilities[2].cooldown *= mult;
    }

    // Lifesteal
    m_combatSystem.setLifesteal(buffs.getLifesteal());

    // Element weapon (0=none, 1=fire, 2=ice, 3=electric)
    int elemType = 0;
    if (buffs.hasFireWeapon()) elemType = 1;
    else if (buffs.hasIceWeapon()) elemType = 2;
    else if (buffs.hasElectricWeapon()) elemType = 3;
    m_combatSystem.setElementWeapon(elemType);

    // Dash refresh on kill
    m_combatSystem.setDashRefreshOnKill(buffs.hasDashRefresh());

    // Entropy shield: reduce entropy gain
    m_entropy.entropyGainMultiplier = buffs.getEntropyGainMultiplier();
}

void PlayState::checkBreakableWalls() {
    if (!m_player || !m_level) return;

    // Only break walls during dash or charged attack
    bool canBreak = m_player->isDashing || m_player->isChargingAttack;
    if (!canBreak) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    int tileSize = m_level->getTileSize();
    int currentDim = m_dimManager.getCurrentDimension();

    // Check tiles around player
    int cx = static_cast<int>(pt.getCenter().x) / tileSize;
    int cy = static_cast<int>(pt.getCenter().y) / tileSize;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int tx = cx + dx;
            int ty = cy + dy;
            if (!m_level->inBounds(tx, ty)) continue;

            auto& tile = m_level->getTile(tx, ty, currentDim);
            if (tile.type == TileType::Breakable) {
                // Destroy the breakable wall
                Tile empty;
                empty.type = TileType::Empty;
                m_level->setTile(tx, ty, currentDim, empty);
                // Also break in other dimension
                int otherDim = (currentDim == 1) ? 2 : 1;
                auto& otherTile = m_level->getTile(tx, ty, otherDim);
                if (otherTile.type == TileType::Breakable) {
                    m_level->setTile(tx, ty, otherDim, empty);
                }

                AudioManager::instance().play(SFX::BreakableWall);
                m_camera.shake(6.0f, 0.2f);

                // Debris particles
                Vec2 breakPos = {static_cast<float>(tx * tileSize + tileSize / 2),
                                 static_cast<float>(ty * tileSize + tileSize / 2)};
                m_particles.burst(breakPos, 20, tile.color, 200.0f, 3.0f);
                m_particles.burst(breakPos, 10, {200, 180, 160, 255}, 150.0f, 2.0f);
            }
        }
    }
}

void PlayState::checkSecretRoomDiscovery() {
    if (!m_player || !m_level) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    int tileSize = m_level->getTileSize();
    int px = static_cast<int>(pt.getCenter().x) / tileSize;
    int py = static_cast<int>(pt.getCenter().y) / tileSize;

    for (auto& sr : m_level->getSecretRooms()) {
        if (sr.discovered) continue;

        // Check if player is inside the secret room bounds
        if (px >= sr.tileX && px < sr.tileX + sr.width &&
            py >= sr.tileY && py < sr.tileY + sr.height) {
            sr.discovered = true;
            AudioManager::instance().play(SFX::SecretRoomDiscover);
            // Lore: The Rift - discovered via secret room
            if (auto* lore = game->getLoreSystem()) {
                if (!lore->isDiscovered(LoreID::TheRift)) {
                    lore->discover(LoreID::TheRift);
                    AudioManager::instance().play(SFX::LoreDiscover);
                }
            }
            m_camera.shake(4.0f, 0.15f);

            Vec2 roomCenter = {
                static_cast<float>((sr.tileX + sr.width / 2) * tileSize),
                static_cast<float>((sr.tileY + sr.height / 2) * tileSize)
            };
            m_particles.burst(roomCenter, 30, {255, 215, 60, 255}, 200.0f, 4.0f);
            m_particles.burst(roomCenter, 15, {180, 150, 255, 255}, 150.0f, 3.0f);

            // Apply rewards based on type
            if (!sr.completed) {
                sr.completed = true;
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();

                switch (sr.type) {
                    case SecretRoomType::TreasureVault: {
                        int shards = sr.shardReward;
                        shards = static_cast<int>(shards * game->getRunBuffSystem().getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        hp.heal(static_cast<float>(sr.hpReward));
                        break;
                    }
                    case SecretRoomType::ShrineRoom: {
                        // Secret shrine: guaranteed blessing (reward for discovery)
                        AudioManager::instance().play(SFX::ShrineActivate);
                        AudioManager::instance().play(SFX::ShrineBlessing);
                        hp.maxHP += 20;
                        hp.heal(20.0f);
                        m_player->damageBoostTimer = 30.0f;
                        m_player->damageBoostMultiplier = 1.2f;
                        m_particles.burst(roomCenter, 25, {180, 120, 255, 255}, 180.0f, 3.0f);
                        m_camera.shake(0.3f, 4.0f);
                        break;
                    }
                    case SecretRoomType::DimensionCache: {
                        // Dimension Cache: shards + guaranteed cursed relic offer
                        int shards = sr.shardReward; // Normal reward (not doubled — the relic is the bonus)
                        shards = static_cast<int>(shards * game->getRunBuffSystem().getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        // Offer a cursed relic from the forbidden vault
                        showCursedRelicChoice();
                        break;
                    }
                    case SecretRoomType::AncientWeapon:
                        // Temporary damage boost
                        m_player->damageBoostTimer = 30.0f; // 30 seconds
                        m_player->damageBoostMultiplier = 2.0f;
                        break;
                    case SecretRoomType::ChallengeRoom:
                        // Reward given after clearing enemies (checked by wave system)
                        break;
                }
            }
        }
    }
}

void PlayState::updateSecretRoomHints(float dt) {
    if (!m_player || !m_level) return;

    m_secretHintTimer += dt;
    if (m_secretHintTimer < 0.4f) return; // Emit every 0.4s
    m_secretHintTimer = 0;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 pPos = pt.getCenter();
    int tileSize = m_level->getTileSize();
    float hintRadius = 6.0f * tileSize; // 6 tiles detection range

    for (const auto& sr : m_level->getSecretRooms()) {
        if (sr.discovered) continue;

        // Check if breakable wall still exists (not already broken)
        int currentDim = m_dimManager.getCurrentDimension();
        if (!m_level->inBounds(sr.entranceX, sr.entranceY)) continue;
        const auto& tile = m_level->getTile(sr.entranceX, sr.entranceY, currentDim);
        if (tile.type != TileType::Breakable) continue;

        // Entrance world position
        Vec2 entrancePos = {
            static_cast<float>(sr.entranceX * tileSize + tileSize / 2),
            static_cast<float>(sr.entranceY * tileSize + tileSize / 2)
        };

        float dist = std::sqrt((pPos.x - entrancePos.x) * (pPos.x - entrancePos.x) +
                                (pPos.y - entrancePos.y) * (pPos.y - entrancePos.y));
        if (dist > hintRadius) continue;

        // Intensity increases as player gets closer (0.0 at edge, 1.0 at wall)
        float proximity = 1.0f - (dist / hintRadius);

        // Emit subtle dust/sparkle particles from the breakable wall
        int particleCount = 1 + static_cast<int>(proximity * 3); // 1-4 particles
        SDL_Color hintColor;
        switch (sr.type) {
            case SecretRoomType::TreasureVault:
                hintColor = {255, 215, 60, static_cast<Uint8>(40 + proximity * 60)};  // Gold
                break;
            case SecretRoomType::ChallengeRoom:
                hintColor = {255, 80, 80, static_cast<Uint8>(40 + proximity * 60)};   // Red
                break;
            case SecretRoomType::ShrineRoom:
                hintColor = {180, 120, 255, static_cast<Uint8>(40 + proximity * 60)}; // Purple
                break;
            case SecretRoomType::DimensionCache:
                hintColor = {100, 200, 255, static_cast<Uint8>(40 + proximity * 60)}; // Cyan
                break;
            case SecretRoomType::AncientWeapon:
                hintColor = {255, 180, 60, static_cast<Uint8>(40 + proximity * 60)};  // Orange
                break;
        }

        // Floating motes rising from the wall
        for (int i = 0; i < particleCount; i++) {
            Vec2 spawnPos = {
                entrancePos.x + (std::rand() % 20 - 10),
                entrancePos.y + (std::rand() % 10 - 5)
            };
            m_particles.burst(spawnPos, 1, hintColor, 20.0f + proximity * 30.0f, 1.5f + proximity);
        }
    }
}

void PlayState::checkEventInteraction() {
    if (!m_player || !m_level) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 playerCenter = pt.getCenter();
    int currentDim = m_dimManager.getCurrentDimension();
    const auto& input = game->getInput();

    m_nearEventIndex = -1;

    auto& events = m_level->getRandomEvents();
    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        float dx = playerCenter.x - event.position.x;
        float dy = playerCenter.y - event.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 60.0f) {
            m_nearEventIndex = i;

            // Interact with Up/W
            if (input.isActionPressed(Action::Jump) || input.isActionPressed(Action::MoveUp)) {
                // Not jump - use a dedicated check. Just use E key via event
            }

            // Interact with Enter or E (check raw key)
            if (input.isActionPressed(Action::Interact)) {
                event.used = true;
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                auto& buffs = game->getRunBuffSystem();

                switch (event.type) {
                    case RandomEventType::Merchant:
                        AudioManager::instance().play(SFX::MerchantGreet);
                        // Buy a random available buff (cheaper)
                        {
                            auto offerings = buffs.generateShopOffering(m_currentDifficulty);
                            if (!offerings.empty()) {
                                auto& pick = offerings[0];
                                int shards = game->getUpgradeSystem().getRiftShards();
                                int discountCost = pick.cost * 3 / 4; // 25% cheaper
                                if (shards >= discountCost) {
                                    game->getUpgradeSystem().addRiftShards(-discountCost);
                                    // Bypass purchase() shard deduction - pass dummy with enough
                                    int dummy = pick.cost;
                                    buffs.purchase(pick.id, dummy);
                                    AudioManager::instance().play(SFX::Pickup);
                                    m_particles.burst(event.position, 15, {255, 215, 60, 255}, 120.0f, 2.0f);
                                } else {
                                    AudioManager::instance().play(SFX::RiftFail);
                                    event.used = false; // Can try again
                                }
                            }
                        }
                        break;

                    case RandomEventType::Shrine: {
                        AudioManager::instance().play(SFX::ShrineActivate);
                        SDL_Color sc = event.getShrineColor();
                        switch (event.shrineType) {
                            case ShrineType::Power:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                m_player->damageBoostTimer = 60.0f;
                                m_player->damageBoostMultiplier = 1.3f;
                                hp.maxHP = std::max(1.0f, hp.maxHP - 15);
                                hp.currentHP = std::min(hp.currentHP, hp.maxHP);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Vitality:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                hp.maxHP += 25;
                                hp.heal(25.0f);
                                m_entropy.addEntropy(8.0f);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Speed:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                m_player->speedBoostTimer = 45.0f;
                                m_player->speedBoostMultiplier = 1.25f;
                                hp.maxHP = std::max(1.0f, hp.maxHP - 10);
                                hp.currentHP = std::min(hp.currentHP, hp.maxHP);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Entropy:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                m_entropy.reduceEntropy(25.0f);
                                hp.currentHP = std::max(1.0f, hp.currentHP - 15);
                                m_particles.burst(event.position, 20, sc, 150.0f, 3.0f);
                                break;
                            case ShrineType::Shards: {
                                AudioManager::instance().play(SFX::Pickup);
                                int shards = event.reward > 0 ? event.reward : 40;
                                shards = static_cast<int>(shards * buffs.getShardMultiplier());
                                shardsCollected += shards;
                                game->getUpgradeSystem().addRiftShards(shards);
                                m_entropy.addEntropy(12.0f);
                                m_particles.burst(event.position, 25, sc, 180.0f, 3.0f);
                                break;
                            }
                            case ShrineType::Renewal:
                                AudioManager::instance().play(SFX::ShrineBlessing);
                                hp.heal(hp.maxHP);
                                m_entropy.addEntropy(5.0f);
                                m_particles.burst(event.position, 25, sc, 150.0f, 3.0f);
                                break;
                            default:
                                break;
                        }
                        m_camera.shake(0.3f, 4.0f);
                        break;
                    }

                    case RandomEventType::DimensionalAnomaly:
                        AudioManager::instance().play(SFX::AnomalySpawn);
                        // Spawn bonus enemies directly (don't re-spawn all)
                        for (int e = 0; e < 3 + m_currentDifficulty; e++) {
                            float sx = event.position.x + (std::rand() % 200 - 100);
                            float sy = event.position.y - 50 - (std::rand() % 100);
                            int etype = std::rand() % std::min(10, 3 + m_currentDifficulty);
                            Enemy::createByType(m_entities, etype, {sx, sy}, currentDim);
                        }
                        m_particles.burst(event.position, 25, {180, 100, 255, 255}, 200.0f, 4.0f);
                        break;

                    case RandomEventType::RiftEcho: {
                        int shards = event.reward;
                        shards = static_cast<int>(shards * buffs.getShardMultiplier());
                        shardsCollected += shards;
                        game->getUpgradeSystem().addRiftShards(shards);
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(event.position, 20, {200, 150, 255, 255}, 180.0f, 3.0f);
                        break;
                    }

                    case RandomEventType::SuitRepairStation:
                        hp.heal(hp.maxHP);
                        m_entropy.reduceEntropy(30.0f);
                        AudioManager::instance().play(SFX::RiftRepair);
                        m_particles.burst(event.position, 25, {100, 255, 100, 255}, 150.0f, 3.0f);
                        break;

                    case RandomEventType::GamblingRift: {
                        int cost = event.cost;
                        int shards = game->getUpgradeSystem().getRiftShards();
                        if (shards >= cost) {
                            game->getUpgradeSystem().addRiftShards(-cost);
                            // Random reward: 0x (40%), 2x (35%), 4x (25%)
                            int roll = std::rand() % 100;
                            int reward = 0;
                            if (roll < 40) {
                                reward = 0;
                                AudioManager::instance().play(SFX::RiftFail);
                                m_particles.burst(event.position, 10, {100, 100, 100, 255}, 80.0f, 2.0f);
                            } else if (roll < 75) {
                                reward = cost * 2;
                                AudioManager::instance().play(SFX::Pickup);
                                m_particles.burst(event.position, 15, {200, 170, 255, 255}, 120.0f, 3.0f);
                            } else {
                                reward = cost * 4;
                                AudioManager::instance().play(SFX::LevelComplete);
                                m_particles.burst(event.position, 30, {255, 215, 60, 255}, 200.0f, 4.0f);
                            }
                            reward = static_cast<int>(reward * buffs.getShardMultiplier());
                            shardsCollected += reward;
                            game->getUpgradeSystem().addRiftShards(reward);
                        } else {
                            AudioManager::instance().play(SFX::RiftFail);
                            event.used = false;
                        }
                        break;
                    }
                }
            }
            break; // Only interact with nearest event
        }
    }
}

void PlayState::renderRandomEvents(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& events = m_level->getRandomEvents();

    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        SDL_FRect worldRect = {event.position.x - 16, event.position.y - 32, 32, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        // Skip if off screen
        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 2.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC body
        SDL_Color npcColor;
        switch (event.type) {
            case RandomEventType::Merchant:           npcColor = {80, 200, 80, 255}; break;
            case RandomEventType::Shrine:             npcColor = event.getShrineColor(); break;
            case RandomEventType::DimensionalAnomaly: npcColor = {200, 50, 200, 255}; break;
            case RandomEventType::RiftEcho:           npcColor = {150, 150, 255, 255}; break;
            case RandomEventType::SuitRepairStation:  npcColor = {100, 255, 150, 255}; break;
            case RandomEventType::GamblingRift:       npcColor = {255, 200, 50, 255}; break;
        }

        // Glow
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i);
        Uint8 ga = static_cast<Uint8>(40 * glow);
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, ga);
        SDL_Rect glowRect = {sr.x - 6, sr.y - 6, sr.w + 12, sr.h + 12};
        SDL_RenderFillRect(renderer, &glowRect);

        // Body
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 220);
        SDL_RenderFillRect(renderer, &sr);

        // Eye
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        SDL_Rect eye = {sr.x + sr.w / 2 - 2, sr.y + 8, 4, 4};
        SDL_RenderFillRect(renderer, &eye);

        // Icon on top based on type
        int iconY = sr.y - 6;
        int iconX = sr.x + sr.w / 2;
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 180);
        switch (event.type) {
            case RandomEventType::Merchant:
                // Coin
                SDL_RenderDrawLine(renderer, iconX - 4, iconY, iconX + 4, iconY);
                SDL_RenderDrawLine(renderer, iconX, iconY - 4, iconX, iconY + 4);
                break;
            case RandomEventType::Shrine:
                // Diamond
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX + 5, iconY);
                SDL_RenderDrawLine(renderer, iconX + 5, iconY, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX, iconY + 5, iconX - 5, iconY);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX, iconY - 5);
                break;
            case RandomEventType::DimensionalAnomaly: {
                // Exclamation
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 1);
                SDL_Rect dot = {iconX - 1, iconY + 3, 2, 2};
                SDL_RenderFillRect(renderer, &dot);
                break;
            }
            default:
                // Star
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX + 5, iconY);
                break;
        }

        // Interaction hint when near
        if (i == m_nearEventIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", event.getName());
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            int nameH = 0;
            if (hs) {
                nameH = hs->h;
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    int offy = (event.type == RandomEventType::Shrine) ? -36 : -24;
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y + offy, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }

            // Shrine: show effect preview below the name
            if (event.type == RandomEventType::Shrine) {
                SDL_Color ec = event.getShrineColor();
                ec.a = static_cast<Uint8>(180 * blink);
                SDL_Surface* es = TTF_RenderText_Blended(font, event.getShrineEffect(), ec);
                if (es) {
                    SDL_Texture* et = SDL_CreateTextureFromSurface(renderer, es);
                    if (et) {
                        SDL_Rect er = {sr.x + sr.w / 2 - es->w / 2, sr.y - 36 + nameH + 2, es->w, es->h};
                        SDL_RenderCopy(renderer, et, nullptr, &er);
                        SDL_DestroyTexture(et);
                    }
                    SDL_FreeSurface(es);
                }
            }
        }
    }
}

void PlayState::endRun() {
    finalizeRun(false);
}

void PlayState::abandonRun() {
    finalizeRun(true);
}

void PlayState::populateRunSummary(int runShards, bool isNewRecord) {
    if (auto* summary = dynamic_cast<RunSummaryState*>(game->getState(StateID::RunSummary))) {
        summary->enemiesKilled = enemiesKilled;
        summary->riftsRepaired = riftsRepaired;
        summary->roomsCleared = roomsCleared;
        summary->shardsEarned = runShards;
        summary->isNewRecord = isNewRecord;
        summary->runTime = m_runTime;
        summary->playerClass = g_selectedClass;
        summary->difficulty = m_currentDifficulty;
        summary->bestCombo = m_bestCombo;
        summary->deathCause = m_deathCause;
        summary->relicsCollected = 0;
        if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
            summary->relicsCollected = static_cast<int>(
                m_player->getEntity()->getComponent<RelicComponent>().relics.size());
        }
        if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
            auto& cb = m_player->getEntity()->getComponent<CombatComponent>();
            summary->meleeWeapon = cb.currentMelee;
            summary->rangedWeapon = cb.currentRanged;
        }
        summary->peakDmgRaw = m_balanceStats.peakDmgRaw;
        summary->peakDmgClamped = m_balanceStats.peakDmgClamped;
        summary->peakSpdRaw = m_balanceStats.peakSpdRaw;
        summary->peakSpdClamped = m_balanceStats.peakSpdClamped;
        summary->cdFloorPercent = (m_balanceStats.activePlayTime > 0)
            ? (m_balanceStats.cdFloorTime / m_balanceStats.activePlayTime * 100.0f) : 0;
        summary->voidResProcs = m_combatSystem.voidResonanceProcs;
        summary->peakResidueZones = m_balanceStats.peakResidueZones;
        summary->peakVoidHunger = m_balanceStats.peakVoidHunger * 100.0f;
        summary->finalVoidHunger = m_balanceStats.finalVoidHunger * 100.0f;
    }
}

void PlayState::finalizeRun(bool abandoned) {
    if (abandoned) {
        game->changeState(StateID::Menu);
        return;
    }

    // Playtest mode: intercept endRun to restart instead of changing state
    if (m_playtest) {
        playtestOnDeath();
        return;
    }

    auto& upgrades = game->getUpgradeSystem();
    bool isNewRecord = (roomsCleared > upgrades.bestRoomReached && roomsCleared > 0);
    upgrades.totalRuns++;
    upgrades.addRunRecord(roomsCleared, enemiesKilled, riftsRepaired,
                          shardsCollected, m_currentDifficulty,
                          m_bestCombo, m_runTime, static_cast<int>(g_selectedClass), m_deathCause);
    upgrades.bestRoomReached = std::max(upgrades.bestRoomReached, roomsCleared);

    // Final unlock check at run end (catches anything missed mid-run)
    // Berserker: 50 total kills across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Berserker) &&
        upgrades.totalEnemiesKilled >= 50) {
        ClassSystem::unlock(PlayerClass::Berserker);
    }
    // Phantom: completed floor 3 (difficulty went past 3)
    if (!ClassSystem::isUnlocked(PlayerClass::Phantom) &&
        m_currentDifficulty > 3) {
        ClassSystem::unlock(PlayerClass::Phantom);
    }
    // Technomancer: 30 rifts repaired across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Technomancer) &&
        upgrades.totalRiftsRepaired >= 30) {
        ClassSystem::unlock(PlayerClass::Technomancer);
    }
    // VoidHammer: defeated any boss
    if (!WeaponSystem::isUnlocked(WeaponID::VoidHammer) && m_bossDefeated) {
        WeaponSystem::unlock(WeaponID::VoidHammer);
    }
    // PhaseDaggers: 10-hit combo
    if (!WeaponSystem::isUnlocked(WeaponID::PhaseDaggers) && m_bestCombo >= 10) {
        WeaponSystem::unlock(WeaponID::PhaseDaggers);
    }
    // VoidBeam: reach floor 5
    if (!WeaponSystem::isUnlocked(WeaponID::VoidBeam) && m_currentDifficulty >= 5) {
        WeaponSystem::unlock(WeaponID::VoidBeam);
    }
    // GrapplingHook: 5 dash kills in a run
    if (!WeaponSystem::isUnlocked(WeaponID::GrapplingHook) && m_dashKillsThisRun >= 5) {
        WeaponSystem::unlock(WeaponID::GrapplingHook);
    }

    // Check milestone rewards
    auto milestone = upgrades.checkMilestones();
    if (milestone.bonusShards > 0) {
        upgrades.addRiftShards(milestone.bonusShards);
    }

    // Save bestiary progress
    Bestiary::save("bestiary_save.dat");

    // Save lore progress
    if (auto* lore = game->getLoreSystem()) {
        lore->save("riftwalker_lore.dat");
    }

    // Ascension: award Rift Cores based on difficulty and ascension level
    int cores = m_currentDifficulty * (1 + AscensionSystem::currentLevel);
    AscensionSystem::riftCores += cores;
    AscensionSystem::save("ascension_save.dat");

    // Void Sovereign defeated -> NG+ and Ending sequence
    if (m_voidSovereignDefeated) {
        g_newGamePlusLevel++;
        if (auto* lore = game->getLoreSystem()) {
            lore->discover(LoreID::FinalRevelation);
            lore->save("riftwalker_lore.dat");
        }
        // Set ending stats
        if (auto* ending = dynamic_cast<EndingState*>(game->getState(StateID::Ending))) {
            ending->finalScore = enemiesKilled * 10 + roomsCleared * 50 + riftsRepaired * 100;
            ending->totalKills = enemiesKilled;
            ending->maxDifficulty = m_currentDifficulty;
            int relicCount = 0;
            if (m_player && m_player->getEntity()->hasComponent<RelicComponent>()) {
                relicCount = static_cast<int>(m_player->getEntity()->getComponent<RelicComponent>().relics.size());
            }
            ending->relicsCollected = relicCount;
            ending->totalTime = m_runTime;
        }
        game->changeState(StateID::Ending);
        return;
    }

    populateRunSummary(shardsCollected, isNewRecord);
    game->changeState(StateID::RunSummary);
}

void PlayState::applyChallengeModifiers() {
    if (g_activeChallenge == ChallengeID::None) return;
    const auto& cd = ChallengeMode::getChallengeData(g_activeChallenge);

    if (cd.playerMaxHP > 0 && m_player) {
        auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
        hp.maxHP = cd.playerMaxHP;
        hp.currentHP = cd.playerMaxHP;
    }
    if (cd.timeLimit > 0) {
        m_challengeTimer = cd.timeLimit;
    }

    // Apply mutators at run start
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::LowGravity) {
            // Reduce gravity for all entities (handled in physics)
        }
    }
}

void PlayState::renderChallengeHUD(SDL_Renderer* renderer, TTF_Font* font) {
    if (g_activeChallenge == ChallengeID::None && g_activeMutators[0] == MutatorID::None) return;
    if (!font) return;

    int y = 90;
    // Challenge name
    if (g_activeChallenge != ChallengeID::None) {
        const auto& cd = ChallengeMode::getChallengeData(g_activeChallenge);
        SDL_Color cc = {255, 200, 60, 200};
        SDL_Surface* s = TTF_RenderText_Blended(font, cd.name, cc);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {15, y, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
        y += 20;

        // Speedrun timer
        if (g_activeChallenge == ChallengeID::Speedrun && m_challengeTimer > 0) {
            int mins = static_cast<int>(m_challengeTimer) / 60;
            int secs = static_cast<int>(m_challengeTimer) % 60;
            char timerText[32];
            std::snprintf(timerText, sizeof(timerText), "%d:%02d", mins, secs);
            SDL_Color tc = (m_challengeTimer < 60) ? SDL_Color{255, 60, 60, 255} : SDL_Color{200, 200, 200, 220};
            SDL_Surface* ts = TTF_RenderText_Blended(font, timerText, tc);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    SDL_Rect tr = {15, y, ts->w, ts->h};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
            y += 20;
        }

        // Endless score
        if (g_activeChallenge == ChallengeID::EndlessRift) {
            char scoreText[32];
            std::snprintf(scoreText, sizeof(scoreText), "Score: %d", m_endlessScore);
            SDL_Surface* ss = TTF_RenderText_Blended(font, scoreText, SDL_Color{200, 180, 255, 220});
            if (ss) {
                SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
                if (st) {
                    SDL_Rect sr = {15, y, ss->w, ss->h};
                    SDL_RenderCopy(renderer, st, nullptr, &sr);
                    SDL_DestroyTexture(st);
                }
                SDL_FreeSurface(ss);
            }
            y += 20;
        }
    }

    // Active mutators
    for (int i = 0; i < 2; i++) {
        if (g_activeMutators[i] == MutatorID::None) continue;
        const auto& md = ChallengeMode::getMutatorData(g_activeMutators[i]);
        SDL_Surface* ms = TTF_RenderText_Blended(font, md.name, SDL_Color{180, 180, 220, 160});
        if (ms) {
            SDL_Texture* mt = SDL_CreateTextureFromSurface(renderer, ms);
            if (mt) {
                SDL_Rect mr = {15, y, ms->w, ms->h};
                SDL_RenderCopy(renderer, mt, nullptr, &mr);
                SDL_DestroyTexture(mt);
            }
            SDL_FreeSurface(ms);
        }
        y += 16;
    }
}

// ============ Combat Challenges ============

void PlayState::startCombatChallenge() {
    // Pick a random challenge type
    int typeCount = static_cast<int>(CombatChallengeType::COUNT);
    auto type = static_cast<CombatChallengeType>(std::rand() % typeCount);

    m_combatChallenge = {};
    m_combatChallenge.type = type;
    m_combatChallenge.active = true;
    m_tookDamageThisWave = false;

    switch (type) {
        case CombatChallengeType::AerialKill:
            m_combatChallenge.name = "Aerial Kill";
            m_combatChallenge.desc = "Kill an enemy while airborne";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 20;
            break;
        case CombatChallengeType::MultiKill:
            m_combatChallenge.name = "Triple Kill";
            m_combatChallenge.desc = "Kill 3 enemies within 4 seconds";
            m_combatChallenge.targetCount = 3;
            m_combatChallenge.timer = 4.0f;
            m_combatChallenge.maxTimer = 4.0f;
            m_combatChallenge.shardReward = 35;
            break;
        case CombatChallengeType::DashKill:
            m_combatChallenge.name = "Dash Slayer";
            m_combatChallenge.desc = "Kill an enemy with a dash attack";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 20;
            break;
        case CombatChallengeType::ComboFinisher:
            m_combatChallenge.name = "Combo Finisher";
            m_combatChallenge.desc = "Kill with a 3rd combo hit";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 25;
            break;
        case CombatChallengeType::ChargedKill:
            m_combatChallenge.name = "Heavy Hitter";
            m_combatChallenge.desc = "Kill with a charged attack";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 25;
            break;
        case CombatChallengeType::NoDamageWave:
            m_combatChallenge.name = "Untouchable";
            m_combatChallenge.desc = "Clear wave without taking damage";
            m_combatChallenge.targetCount = 1;
            m_combatChallenge.shardReward = 40;
            break;
        default: break;
    }
}

void PlayState::updateCombatChallenge(float dt) {
    // Completion popup timer
    if (m_challengeCompleteTimer > 0) {
        m_challengeCompleteTimer -= dt;
    }

    if (!m_combatChallenge.active || m_combatChallenge.completed) return;

    // Note: m_tookDamageThisWave is set directly when player takes damage
    // (combat hits, hazards, DOTs) — no framerate-dependent checks needed

    // Process kill events from CombatSystem
    for (auto& ke : m_combatSystem.killEvents) {
        switch (m_combatChallenge.type) {
            case CombatChallengeType::AerialKill:
                if (ke.wasAerial) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::MultiKill:
                m_combatChallenge.currentCount++;
                // Reset timer on first kill of the sequence
                if (m_combatChallenge.currentCount == 1) {
                    m_combatChallenge.timer = m_combatChallenge.maxTimer;
                }
                break;
            case CombatChallengeType::DashKill:
                if (ke.wasDash) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::ComboFinisher:
                if (ke.wasComboFinisher) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::ChargedKill:
                if (ke.wasCharged) m_combatChallenge.currentCount++;
                break;
            case CombatChallengeType::NoDamageWave:
                // Tracked via wave clear, not per kill
                break;
            default: break;
        }
    }

    // MultiKill timer countdown (only after first kill)
    if (m_combatChallenge.type == CombatChallengeType::MultiKill &&
        m_combatChallenge.currentCount > 0 && m_combatChallenge.currentCount < m_combatChallenge.targetCount) {
        m_combatChallenge.timer -= dt;
        if (m_combatChallenge.timer <= 0) {
            // Failed: reset progress
            m_combatChallenge.currentCount = 0;
            m_combatChallenge.timer = m_combatChallenge.maxTimer;
        }
    }

    // NoDamageWave: check on wave clear (all enemies dead)
    if (m_combatChallenge.type == CombatChallengeType::NoDamageWave) {
        int aliveEnemies = 0;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("enemy") != std::string::npos) aliveEnemies++;
        });
        if (aliveEnemies == 0 && enemiesKilled > 0 && !m_tookDamageThisWave) {
            m_combatChallenge.currentCount = 1;
        }
    }

    // Check completion
    if (m_combatChallenge.currentCount >= m_combatChallenge.targetCount) {
        m_combatChallenge.completed = true;
        m_combatChallenge.active = false;
        m_challengeCompleteTimer = 2.5f;
        m_challengesCompleted++;

        // Reward shards
        if (game) {
            game->getUpgradeSystem().addRiftShards(m_combatChallenge.shardReward);
            shardsCollected += m_combatChallenge.shardReward;
        }

        // Particles + SFX
        if (m_player) {
            auto& t = m_player->getEntity()->getComponent<TransformComponent>();
            m_particles.burst(t.getCenter(), 20, {255, 215, 0, 255}, 200.0f, 4.0f);
            m_particles.burst(t.getCenter(), 12, {255, 255, 180, 255}, 120.0f, 3.0f);
        }
        AudioManager::instance().play(SFX::ShrineBlessing);
        m_camera.shake(4.0f, 0.12f);
    }
}

void PlayState::renderCombatChallenge(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Active challenge indicator (top center, below boss bar)
    if (m_combatChallenge.active && !m_combatChallenge.completed) {
        int cx = SCREEN_WIDTH / 2;
        int cy = m_isBossLevel ? 60 : 28;

        // Background panel
        int panelW = 240, panelH = 36;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 180);
        SDL_Rect bg = {cx - panelW / 2, cy - panelH / 2, panelW, panelH};
        SDL_RenderFillRect(renderer, &bg);

        // Gold border
        SDL_SetRenderDrawColor(renderer, 200, 170, 50, 200);
        SDL_RenderDrawRect(renderer, &bg);

        // Challenge name
        SDL_Color gold = {255, 215, 0, 255};
        SDL_Surface* ns = TTF_RenderText_Blended(font, m_combatChallenge.name, gold);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {cx - ns->w / 2, cy - panelH / 2 + 2, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Description
        SDL_Color desc = {180, 180, 200, 200};
        SDL_Surface* ds = TTF_RenderText_Blended(font, m_combatChallenge.desc, desc);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {cx - ds->w / 2, cy - panelH / 2 + 16, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }

        // Progress bar for multi-kill (timer)
        if (m_combatChallenge.type == CombatChallengeType::MultiKill &&
            m_combatChallenge.currentCount > 0 && m_combatChallenge.maxTimer > 0) {
            float pct = m_combatChallenge.timer / m_combatChallenge.maxTimer;
            int barW = panelW - 8;
            SDL_Rect barBg = {cx - barW / 2, cy + panelH / 2 - 5, barW, 3};
            SDL_SetRenderDrawColor(renderer, 40, 40, 50, 200);
            SDL_RenderFillRect(renderer, &barBg);
            SDL_Rect barFill = {cx - barW / 2, cy + panelH / 2 - 5,
                                static_cast<int>(barW * pct), 3};
            Uint8 r = static_cast<Uint8>(255 * (1.0f - pct));
            Uint8 g = static_cast<Uint8>(255 * pct);
            SDL_SetRenderDrawColor(renderer, r, g, 50, 230);
            SDL_RenderFillRect(renderer, &barFill);

            // Kill count indicator
            char countBuf[16];
            std::snprintf(countBuf, sizeof(countBuf), "%d/%d",
                         m_combatChallenge.currentCount, m_combatChallenge.targetCount);
            SDL_Surface* cs = TTF_RenderText_Blended(font, countBuf, gold);
            if (cs) {
                SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
                if (ct) {
                    SDL_Rect cr = {cx + panelW / 2 + 4, cy - cs->h / 2, cs->w, cs->h};
                    SDL_RenderCopy(renderer, ct, nullptr, &cr);
                    SDL_DestroyTexture(ct);
                }
                SDL_FreeSurface(cs);
            }
        }

        // Shard reward preview
        char rewardBuf[24];
        std::snprintf(rewardBuf, sizeof(rewardBuf), "+%d", m_combatChallenge.shardReward);
        SDL_Color shardCol = {100, 200, 255, 160};
        SDL_Surface* rs = TTF_RenderText_Blended(font, rewardBuf, shardCol);
        if (rs) {
            SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
            if (rt) {
                SDL_Rect rr = {cx + panelW / 2 - rs->w - 4, cy - panelH / 2 + 2, rs->w, rs->h};
                SDL_RenderCopy(renderer, rt, nullptr, &rr);
                SDL_DestroyTexture(rt);
            }
            SDL_FreeSurface(rs);
        }
    }

    // Completion popup (fades out)
    if (m_challengeCompleteTimer > 0 && m_combatChallenge.completed) {
        float alpha = std::min(1.0f, m_challengeCompleteTimer / 0.5f);
        Uint8 a = static_cast<Uint8>(alpha * 255);

        int cx = SCREEN_WIDTH / 2;
        int cy = 80;

        // Slide up as it fades
        float slideUp = (1.0f - alpha) * 20.0f;
        cy -= static_cast<int>(slideUp);

        char completeBuf[64];
        std::snprintf(completeBuf, sizeof(completeBuf), "CHALLENGE COMPLETE! +%d Shards",
                     m_combatChallenge.shardReward);
        SDL_Color compColor = {255, 215, 0, a};
        SDL_Surface* cs = TTF_RenderText_Blended(font, completeBuf, compColor);
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_Rect cr = {cx - cs->w / 2, cy, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }
}

// ============ NPC System ============

void PlayState::checkNPCInteraction() {
    if (!m_player || !m_level || m_showNPCDialog) return;

    auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
    Vec2 playerCenter = pt.getCenter();
    int currentDim = m_dimManager.getCurrentDimension();
    const auto& input = game->getInput();

    m_nearNPCIndex = -1;

    auto& npcs = m_level->getNPCs();
    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        float dx = playerCenter.x - npc.position.x;
        float dy = playerCenter.y - npc.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 60.0f) {
            m_nearNPCIndex = i;

            if (input.isActionPressed(Action::Interact)) {
                m_showNPCDialog = true;
                m_npcDialogChoice = 0;
                AudioManager::instance().play(SFX::MenuConfirm);
            }
            break;
        }
    }
}

void PlayState::renderNPCs(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& npcs = m_level->getNPCs();

    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        SDL_FRect worldRect = {npc.position.x - 12, npc.position.y - 32, 24, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 3.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC color by type
        SDL_Color col;
        switch (npc.type) {
            case NPCType::RiftScholar:  col = {100, 180, 255, 255}; break; // Blue
            case NPCType::DimRefugee:   col = {255, 180, 80, 255}; break;  // Orange
            case NPCType::LostEngineer: col = {180, 255, 120, 255}; break; // Green
            case NPCType::EchoOfSelf:   col = {220, 120, 255, 255}; break; // Purple
            case NPCType::Blacksmith:   col = {255, 160, 50, 255}; break;  // Forge orange
            default:                    col = {200, 200, 200, 255}; break;
        }

        // Glow aura
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i * 2.0f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(35 * glow));
        SDL_Rect glowR = {sr.x - 8, sr.y - 8, sr.w + 16, sr.h + 16};
        SDL_RenderFillRect(renderer, &glowR);

        // Body (hooded figure shape)
        SDL_SetRenderDrawColor(renderer, col.r / 2, col.g / 2, col.b / 2, 220);
        SDL_RenderFillRect(renderer, &sr); // Robe

        // Head (smaller rect on top)
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 240);
        SDL_Rect head = {sr.x + sr.w / 4, sr.y + 2, sr.w / 2, sr.h / 3};
        SDL_RenderFillRect(renderer, &head);

        // Eyes
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        int eyeY = sr.y + sr.h / 5;
        SDL_Rect leftEye = {sr.x + sr.w / 3 - 2, eyeY, 3, 3};
        SDL_Rect rightEye = {sr.x + sr.w * 2 / 3 - 1, eyeY, 3, 3};
        SDL_RenderFillRect(renderer, &leftEye);
        SDL_RenderFillRect(renderer, &rightEye);

        // Type-specific detail
        switch (npc.type) {
            case NPCType::RiftScholar: {
                SDL_SetRenderDrawColor(renderer, 100, 180, 255, 160);
                SDL_Rect book = {sr.x + sr.w / 2 - 4, sr.y - 12, 8, 6};
                SDL_RenderFillRect(renderer, &book);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 12, sr.x + sr.w / 2, sr.y - 6);
                break;
            }
            case NPCType::DimRefugee: {
                SDL_SetRenderDrawColor(renderer, 255, 200, 60, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 14, sr.x + sr.w / 2 + 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 + 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 2);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 2, sr.x + sr.w / 2 - 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 14);
                break;
            }
            case NPCType::LostEngineer: {
                SDL_SetRenderDrawColor(renderer, 180, 255, 120, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 4, sr.y - 12, sr.x + sr.w / 2 + 4, sr.y - 4);
                break;
            }
            case NPCType::EchoOfSelf: {
                SDL_SetRenderDrawColor(renderer, 220, 120, 255, static_cast<Uint8>(100 * glow));
                SDL_Rect mirrorR = {sr.x - 2, sr.y, sr.w + 4, sr.h};
                SDL_RenderDrawRect(renderer, &mirrorR);
                break;
            }
            case NPCType::Blacksmith: {
                // Anvil shape above head
                SDL_SetRenderDrawColor(renderer, 255, 160, 50, 180);
                SDL_Rect anvil = {sr.x + sr.w / 2 - 6, sr.y - 8, 12, 4};
                SDL_RenderFillRect(renderer, &anvil);
                SDL_Rect base = {sr.x + sr.w / 2 - 3, sr.y - 4, 6, 4};
                SDL_RenderFillRect(renderer, &base);
                // Spark particles
                float sparkPhase = std::sin(ticks * 0.008f + i) * 0.5f + 0.5f;
                if (sparkPhase > 0.7f) {
                    SDL_SetRenderDrawColor(renderer, 255, 220, 80, 200);
                    SDL_Rect spark = {sr.x + sr.w / 2 + static_cast<int>(sparkPhase * 8) - 4, sr.y - 14, 2, 2};
                    SDL_RenderFillRect(renderer, &spark);
                }
                break;
            }
            default: break;
        }

        // Interaction hint
        if (i == m_nearNPCIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", npc.name);
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y - 28, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }
}

void PlayState::renderNPCDialog(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_nearNPCIndex < 0 || !m_level) return;

    auto& npcs = m_level->getNPCs();
    if (m_nearNPCIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[m_nearNPCIndex];

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Dialog box (taller for returning NPCs with longer story text)
    int stage = getNPCStoryStage(npc.type);
    int boxW = 500, boxH = 260 + (stage > 0 ? 30 : 0);
    int boxX = 640 - boxW / 2, boxY = 360 - boxH / 2;

    SDL_SetRenderDrawColor(renderer, 20, 15, 35, 240);
    SDL_Rect box = {boxX, boxY, boxW, boxH};
    SDL_RenderFillRect(renderer, &box);

    // Border
    SDL_SetRenderDrawColor(renderer, 120, 80, 200, 200);
    SDL_RenderDrawRect(renderer, &box);

    // NPC name
    SDL_Color nameColor = {180, 140, 255, 255};
    SDL_Surface* ns = TTF_RenderText_Blended(font, npc.name, nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            SDL_Rect nr = {boxX + 20, boxY + 15, ns->w, ns->h};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    // Story stage indicator (show returning NPC badge)
    if (stage > 0) {
        const char* stageLabel = (stage >= 2) ? "[Old Friend]" : "[Returning]";
        SDL_Color stageColor = (stage >= 2) ? SDL_Color{255, 215, 80, 200} : SDL_Color{140, 200, 140, 180};
        SDL_Surface* stS = TTF_RenderText_Blended(font, stageLabel, stageColor);
        if (stS) {
            SDL_Texture* stT = SDL_CreateTextureFromSurface(renderer, stS);
            if (stT) {
                SDL_Rect stR = {boxX + boxW - stS->w - 20, boxY + 15, stS->w, stS->h};
                SDL_RenderCopy(renderer, stT, nullptr, &stR);
                SDL_DestroyTexture(stT);
            }
            SDL_FreeSurface(stS);
        }
    }

    // Greeting text (stage-based)
    const char* greeting = NPCSystem::getGreeting(npc.type, stage);
    SDL_Color textColor = {200, 200, 220, 255};
    SDL_Surface* ds = TTF_RenderText_Blended(font, greeting, textColor);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            SDL_Rect dr = {boxX + 20, boxY + 45, ds->w, ds->h};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }

    // Story line (stage-based, may contain newlines — render line by line)
    const char* storyText = NPCSystem::getStoryLine(npc.type, stage);
    int lineY = boxY + 70;
    {
        // Split on newline and render each line
        std::string story(storyText);
        size_t pos = 0;
        while (pos < story.size()) {
            size_t nl = story.find('\n', pos);
            std::string line = (nl != std::string::npos) ? story.substr(pos, nl - pos) : story.substr(pos);
            pos = (nl != std::string::npos) ? nl + 1 : story.size();
            if (line.empty()) { lineY += 18; continue; }
            SDL_Surface* ls = TTF_RenderText_Blended(font, line.c_str(), SDL_Color{160, 160, 180, 200});
            if (ls) {
                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                if (lt) {
                    SDL_Rect lr = {boxX + 20, lineY, ls->w, ls->h};
                    SDL_RenderCopy(renderer, lt, nullptr, &lr);
                    SDL_DestroyTexture(lt);
                }
                SDL_FreeSurface(ls);
            }
            lineY += 18;
        }
    }

    // Dialog options (stage-based) — positioned below story text
    auto options = NPCSystem::getDialogOptions(npc.type, stage);
    int optY = std::max(lineY + 8, boxY + 110);
    for (int i = 0; i < static_cast<int>(options.size()); i++) {
        bool selected = (i == m_npcDialogChoice);
        SDL_Color oc = selected ? SDL_Color{255, 220, 100, 255} : SDL_Color{180, 180, 200, 200};

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 80, 60, 120, 100);
            SDL_Rect selR = {boxX + 15, optY - 2, boxW - 30, 22};
            SDL_RenderFillRect(renderer, &selR);

            // Arrow
            SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
            SDL_RenderDrawLine(renderer, boxX + 20, optY + 8, boxX + 28, optY + 4);
            SDL_RenderDrawLine(renderer, boxX + 28, optY + 4, boxX + 20, optY);
        }

        SDL_Surface* os = TTF_RenderText_Blended(font, options[i], oc);
        if (os) {
            SDL_Texture* ot = SDL_CreateTextureFromSurface(renderer, os);
            if (ot) {
                SDL_Rect or_ = {boxX + 35, optY, os->w, os->h};
                SDL_RenderCopy(renderer, ot, nullptr, &or_);
                SDL_DestroyTexture(ot);
            }
            SDL_FreeSurface(os);
        }
        optY += 26;
    }

    // Controls hint
    SDL_Surface* hs = TTF_RenderText_Blended(font, "[W/S] Navigate  [Enter] Select  [Esc] Close",
                                              SDL_Color{120, 120, 140, 150});
    if (hs) {
        SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
        if (ht) {
            SDL_Rect hr = {boxX + boxW / 2 - hs->w / 2, boxY + boxH - 25, hs->w, hs->h};
            SDL_RenderCopy(renderer, ht, nullptr, &hr);
            SDL_DestroyTexture(ht);
        }
        SDL_FreeSurface(hs);
    }
}

void PlayState::handleNPCDialogChoice(int npcIndex, int choice) {
    if (!m_level || npcIndex < 0) return;
    auto& npcs = m_level->getNPCs();
    if (npcIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[npcIndex];
    int npcStage = getNPCStoryStage(npc.type);
    auto options = NPCSystem::getDialogOptions(npc.type, npcStage);

    // Last option is always [Leave]
    if (choice == static_cast<int>(options.size()) - 1) {
        m_showNPCDialog = false;
        return;
    }

    if (!m_player->getEntity()->hasComponent<HealthComponent>() ||
        !m_player->getEntity()->hasComponent<CombatComponent>()) return;
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();

    // Lore: Forgotten Craft - NPC interaction
    if (auto* lore = game->getLoreSystem()) {
        if (!lore->isDiscovered(LoreID::ForgottenCraft)) {
            lore->discover(LoreID::ForgottenCraft);
            AudioManager::instance().play(SFX::LoreDiscover);
        }
    }

    int stage = getNPCStoryStage(npc.type);
    // Advance story progress after this interaction
    int typeIdx = static_cast<int>(npc.type);
    if (typeIdx >= 0 && typeIdx < static_cast<int>(NPCType::COUNT))
        m_npcStoryProgress[typeIdx]++;

    switch (npc.type) {
        case NPCType::RiftScholar:
            if (stage >= 2) {
                // Stage 2: Learn the truth — big shards + full heal
                AudioManager::instance().play(SFX::LoreDiscover);
                game->getUpgradeSystem().addRiftShards(40);
                shardsCollected += 40;
                hp.heal(hp.maxHP);
                m_player->damageBoostTimer = 20.0f;
                m_player->damageBoostMultiplier = 1.2f;
                m_particles.burst(npc.position, 25, {180, 120, 255, 255}, 200.0f, 4.0f);
                m_particles.burst(npc.position, 12, {255, 255, 200, 255}, 120.0f, 3.0f);
                if (auto* lore = game->getLoreSystem()) {
                    lore->discover(LoreID::SovereignTruth);
                }
            } else if (stage >= 1) {
                if (choice == 0) {
                    // Listen: better shards + entropy reduction
                    AudioManager::instance().play(SFX::Pickup);
                    game->getUpgradeSystem().addRiftShards(25);
                    shardsCollected += 25;
                    m_entropy.addEntropy(-15.0f);
                    m_particles.burst(npc.position, 18, {120, 200, 255, 255}, 150.0f, 3.0f);
                } else if (choice == 1) {
                    // Ask about Sovereign: lore + speed
                    AudioManager::instance().play(SFX::LoreDiscover);
                    m_player->speedBoostTimer = 30.0f;
                    m_player->speedBoostMultiplier = 1.2f;
                    game->getUpgradeSystem().addRiftShards(15);
                    shardsCollected += 15;
                    m_particles.burst(npc.position, 15, {180, 140, 255, 255}, 120.0f, 2.5f);
                }
            } else {
                if (choice == 0) {
                    // Listen to tip: shard bonus
                    AudioManager::instance().play(SFX::Pickup);
                    game->getUpgradeSystem().addRiftShards(15);
                    shardsCollected += 15;
                    m_particles.burst(npc.position, 12, {100, 180, 255, 255}, 100.0f, 2.0f);
                } else if (choice == 1) {
                    // Ask about enemies: brief speed boost
                    AudioManager::instance().play(SFX::LoreDiscover);
                    m_player->speedBoostTimer = 20.0f;
                    m_player->speedBoostMultiplier = 1.15f;
                    game->getUpgradeSystem().addRiftShards(10);
                    shardsCollected += 10;
                    m_particles.burst(npc.position, 15, {180, 220, 255, 255}, 120.0f, 2.5f);
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::DimRefugee:
            if (stage >= 2) {
                // Stage 2: Free relic gift
                if (m_player->getEntity()->hasComponent<RelicComponent>()) {
                    auto& relics = m_player->getEntity()->getComponent<RelicComponent>();
                    RelicID gift = RelicSystem::generateDrop(m_currentDifficulty, relics.relics);
                    relics.addRelic(gift);
                    RelicSystem::applyStatEffects(relics, *m_player, hp, combat);
                    AudioManager::instance().play(SFX::ShrineBlessing);
                    m_camera.shake(8.0f, 0.3f);
                    m_particles.burst(npc.position, 30, {255, 215, 80, 255}, 250.0f, 5.0f);
                    m_particles.burst(npc.position, 15, {255, 200, 60, 255}, 180.0f, 3.0f);
                }
            } else if (stage >= 1) {
                if (choice == 0) {
                    // Free healing +30 HP
                    hp.heal(30.0f);
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 15, {80, 255, 120, 255}, 120.0f, 2.0f);
                } else if (choice == 1) {
                    // Better trade: 15 HP for 60 shards
                    if (hp.currentHP > 20) {
                        hp.currentHP -= 15;
                        game->getUpgradeSystem().addRiftShards(60);
                        shardsCollected += 60;
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(npc.position, 15, {255, 200, 60, 255}, 120.0f, 2.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                        m_showNPCDialog = false;
                        return;
                    }
                }
            } else {
                if (choice == 0) {
                    // Trade 20 HP for 50 Shards
                    if (hp.currentHP > 25) {
                        hp.currentHP -= 20;
                        game->getUpgradeSystem().addRiftShards(50);
                        shardsCollected += 50;
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(npc.position, 15, {255, 200, 60, 255}, 120.0f, 2.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                        m_showNPCDialog = false;
                        return;
                    }
                } else if (choice == 1) {
                    // Trade 30 HP for 80 Shards
                    if (hp.currentHP > 35) {
                        hp.currentHP -= 30;
                        game->getUpgradeSystem().addRiftShards(80);
                        shardsCollected += 80;
                        AudioManager::instance().play(SFX::Pickup);
                        m_particles.burst(npc.position, 20, {255, 200, 60, 255}, 150.0f, 2.5f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                        m_showNPCDialog = false;
                        return;
                    }
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::LostEngineer:
            if (stage >= 2) {
                // Stage 2: Permanent +25% damage boost for rest of run
                m_player->damageBoostTimer = 99999.0f; // effectively permanent
                m_player->damageBoostMultiplier = 1.25f;
                AudioManager::instance().play(SFX::ShrineBlessing);
                m_camera.shake(8.0f, 0.3f);
                m_particles.burst(npc.position, 25, {100, 255, 80, 255}, 250.0f, 5.0f);
                m_particles.burst(npc.position, 12, {255, 255, 200, 255}, 150.0f, 3.0f);
            } else if (stage >= 1) {
                if (choice == 0) {
                    // +40% damage for 60s
                    m_player->damageBoostTimer = 60.0f;
                    m_player->damageBoostMultiplier = 1.4f;
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 20, {180, 255, 120, 255}, 150.0f, 3.0f);
                } else if (choice == 1) {
                    // +20% attack speed for 45s
                    m_player->speedBoostTimer = 45.0f;
                    m_player->speedBoostMultiplier = 1.2f;
                    AudioManager::instance().play(SFX::Pickup);
                    m_particles.burst(npc.position, 18, {120, 255, 200, 255}, 140.0f, 2.5f);
                }
            } else {
                // Stage 0: +30% damage for 45s
                m_player->damageBoostTimer = 45.0f;
                m_player->damageBoostMultiplier = 1.3f;
                AudioManager::instance().play(SFX::Pickup);
                m_particles.burst(npc.position, 20, {180, 255, 120, 255}, 150.0f, 3.0f);
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::EchoOfSelf:
            if (choice == 0) {
                // Fight! Spawn a tough enemy as "mirror match"
                // Scales with story stage — harder echo, better reward
                Vec2 spawnPos = {npc.position.x + 60, npc.position.y};
                auto& mirror = Enemy::createByType(m_entities, static_cast<int>(EnemyType::Charger),
                                                   spawnPos, m_dimManager.getCurrentDimension());
                mirror.setTag("enemy_echo");
                m_echoSpawned = true;
                m_echoRewarded = false;
                auto& mirrorHP = mirror.getComponent<HealthComponent>();
                // Higher stages = tougher echo
                float hpScale = 1.0f + stage * 0.4f; // 1.0x, 1.4x, 1.8x
                mirrorHP.maxHP = hp.maxHP * hpScale;
                mirrorHP.currentHP = mirrorHP.maxHP;
                auto& mirrorCombat = mirror.getComponent<CombatComponent>();
                mirrorCombat.meleeAttack.damage = combat.meleeAttack.damage * (1.0f + stage * 0.2f);
                Enemy::makeElite(mirror, EliteModifier::Berserker);
                AudioManager::instance().play(SFX::AnomalySpawn);
                m_camera.shake(6.0f + stage * 3.0f, 0.2f + stage * 0.1f);
                m_particles.burst(npc.position, 25 + stage * 10, {220, 120, 255, 255}, 200.0f + stage * 50.0f, 3.0f);
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;

        case NPCType::Blacksmith: {
            auto options = NPCSystem::getDialogOptions(npc.type, stage);
            bool isLeave = (choice == static_cast<int>(options.size()) - 1);
            if (!isLeave && m_player) {
                if (stage >= 2) {
                    // Free masterwork: +30% both weapons
                    m_player->smithMeleeDmgMult += 0.30f;
                    m_player->smithRangedDmgMult += 0.30f;
                    AudioManager::instance().play(SFX::ShrineBlessing);
                    m_camera.shake(8.0f, 0.3f);
                    m_particles.burst(npc.position, 30, {255, 200, 50, 255}, 200.0f, 4.0f);
                } else if (stage >= 1) {
                    int cost = (choice == 2) ? 45 : 35;
                    if (shardsCollected >= cost) {
                        shardsCollected -= cost;
                        if (choice == 0) m_player->smithMeleeDmgMult += 0.25f;
                        else if (choice == 1) m_player->smithRangedDmgMult += 0.25f;
                        else m_player->smithAtkSpdMult += 0.15f;
                        AudioManager::instance().play(SFX::ShrineBlessing);
                        m_camera.shake(5.0f, 0.2f);
                        m_particles.burst(npc.position, 20, {255, 180, 50, 255}, 150.0f, 3.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                    }
                } else {
                    int cost = 40;
                    if (shardsCollected >= cost) {
                        shardsCollected -= cost;
                        if (choice == 0) m_player->smithMeleeDmgMult += 0.20f;
                        else m_player->smithRangedDmgMult += 0.20f;
                        AudioManager::instance().play(SFX::ShrineBlessing);
                        m_camera.shake(5.0f, 0.2f);
                        m_particles.burst(npc.position, 20, {255, 180, 50, 255}, 150.0f, 3.0f);
                    } else {
                        AudioManager::instance().play(SFX::RiftFail);
                    }
                }
            }
            npc.interacted = true;
            m_showNPCDialog = false;
            break;
        }

        default:
            m_showNPCDialog = false;
            break;
    }
}

// ============ Ascension System ============

void PlayState::applyAscensionModifiers() {
    if (AscensionSystem::currentLevel <= 0) return;

    // Lore: The Ascended - triggered on first ascended run
    if (auto* lore = game->getLoreSystem()) {
        if (!lore->isDiscovered(LoreID::AscendedOnes)) {
            lore->discover(LoreID::AscendedOnes);
        }
    }

    const auto& asc = AscensionSystem::getLevel(AscensionSystem::currentLevel);

    if (!m_player) return;
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();

    // Bonus: start shards
    if (asc.startShardBonus > 0) {
        int bonus = static_cast<int>(asc.startShardBonus * 100);
        game->getUpgradeSystem().addRiftShards(bonus);
    }

    // Bonus: shop discount (stored for later use in shop)
    // Bonus: shard gain multiplier (applied in combat loot)
    // Bonus: abilities start at 50% CD
    if (asc.abilitiesStartHalfCD && m_player->getEntity()->hasComponent<AbilityComponent>()) {
        auto& abil = m_player->getEntity()->getComponent<AbilityComponent>();
        for (int i = 0; i < 3; i++) {
            abil.abilities[i].cooldownTimer = abil.abilities[i].cooldown * 0.5f;
        }
    }

    // Bonus: keep weapon upgrade from previous run (mark flag)
    // (Would require meta-save, simplified: +10% damage)
    if (asc.keepWeaponUpgrade) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
        combat.meleeAttack.damage *= 1.1f;
        combat.rangedAttack.damage *= 1.1f;
    }

    // Malus: enemies get buffs (applied per-entity in spawnEnemies isn't practical,
    // so we scale globally via combat system multipliers)
    // These are applied when enemies spawn - we modify the base stats in spawnEnemies
    // For now, store on entropy system's passive modifier as a global scaling hint

    // Malus: enemy HP/DMG/Speed scaling - applied to enemies after spawn
    m_entities.forEach([&](Entity& e) {
        if (!e.hasComponent<AIComponent>() || !e.hasComponent<HealthComponent>()) return;
        auto& eHP = e.getComponent<HealthComponent>();
        eHP.maxHP *= asc.enemyHPMult;
        eHP.currentHP = eHP.maxHP;
        if (e.hasComponent<CombatComponent>()) {
            e.getComponent<CombatComponent>().meleeAttack.damage *= asc.enemyDMGMult;
        }
        if (e.hasComponent<PhysicsBody>()) {
            // Speed scaling for chase/patrol
            auto& ai = e.getComponent<AIComponent>();
            ai.chaseSpeed *= asc.enemySpeedMult;
            ai.patrolSpeed *= asc.enemySpeedMult;
        }
    });
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

    bool targetIsExit = std::strcmp(targetInfo.type, "exit") == 0;
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

    // Legacy nearest-rift fallback remains inert; buildSmokeTargetInfo owns target selection.
    [[maybe_unused]] auto rifts = m_level->getRiftPositions();
    [[maybe_unused]] float nearestRiftDist = (std::strcmp(targetInfo.type, "rift") == 0) ? targetInfo.distance : 99999.0f;
    if (false && !m_collapsing) {
        for (int ri = 0; ri < static_cast<int>(rifts.size()); ri++) {
            if (m_repairedRiftIndices.count(ri)) continue; // Skip repaired
            if (smokeSkipRiftMask & (1 << ri)) continue; // Skip temporarily unreachable
            float dx = rifts[ri].x - playerPos.x;
            float dy = rifts[ri].y - playerPos.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < nearestRiftDist) {
                nearestRiftDist = d;
                target = rifts[ri];
                smokeCurrentTarget = ri;
            }
        }
        // If all remaining rifts are skipped, clear skip mask and retry
        if (nearestRiftDist > 99998.0f && smokeSkipRiftMask != 0) {
            smokeSkipRiftMask = 0;
            smokeLog("[SMOKE] Cleared skip mask — retrying all rifts");
            for (int ri = 0; ri < static_cast<int>(rifts.size()); ri++) {
                if (m_repairedRiftIndices.count(ri)) continue;
                float dx = rifts[ri].x - playerPos.x;
                float dy = rifts[ri].y - playerPos.y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < nearestRiftDist) {
                    nearestRiftDist = d;
                    target = rifts[ri];
                    smokeCurrentTarget = ri;
                }
            }
        }
    }

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

    // Interact with rift when near
    if (nearestRiftDist < 60.0f && !m_activePuzzle && m_nearRiftIndex >= 0 &&
        m_level->isRiftActiveInDimension(m_nearRiftIndex, currentDim)) {
        game->getInputMutable().injectActionPress(Action::Interact);
        smokeLog("[SMOKE] Interacting with rift %d (dist=%.0f)", m_nearRiftIndex, nearestRiftDist);
    }

    // Attack nearby enemies
    if (m_smokeActionTimer <= 0) {
        auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
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
    m_playtestDeaths++;
    if (m_currentDifficulty > m_playtestBestLevel)
        m_playtestBestLevel = m_currentDifficulty;

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

    // Successful run completion (level 5+)
    if (m_currentDifficulty > 5 && !m_levelComplete) {
        playtestLog("  [%.0fs] === RUN ERFOLGREICH! Level %d erreicht ===", m_playtestRunTimer, m_currentDifficulty);
        m_playtestRunActive = false;
        if (m_currentDifficulty > m_playtestBestLevel)
            m_playtestBestLevel = m_currentDifficulty;
        playtestWriteReport();
        game->quit();
    }
}

// ====== Event Chain System ======

void PlayState::spawnChainEvent() {
    if (m_eventChain.stage <= 0 || m_eventChain.completed || m_chainEventSpawned) return;
    m_chainEventSpawned = true;

    // Place a chain-themed random event in the level
    auto& events = m_level->getRandomEvents();
    m_chainEventIndex = static_cast<int>(events.size()); // Will be the index of the new event
    Vec2 spawnPos = m_level->getSpawnPoint();
    // Offset from spawn to make it discoverable but not immediate
    spawnPos.x += 200.0f + static_cast<float>(std::rand() % 300);
    spawnPos.y -= 32.0f;

    RandomEvent chainEvt;
    chainEvt.dimension = 0; // Visible in both dimensions
    chainEvt.position = spawnPos;
    chainEvt.used = false;
    chainEvt.active = true;

    switch (m_eventChain.type) {
        case EventChainType::MerchantQuest:
            if (m_eventChain.stage == 1 || m_eventChain.stage == 3) {
                chainEvt.type = RandomEventType::Merchant;
                chainEvt.cost = 0; // Free — chain merchant
            } else {
                // Stage 2: artifact is a shard pickup (RiftEcho)
                chainEvt.type = RandomEventType::RiftEcho;
                chainEvt.reward = 15 + m_currentDifficulty * 5;
            }
            break;
        case EventChainType::DimensionalTear:
            chainEvt.type = RandomEventType::DimensionalAnomaly;
            chainEvt.reward = 10 * m_eventChain.stage;
            break;
        case EventChainType::EntropySurge:
            if (m_eventChain.stage < 3) {
                chainEvt.type = RandomEventType::SuitRepairStation;
            } else {
                chainEvt.type = RandomEventType::Shrine;
                chainEvt.shrineType = ShrineType::Entropy;
            }
            break;
        case EventChainType::LostCache:
            if (m_eventChain.stage < 3) {
                chainEvt.type = RandomEventType::RiftEcho;
                chainEvt.reward = 10 + m_eventChain.stage * 10;
            } else {
                chainEvt.type = RandomEventType::GamblingRift;
                chainEvt.cost = 0; // Free final cache
                chainEvt.reward = 80 + m_currentDifficulty * 10;
            }
            break;
        default: break;
    }

    events.push_back(chainEvt);
}

void PlayState::advanceChain() {
    if (m_eventChain.stage <= 0 || m_eventChain.completed) return;

    m_eventChain.stage++;
    if (m_eventChain.stage > m_eventChain.maxStages) {
        completeChain();
    } else {
        m_chainNotifyTimer = 3.5f; // Show stage advance notification
        AudioManager::instance().play(SFX::LoreDiscover);
    }
}

void PlayState::completeChain() {
    m_eventChain.completed = true;
    m_chainRewardShards = m_eventChain.getCompletionReward(m_currentDifficulty);

    // Grant shards
    float shardMult = game->getRunBuffSystem().getShardMultiplier() * m_achievementShardMult;
    if (m_player && m_player->getEntity()->hasComponent<RelicComponent>())
        shardMult *= RelicSystem::getShardDropMultiplier(m_player->getEntity()->getComponent<RelicComponent>());
    m_chainRewardShards = static_cast<int>(m_chainRewardShards * shardMult);
    shardsCollected += m_chainRewardShards;
    game->getUpgradeSystem().addRiftShards(m_chainRewardShards);

    // Chain-specific completion bonus
    switch (m_eventChain.type) {
        case EventChainType::MerchantQuest:
            // Damage boost
            if (m_player) {
                m_player->damageBoostTimer = 45.0f;
                m_player->damageBoostMultiplier = 1.25f;
            }
            break;
        case EventChainType::DimensionalTear:
            // Heal to full
            if (m_player) {
                auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
                hp.heal(hp.maxHP);
            }
            break;
        case EventChainType::EntropySurge:
            // Big entropy reduction
            m_entropy.reduceEntropy(40.0f);
            break;
        case EventChainType::LostCache:
            // Extra shard bonus already in higher reward
            break;
        default: break;
    }

    m_chainRewardTimer = 4.0f;
    AudioManager::instance().play(SFX::LevelComplete);
    m_camera.shake(12.0f, 0.5f);
    if (m_player) {
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        SDL_Color cc = m_eventChain.getColor();
        m_particles.burst(pPos, 50, cc, 300.0f, 5.0f);
        m_particles.burst(pPos, 25, {255, 215, 0, 255}, 200.0f, 4.0f);
    }
}

void PlayState::updateEventChain(float dt) {
    if (m_chainNotifyTimer > 0) m_chainNotifyTimer -= dt;
    if (m_chainRewardTimer > 0) m_chainRewardTimer -= dt;

    // Chain stage auto-advance: when the chain event on this level is interacted with
    if (m_eventChain.stage > 0 && !m_eventChain.completed && m_chainEventSpawned && m_chainEventIndex >= 0) {
        auto& events = m_level->getRandomEvents();
        if (m_chainEventIndex < static_cast<int>(events.size()) && events[m_chainEventIndex].used) {
            advanceChain();
            m_chainEventSpawned = false; // Prevent double-advance
        }
    }
}

void PlayState::renderEventChain(SDL_Renderer* renderer, TTF_Font* font) {
    if (m_eventChain.stage <= 0 && m_chainRewardTimer <= 0) return;
    if (!font) return;

    SDL_Color cc = m_eventChain.getColor();

    // Active chain: show progress tracker at top-left
    if (m_eventChain.stage > 0 && !m_eventChain.completed) {
        int panelX = 10;
        int panelY = 90; // Below existing HUD elements
        int panelW = 220;
        int panelH = 50;

        // Background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect bg = {panelX, panelY, panelW, panelH};
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 180);
        SDL_RenderFillRect(renderer, &bg);

        // Border in chain color
        SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 200);
        SDL_RenderDrawRect(renderer, &bg);

        // Chain icon: linked circles
        for (int i = 0; i < m_eventChain.maxStages; i++) {
            int cx = panelX + 15 + i * 18;
            int cy = panelY + 14;
            int r = 5;
            if (i < m_eventChain.stage) {
                // Filled circle for completed stages
                SDL_Rect dot = {cx - r, cy - r, r * 2, r * 2};
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 255);
                SDL_RenderFillRect(renderer, &dot);
            } else {
                // Hollow for pending
                SDL_Rect dot = {cx - r, cy - r, r * 2, r * 2};
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 100);
                SDL_RenderDrawRect(renderer, &dot);
            }
            // Link line
            if (i < m_eventChain.maxStages - 1) {
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b,
                    static_cast<Uint8>(i < m_eventChain.stage - 1 ? 200 : 60));
                SDL_RenderDrawLine(renderer, cx + r + 1, cy, cx + 13 - r, cy);
            }
        }

        // Chain name
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* nameSurf = TTF_RenderText_Blended(font, m_eventChain.getName(), white);
        if (nameSurf) {
            SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
            SDL_Rect nameR = {panelX + 70, panelY + 4, nameSurf->w, nameSurf->h};
            SDL_RenderCopy(renderer, nameTex, nullptr, &nameR);
            SDL_DestroyTexture(nameTex);
            SDL_FreeSurface(nameSurf);
        }

        // Stage description
        const char* desc = m_eventChain.getStageDesc();
        if (desc && desc[0]) {
            SDL_Color dimWhite = {200, 200, 200, 200};
            SDL_Surface* descSurf = TTF_RenderText_Blended(font, desc, dimWhite);
            if (descSurf) {
                SDL_Texture* descTex = SDL_CreateTextureFromSurface(renderer, descSurf);
                int maxW = panelW - 10;
                int dw = std::min(descSurf->w, maxW);
                int dh = descSurf->h * dw / std::max(descSurf->w, 1);
                SDL_Rect descR = {panelX + 5, panelY + 26, dw, dh};
                SDL_RenderCopy(renderer, descTex, nullptr, &descR);
                SDL_DestroyTexture(descTex);
                SDL_FreeSurface(descSurf);
            }
        }
    }

    // Stage advance notification (slide-in from top)
    if (m_chainNotifyTimer > 0 && !m_eventChain.completed) {
        float slideT = std::min(m_chainNotifyTimer / 3.5f, 1.0f);
        float slideIn = (slideT > 0.85f) ? (1.0f - slideT) / 0.15f : (slideT < 0.15f ? slideT / 0.15f : 1.0f);
        Uint8 alpha = static_cast<Uint8>(255 * slideIn);

        char stageText[64];
        snprintf(stageText, sizeof(stageText), "CHAIN: Stage %d/%d", m_eventChain.stage, m_eventChain.maxStages);

        SDL_Color textCol = {cc.r, cc.g, cc.b, alpha};
        SDL_Surface* surf = TTF_RenderText_Blended(font, stageText, textCol);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_SetTextureAlphaMod(tex, alpha);
            int tx = (SCREEN_WIDTH - surf->w) / 2;
            int ty = 50 + static_cast<int>((1.0f - slideIn) * -20);
            SDL_Rect r = {tx, ty, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &r);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }

    // Completion reward popup (event chain)
    if (m_chainRewardTimer > 0) {
        float fadeT = std::min(m_chainRewardTimer / 4.0f, 1.0f);
        float alpha01 = (fadeT > 0.8f) ? (1.0f - fadeT) / 0.2f : (fadeT < 0.2f ? fadeT / 0.2f : 1.0f);
        Uint8 alpha = static_cast<Uint8>(255 * alpha01);

        // "CHAIN COMPLETE" title
        SDL_Color goldCol = {255, 215, 60, alpha};
        SDL_Surface* titleSurf = TTF_RenderText_Blended(font, "CHAIN COMPLETE!", goldCol);
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_SetTextureAlphaMod(titleTex, alpha);
            int tx = (SCREEN_WIDTH - titleSurf->w) / 2;
            int ty = 100;
            SDL_Rect r = {tx, ty, titleSurf->w, titleSurf->h};
            SDL_RenderCopy(renderer, titleTex, nullptr, &r);
            SDL_DestroyTexture(titleTex);
            SDL_FreeSurface(titleSurf);
        }

        // Chain name + reward
        char rewardText[64];
        snprintf(rewardText, sizeof(rewardText), "%s — +%d Shards", m_eventChain.getName(), m_chainRewardShards);
        SDL_Color rewCol = {cc.r, cc.g, cc.b, alpha};
        SDL_Surface* rewSurf = TTF_RenderText_Blended(font, rewardText, rewCol);
        if (rewSurf) {
            SDL_Texture* rewTex = SDL_CreateTextureFromSurface(renderer, rewSurf);
            SDL_SetTextureAlphaMod(rewTex, alpha);
            int rx = (SCREEN_WIDTH - rewSurf->w) / 2;
            int ry = 125;
            SDL_Rect r = {rx, ry, rewSurf->w, rewSurf->h};
            SDL_RenderCopy(renderer, rewTex, nullptr, &r);
            SDL_DestroyTexture(rewTex);
            SDL_FreeSurface(rewSurf);
        }
    }
}

// ─── Kill Feed ───

void PlayState::addKillFeedEntry(const KillEvent& ke) {
    auto& entry = m_killFeed[m_killFeedHead % MAX_KILL_FEED];
    entry.timer = 4.0f; // visible for 4 seconds

    // Get enemy name from Bestiary
    const char* enemyName = "Enemy";
    if (ke.enemyType >= 0 && ke.enemyType < static_cast<int>(EnemyType::Boss)) {
        enemyName = Bestiary::getEntry(static_cast<EnemyType>(ke.enemyType)).name;
    } else if (ke.wasBoss) {
        enemyName = "BOSS";
    }

    // Build kill text with context
    const char* prefix = "";
    if (ke.wasMiniBoss) prefix = "[MINI-BOSS] ";
    else if (ke.wasElite) prefix = "[ELITE] ";

    const char* method = "";
    if (ke.wasDash) method = " (Dash)";
    else if (ke.wasCharged) method = " (Charged)";
    else if (ke.wasSlam) method = " (Slam)";
    else if (ke.wasComboFinisher) method = " (Finisher)";
    else if (ke.wasAerial) method = " (Aerial)";
    else if (ke.wasRanged) method = " (Ranged)";

    std::snprintf(entry.text, sizeof(entry.text), "%s%s killed%s", prefix, enemyName, method);

    // Color based on kill type
    if (ke.wasBoss) entry.color = {255, 80, 80, 255};
    else if (ke.wasMiniBoss) entry.color = {255, 160, 60, 255};
    else if (ke.wasElite) entry.color = {200, 140, 255, 255};
    else if (ke.wasDash || ke.wasCharged || ke.wasSlam) entry.color = {255, 215, 80, 255};
    else if (ke.wasAerial || ke.wasComboFinisher) entry.color = {100, 220, 255, 255};
    else entry.color = {180, 180, 200, 220};

    m_killFeedHead++;
}

void PlayState::updateKillFeed(float dt) {
    for (int i = 0; i < MAX_KILL_FEED; i++) {
        if (m_killFeed[i].timer > 0) m_killFeed[i].timer -= dt;
    }
}

void PlayState::renderKillFeed(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    int x = SCREEN_WIDTH - 20;
    int y = SCREEN_HEIGHT - 140;

    // Collect active entries in display order (newest at bottom)
    struct DisplayEntry { const char* text; SDL_Color color; float timer; };
    DisplayEntry display[MAX_KILL_FEED];
    int count = 0;

    for (int i = MAX_KILL_FEED - 1; i >= 0; i--) {
        int idx = (m_killFeedHead - 1 - i + MAX_KILL_FEED * 2) % MAX_KILL_FEED;
        if (m_killFeed[idx].timer > 0 && m_killFeed[idx].text[0] != '\0') {
            display[count++] = {m_killFeed[idx].text, m_killFeed[idx].color, m_killFeed[idx].timer};
        }
    }

    for (int i = 0; i < count; i++) {
        float alpha = std::min(display[i].timer / 0.5f, 1.0f); // fade out in last 0.5s
        SDL_Color c = display[i].color;
        c.a = static_cast<Uint8>(c.a * alpha);

        SDL_Surface* surf = TTF_RenderText_Blended(font, display[i].text, c);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, c.a);
                SDL_Rect r = {x - surf->w, y, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
        y += 18;
    }
}

// ─── Unlock Notifications ───

void PlayState::pushUnlockNotification(const char* name, bool isWeapon) {
    auto& notif = m_unlockNotifs[m_unlockNotifHead % MAX_UNLOCK_NOTIFS];
    std::snprintf(notif.text, sizeof(notif.text),
                  "%s UNLOCKED: %s", isWeapon ? "WEAPON" : "CLASS", name);
    notif.timer = notif.maxTimer;
    m_unlockNotifHead++;
}

void PlayState::updateUnlockNotifications(float dt) {
    for (int i = 0; i < MAX_UNLOCK_NOTIFS; i++) {
        if (m_unlockNotifs[i].timer > 0) m_unlockNotifs[i].timer -= dt;
    }
}

void PlayState::renderUnlockNotifications(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;
    // Stack popups in top-center
    int y = 140;
    for (int i = 0; i < MAX_UNLOCK_NOTIFS; i++) {
        auto& notif = m_unlockNotifs[i];
        if (notif.timer <= 0) continue;

        // Slide in from top during first 0.2s, fade out during last 0.5s
        float slideIn = std::min(1.0f, (notif.maxTimer - notif.timer) / 0.2f);
        float fadeOut = std::min(1.0f, notif.timer / 0.5f);
        Uint8 alpha = static_cast<Uint8>(255 * slideIn * fadeOut);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Golden pulsing background panel
        float pulse = 0.7f + 0.3f * std::sin(notif.timer * 8.0f);
        SDL_SetRenderDrawColor(renderer, 40, 30, 5,
                               static_cast<Uint8>(180 * slideIn * fadeOut));
        SDL_Rect bg = {640 - 220, y - 4, 440, 30};
        SDL_RenderFillRect(renderer, &bg);
        // Gold border
        SDL_SetRenderDrawColor(renderer, 255, 200, 50,
                               static_cast<Uint8>(220 * pulse * slideIn * fadeOut));
        SDL_RenderDrawRect(renderer, &bg);

        // Text
        SDL_Color textColor = {255, 215, 0, alpha};
        SDL_Surface* surf = TTF_RenderText_Blended(font, notif.text, textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, alpha);
                int tw = surf->w, th = surf->h;
                SDL_Rect r = {640 - tw / 2, y, tw, th};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
        y += 36;
    }
}

// ─── Unlock Condition Checks (called each frame after kill events) ───

void PlayState::checkUnlockConditions() {
    auto& upgrades = game->getUpgradeSystem();

    // ── Class unlock checks ──

    // Berserker: kill 50 total enemies across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Berserker) &&
        upgrades.totalEnemiesKilled >= 50) {
        ClassSystem::unlock(PlayerClass::Berserker);
        pushUnlockNotification("Berserker", false);
    }

    // Phantom: complete floor 3 in any run (m_currentDifficulty > 3)
    if (!ClassSystem::isUnlocked(PlayerClass::Phantom) &&
        m_currentDifficulty > 3) {
        ClassSystem::unlock(PlayerClass::Phantom);
        pushUnlockNotification("Phantom", false);
    }

    // Technomancer: repair 30 rifts across all runs
    if (!ClassSystem::isUnlocked(PlayerClass::Technomancer) &&
        upgrades.totalRiftsRepaired >= 30) {
        ClassSystem::unlock(PlayerClass::Technomancer);
        pushUnlockNotification("Technomancer", false);
    }

    // ── Weapon unlock checks ──

    // PhaseDaggers: get a 10-hit combo
    if (!WeaponSystem::isUnlocked(WeaponID::PhaseDaggers) &&
        m_bestCombo >= 10) {
        WeaponSystem::unlock(WeaponID::PhaseDaggers);
        pushUnlockNotification("Phase Daggers", true);
    }

    // VoidHammer: defeat any boss
    if (!WeaponSystem::isUnlocked(WeaponID::VoidHammer) &&
        m_bossDefeated && !m_unlockedBossWeaponThisRun) {
        WeaponSystem::unlock(WeaponID::VoidHammer);
        m_unlockedBossWeaponThisRun = true;
        pushUnlockNotification("Void Hammer", true);
    }

    // VoidBeam: reach floor 5
    if (!WeaponSystem::isUnlocked(WeaponID::VoidBeam) &&
        m_currentDifficulty >= 5) {
        WeaponSystem::unlock(WeaponID::VoidBeam);
        pushUnlockNotification("Void Beam", true);
    }

    // GrapplingHook: 5 dash-kills in one run
    if (!WeaponSystem::isUnlocked(WeaponID::GrapplingHook) &&
        m_dashKillsThisRun >= 5) {
        WeaponSystem::unlock(WeaponID::GrapplingHook);
        pushUnlockNotification("Grappling Hook", true);
    }

    // RiftShotgun: 3+ kills from one attack (tracked in kill event loop)
    if (!WeaponSystem::isUnlocked(WeaponID::RiftShotgun) &&
        m_aoeKillCountThisRun >= 3) {
        WeaponSystem::unlock(WeaponID::RiftShotgun);
        m_aoeKillCountThisRun = 0;
        pushUnlockNotification("Rift Shotgun", true);
    }
}
