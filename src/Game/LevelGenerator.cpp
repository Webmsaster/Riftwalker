#include "LevelGenerator.h"
#include <algorithm>
#include <cmath>

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
    struct Room { int x, y, w, h; };
    std::vector<Room> rooms;

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
    for (size_t i = 1; i < rooms.size(); i++) {
        int x1 = rooms[i-1].x + rooms[i-1].w;
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
        }
    }

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

    for (int i = 0; i < obstacleCount; i++) {
        int ox = roomX + 2 + m_rng() % (roomW - 4);
        int oy = roomY + 2 + m_rng() % (roomH - 4);
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
    int minX = std::min(x1, x2);
    int maxX = std::max(x1, x2);
    int corridorH = 4;

    for (int x = minX; x <= maxX; x++) {
        for (int dy = -1; dy < corridorH + 1; dy++) {
            int y = std::min(y1, y2) + dy;
            if (dy == -1 || dy == corridorH) {
                Tile wall;
                wall.type = TileType::Solid;
                wall.color = theme.colors.solid;
                level.setTile(x, y, dim, wall);
            } else {
                Tile empty;
                empty.type = TileType::Empty;
                level.setTile(x, y, dim, empty);
            }
        }
    }

    if (std::abs(y1 - y2) > 2) {
        int midX = (x1 + x2) / 2;
        int minY = std::min(y1, y2);
        int maxY = std::max(y1, y2);
        for (int y = minY; y <= maxY; y++) {
            for (int dx = -1; dx <= 2; dx++) {
                if (dx == -1 || dx == 2) {
                    Tile wall;
                    wall.type = TileType::Solid;
                    wall.color = theme.colors.solid;
                    level.setTile(midX + dx, y, dim, wall);
                } else {
                    Tile empty;
                    empty.type = TileType::Empty;
                    level.setTile(midX + dx, y, dim, empty);
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
    int count = static_cast<int>(density * w * 0.15f) + difficulty / 2;

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

        // Fallback
        if (!placed) {
            rx = static_cast<int>(w * fraction);
            ry = h / 2;
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
