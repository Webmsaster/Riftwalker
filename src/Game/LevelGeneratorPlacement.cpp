// LevelGeneratorPlacement.cpp -- Split from LevelGenerator.cpp (rifts, events, NPCs, secrets, puzzles)
#include "LevelGenerator.h"
#include "LevelGeneratorInternal.h"
#include "NPCSystem.h"
#include "DimensionShiftBalance.h"
#include <algorithm>
#include <cmath>

namespace {
using LevelGeneratorInternal::isStableStandingTile;

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

void LevelGenerator::placeDimPuzzles(Level& level, const std::vector<LGRoom>& rooms, int difficulty,
                                     std::vector<LevelGraphSwitch>* outSwitches) {
    if (rooms.size() < 3) return;

    // Zone-based puzzle count: Zone 0=1-2, Zone 1-2=2-3, Zone 3-4=3-4
    int pZone = std::clamp((difficulty - 1) / 6, 0, 4);
    int puzzleCount = std::min(1 + pZone + (((difficulty - 1) % 6) >= 3 ? 1 : 0), 4);
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

void LevelGenerator::placeRandomEvents(Level& level, const std::vector<LGRoom>& rooms, int difficulty) {
    if (rooms.size() < 3) return;

    // Zone-based event count: 2-5 per level
    int eZone = std::clamp((difficulty - 1) / 6, 0, 4);
    int eventCount = 2 + (eZone >= 1 ? 1 : 0) + (eZone >= 3 ? 1 : 0) + (eZone >= 4 ? 1 : 0);

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

void LevelGenerator::addEnemySpawns(Level& level, int startX, int startY,
                                      int w, int h, int dim, int difficulty,
                                      const WorldTheme& theme) {
    float density = theme.enemyDensity;
    // Zone-based density scaling: significantly more enemies in later zones
    int zone = std::clamp((difficulty - 1) / 6, 0, 4);
    float zoneDensityMult = 1.0f + zone * 0.3f; // +30% per zone (was +15%)
    int count = static_cast<int>((density * zoneDensityMult * static_cast<float>(w * h)) / 32.0f) + zone;
    // Minimum enemy count scales with zone to ensure combat encounters
    int minEnemies = (zone == 0) ? 1 : (1 + zone);
    if (w >= 10 && h >= 6) {
        count = std::max(count, minEnemies);
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
            type = m_rng() % 16; // 0-15: all non-boss enemy types
        }

        // Zone-based enemy type gating (gradual unlock over 30 floors)
        int zone = std::clamp((difficulty - 1) / 6, 0, 4);
        int fiz = ((difficulty - 1) % 6) + 1;
        if (zone == 0 && fiz <= 2) type = std::min(type, 2);        // Zone1 F1-2: Walker, Flyer, Turret
        else if (zone == 0 && fiz <= 4) type = std::min(type, 4);   // Zone1 F3-4: +Charger, Phaser
        else if (zone == 0) type = std::min(type, 6);               // Zone1 F5-6: +Exploder, Shielder
        else if (zone == 1 && fiz <= 3) type = std::min(type, 13);  // Zone2 F1-3: +Crawler..Leech, Swarmer
        else if (zone == 1) type = std::min(type, 15);              // Zone2 F4+: +GravityWell, Mimic
        // Zone3+: all 16 types available

        // Crawler should spawn near ceiling for best effect
        if (type == 7) { // Crawler
            ey = startY + 2;
        }

        // Ground-based enemies need solid tile below (skip for Flyer=1, Crawler=7, GravityWell=14)
        if (type != 1 && type != 7 && type != 14 && !level.isSolid(ex, ey + 1, dim)) continue;

        // Zone-based dimension-exclusive chance: ramps from 0% to 50%
        int spawnDim = 0; // default: visible in both
        int dimExclusiveChance = zone * 12 + (((difficulty - 1) % 6) * 2);
        if (zone >= 1 && static_cast<int>(m_rng() % 100) < dimExclusiveChance) {
            spawnDim = (m_rng() % 2 == 0) ? 1 : 2;
        }

        level.addEnemySpawn(
            {static_cast<float>(ex * 32), static_cast<float>(ey * 32)},
            type, spawnDim
        );
    }
}

void LevelGenerator::placeNPCs(Level& level, const std::vector<LGRoom>& rooms, int difficulty) {
    if (rooms.size() < 4) return;

    // Zone-based NPC count: 1-3 per level
    int nZone = std::clamp((difficulty - 1) / 6, 0, 4);
    int npcCount = 1 + (nZone >= 1 ? 1 : 0) + (nZone >= 3 ? 1 : 0);

    NPCType npcTypes[] = {
        NPCType::RiftScholar,
        NPCType::DimRefugee,
        NPCType::LostEngineer,
        NPCType::EchoOfSelf,
        NPCType::Blacksmith,
        NPCType::FortuneTeller,
        NPCType::VoidMerchant
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

        bool placed = false;
        for (int attempt = 0; attempt < 10; attempt++) {
            if (!level.isSolid(nx, ny, dim) && level.isSolid(nx, ny + 1, dim)) { placed = true; break; }
            nx = room.x + 2 + m_rng() % std::max(1, room.w - 4);
            ny = room.y + 2 + m_rng() % std::max(1, room.h - 4);
        }
        if (!placed) continue; // Skip this NPC if no valid position found

        Vec2 pos = {static_cast<float>(nx * 32), static_cast<float>(ny * 32)};
        NPCData npc = NPCSystem::createNPC(type, pos, dim);
        level.addNPC(npc);
    }
}

void LevelGenerator::placeCrates(Level& level, const std::vector<LGRoom>& rooms, int difficulty) {
    if (rooms.size() < 3) return;

    // 1-3 crates per non-start/exit room
    for (size_t i = 1; i + 1 < rooms.size(); i++) {
        auto& room = rooms[i];
        if (room.w < 6 || room.h < 4) continue;

        int crateCount = 1 + static_cast<int>(m_rng() % 3); // 1-3
        for (int c = 0; c < crateCount; c++) {
            int cx = room.x + 2 + static_cast<int>(m_rng() % std::max(1, room.w - 4));
            int cy = room.y + room.h - 3;

            // Find valid floor position (empty tile with solid below)
            bool placed = false;
            for (int attempt = 0; attempt < 10; attempt++) {
                int dim = (m_rng() % 2 == 0) ? 1 : 2;
                if (level.inBounds(cx, cy) && level.inBounds(cx, cy + 1) &&
                    !level.isSolid(cx, cy, dim) && level.isSolid(cx, cy + 1, dim)) {
                    level.addCrateSpawn(
                        {static_cast<float>(cx * 32), static_cast<float>(cy * 32)},
                        dim
                    );
                    placed = true;
                    break;
                }
                cx = room.x + 2 + static_cast<int>(m_rng() % std::max(1, room.w - 4));
                cy = room.y + 2 + static_cast<int>(m_rng() % std::max(1, room.h - 4));
            }
        }
    }
}

RoomTemplate LevelGenerator::getRandomRoom(int difficulty) {
    (void)difficulty;
    auto& templates = getRoomTemplates();
    if (templates.empty()) return {12, 8, {}};
    return templates[m_rng() % templates.size()];
}
