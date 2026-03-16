#pragma once

#include <array>
#include <vector>

enum LevelValidationFailure : int {
    LevelValidationFailure_None = 0,
    LevelValidationFailure_Spawn = 1 << 0,
    LevelValidationFailure_Exit = 1 << 1,
    LevelValidationFailure_RiftAnchor = 1 << 2,
    LevelValidationFailure_RiftReachability = 1 << 3,
    LevelValidationFailure_ExitReachability = 1 << 4,
    LevelValidationFailure_MainPath = 1 << 5,
    LevelValidationFailure_DimSwitch = 1 << 6,
    LevelValidationFailure_DimPuzzle = 1 << 7
};

enum class LevelGraphEdgeType {
    Corridor,
    VerticalShaft
};

struct LevelGraphRoom {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::array<bool, 3> hasStableFooting{};
    bool hasDimensionSwitchAnchor = false;
};

struct LevelGraphEdge {
    int fromRoom = -1;
    int toRoom = -1;
    int dimension = 1;
    LevelGraphEdgeType type = LevelGraphEdgeType::Corridor;
    int requiredSwitchId = -1;
    bool validated = false;
};

struct LevelGraphRift {
    int roomIndex = -1;
    int tileX = -1;
    int tileY = -1;
    int requiredDimension = 0;
    std::array<bool, 3> accessibleDimensions{};
    bool validated = false;
};

struct LevelGraphSwitch {
    int pairId = -1;
    int roomIndex = -1;
    int gateRoomIndex = -1;
    int switchDimension = 1;
    int gateDimension = 2;
    int switchTileX = -1;
    int switchTileY = -1;
    int gateTileX = -1;
    int gateTileY = -1;
    int gateHeight = 0;
    bool validated = false;
};

struct LevelValidationSummary {
    bool passed = false;
    int requestedSeed = 0;
    int usedSeed = 0;
    int attempt = 0;
    int failureMask = LevelValidationFailure_None;
};

struct LevelTopology {
    int spawnRoomIndex = -1;
    int exitRoomIndex = -1;
    int spawnTileX = -1;
    int spawnTileY = -1;
    int exitTileX = -1;
    int exitTileY = -1;
    std::array<bool, 3> spawnValid{};
    std::array<bool, 3> exitValid{};
    std::vector<LevelGraphRoom> rooms;
    std::vector<LevelGraphEdge> edges;
    std::vector<LevelGraphRift> rifts;
    std::vector<LevelGraphSwitch> switches;
    LevelValidationSummary validation;
};
