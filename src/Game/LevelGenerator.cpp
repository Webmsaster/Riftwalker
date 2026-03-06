#include "LevelGenerator.h"
#include "NPCSystem.h"
#include <algorithm>
#include <cmath>

// Room data shared between generate() and helper methods
struct LGRoom { int x, y, w, h; };

LevelGenerator::LevelGenerator() : m_rng(42) {}

void LevelGenerator::setThemes(const WorldTheme& themeA, const WorldTheme& themeB) {
    m_themeA = themeA;
    m_themeB = themeB;
}

Level LevelGenerator::generate(int difficulty, int seed) {
    m_rng.seed(seed);

    int roomCount = 8 + difficulty * 2;
    if (roomCount > 16) roomCount = 16;

    // Level size in tiles
    int levelW = 120 + difficulty * 20;
    int levelH = 60 + difficulty * 10;

    Level level(levelW, levelH, 32);

    // Place borders for both dimensions
    placeBorders(level, 1, m_themeA);
    placeBorders(level, 2, m_themeB);

    // Generate rooms along a path
    std::vector<LGRoom> rooms;

    int curX = 3;
    int curY = levelH / 2;

    auto& templates = getRoomTemplates();

    for (int i = 0; i < roomCount; i++) {
        // Decide: use template (40% chance) or procedural
        bool useTemplate = (m_rng() % 5 < 2) && !templates.empty();

        int rw, rh;
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
            // Apply template to both dimensions (contrasting templates for dim B)
            applyTemplate(level, curX, curY, templates[templateIdx], 1, m_themeA);
            // Pick a contrasting template for dimension B (different size preferred)
            int tmplB = templateIdx;
            for (int t = 0; t < 5; t++) {
                int candidate = m_rng() % templates.size();
                if (candidate != templateIdx) {
                    tmplB = candidate;
                    // Prefer templates with different dimensions
                    if (std::abs(templates[templateIdx].width - templates[candidate].width) > 2 ||
                        std::abs(templates[templateIdx].height - templates[candidate].height) > 2) {
                        break;
                    }
                }
            }
            applyTemplate(level, curX, curY, templates[tmplB], 2, m_themeB);
        } else {
            // Challenge room: 12.5% chance at difficulty 2+
            bool isChallenge = (m_rng() % 8 == 0) && (difficulty >= 2) && (i > 0);
            generateRoom(level, curX, curY, rw, rh, 1, m_themeA);
            generateRoom(level, curX, curY, rw, rh, 2, m_themeB);

            if (isChallenge) {
                // Add spike pit with narrow platforms in both dimensions
                for (int dim = 1; dim <= 2; dim++) {
                    const auto& theme = (dim == 1) ? m_themeA : m_themeB;
                    // Bottom half becomes spikes
                    int spikeStartY = curY + rh / 2 + 1;
                    for (int sy = spikeStartY; sy < curY + rh - 1; sy++) {
                        for (int sx = curX + 1; sx < curX + rw - 1; sx++) {
                            Tile spike;
                            spike.type = TileType::Spike;
                            spike.color = theme.colors.spike;
                            level.setTile(sx, sy, dim, spike);
                        }
                    }
                    // Narrow platforms across the spike pit
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

        // Add platforms (procedural rooms already have some, templates may need more)
        addPlatforms(level, curX, curY, rw, rh, 1, m_themeA);
        addPlatforms(level, curX, curY, rw, rh, 2, m_themeB);

        // Dimension-exclusive bridge platforms: exist in only one dimension,
        // creating paths that require dimension-switching to navigate
        if (rw >= 10 && rh >= 6) {
            int bridgeCount = 1 + m_rng() % 2; // 1-2 per room
            for (int b = 0; b < bridgeCount; b++) {
                int bridgeDim = (m_rng() % 2 == 0) ? 1 : 2;
                const auto& theme = (bridgeDim == 1) ? m_themeA : m_themeB;
                int bx = curX + 2 + m_rng() % (rw - 5);
                int by = curY + 2 + m_rng() % (rh - 4);
                int bw = 3 + m_rng() % 3; // 3-5 tiles wide

                for (int dx = 0; dx < bw; dx++) {
                    int tx = bx + dx;
                    if (tx >= curX + rw - 1) break;
                    // Only place if empty in target dim and empty in other dim
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

        // Add enemies
        addEnemySpawns(level, curX, curY, rw, rh,
                       (m_rng() % 2 == 0) ? 1 : 2, difficulty);

        // Move to next room position
        curX += rw + 2 + m_rng() % 4;
        int yShift = static_cast<int>(m_rng() % 12) - 6;
        // 25% chance of larger vertical shift for variety
        if (m_rng() % 4 == 0) {
            yShift = (m_rng() % 2 == 0) ? -10 : 10;
        }
        curY += yShift;
        curY = std::clamp(curY, 5, levelH - 15);
    }

    // Connect rooms with corridors
    // FIX: x1 was rooms[i-1].x + rooms[i-1].w (one past the right wall),
    // so corridors never carved through room A's right wall. Now includes
    // the wall tile so both room exits are opened.
    for (size_t i = 1; i < rooms.size(); i++) {
        int x1 = rooms[i-1].x + rooms[i-1].w - 1;
        int y1 = rooms[i-1].y + rooms[i-1].h / 2;
        int x2 = rooms[i].x;
        int y2 = rooms[i].y + rooms[i].h / 2;
        connectRooms(level, x1, y1, x2, y2, 1, m_themeA);
        connectRooms(level, x1, y1, x2, y2, 2, m_themeB);
    }

    // Set spawn and exit
    if (!rooms.empty()) {
        auto& first = rooms.front();
        int spawnTX = first.x + 2;
        int spawnTY = first.y + first.h - 3;
        level.setSpawnPoint({
            static_cast<float>(spawnTX * 32),
            static_cast<float>(spawnTY * 32)
        });

        // Clear area around spawn point (3 wide, 4 tall) in both dimensions
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
            // Ensure there's a floor below spawn
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
        level.setExitPoint({
            static_cast<float>(exitTX * 32),
            static_cast<float>(exitTY * 32)
        });

        // Clear area around exit point too
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
            // FIX: Ensure floor below exit (same guarantee as spawn point)
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

    // Secret rooms with breakable wall entrances
    placeSecretRooms(level, rooms, difficulty);

    // Random events (merchant, shrine, anomaly, etc.)
    placeRandomEvents(level, rooms, difficulty);

    // NPC encounters
    placeNPCs(level, rooms, difficulty);

    // Add rifts (puzzles)
    int riftCount = 2 + difficulty;
    addRifts(level, riftCount);

    return level;
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
                                      int w, int h, int dim, int difficulty) {
    float density = (dim == 1 ? m_themeA : m_themeB).enemyDensity;
    // BALANCE: Enemy density formula reduced: 0.15 -> 0.12, diff/2 -> diff/3
    // Diff 1: ~2/room, Diff 5: ~3/room, Diff 10: ~5/room (was 2, 4, 7)
    int count = static_cast<int>(density * w * 0.12f) + difficulty / 3;

    if (w <= 6 || h <= 4) return;
    for (int i = 0; i < count; i++) {
        int ex = startX + 3 + m_rng() % (w - 6);
        int ey = startY + 2 + m_rng() % (h - 4);

        if (level.isSolid(ex, ey, dim)) continue;

        int type = m_rng() % 10; // 0-9: all enemy types
        if (difficulty < 2) type = std::min(type, 2);       // Easy: Walker, Flyer, Turret
        else if (difficulty < 3) type = std::min(type, 4);  // Medium: +Charger, Phaser
        else if (difficulty < 5) type = std::min(type, 7);  // Hard: +Exploder, Shielder, Crawler
        // Very Hard (5+): all types including Summoner, Sniper

        // Crawler should spawn near ceiling for best effect
        if (type == 7) { // Crawler
            ey = startY + 2;
        }

        level.addEnemySpawn(
            {static_cast<float>(ex * 32), static_cast<float>(ey * 32)},
            type, dim
        );
    }
}

void LevelGenerator::addRifts(Level& level, int count) {
    int w = level.getWidth();
    int h = level.getHeight();

    for (int i = 0; i < count; i++) {
        float fraction = static_cast<float>(i + 1) / (count + 1);
        int rx = static_cast<int>(w * fraction);
        // More vertical variety
        int ry = h / 4 + static_cast<int>(m_rng() % (h / 2));

        bool placed = false;
        for (int attempt = 0; attempt < 40; attempt++) {
            // Rift should be in open area (not solid in at least one dimension)
            if (!level.isSolid(rx, ry, 1) && !level.isSolid(rx, ry, 2)) {
                // Check for reachable floor within 5 tiles below
                bool hasFloor = false;
                for (int below = 1; below <= 5; below++) {
                    if (level.isSolid(rx, ry + below, 1) ||
                        level.isSolid(rx, ry + below, 2)) {
                        hasFloor = true;
                        break;
                    }
                }
                if (hasFloor) {
                    level.addRiftPosition({
                        static_cast<float>(rx * 32),
                        static_cast<float>(ry * 32)
                    });
                    placed = true;
                    break;
                }
            }
            // Spiral outward
            rx += static_cast<int>(m_rng() % 8) - 4;
            ry += static_cast<int>(m_rng() % 8) - 4;
            rx = std::clamp(rx, 4, w - 4);
            ry = std::clamp(ry, 4, h - 4);
        }

        // FIX: Fallback validates position is not inside solid tile and has reachable floor
        if (!placed) {
            rx = static_cast<int>(w * fraction);
            ry = h / 2;
            // Search downward for a valid open position with floor
            for (int scan = 0; scan < h / 2; scan++) {
                int testY = ry + scan;
                if (testY >= h - 4) break;
                if (!level.isSolid(rx, testY, 1) && !level.isSolid(rx, testY, 2)) {
                    bool hasFloor = false;
                    for (int below = 1; below <= 5; below++) {
                        if (level.isSolid(rx, testY + below, 1) ||
                            level.isSolid(rx, testY + below, 2)) {
                            hasFloor = true;
                            break;
                        }
                    }
                    if (hasFloor) {
                        ry = testY;
                        break;
                    }
                }
            }
            level.addRiftPosition({
                static_cast<float>(rx * 32),
                static_cast<float>(ry * 32)
            });
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

        SecretRoomType sType = secretTypes[m_rng() % 5];

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

            // BREAKABLE WALL entrance (replaces open passage)
            int passX = sx + secretW / 2;
            int passY = placeAbove ? sy + secretH - 1 : sy;
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
        NPCType::EchoOfSelf
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

        // Echo of Self only at difficulty 4+
        NPCType type = npcTypes[m_rng() % 4];
        if (type == NPCType::EchoOfSelf && difficulty < 4) {
            type = NPCType::RiftScholar;
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
