// LevelGeneratorRooms.cpp -- Split from LevelGenerator.cpp (room generation, templates, hazards)
#include "LevelGenerator.h"
#include <algorithm>
#include <cmath>

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
                case 'R':
                    tile.type = TileType::Empty;
                    if (dim == 1) { // Only place rift once (from dimension A)
                        level.addRiftPosition({
                            static_cast<float>(tx * 32),
                            static_cast<float>(ty * 32)
                        });
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
    float hazardDensity = theme.hazardDensity * m_trapDensityMult;
    if (hazardDensity > 0) {
        int spikeCount = static_cast<int>(roomW * hazardDensity);
        for (int i = 0; i < spikeCount; i++) {
            int sx = roomX + 2 + m_rng() % (roomW - 4);
            Tile spike;
            spike.type = TileType::Spike;
            spike.color = theme.colors.spike;
            level.setTile(sx, roomY + roomH - 2, dim, spike);
        }

        // Ceiling spikes for high-hazard themes
        if (hazardDensity > 0.15f) {
            int ceilSpikes = static_cast<int>(roomW * (hazardDensity - 0.1f));
            for (int i = 0; i < ceilSpikes; i++) {
                int sx = roomX + 2 + m_rng() % (roomW - 4);
                Tile spike;
                spike.type = TileType::Spike;
                spike.color = theme.colors.spike;
                level.setTile(sx, roomY + 1, dim, spike);
            }
        }

        // Fire pits (replace some floor spikes at higher hazard density)
        if (hazardDensity > 0.1f) {
            int fireCount = static_cast<int>(roomW * (hazardDensity - 0.05f) * 0.5f);
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
        if (hazardDensity > 0.08f && roomW >= 8) {
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
        if (hazardDensity > 0.12f && roomH >= 6) {
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
    if (hazardDensity > 0 && roomW >= 6 && roomH >= 5) {
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
        // Tall chimney — vertical climbing challenge with alternating ledges
        {12, 16, {
            "############",
            "#..........#",
            "#..--......#",
            "#..........#",
            "#......--..#",
            "#..........#",
            "#..--......#",
            "#..........#",
            "#......--..#",
            "#..........#",
            "#..--......#",
            "#..........#",
            "#......--..#",
            "#..........#",
            "#....XX....#",
            "############"
        }},
        // Gauntlet — wide arena with spike floor and scattered safe platforms
        {22, 10, {
            "######################",
            "#....................#",
            "#..X...........X.....#",
            "#....................#",
            "#..---....----...----#",
            "#....................#",
            "#....................#",
            "#...---....---...----#",
            "#....................#",
            "####^^####^^####^^####"
        }},
        // Rift crossroads — central rift with 4-way platform access
        {16, 12, {
            "################",
            "#..............#",
            "#..--......--..#",
            "#..............#",
            "#......RR......#",
            "#..............#",
            "#..............#",
            "#......RR......#",
            "#..............#",
            "#..--......--..#",
            "#..............#",
            "################"
        }},
        // L-shaped alcove — asymmetric with upper nook for exploration
        {18, 12, {
            "##################",
            "#.......#........#",
            "#.......#..X.....#",
            "#..X..--#........#",
            "#.......#........#",
            "#................#",
            "#................#",
            "#..--........--..#",
            "#................#",
            "#........X.......#",
            "#..............^^#",
            "##################"
        }},
        // Central pit — donut-shaped room with hazardous center hole
        {18, 12, {
            "##################",
            "#................#",
            "#..--........--..#",
            "#................#",
            "#....########....#",
            "#....#^^^^^^#....#",
            "#....#^^^^^^#....#",
            "#....########....#",
            "#................#",
            "#..--........--..#",
            "#..X..........X..#",
            "##################"
        }},
    };
    return templates;
}
