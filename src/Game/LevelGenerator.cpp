#include "LevelGenerator.h"
#include "NPCSystem.h"
#include "DimensionShiftBalance.h"
#include <algorithm>
#include <cmath>
#include <deque>

// Room data shared between generate() and helper methods
struct LGRoom { int x, y, w, h; };

namespace {
constexpr int kMaxGenerationValidationAttempts = 512;
bool isStableStandingTile(const Level& level, int x, int y, int dim);

bool hasSupportBelow(const Level& level, int x, int y, int dim) {
    for (int below = 1; below <= 5; below++) {
        if (level.isSolid(x, y + below, dim) || level.isOneWay(x, y + below, dim)) {
            return true;
        }
    }
    return false;
}

bool isValidRiftAnchor(const Level& level, int x, int y) {
    if (!level.inBounds(x, y)) return false;
    if (level.isSolid(x, y, 1) || level.isSolid(x, y, 2)) return false;
    return isStableStandingTile(level, x, y, 1) || isStableStandingTile(level, x, y, 2);
}

bool isValidRiftAnchorForDimension(const Level& level, int x, int y, int requiredDimension) {
    if (requiredDimension != 1 && requiredDimension != 2) {
        return isValidRiftAnchor(level, x, y);
    }
    if (!level.inBounds(x, y)) return false;
    if (level.isSolid(x, y, requiredDimension)) return false;
    return isStableStandingTile(level, x, y, requiredDimension);
}

bool tryFindRiftAnchorInRoom(const Level& level, const LGRoom& room,
                             std::mt19937& rng, int requiredDimension,
                             int& outX, int& outY) {
    int minX = room.x + 2;
    int maxX = room.x + room.w - 3;
    int minY = room.y + 1;
    int maxY = room.y + room.h - 3;
    if (maxX < minX || maxY < minY) return false;

    int roomW = maxX - minX + 1;
    int roomH = maxY - minY + 1;
    for (int attempt = 0; attempt < 64; attempt++) {
        int rx = minX + static_cast<int>(rng() % roomW);
        int ry = minY + static_cast<int>(rng() % roomH);
        if (isValidRiftAnchorForDimension(level, rx, ry, requiredDimension)) {
            outX = rx;
            outY = ry;
            return true;
        }
    }

    for (int y = maxY; y >= minY; y--) {
        for (int x = minX; x <= maxX; x++) {
            if (isValidRiftAnchorForDimension(level, x, y, requiredDimension)) {
                outX = x;
                outY = y;
                return true;
            }
        }
    }

    return false;
}

bool isSafeOccupiableTile(const Level& level, int x, int y, int dim) {
    if (!level.inBounds(x, y)) return false;
    const Tile& tile = level.getTile(x, y, dim);
    return !tile.isSolid() && !tile.isOneWay() && !tile.isDangerous();
}

bool hasStableSupport(const Level& level, int x, int y, int dim) {
    if (!level.inBounds(x, y + 1)) return false;
    return level.isSolid(x, y + 1, dim) || level.isOneWay(x, y + 1, dim);
}

bool isStableStandingTile(const Level& level, int x, int y, int dim) {
    return isSafeOccupiableTile(level, x, y, dim) && hasStableSupport(level, x, y, dim);
}

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

void restoreMainPathTraversal(Level& level,
                              int x1,
                              int y1,
                              int x2,
                              int y2,
                              int dim,
                              const WorldTheme& theme) {
    Tile empty;
    empty.type = TileType::Empty;

    auto clearHorizontal = [&](int fromX, int toX, int atY) {
        int lo = std::min(fromX, toX);
        int hi = std::max(fromX, toX);
        for (int x = lo; x <= hi; x++) {
            for (int dy = 0; dy < 4; dy++) {
                int y = atY + dy;
                if (!level.inBounds(x, y)) continue;
                level.setTile(x, y, dim, empty);
            }
        }
    };

    if (std::abs(y1 - y2) <= 2) {
        clearHorizontal(x1, x2, y1);
        return;
    }

    int midX = (x1 + x2) / 2;
    clearHorizontal(x1, midX, y1);
    clearHorizontal(midX, x2, y2);

    int shaftTop = std::min(y1, y2) - 1;
    int shaftBottom = std::max(y1, y2) + 4;
    for (int x = midX; x <= midX + 1; x++) {
        for (int y = shaftTop; y <= shaftBottom; y++) {
            if (!level.inBounds(x, y)) continue;
            level.setTile(x, y, dim, empty);
        }
    }

    Tile oneWay;
    oneWay.type = TileType::OneWay;
    oneWay.color = theme.colors.oneWay;
    int shaftMinY = std::min(y1, y2);
    int shaftMaxY = std::max(y1, y2);
    for (int y = shaftMinY + 3; y < shaftMaxY; y += 4) {
        if (level.inBounds(midX, y)) level.setTile(midX, y, dim, oneWay);
        if (level.inBounds(midX + 1, y)) level.setTile(midX + 1, y, dim, oneWay);
    }
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

struct IntRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

bool rectsOverlap(const IntRect& a, const IntRect& b) {
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

IntRect expandRect(const IntRect& rect, int padding) {
    return {
        rect.x - padding,
        rect.y - padding,
        rect.w + padding * 2,
        rect.h + padding * 2
    };
}

bool overlapsExistingSecretRooms(const Level& level, const IntRect& rect) {
    for (const auto& secretRoom : level.getSecretRooms()) {
        IntRect existing{
            secretRoom.tileX,
            secretRoom.tileY,
            secretRoom.width,
            secretRoom.height
        };
        if (rectsOverlap(rect, existing)) {
            return true;
        }
    }
    return false;
}

bool overlapsMainPathGeometry(const std::vector<LGRoom>& rooms, const IntRect& rect) {
    for (const auto& room : rooms) {
        if (rectsOverlap(rect, {room.x, room.y, room.w, room.h})) {
            return true;
        }
    }

    for (size_t i = 1; i < rooms.size(); i++) {
        const auto& fromRoom = rooms[i - 1];
        const auto& toRoom = rooms[i];
        int x1 = fromRoom.x + fromRoom.w - 1;
        int y1 = fromRoom.y + fromRoom.h / 2;
        int x2 = toRoom.x;
        int y2 = toRoom.y + toRoom.h / 2;

        if (std::abs(y1 - y2) <= 2) {
            IntRect corridor{
                std::min(x1, x2),
                y1,
                std::abs(x2 - x1) + 1,
                4
            };
            if (rectsOverlap(rect, corridor)) {
                return true;
            }
            continue;
        }

        int midX = (x1 + x2) / 2;
        IntRect upperSegment{
            std::min(x1, midX),
            y1,
            std::abs(midX - x1) + 1,
            4
        };
        IntRect lowerSegment{
            std::min(midX, x2),
            y2,
            std::abs(x2 - midX) + 1,
            4
        };
        IntRect shaft{
            midX,
            std::min(y1, y2) - 1,
            2,
            std::abs(y2 - y1) + 6
        };

        if (rectsOverlap(rect, upperSegment) ||
            rectsOverlap(rect, lowerSegment) ||
            rectsOverlap(rect, shaft)) {
            return true;
        }
    }

    return false;
}
} // namespace

LevelGenerator::LevelGenerator() : m_rng(42) {}

void LevelGenerator::setThemes(const WorldTheme& themeA, const WorldTheme& themeB) {
    m_themeA = themeA;
    m_themeB = themeB;
}

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

Level LevelGenerator::generateCandidate(int difficulty, int seed, LevelTopology& topology) {
    m_rng.seed(seed);

    int roomCount = 8 + difficulty * 2;
    if (roomCount > 16) roomCount = 16;

    int levelW = 120 + difficulty * 20;
    int levelH = 60 + difficulty * 10;

    Level level(levelW, levelH, 32);
    std::vector<LGRoom> rooms;
    std::vector<LevelGraphSwitch> switches;
    std::vector<LevelGraphRift> rifts;

    placeBorders(level, 1, m_themeA);
    placeBorders(level, 2, m_themeB);

    int curX = 3;
    int curY = levelH / 2;

    auto& templates = getRoomTemplates();

    for (int i = 0; i < roomCount; i++) {
        bool useTemplate = (m_rng() % 5 < 2) && !templates.empty();

        int rw;
        int rh;
        int templateIdx = -1;

        if (useTemplate) {
            templateIdx = m_rng() % templates.size();
            rw = templates[templateIdx].width;
            rh = templates[templateIdx].height;
        } else {
            rw = 12 + m_rng() % 16;
            rh = 8 + m_rng() % 10;
        }

        if (curX + rw >= levelW - 3) break;
        if (curY + rh >= levelH - 3) curY = levelH - rh - 3;
        if (curY < 3) curY = 3;

        rooms.push_back({curX, curY, rw, rh});

        if (useTemplate && templateIdx >= 0) {
            applyTemplate(level, curX, curY, templates[templateIdx], 1, m_themeA);
            int tmplB = templateIdx;
            for (int t = 0; t < 5; t++) {
                int candidate = m_rng() % templates.size();
                if (candidate != templateIdx) {
                    tmplB = candidate;
                    if (std::abs(templates[templateIdx].width - templates[candidate].width) > 2 ||
                        std::abs(templates[templateIdx].height - templates[candidate].height) > 2) {
                        break;
                    }
                }
            }
            applyTemplate(level, curX, curY, templates[tmplB], 2, m_themeB);
        } else {
            bool isChallenge = (m_rng() % 8 == 0) && (difficulty >= 2) && (i > 0);
            generateRoom(level, curX, curY, rw, rh, 1, m_themeA);
            generateRoom(level, curX, curY, rw, rh, 2, m_themeB);

            if (isChallenge) {
                for (int dim = 1; dim <= 2; dim++) {
                    const auto& theme = (dim == 1) ? m_themeA : m_themeB;
                    int spikeStartY = curY + rh / 2 + 1;
                    for (int sy = spikeStartY; sy < curY + rh - 1; sy++) {
                        for (int sx = curX + 1; sx < curX + rw - 1; sx++) {
                            Tile spike;
                            spike.type = TileType::Spike;
                            spike.color = theme.colors.spike;
                            level.setTile(sx, sy, dim, spike);
                        }
                    }
                    int platCount = 2 + m_rng() % 3;
                    for (int p = 0; p < platCount; p++) {
                        int platX = curX + 2 + p * ((rw - 4) / platCount);
                        int platY = curY + rh / 2;
                        for (int dx = 0; dx < 2; dx++) {
                            if (platX + dx < curX + rw - 1) {
                                Tile plat;
                                plat.type = TileType::OneWay;
                                plat.color = theme.colors.oneWay;
                                level.setTile(platX + dx, platY, dim, plat);
                            }
                        }
                    }
                }
            }
        }

        addPlatforms(level, curX, curY, rw, rh, 1, m_themeA);
        addPlatforms(level, curX, curY, rw, rh, 2, m_themeB);

        if (rw >= 10 && rh >= 6) {
            int bridgeCount = 1 + m_rng() % 2;
            for (int b = 0; b < bridgeCount; b++) {
                int bridgeDim = (m_rng() % 2 == 0) ? 1 : 2;
                const auto& theme = (bridgeDim == 1) ? m_themeA : m_themeB;
                int bx = curX + 2 + m_rng() % (rw - 5);
                int by = curY + 2 + m_rng() % (rh - 4);
                int bw = 3 + m_rng() % 3;

                for (int dx = 0; dx < bw; dx++) {
                    int tx = bx + dx;
                    if (tx >= curX + rw - 1) break;
                    int otherDim = (bridgeDim == 1) ? 2 : 1;
                    if (level.getTile(tx, by, bridgeDim).type == TileType::Empty &&
                        level.getTile(tx, by, otherDim).type == TileType::Empty) {
                        Tile plat;
                        plat.type = TileType::OneWay;
                        plat.color = theme.colors.oneWay;
                        level.setTile(tx, by, bridgeDim, plat);
                    }
                }
            }
        }

        int enemyDim = (m_rng() % 2 == 0) ? 1 : 2;
        addEnemySpawns(level, curX, curY, rw, rh,
                       enemyDim, difficulty, enemyDim == 1 ? m_themeA : m_themeB);

        curX += rw + 2 + m_rng() % 4;
        int yShift = static_cast<int>(m_rng() % 12) - 6;
        if (m_rng() % 4 == 0) {
            yShift = (m_rng() % 2 == 0) ? -10 : 10;
        }
        curY += yShift;
        curY = std::clamp(curY, 5, levelH - 15);
    }

    for (size_t i = 1; i < rooms.size(); i++) {
        int x1 = rooms[i - 1].x + rooms[i - 1].w - 1;
        int y1 = rooms[i - 1].y + rooms[i - 1].h / 2;
        int x2 = rooms[i].x;
        int y2 = rooms[i].y + rooms[i].h / 2;
        connectRooms(level, x1, y1, x2, y2, 1, m_themeA);
        connectRooms(level, x1, y1, x2, y2, 2, m_themeB);
    }

    topology.spawnRoomIndex = rooms.empty() ? -1 : 0;
    topology.exitRoomIndex = rooms.empty() ? -1 : static_cast<int>(rooms.size()) - 1;

    if (!rooms.empty()) {
        auto& first = rooms.front();
        int spawnTX = first.x + 2;
        int spawnTY = first.y + first.h - 3;
        topology.spawnTileX = spawnTX;
        topology.spawnTileY = spawnTY;
        level.setSpawnPoint({
            static_cast<float>(spawnTX * 32),
            static_cast<float>(spawnTY * 32)
        });

        for (int dim = 1; dim <= 2; dim++) {
            for (int dy = -3; dy <= 0; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int cx = spawnTX + dx;
                    int cy = spawnTY + dy;
                    if (level.inBounds(cx, cy)) {
                        Tile empty;
                        empty.type = TileType::Empty;
                        level.setTile(cx, cy, dim, empty);
                    }
                }
            }
            if (level.inBounds(spawnTX, spawnTY + 1)) {
                const auto& floorTile = level.getTile(spawnTX, spawnTY + 1, dim);
                if (!floorTile.isSolid()) {
                    Tile floor;
                    floor.type = TileType::Solid;
                    floor.color = (dim == 1) ? m_themeA.colors.solid : m_themeB.colors.solid;
                    level.setTile(spawnTX, spawnTY + 1, dim, floor);
                    level.setTile(spawnTX - 1, spawnTY + 1, dim, floor);
                    level.setTile(spawnTX + 1, spawnTY + 1, dim, floor);
                }
            }
        }

        auto& last = rooms.back();
        int exitTX = last.x + last.w - 2;
        int exitTY = last.y + last.h - 3;
        topology.exitTileX = exitTX;
        topology.exitTileY = exitTY;
        level.setExitPoint({
            static_cast<float>(exitTX * 32),
            static_cast<float>(exitTY * 32)
        });

        for (int dim = 1; dim <= 2; dim++) {
            for (int dy = -3; dy <= 0; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int cx = exitTX + dx;
                    int cy = exitTY + dy;
                    if (level.inBounds(cx, cy)) {
                        Tile empty;
                        empty.type = TileType::Empty;
                        level.setTile(cx, cy, dim, empty);
                    }
                }
            }
            if (level.inBounds(exitTX, exitTY + 1)) {
                const auto& floorTile = level.getTile(exitTX, exitTY + 1, dim);
                if (!floorTile.isSolid()) {
                    Tile floor;
                    floor.type = TileType::Solid;
                    floor.color = (dim == 1) ? m_themeA.colors.solid : m_themeB.colors.solid;
                    level.setTile(exitTX, exitTY + 1, dim, floor);
                    level.setTile(exitTX - 1, exitTY + 1, dim, floor);
                    level.setTile(exitTX + 1, exitTY + 1, dim, floor);
                }
            }
        }
    }

    placeDimPuzzles(level, rooms, difficulty, &switches);
    placeSecretRooms(level, rooms, difficulty);
    for (size_t i = 1; i < rooms.size(); i++) {
        int x1 = rooms[i - 1].x + rooms[i - 1].w - 1;
        int y1 = rooms[i - 1].y + rooms[i - 1].h / 2;
        int x2 = rooms[i].x;
        int y2 = rooms[i].y + rooms[i].h / 2;
        restoreMainPathTraversal(level, x1, y1, x2, y2, 1, m_themeA);
        restoreMainPathTraversal(level, x1, y1, x2, y2, 2, m_themeB);
    }
    placeRandomEvents(level, rooms, difficulty);
    placeNPCs(level, rooms, difficulty);

    int riftCount = getDimensionShiftRiftCount(difficulty);
    addRifts(level, rooms, difficulty, riftCount, &rifts);

    topology.rooms.reserve(rooms.size());
    for (const auto& room : rooms) {
        LevelGraphRoom graphRoom;
        graphRoom.x = room.x;
        graphRoom.y = room.y;
        graphRoom.w = room.w;
        graphRoom.h = room.h;
        topology.rooms.push_back(graphRoom);
    }

    topology.switches = switches;
    topology.rifts = rifts;

    for (size_t i = 1; i < rooms.size(); i++) {
        int yPrev = rooms[i - 1].y + rooms[i - 1].h / 2;
        int yCurr = rooms[i].y + rooms[i].h / 2;
        for (int dim = 1; dim <= 2; dim++) {
            LevelGraphEdge edge;
            edge.fromRoom = static_cast<int>(i) - 1;
            edge.toRoom = static_cast<int>(i);
            edge.dimension = dim;
            edge.type = (std::abs(yPrev - yCurr) <= 2)
                ? LevelGraphEdgeType::Corridor
                : LevelGraphEdgeType::VerticalShaft;
            for (const auto& sw : topology.switches) {
                if (sw.roomIndex == static_cast<int>(i) - 1 &&
                    sw.gateRoomIndex == static_cast<int>(i) &&
                    sw.gateDimension == dim) {
                    edge.requiredSwitchId = sw.pairId;
                    break;
                }
            }
            topology.edges.push_back(edge);
        }
    }

    return level;
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

    if (topology.spawnValid[1]) {
        tryVisit(topology.spawnRoomIndex, 1, 0);
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

void LevelGenerator::applyTemplate(Level& level, int startX, int startY,
                                     const RoomTemplate& tmpl, int dim,
                                     const WorldTheme& theme) {
    for (int y = 0; y < tmpl.height && y < static_cast<int>(tmpl.layout.size()); y++) {
        for (int x = 0; x < tmpl.width && x < static_cast<int>(tmpl.layout[y].size()); x++) {
            char c = tmpl.layout[y][x];
            Tile tile;
            int tx = startX + x;
            int ty = startY + y;
            if (!level.inBounds(tx, ty)) continue;

            switch (c) {
                case '#':
                    tile.type = TileType::Solid;
                    tile.color = theme.colors.solid;
                    tile.color.r = static_cast<Uint8>(std::clamp(tile.color.r + static_cast<int>(m_rng() % 20) - 10, 0, 255));
                    tile.color.g = static_cast<Uint8>(std::clamp(tile.color.g + static_cast<int>(m_rng() % 20) - 10, 0, 255));
                    tile.color.b = static_cast<Uint8>(std::clamp(tile.color.b + static_cast<int>(m_rng() % 20) - 10, 0, 255));
                    break;
                case '-':
                    tile.type = TileType::OneWay;
                    tile.color = theme.colors.oneWay;
                    break;
                case '^':
                    tile.type = TileType::Spike;
                    tile.color = theme.colors.spike;
                    break;
                case 'X':
                    tile.type = TileType::Empty;
                    if (!level.isSolid(tx, ty, dim)) {
                        level.addEnemySpawn(
                            {static_cast<float>(tx * 32), static_cast<float>(ty * 32)},
                            m_rng() % 7, dim
                        );
                    }
                    break;
                default:
                    tile.type = TileType::Empty;
                    break;
            }
            level.setTile(tx, ty, dim, tile);
        }
    }
}

void LevelGenerator::generateRoom(Level& level, int roomX, int roomY,
                                    int roomW, int roomH, int dim,
                                    const WorldTheme& theme) {
    // Create room borders
    for (int x = roomX; x < roomX + roomW; x++) {
        Tile floor;
        floor.type = TileType::Solid;
        floor.color = theme.colors.solid;
        level.setTile(x, roomY, dim, floor);
        level.setTile(x, roomY + roomH - 1, dim, floor);
    }
    for (int y = roomY; y < roomY + roomH; y++) {
        Tile wall;
        wall.type = TileType::Solid;
        wall.color = theme.colors.solid;
        level.setTile(roomX, y, dim, wall);
        level.setTile(roomX + roomW - 1, y, dim, wall);
    }

    // Clear interior
    for (int y = roomY + 1; y < roomY + roomH - 1; y++) {
        for (int x = roomX + 1; x < roomX + roomW - 1; x++) {
            Tile empty;
            empty.type = TileType::Empty;
            level.setTile(x, y, dim, empty);
        }
    }

    // Random interior features (openness reduces obstacle count and height)
    float density = theme.platformDensity;
    float openFactor = 1.0f - theme.openness; // high openness = fewer obstacles

    int obstacleCount = static_cast<int>(roomW * roomH * density * 0.02f * openFactor);
    int maxObstacleH = std::max(1, static_cast<int>(2 + (1.0f - theme.openness) * 4));

    if (roomW <= 4 || roomH <= 4) obstacleCount = 0;
    for (int i = 0; i < obstacleCount; i++) {
        int ox = roomX + 2 + m_rng() % std::max(1, roomW - 4);
        int oy = roomY + 2 + m_rng() % std::max(1, roomH - 4);
        int oh = 1 + m_rng() % maxObstacleH;

        for (int j = 0; j < oh; j++) {
            if (oy + j < roomY + roomH - 1) {
                Tile solid;
                solid.type = TileType::Solid;
                solid.color = theme.colors.solid;
                solid.color.r = static_cast<Uint8>(std::max(0, std::min(255, solid.color.r + static_cast<int>(m_rng() % 20) - 10)));
                solid.color.g = static_cast<Uint8>(std::max(0, std::min(255, solid.color.g + static_cast<int>(m_rng() % 20) - 10)));
                solid.color.b = static_cast<Uint8>(std::max(0, std::min(255, solid.color.b + static_cast<int>(m_rng() % 20) - 10)));
                level.setTile(ox, oy + j, dim, solid);
            }
        }
    }

    // Floor spikes
    if (theme.hazardDensity > 0) {
        int spikeCount = static_cast<int>(roomW * theme.hazardDensity);
        for (int i = 0; i < spikeCount; i++) {
            int sx = roomX + 2 + m_rng() % (roomW - 4);
            Tile spike;
            spike.type = TileType::Spike;
            spike.color = theme.colors.spike;
            level.setTile(sx, roomY + roomH - 2, dim, spike);
        }

        // Ceiling spikes for high-hazard themes
        if (theme.hazardDensity > 0.15f) {
            int ceilSpikes = static_cast<int>(roomW * (theme.hazardDensity - 0.1f));
            for (int i = 0; i < ceilSpikes; i++) {
                int sx = roomX + 2 + m_rng() % (roomW - 4);
                Tile spike;
                spike.type = TileType::Spike;
                spike.color = theme.colors.spike;
                level.setTile(sx, roomY + 1, dim, spike);
            }
        }

        // Fire pits (replace some floor spikes at higher hazard density)
        if (theme.hazardDensity > 0.1f) {
            int fireCount = static_cast<int>(roomW * (theme.hazardDensity - 0.05f) * 0.5f);
            for (int i = 0; i < fireCount; i++) {
                int fx = roomX + 2 + m_rng() % (roomW - 4);
                int fy = roomY + roomH - 2;
                if (level.getTile(fx, fy, dim).type != TileType::Empty &&
                    level.getTile(fx, fy, dim).type != TileType::Spike) continue;
                Tile fire;
                fire.type = TileType::Fire;
                fire.color = theme.colors.fire;
                level.setTile(fx, fy, dim, fire);
            }
        }

        // Conveyor belts (on platforms, medium+ hazard)
        if (theme.hazardDensity > 0.08f && roomW >= 8) {
            int conveyorChance = m_rng() % 4;
            if (conveyorChance == 0) {
                int cx = roomX + 2 + m_rng() % (roomW / 2);
                int cy = roomY + roomH - 2;
                int cLen = 3 + m_rng() % 4;
                int dir = m_rng() % 2; // 0=right, 1=left
                for (int ci = 0; ci < cLen && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile conv;
                    conv.type = TileType::Conveyor;
                    conv.color = theme.colors.conveyor;
                    conv.variant = dir;
                    level.setTile(cx + ci, cy, dim, conv);
                }
            }
        }

        // Laser emitters (on walls, higher difficulty)
        if (theme.hazardDensity > 0.12f && roomH >= 6) {
            int laserChance = m_rng() % 5;
            if (laserChance == 0) {
                // Place on left wall shooting right
                int ly = roomY + 2 + m_rng() % (roomH - 4);
                Tile laser;
                laser.type = TileType::LaserEmitter;
                laser.color = theme.colors.laser;
                laser.variant = 0; // shoots right
                level.setTile(roomX, ly, dim, laser);
            } else if (laserChance == 1) {
                // Place on right wall shooting left
                int ly = roomY + 2 + m_rng() % (roomH - 4);
                Tile laser;
                laser.type = TileType::LaserEmitter;
                laser.color = theme.colors.laser;
                laser.variant = 1; // shoots left
                level.setTile(roomX + roomW - 1, ly, dim, laser);
            }
        }
    }

    // Theme-specific hazards: place Ice, Crumbling, GravityWell, Teleporter based on theme
    if (theme.hazardDensity > 0 && roomW >= 6 && roomH >= 5) {
        switch (theme.id) {
        case ThemeID::FrozenWasteland: {
            // Ice patches on floor (reduced friction)
            int iceCount = 2 + m_rng() % 3;
            for (int i = 0; i < iceCount; i++) {
                int ix = roomX + 2 + m_rng() % (roomW - 4);
                int iy = roomY + roomH - 2;
                auto& existing = level.getTile(ix, iy, dim);
                if (existing.type == TileType::Spike || existing.type == TileType::Empty) {
                    Tile ice;
                    ice.type = TileType::Ice;
                    ice.color = {180, 210, 255, 255};
                    level.setTile(ix, iy, dim, ice);
                }
            }
            // Crumbling platforms (fragile ice)
            if (m_rng() % 3 == 0) {
                int cx = roomX + 2 + m_rng() % (roomW - 5);
                int cy = roomY + 2 + m_rng() % (roomH - 4);
                for (int ci = 0; ci < 3 && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile crumb;
                    crumb.type = TileType::Crumbling;
                    crumb.color = {160, 190, 220, 255};
                    level.setTile(cx + ci, cy, dim, crumb);
                }
            }
            break;
        }
        case ThemeID::VolcanicCore: {
            // Extra fire pits
            int extraFire = 2 + m_rng() % 3;
            for (int i = 0; i < extraFire; i++) {
                int fx = roomX + 2 + m_rng() % (roomW - 4);
                int fy = roomY + roomH - 2;
                auto& existing = level.getTile(fx, fy, dim);
                if (existing.type == TileType::Empty || existing.type == TileType::Spike) {
                    Tile fire;
                    fire.type = TileType::Fire;
                    fire.color = {255, 80, 20, 255};
                    level.setTile(fx, fy, dim, fire);
                }
            }
            // Crumbling platforms over fire (unstable volcanic rock)
            if (m_rng() % 3 == 0) {
                int cx = roomX + 3 + m_rng() % (roomW - 6);
                int cy = roomY + roomH - 4;
                for (int ci = 0; ci < 2; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile crumb;
                    crumb.type = TileType::Crumbling;
                    crumb.color = {100, 50, 30, 255};
                    level.setTile(cx + ci, cy, dim, crumb);
                }
            }
            break;
        }
        case ThemeID::NeonCity: {
            // Extra laser (doubled chance)
            if (m_rng() % 3 == 0 && roomH >= 6) {
                int ly = roomY + 2 + m_rng() % (roomH - 4);
                int side = m_rng() % 2;
                Tile laser;
                laser.type = TileType::LaserEmitter;
                laser.color = {0, 255, 200, 255}; // Neon cyan laser
                laser.variant = side; // 0=right, 1=left
                level.setTile(side == 0 ? roomX : roomX + roomW - 1, ly, dim, laser);
            }
            // Extra conveyor
            if (m_rng() % 3 == 0) {
                int cx = roomX + 2 + m_rng() % (roomW / 2);
                int cy = roomY + roomH - 2;
                int cLen = 3 + m_rng() % 3;
                int dir = m_rng() % 2;
                for (int ci = 0; ci < cLen && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile conv;
                    conv.type = TileType::Conveyor;
                    conv.color = {0, 200, 180, 255}; // Neon conveyor
                    conv.variant = dir;
                    level.setTile(cx + ci, cy, dim, conv);
                }
            }
            break;
        }
        case ThemeID::CrystalCavern: {
            // Gravity wells in open spaces
            if (m_rng() % 3 == 0) {
                int gx = roomX + 3 + m_rng() % (roomW - 6);
                int gy = roomY + 2 + m_rng() % (roomH - 4);
                if (level.getTile(gx, gy, dim).type == TileType::Empty) {
                    Tile grav;
                    grav.type = TileType::GravityWell;
                    grav.color = {200, 120, 255, 255};
                    grav.variant = dim; // Dimension-dependent pull direction
                    level.setTile(gx, gy, dim, grav);
                }
            }
            // Teleporter pair
            if (m_rng() % 4 == 0 && roomW >= 10) {
                int t1x = roomX + 2 + m_rng() % 3;
                int t2x = roomX + roomW - 4 + m_rng() % 2;
                int ty = roomY + roomH - 2;
                int pairId = 100 + m_rng() % 900; // High ID to avoid dim puzzle conflicts
                if (level.getTile(t1x, ty, dim).type == TileType::Empty &&
                    level.getTile(t2x, ty, dim).type == TileType::Empty) {
                    Tile tp1, tp2;
                    tp1.type = TileType::Teleporter;
                    tp1.color = {180, 100, 255, 255};
                    tp1.variant = pairId;
                    tp2.type = TileType::Teleporter;
                    tp2.color = {180, 100, 255, 255};
                    tp2.variant = pairId;
                    level.setTile(t1x, ty, dim, tp1);
                    level.setTile(t2x, ty, dim, tp2);
                }
            }
            break;
        }
        case ThemeID::DeepOcean: {
            // Conveyor currents (always)
            if (m_rng() % 2 == 0) {
                int cx = roomX + 1 + m_rng() % (roomW - 3);
                int cy = roomY + roomH - 2;
                int cLen = 4 + m_rng() % 4;
                int dir = m_rng() % 2;
                for (int ci = 0; ci < cLen && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile conv;
                    conv.type = TileType::Conveyor;
                    conv.color = {40, 100, 160, 255}; // Deep blue current
                    conv.variant = dir;
                    level.setTile(cx + ci, cy, dim, conv);
                }
            }
            // Replace fire with ice (cold deep water)
            for (int tx = roomX + 1; tx < roomX + roomW - 1; tx++) {
                for (int ty = roomY + 1; ty < roomY + roomH - 1; ty++) {
                    if (level.getTile(tx, ty, dim).type == TileType::Fire) {
                        Tile ice;
                        ice.type = TileType::Ice;
                        ice.color = {80, 140, 200, 255};
                        level.setTile(tx, ty, dim, ice);
                    }
                }
            }
            break;
        }
        case ThemeID::BioMechanical: {
            // Organic conveyors (tendrils)
            if (m_rng() % 3 == 0) {
                int cx = roomX + 2 + m_rng() % (roomW / 2);
                int cy = roomY + roomH - 2;
                int cLen = 3 + m_rng() % 3;
                for (int ci = 0; ci < cLen && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile conv;
                    conv.type = TileType::Conveyor;
                    conv.color = {120, 180, 60, 255}; // Bio-green
                    conv.variant = m_rng() % 2;
                    level.setTile(cx + ci, cy, dim, conv);
                }
            }
            // Crumbling organic floors
            if (m_rng() % 3 == 0) {
                int cx = roomX + 2 + m_rng() % (roomW - 5);
                int cy = roomY + roomH - 3;
                for (int ci = 0; ci < 2 + static_cast<int>(m_rng() % 2); ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile crumb;
                    crumb.type = TileType::Crumbling;
                    crumb.color = {80, 100, 50, 255};
                    level.setTile(cx + ci, cy, dim, crumb);
                }
            }
            break;
        }
        case ThemeID::FloatingIslands: {
            // Crumbling platforms (unstable floating rock)
            int crumbPlatforms = 1 + m_rng() % 2;
            for (int p = 0; p < crumbPlatforms; p++) {
                int cx = roomX + 2 + m_rng() % (roomW - 5);
                int cy = roomY + 2 + m_rng() % (roomH - 4);
                int len = 2 + m_rng() % 3;
                for (int ci = 0; ci < len && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile crumb;
                    crumb.type = TileType::Crumbling;
                    crumb.color = {100, 140, 100, 255};
                    level.setTile(cx + ci, cy, dim, crumb);
                }
            }
            // Gravity well (updraft)
            if (m_rng() % 3 == 0) {
                int gx = roomX + 3 + m_rng() % (roomW - 6);
                int gy = roomY + roomH - 3;
                if (level.getTile(gx, gy, dim).type == TileType::Empty) {
                    Tile grav;
                    grav.type = TileType::GravityWell;
                    grav.color = {140, 220, 140, 255};
                    grav.variant = dim;
                    level.setTile(gx, gy, dim, grav);
                }
            }
            break;
        }
        case ThemeID::VoidRealm: {
            // Gravity wells (void distortion)
            if (m_rng() % 2 == 0) {
                int gx = roomX + 3 + m_rng() % (roomW - 6);
                int gy = roomY + 2 + m_rng() % (roomH - 4);
                if (level.getTile(gx, gy, dim).type == TileType::Empty) {
                    Tile grav;
                    grav.type = TileType::GravityWell;
                    grav.color = {200, 50, 200, 255};
                    grav.variant = dim;
                    level.setTile(gx, gy, dim, grav);
                }
            }
            // Teleporter pair (void portals)
            if (m_rng() % 3 == 0 && roomW >= 8) {
                int t1x = roomX + 2;
                int t2x = roomX + roomW - 3;
                int t1y = roomY + roomH - 2;
                int t2y = roomY + 2;
                int pairId = 100 + m_rng() % 900;
                if (level.getTile(t1x, t1y, dim).type == TileType::Empty &&
                    level.getTile(t2x, t2y, dim).type == TileType::Empty) {
                    Tile tp1, tp2;
                    tp1.type = TileType::Teleporter;
                    tp1.color = {200, 50, 200, 255};
                    tp1.variant = pairId;
                    tp2.type = TileType::Teleporter;
                    tp2.color = {200, 50, 200, 255};
                    tp2.variant = pairId;
                    level.setTile(t1x, t1y, dim, tp1);
                    level.setTile(t2x, t2y, dim, tp2);
                }
            }
            break;
        }
        case ThemeID::VictorianClockwork: {
            // Extra conveyors (clockwork gears)
            if (m_rng() % 2 == 0) {
                int cx = roomX + 2 + m_rng() % (roomW / 2);
                int cy = roomY + roomH - 2;
                int cLen = 4 + m_rng() % 3;
                int dir = m_rng() % 2;
                for (int ci = 0; ci < cLen && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile conv;
                    conv.type = TileType::Conveyor;
                    conv.color = {160, 130, 80, 255}; // Brass
                    conv.variant = dir;
                    level.setTile(cx + ci, cy, dim, conv);
                }
            }
            // Extra laser (steam jet)
            if (m_rng() % 3 == 0 && roomH >= 6) {
                int ly = roomY + 2 + m_rng() % (roomH - 4);
                Tile laser;
                laser.type = TileType::LaserEmitter;
                laser.color = {200, 180, 120, 255}; // Brass steam
                laser.variant = m_rng() % 2;
                level.setTile(laser.variant == 0 ? roomX : roomX + roomW - 1, ly, dim, laser);
            }
            break;
        }
        case ThemeID::AncientRuins: {
            // Crumbling platforms (decaying stone)
            if (m_rng() % 2 == 0) {
                int cx = roomX + 2 + m_rng() % (roomW - 5);
                int cy = roomY + 2 + m_rng() % (roomH - 4);
                int len = 3 + m_rng() % 2;
                for (int ci = 0; ci < len && cx + ci < roomX + roomW - 1; ci++) {
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile crumb;
                    crumb.type = TileType::Crumbling;
                    crumb.color = {130, 120, 100, 255};
                    level.setTile(cx + ci, cy, dim, crumb);
                }
            }
            break;
        }
        case ThemeID::SpaceWestern: {
            // Lasers (blasters on walls)
            if (m_rng() % 3 == 0 && roomH >= 6) {
                int ly = roomY + 2 + m_rng() % (roomH - 4);
                Tile laser;
                laser.type = TileType::LaserEmitter;
                laser.color = {255, 180, 50, 255}; // Golden blaster
                laser.variant = m_rng() % 2;
                level.setTile(laser.variant == 0 ? roomX : roomX + roomW - 1, ly, dim, laser);
            }
            break;
        }
        case ThemeID::Biopunk: {
            // Acid pools (fire with green tint)
            int acidCount = 1 + m_rng() % 3;
            for (int i = 0; i < acidCount; i++) {
                int fx = roomX + 2 + m_rng() % (roomW - 4);
                int fy = roomY + roomH - 2;
                auto& existing = level.getTile(fx, fy, dim);
                if (existing.type == TileType::Empty || existing.type == TileType::Spike) {
                    Tile fire;
                    fire.type = TileType::Fire;
                    fire.color = {80, 255, 80, 255}; // Green acid
                    level.setTile(fx, fy, dim, fire);
                }
            }
            // Organic conveyors
            if (m_rng() % 3 == 0) {
                int cx = roomX + 2 + m_rng() % (roomW / 2);
                int cy = roomY + roomH - 2;
                for (int ci = 0; ci < 3; ci++) {
                    if (cx + ci >= roomX + roomW - 1) break;
                    if (level.getTile(cx + ci, cy, dim).type != TileType::Empty) continue;
                    Tile conv;
                    conv.type = TileType::Conveyor;
                    conv.color = {60, 200, 60, 255}; // Bio-green
                    conv.variant = m_rng() % 2;
                    level.setTile(cx + ci, cy, dim, conv);
                }
            }
            break;
        }
        }
    }

    // Decorations: subtle accent dots in empty spaces
    int decoCount = roomW / 4;
    for (int i = 0; i < decoCount; i++) {
        int dx = roomX + 2 + m_rng() % (roomW - 4);
        int dy = roomY + 2 + m_rng() % (roomH - 4);
        if (level.getTile(dx, dy, dim).type != TileType::Empty) continue;
        Tile deco;
        deco.type = TileType::Decoration;
        deco.color = theme.colors.accent;
        deco.color.a = static_cast<Uint8>(40 + m_rng() % 50);
        level.setTile(dx, dy, dim, deco);
    }
}

void LevelGenerator::connectRooms(Level& level, int x1, int y1, int x2, int y2,
                                    int dim, const WorldTheme& theme) {
    // FIX: Redesigned corridor system. Previous approach carved a single horizontal
    // corridor at min(y1,y2) which left the lower room's wall opening unreachable.
    // New approach: two horizontal segments at each room's center height, connected
    // by a vertical shaft at the midpoint. This guarantees both rooms have accessible
    // corridor openings at their center heights.

    int corridorH = 4;
    int midX = (x1 + x2) / 2;

    // Helper to carve a horizontal corridor segment
    auto carveHorizontal = [&](int fromX, int toX, int atY) {
        int lo = std::min(fromX, toX);
        int hi = std::max(fromX, toX);
        for (int x = lo; x <= hi; x++) {
            for (int dy = -1; dy < corridorH + 1; dy++) {
                int y = atY + dy;
                if (!level.inBounds(x, y)) continue;
                if (dy == -1 || dy == corridorH) {
                    // Only place wall if tile is empty (don't overwrite room interiors)
                    if (level.getTile(x, y, dim).type == TileType::Empty) {
                        Tile wall;
                        wall.type = TileType::Solid;
                        wall.color = theme.colors.solid;
                        level.setTile(x, y, dim, wall);
                    }
                } else {
                    Tile empty;
                    empty.type = TileType::Empty;
                    level.setTile(x, y, dim, empty);
                }
            }
        }
    };

    if (std::abs(y1 - y2) <= 2) {
        // Rooms at similar height: simple horizontal corridor at y1
        carveHorizontal(x1, x2, y1);
    } else {
        // Rooms at different heights:
        // Segment 1: horizontal from x1 at y1 to midX
        carveHorizontal(x1, midX, y1);
        // Segment 2: horizontal from midX at y2 to x2
        carveHorizontal(midX, x2, y2);

        // Vertical shaft at midX connecting y1 and y2
        int minY = std::min(y1, y2);
        int maxY = std::max(y1, y2) + corridorH - 1;
        for (int y = minY - 1; y <= maxY + 1; y++) {
            if (!level.inBounds(midX - 1, y) || !level.inBounds(midX + 2, y)) continue;
            for (int dx = -1; dx <= 2; dx++) {
                if (dx == -1 || dx == 2) {
                    auto& existing = level.getTile(midX + dx, y, dim);
                    if (existing.type == TileType::Empty) {
                        Tile wall;
                        wall.type = TileType::Solid;
                        wall.color = theme.colors.solid;
                        level.setTile(midX + dx, y, dim, wall);
                    }
                } else {
                    Tile empty;
                    empty.type = TileType::Empty;
                    level.setTile(midX + dx, y, dim, empty);
                }
            }
        }

        // One-way platforms in shaft for easier climbing (every 4 tiles)
        int shaftMinY = std::min(y1, y2);
        int shaftMaxY = std::max(y1, y2);
        for (int y = shaftMinY + 3; y < shaftMaxY; y += 4) {
            Tile plat;
            plat.type = TileType::OneWay;
            plat.color = theme.colors.oneWay;
            if (level.inBounds(midX, y)) level.setTile(midX, y, dim, plat);
            if (level.inBounds(midX + 1, y)) level.setTile(midX + 1, y, dim, plat);
        }
    }

    // Doorway: carve tall openings at room walls (x1 and x2) so players
    // can find and enter the corridor from anywhere in the room.
    // At x1: open from min(y1, y1-6) to y1+corridorH (generous 10-tile opening)
    // At x2: open from min(y2, y2-6) to y2+corridorH
    for (int doorX : {x1, x2}) {
        int doorY = (doorX == x1) ? y1 : y2;
        int doorTop = doorY - 6;
        int doorBot = doorY + corridorH;
        for (int y = doorTop; y <= doorBot; y++) {
            if (level.inBounds(doorX, y)) {
                Tile empty;
                empty.type = TileType::Empty;
                level.setTile(doorX, y, dim, empty);
            }
            // Also clear one tile to each side for wider doorway
            if (level.inBounds(doorX - 1, y)) {
                auto& t = level.getTile(doorX - 1, y, dim);
                if (t.isSolid()) {
                    Tile empty;
                    empty.type = TileType::Empty;
                    level.setTile(doorX - 1, y, dim, empty);
                }
            }
            if (level.inBounds(doorX + 1, y)) {
                auto& t = level.getTile(doorX + 1, y, dim);
                if (t.isSolid()) {
                    Tile empty;
                    empty.type = TileType::Empty;
                    level.setTile(doorX + 1, y, dim, empty);
                }
            }
        }
    }
}

void LevelGenerator::addPlatforms(Level& level, int startX, int startY,
                                    int w, int h, int dim, const WorldTheme& theme) {
    int platCount = static_cast<int>(w * theme.platformDensity * 0.3f);

    for (int i = 0; i < platCount; i++) {
        int px = startX + 2 + m_rng() % (w - 4);
        int py = startY + 2 + m_rng() % (h - 4);
        int pw = 2 + m_rng() % 4;

        for (int dx = 0; dx < pw; dx++) {
            if (px + dx < startX + w - 1) {
                Tile plat;
                plat.type = TileType::OneWay;
                plat.color = theme.colors.oneWay;
                level.setTile(px + dx, py, dim, plat);
            }
        }
    }
}

void LevelGenerator::addEnemySpawns(Level& level, int startX, int startY,
                                      int w, int h, int dim, int difficulty,
                                      const WorldTheme& theme) {
    float density = theme.enemyDensity;
    // Scale by room area so early floors do not round down to zero enemies.
    int count = static_cast<int>((density * static_cast<float>(w * h)) / 32.0f) + difficulty / 3;
    if (difficulty <= 2 && w >= 10 && h >= 6) {
        count = std::max(count, 1);
    }

    auto themeConfig = ThemeEnemyConfig::getConfig(theme.id);

    if (w <= 6 || h <= 4) return;
    for (int i = 0; i < count; i++) {
        int ex = startX + 3 + m_rng() % (w - 6);
        int ey = startY + 2 + m_rng() % (h - 4);

        if (level.isSolid(ex, ey, dim)) continue;

        int type;
        // 50% chance to pick a theme-preferred enemy type
        if (m_rng() % 2 == 0) {
            type = themeConfig.preferredTypes[m_rng() % 3];
        } else {
            type = m_rng() % 10; // 0-9: all enemy types
        }

        // Difficulty caps still apply
        if (difficulty < 2) type = std::min(type, 2);       // Easy: Walker, Flyer, Turret
        else if (difficulty < 3) type = std::min(type, 4);  // Medium: +Charger, Phaser
        else if (difficulty < 5) type = std::min(type, 7);  // Hard: +Exploder, Shielder, Crawler
        // Very Hard (5+): all types including Summoner, Sniper

        // Crawler should spawn near ceiling for best effect
        if (type == 7) { // Crawler
            ey = startY + 2;
        }

        // At difficulty 3+, 30% of enemies are dimension-exclusive (only in one dim),
        // forcing tactical dimension switching. Others use dim 0 (both dimensions).
        int spawnDim = 0; // default: visible in both
        if (difficulty >= 3 && (m_rng() % 100) < 30) {
            spawnDim = (m_rng() % 2 == 0) ? 1 : 2;
        }

        level.addEnemySpawn(
            {static_cast<float>(ex * 32), static_cast<float>(ey * 32)},
            type, spawnDim
        );
    }
}

void LevelGenerator::addRifts(Level& level, const std::vector<LGRoom>& rooms, int floor, int count,
                              std::vector<LevelGraphRift>* outRifts) {
    if (count <= 0 || rooms.empty()) return;

    int firstUsableRoom = (rooms.size() > 2) ? 1 : 0;
    int lastUsableRoom = (rooms.size() > 2) ? static_cast<int>(rooms.size()) - 2
                                            : static_cast<int>(rooms.size()) - 1;
    std::vector<std::pair<int, int>> usedTiles;

    auto isUnusedTile = [&](int x, int y) {
        return std::none_of(usedTiles.begin(), usedTiles.end(),
            [x, y](const auto& tile) { return tile.first == x && tile.second == y; });
    };

    int targetDimBRifts = getDimensionShiftDimBRiftCount(floor, count);
    for (int i = 0; i < count; i++) {
        int requiredDimension = getDimensionShiftRequiredDimension(floor, count, i);
        float fraction = static_cast<float>(i + 1) / (count + 1);
        int roomSpan = lastUsableRoom - firstUsableRoom + 1;
        int preferredRoom = firstUsableRoom;
        if (roomSpan > 1) {
            preferredRoom += std::clamp(static_cast<int>(fraction * roomSpan), 0, roomSpan - 1);
        }

        int rx = -1;
        int ry = -1;
        bool placed = false;
        auto tryPlaceRift = [&](int targetDimension) {
            for (int offset = 0; offset < roomSpan && !placed; offset++) {
                int candidateRoom = preferredRoom + ((offset % 2 == 0) ? offset / 2 : -((offset + 1) / 2));
                if (candidateRoom < firstUsableRoom || candidateRoom > lastUsableRoom) continue;
                if (!tryFindRiftAnchorInRoom(level, rooms[candidateRoom], m_rng, targetDimension, rx, ry)) continue;
                if (!isUnusedTile(rx, ry)) continue;

                level.addRiftPosition({
                    static_cast<float>(rx * 32),
                    static_cast<float>(ry * 32)
                });
                if (outRifts) {
                    LevelGraphRift rift;
                    rift.roomIndex = candidateRoom;
                    rift.tileX = rx;
                    rift.tileY = ry;
                    rift.requiredDimension = requiredDimension;
                    outRifts->push_back(rift);
                }
                usedTiles.push_back({rx, ry});
                placed = true;
            }
        };

        tryPlaceRift(requiredDimension);
        if (!placed) {
            tryPlaceRift(0);
        }

        if (!placed &&
            tryFindRiftAnchorInRoom(level, rooms.front(), m_rng, requiredDimension, rx, ry) &&
            isUnusedTile(rx, ry)) {
            level.addRiftPosition({
                static_cast<float>(rx * 32),
                static_cast<float>(ry * 32)
            });
            if (outRifts) {
                LevelGraphRift rift;
                rift.roomIndex = 0;
                rift.tileX = rx;
                rift.tileY = ry;
                rift.requiredDimension = requiredDimension;
                outRifts->push_back(rift);
            }
            usedTiles.push_back({rx, ry});
            placed = true;
        }

        if (!placed &&
            tryFindRiftAnchorInRoom(level, rooms.front(), m_rng, 0, rx, ry) &&
            isUnusedTile(rx, ry)) {
            level.addRiftPosition({
                static_cast<float>(rx * 32),
                static_cast<float>(ry * 32)
            });
            if (outRifts) {
                LevelGraphRift rift;
                rift.roomIndex = 0;
                rift.tileX = rx;
                rift.tileY = ry;
                rift.requiredDimension = requiredDimension;
                outRifts->push_back(rift);
            }
            usedTiles.push_back({rx, ry});
        }
    }

    if (outRifts && !outRifts->empty()) {
        int dimBPlaced = 0;
        for (const auto& rift : *outRifts) {
            if (rift.requiredDimension == 2) dimBPlaced++;
        }
        SDL_Log("LevelGenerator: floor %d rifts=%d dimB=%d/%d",
                floor,
                static_cast<int>(outRifts->size()),
                dimBPlaced,
                targetDimBRifts);
    }
}

void LevelGenerator::placeBorders(Level& level, int dim, const WorldTheme& theme) {
    int w = level.getWidth();
    int h = level.getHeight();

    Tile border;
    border.type = TileType::Solid;
    border.color = theme.colors.solid;
    border.color.r = border.color.r / 2;
    border.color.g = border.color.g / 2;
    border.color.b = border.color.b / 2;

    for (int x = 0; x < w; x++) {
        level.setTile(x, 0, dim, border);
        level.setTile(x, 1, dim, border);
        level.setTile(x, h - 1, dim, border);
        level.setTile(x, h - 2, dim, border);
    }
    for (int y = 0; y < h; y++) {
        level.setTile(0, y, dim, border);
        level.setTile(1, y, dim, border);
        level.setTile(w - 1, y, dim, border);
        level.setTile(w - 2, y, dim, border);
    }
}

void LevelGenerator::placeSecretRooms(Level& level, const std::vector<LGRoom>& rooms, int difficulty) {
    int levelW = level.getWidth();
    int levelH = level.getHeight();

    // Secret room types cycle through based on RNG
    SecretRoomType secretTypes[] = {
        SecretRoomType::TreasureVault,
        SecretRoomType::ChallengeRoom,
        SecretRoomType::ShrineRoom,
        SecretRoomType::DimensionCache,
        SecretRoomType::AncientWeapon
    };

    for (size_t i = 1; i + 1 < rooms.size(); i++) {
        // 15% chance per room (up from 10%)
        if (m_rng() % 7 != 0) continue;

        auto& room = rooms[i];
        int secretW = 6 + m_rng() % 3; // 6-8 wide
        int secretH = 5 + m_rng() % 2; // 5-6 tall

        // Place above or below the room
        bool placeAbove = (m_rng() % 2 == 0) && (room.y - secretH - 1 > 1);
        int sx = room.x + 1 + m_rng() % std::max(1, room.w - secretW - 2);
        int sy = placeAbove ? (room.y - secretH - 1) : (room.y + room.h + 1);

        if (sy < 2 || sy + secretH >= levelH - 1) continue;
        if (sx < 2 || sx + secretW >= levelW - 1) continue;

        IntRect secretBounds{sx, sy, secretW, secretH};
        IntRect paddedSecretBounds = expandRect(secretBounds, 1);
        if (overlapsMainPathGeometry(rooms, paddedSecretBounds) ||
            overlapsExistingSecretRooms(level, paddedSecretBounds)) {
            continue;
        }

        SecretRoomType sType = secretTypes[m_rng() % 5];

        // Entrance position (same for both dimensions)
        int passX = sx + secretW / 2;
        int passY = placeAbove ? (sy + secretH - 1) : sy;

        // Build the secret room in both dimensions
        for (int dim = 1; dim <= 2; dim++) {
            auto& theme = (dim == 1) ? m_themeA : m_themeB;

            // Walls
            for (int ry = 0; ry < secretH; ry++) {
                for (int rx = 0; rx < secretW; rx++) {
                    bool isWall = (ry == 0 || ry == secretH - 1 || rx == 0 || rx == secretW - 1);
                    Tile t;
                    if (isWall) {
                        t.type = TileType::Solid;
                        t.color = theme.colors.accent;
                    } else {
                        t.type = TileType::Empty;
                    }
                    level.setTile(sx + rx, sy + ry, dim, t);
                }
            }

            // Accent ceiling
            for (int rx = 1; rx < secretW - 1; rx++) {
                Tile accent;
                accent.type = TileType::Solid;
                accent.color = theme.colors.accent;
                level.setTile(sx + rx, sy, dim, accent);
            }

            // BREAKABLE WALL entrance
            Tile breakable;
            breakable.type = TileType::Breakable;
            breakable.color = theme.colors.solid;
            level.setTile(passX, passY, dim, breakable);
            // Second breakable tile for wider entrance
            if (level.inBounds(passX + 1, passY)) {
                level.setTile(passX + 1, passY, dim, breakable);
            }

            // Clear corridor behind breakable wall
            int corridorDir = placeAbove ? 1 : -1;
            Tile empty;
            empty.type = TileType::Empty;
            for (int cy = 1; cy <= 2; cy++) {
                int tileY = passY + cy * corridorDir;
                if (level.inBounds(passX, tileY)) {
                    level.setTile(passX, tileY, dim, empty);
                }
                if (level.inBounds(passX + 1, tileY)) {
                    level.setTile(passX + 1, tileY, dim, empty);
                }
            }

            // Type-specific interior
            switch (sType) {
                case SecretRoomType::TreasureVault:
                    // Gold-accented decorations
                    for (int rx = 2; rx < secretW - 2; rx++) {
                        Tile deco;
                        deco.type = TileType::Decoration;
                        deco.color = {255, 215, 60, 120};
                        level.setTile(sx + rx, sy + secretH / 2, dim, deco);
                    }
                    break;

                case SecretRoomType::ChallengeRoom:
                    // Spike border inside
                    for (int rx = 1; rx < secretW - 1; rx++) {
                        Tile spike;
                        spike.type = TileType::Spike;
                        spike.color = theme.colors.spike;
                        level.setTile(sx + rx, sy + secretH - 2, dim, spike);
                    }
                    break;

                case SecretRoomType::ShrineRoom: {
                    // Shrine base in center
                    int shrineX = sx + secretW / 2;
                    int shrineY = sy + secretH - 2;
                    Tile shrine;
                    shrine.type = TileType::ShrineBase;
                    shrine.color = {180, 120, 255, 255};
                    level.setTile(shrineX, shrineY, dim, shrine);
                    break;
                }

                case SecretRoomType::DimensionCache:
                    // Extra decorations only in one dimension
                    if (dim == 1) {
                        for (int rx = 2; rx < secretW - 2; rx++) {
                            Tile deco;
                            deco.type = TileType::Decoration;
                            deco.color = {100, 200, 255, 150};
                            level.setTile(sx + rx, sy + 2, dim, deco);
                        }
                    }
                    break;

                case SecretRoomType::AncientWeapon: {
                    // Pedestal
                    Tile pedestal;
                    pedestal.type = TileType::ShrineBase;
                    pedestal.color = {255, 180, 60, 255};
                    level.setTile(sx + secretW / 2, sy + secretH - 2, dim, pedestal);
                    break;
                }
            }
        }

        // Register secret room
        SecretRoom sr;
        sr.type = sType;
        sr.tileX = sx;
        sr.tileY = sy;
        sr.width = secretW;
        sr.height = secretH;
        sr.entranceX = passX;
        sr.entranceY = passY;
        sr.shardReward = 15 + difficulty * 10;
        sr.hpReward = (sType == SecretRoomType::TreasureVault) ? 20.0f : 0.0f;
        level.addSecretRoom(sr);

        // Challenge rooms get extra enemies
        if (sType == SecretRoomType::ChallengeRoom) {
            int enemyCount = 2 + difficulty;
            for (int e = 0; e < enemyCount; e++) {
                int ex = sx + 2 + m_rng() % std::max(1, secretW - 4);
                int ey = sy + 2;
                int spawnDim = 1 + (m_rng() % 2); // Dimension 1 or 2
                level.addEnemySpawn(
                    {static_cast<float>(ex * 32), static_cast<float>(ey * 32)},
                    m_rng() % std::min(10, 3 + difficulty), spawnDim
                );
            }
        }
    }
}

void LevelGenerator::placeRandomEvents(Level& level, const std::vector<LGRoom>& rooms, int difficulty) {
    if (rooms.size() < 3) return;

    // Place 2-4 events per level
    int eventCount = 2 + (difficulty >= 3 ? 1 : 0) + (difficulty >= 5 ? 1 : 0);

    RandomEventType eventTypes[] = {
        RandomEventType::Merchant,
        RandomEventType::Shrine,
        RandomEventType::DimensionalAnomaly,
        RandomEventType::RiftEcho,
        RandomEventType::SuitRepairStation,
        RandomEventType::GamblingRift
    };

    // Distribute events across rooms (skip first and last)
    std::vector<int> availableRooms;
    for (int i = 1; i + 1 < static_cast<int>(rooms.size()); i++) {
        availableRooms.push_back(i);
    }

    // Shuffle available rooms
    for (int i = static_cast<int>(availableRooms.size()) - 1; i > 0; i--) {
        int j = m_rng() % (i + 1);
        std::swap(availableRooms[i], availableRooms[j]);
    }

    for (int e = 0; e < eventCount && e < static_cast<int>(availableRooms.size()); e++) {
        auto& room = rooms[availableRooms[e]];

        RandomEventType eType = eventTypes[m_rng() % 6];

        // Repair station only at difficulty 3+
        if (eType == RandomEventType::SuitRepairStation && difficulty < 3) {
            eType = RandomEventType::RiftEcho;
        }
        // Gambling only at difficulty 2+
        if (eType == RandomEventType::GamblingRift && difficulty < 2) {
            eType = RandomEventType::Merchant;
        }

        // Place in an open area of the room
        int ex = room.x + 2 + m_rng() % std::max(1, room.w - 4);
        int ey = room.y + room.h - 3; // near floor

        // Find open space
        int dim = (m_rng() % 2 == 0) ? 1 : 2;
        for (int attempt = 0; attempt < 10; attempt++) {
            if (!level.isSolid(ex, ey, dim) && level.isSolid(ex, ey + 1, dim)) break;
            ex = room.x + 2 + m_rng() % std::max(1, room.w - 4);
            ey = room.y + 2 + m_rng() % std::max(1, room.h - 4);
        }

        RandomEvent event;
        event.type = eType;
        event.position = {static_cast<float>(ex * 32), static_cast<float>(ey * 32)};
        event.dimension = dim;

        switch (eType) {
            case RandomEventType::Merchant:
                event.cost = 20 + difficulty * 5; // Cheaper than shop
                break;
            case RandomEventType::Shrine:
                event.shrineType = static_cast<ShrineType>(m_rng() % static_cast<int>(ShrineType::COUNT));
                if (event.shrineType == ShrineType::Shards) {
                    event.reward = 40 + difficulty * 4; // Shard reward scales
                }
                break;
            case RandomEventType::RiftEcho:
                event.reward = 10 + difficulty * 5;
                break;
            case RandomEventType::GamblingRift:
                event.cost = 15 + difficulty * 3;
                event.reward = 30 + difficulty * 10; // Potential high reward
                break;
            default:
                break;
        }

        level.addRandomEvent(event);
    }
}

RoomTemplate LevelGenerator::getRandomRoom(int difficulty) {
    auto& templates = getRoomTemplates();
    if (templates.empty()) return {12, 8, {}};
    return templates[m_rng() % templates.size()];
}

const std::vector<RoomTemplate>& LevelGenerator::getRoomTemplates() {
    static std::vector<RoomTemplate> templates = {
        // Basic room with platforms
        {16, 10, {
            "################",
            "#..............#",
            "#..............#",
            "#....##........#",
            "#..............#",
            "#........##....#",
            "#..............#",
            "#..----..----..#",
            "#..............#",
            "################"
        }},
        // Simple symmetric
        {14, 8, {
            "##############",
            "#............#",
            "#..##....##..#",
            "#............#",
            "#............#",
            "#....----....#",
            "#............#",
            "##############"
        }},
        // Arena with 4 pillars
        {18, 10, {
            "##################",
            "#................#",
            "#..##........##..#",
            "#..##........##..#",
            "#................#",
            "#................#",
            "#..##........##..#",
            "#..##........##..#",
            "#................#",
            "##################"
        }},
        // Vertical shaft with staggered platforms
        {10, 14, {
            "##########",
            "#........#",
            "#..----..#",
            "#........#",
            "#........#",
            "#....--..#",
            "#........#",
            "#..--....#",
            "#........#",
            "#....--..#",
            "#........#",
            "#..----..#",
            "#........#",
            "##########"
        }},
        // Pit with spikes
        {16, 8, {
            "################",
            "#..............#",
            "#..............#",
            "#..............#",
            "#..----..----..#",
            "#..............#",
            "####..^^^^..####",
            "################"
        }},
        // Bridge room
        {20, 8, {
            "####################",
            "#..................#",
            "#..................#",
            "####..--------..####",
            "#..................#",
            "#..................#",
            "#..............^^..#",
            "####################"
        }},
        // Maze corridors
        {16, 12, {
            "################",
            "#..............#",
            "#.##.##.##.....#",
            "#..............#",
            "#.....##.##.##.#",
            "#..............#",
            "#.##.##........#",
            "#..............#",
            "#........##.##.#",
            "#..............#",
            "#..----..----..#",
            "################"
        }},
        // Throne room with center structure
        {20, 12, {
            "####################",
            "#..................#",
            "#..................#",
            "#......######......#",
            "#......#....#......#",
            "#......#....#......#",
            "#......######......#",
            "#..................#",
            "#..----......----..#",
            "#..................#",
            "#.^^..........^^...#",
            "####################"
        }},
        // Staircase room
        {16, 10, {
            "################",
            "#..............#",
            "#..............#",
            "#..........--..#",
            "#........--....#",
            "#......--......#",
            "#....--........#",
            "#..--..........#",
            "#..............#",
            "################"
        }},
        // Split room (two levels)
        {18, 10, {
            "##################",
            "#................#",
            "#................#",
            "#................#",
            "#.######--######.#",
            "#................#",
            "#................#",
            "#..^^........^^..#",
            "#................#",
            "##################"
        }},
    };
    return templates;
}

void LevelGenerator::placeNPCs(Level& level, const std::vector<LGRoom>& rooms, int difficulty) {
    if (rooms.size() < 4) return;

    // 1-2 NPCs per level
    int npcCount = 1 + (difficulty >= 3 ? 1 : 0);

    NPCType npcTypes[] = {
        NPCType::RiftScholar,
        NPCType::DimRefugee,
        NPCType::LostEngineer,
        NPCType::EchoOfSelf,
        NPCType::Blacksmith
    };

    // Pick rooms (skip first 2 and last)
    std::vector<int> availableRooms;
    for (int i = 2; i + 1 < static_cast<int>(rooms.size()); i++) {
        availableRooms.push_back(i);
    }
    for (int i = static_cast<int>(availableRooms.size()) - 1; i > 0; i--) {
        int j = m_rng() % (i + 1);
        std::swap(availableRooms[i], availableRooms[j]);
    }

    for (int n = 0; n < npcCount && n < static_cast<int>(availableRooms.size()); n++) {
        auto& room = rooms[availableRooms[n]];

        // Echo of Self only at difficulty 4+, Blacksmith at difficulty 2+
        NPCType type = npcTypes[m_rng() % 5];
        if (type == NPCType::EchoOfSelf && difficulty < 4) {
            type = NPCType::RiftScholar;
        }
        if (type == NPCType::Blacksmith && difficulty < 2) {
            type = NPCType::LostEngineer;
        }

        // Find open floor position
        int nx = room.x + 2 + m_rng() % std::max(1, room.w - 4);
        int ny = room.y + room.h - 3;
        int dim = (m_rng() % 2 == 0) ? 1 : 2;

        for (int attempt = 0; attempt < 10; attempt++) {
            if (!level.isSolid(nx, ny, dim) && level.isSolid(nx, ny + 1, dim)) break;
            nx = room.x + 2 + m_rng() % std::max(1, room.w - 4);
            ny = room.y + 2 + m_rng() % std::max(1, room.h - 4);
        }

        Vec2 pos = {static_cast<float>(nx * 32), static_cast<float>(ny * 32)};
        NPCData npc = NPCSystem::createNPC(type, pos, dim);
        level.addNPC(npc);
    }
}

void LevelGenerator::placeDimPuzzles(Level& level, const std::vector<LGRoom>& rooms, int difficulty,
                                     std::vector<LevelGraphSwitch>* outSwitches) {
    if (rooms.size() < 3) return;

    // Place 1-3 switch-gate pairs depending on difficulty
    int puzzleCount = std::min(1 + difficulty / 2, 3);
    int pairId = 0;

    for (int p = 0; p < puzzleCount && p < static_cast<int>(rooms.size()) - 1; p++) {
        // Pick a room for the gate (blocks path) and the previous room for the switch
        // Skip first and last rooms (spawn/exit)
        int gateRoomIdx = 2 + (p * static_cast<int>(rooms.size() - 3)) / std::max(1, puzzleCount);
        if (gateRoomIdx >= static_cast<int>(rooms.size()) - 1) gateRoomIdx = static_cast<int>(rooms.size()) - 2;
        if (gateRoomIdx < 1) gateRoomIdx = 1;
        int switchRoomIdx = gateRoomIdx - 1;

        const auto& switchRoom = rooms[switchRoomIdx];
        const auto& gateRoom = rooms[gateRoomIdx];

        // Switch goes in one dimension, gate in the other
        int switchDim = (m_rng() % 2 == 0) ? 1 : 2;
        int gateDim = (switchDim == 1) ? 2 : 1;
        const auto& switchTheme = (switchDim == 1) ? m_themeA : m_themeB;

        // Place switch: on the floor of the switch room
        int sx = switchRoom.x + 2 + m_rng() % std::max(1, switchRoom.w - 4);
        int sy = switchRoom.y + switchRoom.h - 2;

        // Find valid floor position for switch
        bool switchPlaced = false;
        for (int attempt = 0; attempt < 15; attempt++) {
            if (level.inBounds(sx, sy) && level.inBounds(sx, sy + 1) &&
                !level.isSolid(sx, sy, switchDim) &&
                level.isSolid(sx, sy + 1, switchDim)) {
                Tile switchTile;
                switchTile.type = TileType::DimSwitch;
                switchTile.color = switchTheme.colors.oneWay;
                switchTile.variant = pairId;
                level.setTile(sx, sy, switchDim, switchTile);
                switchPlaced = true;
                break;
            }
            sx = switchRoom.x + 2 + m_rng() % std::max(1, switchRoom.w - 4);
            sy = switchRoom.y + 2 + m_rng() % std::max(1, switchRoom.h - 3);
        }

        if (!switchPlaced) continue;

        // Place gate: block the entrance of the gate room (left wall area)
        // Stack 2-3 gate tiles vertically to form a proper barrier
        int gx = gateRoom.x + 1;
        int gyBase = gateRoom.y + gateRoom.h / 2 - 1;
        int gateHeight = 2 + m_rng() % 2; // 2-3 tiles tall

        bool gatePlaced = false;
        for (int attempt = 0; attempt < 10; attempt++) {
            bool valid = true;
            for (int dy = 0; dy < gateHeight; dy++) {
                int gy = gyBase + dy;
                if (!level.inBounds(gx, gy)) { valid = false; break; }
            }
            if (valid) {
                for (int dy = 0; dy < gateHeight; dy++) {
                    Tile gateTile;
                    gateTile.type = TileType::DimGate;
                    gateTile.color = {200, 60, 60, 255};
                    gateTile.variant = pairId;
                    level.setTile(gx, gyBase + dy, gateDim, gateTile);
                }
                gatePlaced = true;
                break;
            }
            gx = gateRoom.x + 1 + m_rng() % std::max(1, gateRoom.w / 3);
            gyBase = gateRoom.y + 2 + m_rng() % std::max(1, gateRoom.h - 4);
        }

        if (gatePlaced) {
            if (outSwitches) {
                LevelGraphSwitch graphSwitch;
                graphSwitch.pairId = pairId;
                graphSwitch.roomIndex = switchRoomIdx;
                graphSwitch.gateRoomIndex = gateRoomIdx;
                graphSwitch.switchDimension = switchDim;
                graphSwitch.gateDimension = gateDim;
                graphSwitch.switchTileX = sx;
                graphSwitch.switchTileY = sy;
                graphSwitch.gateTileX = gx;
                graphSwitch.gateTileY = gyBase;
                graphSwitch.gateHeight = gateHeight;
                outSwitches->push_back(graphSwitch);
            }
            pairId++;
        }
    }
}
