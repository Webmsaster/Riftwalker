// LevelGeneratorValidation.cpp -- Split from LevelGenerator.cpp (level validation, BFS traversal)
#include "LevelGenerator.h"
#include "LevelGeneratorInternal.h"
#include <algorithm>
#include <cmath>
#include <deque>

namespace {
using LevelGeneratorInternal::isStableStandingTile;

constexpr int kMaxGenerationValidationAttempts = 512;

bool roomContainsTile(const LevelGraphRoom& room, int x, int y) {
    return x >= room.x && x < room.x + room.w &&
           y >= room.y && y < room.y + room.h;
}

int findRoomIndexForTile(const std::vector<LevelGraphRoom>& rooms, int x, int y) {
    for (int i = 0; i < static_cast<int>(rooms.size()); i++) {
        if (roomContainsTile(rooms[i], x, y)) return i;
    }
    return -1;
}

bool roomHasStableStandingTile(const Level& level, const LevelGraphRoom& room, int dim) {
    int minX = room.x + 1;
    int maxX = room.x + room.w - 2;
    int minY = room.y + 1;
    int maxY = room.y + room.h - 2;
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            if (isStableStandingTile(level, x, y, dim)) {
                return true;
            }
        }
    }
    return false;
}

bool roomHasSharedSwitchAnchor(const Level& level, const LevelGraphRoom& room) {
    int minX = room.x + 1;
    int maxX = room.x + room.w - 2;
    int minY = room.y + 1;
    int maxY = room.y + room.h - 2;
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            const Tile& tileA = level.getTile(x, y, 1);
            const Tile& tileB = level.getTile(x, y, 2);
            if (!tileA.isSolid() && !tileA.isDangerous() &&
                !tileB.isSolid() && !tileB.isDangerous()) {
                return true;
            }
        }
    }
    return false;
}

bool horizontalCorridorCarved(const Level& level,
                              int fromX,
                              int toX,
                              int atY,
                              int dim,
                              int allowedOneWayLoX = -1,
                              int allowedOneWayHiX = -1) {
    int lo = std::min(fromX, toX);
    int hi = std::max(fromX, toX);
    for (int x = lo; x <= hi; x++) {
        for (int dy = 0; dy < 4; dy++) {
            if (!level.inBounds(x, atY + dy)) {
                return false;
            }
            const Tile& tile = level.getTile(x, atY + dy, dim);
            bool allowOneWayLanding = tile.isOneWay() &&
                                      dy == 3 &&
                                      x >= allowedOneWayLoX &&
                                      x <= allowedOneWayHiX;
            if (tile.isSolid() || tile.isDangerous() || (tile.isOneWay() && !allowOneWayLanding)) {
                return false;
            }
        }
    }
    return true;
}

bool verticalShaftCarved(const Level& level, int x1, int x2, int y1, int y2, int dim) {
    int midX = (x1 + x2) / 2;
    int minY = std::min(y1, y2);
    int maxY = std::max(y1, y2) + 3;
    for (int y = minY - 1; y <= maxY + 1; y++) {
        for (int x = midX; x <= midX + 1; x++) {
            if (!level.inBounds(x, y)) return false;
            const Tile& tile = level.getTile(x, y, dim);
            if (tile.isSolid() || tile.isDangerous()) {
                return false;
            }
        }
    }
    return true;
}

bool shaftHasOneWayTraversal(const Level& level, int x1, int x2, int y1, int y2, int dim) {
    int midX = (x1 + x2) / 2;
    int minY = std::min(y1, y2);
    int maxY = std::max(y1, y2);
    for (int y = minY + 1; y < maxY; y++) {
        if (level.isOneWay(midX, y, dim) || level.isOneWay(midX + 1, y, dim)) {
            return true;
        }
    }
    return false;
}

bool roomReachableInVisitedMask(const std::vector<char>& visited,
                                int roomIndex,
                                const std::array<bool, 3>& validDims,
                                int roomCount,
                                unsigned int switchStateCount) {
    for (int dim = 1; dim <= 2; dim++) {
        if (!validDims[dim]) continue;
        for (unsigned int mask = 0; mask < switchStateCount; mask++) {
            int idx = static_cast<int>(mask * roomCount * 2 + roomIndex * 2 + (dim - 1));
            if (idx >= 0 && idx < static_cast<int>(visited.size()) && visited[idx]) {
                return true;
            }
        }
    }
    return false;
}
} // namespace

Level LevelGenerator::generate(int difficulty, int seed) {
    Level lastInvalid(8, 8, 32);
    bool haveInvalid = false;

    for (int attempt = 0; attempt < kMaxGenerationValidationAttempts; attempt++) {
        int candidateSeed = seed + attempt;
        LevelTopology topology;
        Level level = generateCandidate(difficulty, candidateSeed, topology);

        topology.validation.requestedSeed = seed;
        topology.validation.usedSeed = candidateSeed;
        topology.validation.attempt = attempt;
        topology.validation.passed = validateGeneratedLevel(level, topology);
        level.setTopology(topology);

        if (topology.validation.passed) {
            if (attempt > 0) {
                SDL_Log("LevelGenerator: requested seed %d resolved to valid seed %d after %d retries",
                        seed, candidateSeed, attempt);
            }
            return level;
        }

        lastInvalid = level;
        haveInvalid = true;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "LevelGenerator: failed to produce validated level for requested seed %d after %d attempts",
                 seed, kMaxGenerationValidationAttempts);
    return haveInvalid ? lastInvalid : Level(8, 8, 32);
}

Level LevelGenerator::generateCandidateForTesting(int difficulty, int seed, LevelTopology& topology) {
    return generateCandidate(difficulty, seed, topology);
}

bool LevelGenerator::validateLevelForTesting(const Level& level, LevelTopology& topology) const {
    return validateGeneratedLevel(level, topology);
}

bool LevelGenerator::revalidateLevel(Level& level) const {
    LevelTopology topology = level.getTopology();
    bool passed = validateGeneratedLevel(level, topology);
    topology.validation.passed = passed;
    level.setTopology(std::move(topology));
    return passed;
}

bool LevelGenerator::validateGeneratedLevel(const Level& level, LevelTopology& topology) const {
    topology.validation.failureMask = LevelValidationFailure_None;
    auto fail = [&](int failureBit) {
        topology.validation.failureMask |= failureBit;
    };

    if (topology.rooms.empty() || topology.spawnRoomIndex < 0 || topology.exitRoomIndex < 0) {
        fail(LevelValidationFailure_MainPath);
        fail(LevelValidationFailure_Spawn);
        fail(LevelValidationFailure_Exit);
        return false;
    }

    for (auto& room : topology.rooms) {
        room.hasStableFooting[1] = roomHasStableStandingTile(level, room, 1);
        room.hasStableFooting[2] = roomHasStableStandingTile(level, room, 2);
        room.hasDimensionSwitchAnchor = roomHasSharedSwitchAnchor(level, room);
    }

    int spawnRoomByTile = findRoomIndexForTile(topology.rooms, topology.spawnTileX, topology.spawnTileY);
    topology.spawnValid[1] = isStableStandingTile(level, topology.spawnTileX, topology.spawnTileY, 1);
    topology.spawnValid[2] = isStableStandingTile(level, topology.spawnTileX, topology.spawnTileY, 2);
    if (spawnRoomByTile != topology.spawnRoomIndex || !topology.spawnValid[1]) {
        fail(LevelValidationFailure_Spawn);
    }

    int exitRoomByTile = findRoomIndexForTile(topology.rooms, topology.exitTileX, topology.exitTileY);
    topology.exitValid[1] = isStableStandingTile(level, topology.exitTileX, topology.exitTileY, 1);
    topology.exitValid[2] = isStableStandingTile(level, topology.exitTileX, topology.exitTileY, 2);
    if (exitRoomByTile != topology.exitRoomIndex || (!topology.exitValid[1] && !topology.exitValid[2])) {
        fail(LevelValidationFailure_Exit);
    }

    for (auto& rift : topology.rifts) {
        if (rift.roomIndex < 0) {
            rift.roomIndex = findRoomIndexForTile(topology.rooms, rift.tileX, rift.tileY);
        }
        rift.accessibleDimensions[1] = isStableStandingTile(level, rift.tileX, rift.tileY, 1);
        rift.accessibleDimensions[2] = isStableStandingTile(level, rift.tileX, rift.tileY, 2);
        if (rift.requiredDimension != 1 && rift.requiredDimension != 2) {
            rift.requiredDimension = rift.accessibleDimensions[2] && !rift.accessibleDimensions[1] ? 2 : 1;
        }
        rift.validated = rift.roomIndex >= 0 &&
                         (rift.accessibleDimensions[1] || rift.accessibleDimensions[2]);
        if (!rift.validated) {
            fail(LevelValidationFailure_RiftAnchor);
        }
    }

    for (auto& sw : topology.switches) {
        bool switchRoomMatches = findRoomIndexForTile(topology.rooms, sw.switchTileX, sw.switchTileY) == sw.roomIndex;
        bool gateRoomMatches = findRoomIndexForTile(topology.rooms, sw.gateTileX, sw.gateTileY) == sw.gateRoomIndex;
        bool switchOk = switchRoomMatches &&
                        level.getTile(sw.switchTileX, sw.switchTileY, sw.switchDimension).type == TileType::DimSwitch &&
                        isStableStandingTile(level, sw.switchTileX, sw.switchTileY, sw.switchDimension);
        bool gateOk = gateRoomMatches && sw.gateHeight > 0;
        for (int dy = 0; dy < sw.gateHeight && gateOk; dy++) {
            if (!level.inBounds(sw.gateTileX, sw.gateTileY + dy) ||
                level.getTile(sw.gateTileX, sw.gateTileY + dy, sw.gateDimension).type != TileType::DimGate) {
                gateOk = false;
            }
        }
        sw.validated = switchOk && gateOk;
        if (!sw.validated) {
            fail(LevelValidationFailure_DimPuzzle);
        }
    }

    for (auto& edge : topology.edges) {
        if (edge.fromRoom < 0 || edge.fromRoom >= static_cast<int>(topology.rooms.size()) ||
            edge.toRoom < 0 || edge.toRoom >= static_cast<int>(topology.rooms.size())) {
            fail(LevelValidationFailure_MainPath);
            continue;
        }

        const auto& fromRoom = topology.rooms[edge.fromRoom];
        const auto& toRoom = topology.rooms[edge.toRoom];
        int x1 = fromRoom.x + fromRoom.w - 1;
        int y1 = fromRoom.y + fromRoom.h / 2;
        int x2 = toRoom.x;
        int y2 = toRoom.y + toRoom.h / 2;

        bool pathOk = false;
        if (edge.type == LevelGraphEdgeType::Corridor) {
            pathOk = horizontalCorridorCarved(level, x1, x2, y1, edge.dimension);
        } else {
            int midX = (x1 + x2) / 2;
            bool shortJumpableShaft = std::abs(y1 - y2) <= 4;
            pathOk = horizontalCorridorCarved(level, x1, midX, y1, edge.dimension, midX, midX + 1) &&
                     horizontalCorridorCarved(level, midX, x2, y2, edge.dimension, midX, midX + 1) &&
                     verticalShaftCarved(level, x1, x2, y1, y2, edge.dimension) &&
                     (shaftHasOneWayTraversal(level, x1, x2, y1, y2, edge.dimension) || shortJumpableShaft);
        }

        edge.validated = pathOk;
    }

    for (int roomIndex = 1; roomIndex < static_cast<int>(topology.rooms.size()); roomIndex++) {
        bool pairHasTraversal = false;
        for (const auto& edge : topology.edges) {
            if (edge.fromRoom == roomIndex - 1 &&
                edge.toRoom == roomIndex &&
                edge.validated) {
                pairHasTraversal = true;
                break;
            }
        }
        if (!pairHasTraversal) {
            fail(LevelValidationFailure_MainPath);
            break;
        }
    }

    if (!topology.rooms[topology.spawnRoomIndex].hasDimensionSwitchAnchor) {
        bool needsCrossDim = false;
        for (const auto& rift : topology.rifts) {
            if (rift.accessibleDimensions[2] && !rift.accessibleDimensions[1]) {
                needsCrossDim = true;
                break;
            }
        }
        if (!needsCrossDim && topology.exitValid[2] && !topology.exitValid[1]) {
            needsCrossDim = true;
        }
        if (needsCrossDim) {
            fail(LevelValidationFailure_DimSwitch);
        }
    }

    struct TraversalState {
        int roomIndex = -1;
        int dimension = 1;
        unsigned int switchMask = 0;
    };

    int roomCount = static_cast<int>(topology.rooms.size());
    unsigned int switchStateCount = 1u << static_cast<unsigned int>(topology.switches.size());
    std::vector<char> visited(static_cast<size_t>(roomCount) * 2u * switchStateCount, 0);
    std::deque<TraversalState> queue;

    auto tryVisit = [&](int roomIndex, int dimension, unsigned int switchMask) {
        if (roomIndex < 0 || roomIndex >= roomCount || dimension < 1 || dimension > 2) return;
        size_t idx = static_cast<size_t>(switchMask) * static_cast<size_t>(roomCount) * 2u +
                     static_cast<size_t>(roomIndex) * 2u +
                     static_cast<size_t>(dimension - 1);
        if (idx >= visited.size() || visited[idx]) return;
        visited[idx] = 1;
        queue.push_back({roomIndex, dimension, switchMask});
    };

    // Bug fix: seed BFS from BOTH dimensions if spawn is valid there.
    // The spawn clear loop in LevelGenerator writes empty tiles in both dims,
    // so spawnValid[2] should typically also be true. Previously, only dim-1
    // was seeded — BFS could only reach dim-2 via a dimensionSwitchAnchor room,
    // which forced false exit-reachability failures when the spawn room lacked
    // the shared-anchor heuristic. Validator then wasted retries on valid levels.
    if (topology.spawnValid[1]) {
        tryVisit(topology.spawnRoomIndex, 1, 0);
    }
    if (topology.spawnValid[2]) {
        tryVisit(topology.spawnRoomIndex, 2, 0);
    }

    while (!queue.empty()) {
        TraversalState state = queue.front();
        queue.pop_front();

        const auto& room = topology.rooms[state.roomIndex];
        if (room.hasDimensionSwitchAnchor) {
            tryVisit(state.roomIndex, state.dimension == 1 ? 2 : 1, state.switchMask);
        }

        for (const auto& sw : topology.switches) {
            if (!sw.validated || sw.roomIndex != state.roomIndex || sw.switchDimension != state.dimension) continue;
            tryVisit(state.roomIndex, state.dimension, state.switchMask | (1u << static_cast<unsigned int>(sw.pairId)));
        }

        for (const auto& edge : topology.edges) {
            if (!edge.validated || edge.fromRoom != state.roomIndex || edge.dimension != state.dimension) continue;
            if (edge.requiredSwitchId >= 0 &&
                (state.switchMask & (1u << static_cast<unsigned int>(edge.requiredSwitchId))) == 0) {
                continue;
            }
            tryVisit(edge.toRoom, state.dimension, state.switchMask);
        }
    }

    for (const auto& rift : topology.rifts) {
        if (!rift.validated) continue;
        if (!roomReachableInVisitedMask(visited, rift.roomIndex, rift.accessibleDimensions, roomCount, switchStateCount)) {
            fail(LevelValidationFailure_RiftReachability);
        }
    }

    if (!roomReachableInVisitedMask(visited, topology.exitRoomIndex, topology.exitValid, roomCount, switchStateCount)) {
        fail(LevelValidationFailure_ExitReachability);
    }

    return topology.validation.failureMask == LevelValidationFailure_None;
}
