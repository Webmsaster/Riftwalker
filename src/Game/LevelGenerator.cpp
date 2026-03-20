// LevelGenerator.cpp -- Core orchestrator (constructor, generateCandidate, connectRooms, addPlatforms, placeBorders)
#include "LevelGenerator.h"
#include "DimensionShiftBalance.h"
#include <algorithm>
#include <cmath>

namespace {
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
} // namespace

LevelGenerator::LevelGenerator() : m_rng(42) {}

void LevelGenerator::setThemes(const WorldTheme& themeA, const WorldTheme& themeB) {
    m_themeA = themeA;
    m_themeB = themeB;
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
