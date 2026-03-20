// LevelGeneratorInternal.h -- Shared inline helpers for LevelGenerator split files
#pragma once
#include "Level.h"

namespace LevelGeneratorInternal {

inline bool isSafeOccupiableTile(const Level& level, int x, int y, int dim) {
    if (!level.inBounds(x, y)) return false;
    const Tile& tile = level.getTile(x, y, dim);
    return !tile.isSolid() && !tile.isOneWay() && !tile.isDangerous();
}

inline bool hasStableSupport(const Level& level, int x, int y, int dim) {
    if (!level.inBounds(x, y + 1)) return false;
    return level.isSolid(x, y + 1, dim) || level.isOneWay(x, y + 1, dim);
}

inline bool isStableStandingTile(const Level& level, int x, int y, int dim) {
    return isSafeOccupiableTile(level, x, y, dim) && hasStableSupport(level, x, y, dim);
}

} // namespace LevelGeneratorInternal
