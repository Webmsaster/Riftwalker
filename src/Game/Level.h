#pragma once
#include "LevelTopology.h"
#include "Tile.h"
#include "SecretRoom.h"
#include "RandomEvent.h"
#include "NPCSystem.h"
#include "Core/Camera.h"
#include <utility>
#include <vector>
#include <SDL2/SDL.h>

class Level {
public:
    Level(int width, int height, int tileSize = 32);

    // Tile access
    Tile& getTile(int x, int y, int dimension);
    const Tile& getTile(int x, int y, int dimension) const;
    void setTile(int x, int y, int dimension, const Tile& tile);

    bool isSolid(int x, int y, int dimension) const;
    bool isOneWay(int x, int y, int dimension) const;
    bool isDimensionExclusive(int x, int y, int dimension) const;
    bool inBounds(int x, int y) const;

    // Tileset loading (returns true if loaded, uses ResourceManager)
    bool loadTileset(const std::string& path = "assets/textures/tiles/tileset_universal.png");
    bool hasTileset() const { return m_tileset != nullptr; }

    // Rendering
    void render(SDL_Renderer* renderer, const Camera& camera,
                int currentDim, float blendAlpha = 0.0f);

    // Properties
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getTileSize() const { return m_tileSize; }
    float getPixelWidth() const { return static_cast<float>(m_width * m_tileSize); }
    float getPixelHeight() const { return static_cast<float>(m_height * m_tileSize); }

    Vec2 getSpawnPoint() const { return m_spawnPoint; }
    void setSpawnPoint(Vec2 pos) { m_spawnPoint = pos; }
    Vec2 getExitPoint() const { return m_exitPoint; }
    void setExitPoint(Vec2 pos) { m_exitPoint = pos; }
    // FIX: Exit active state for visual feedback (locked until all rifts repaired)
    void setExitActive(bool active) { m_exitActive = active; }
    bool isExitActive() const { return m_exitActive; }
    const LevelTopology& getTopology() const { return m_topology; }
    void setTopology(const LevelTopology& topology) { m_topology = topology; }
    void setTopology(LevelTopology&& topology) { m_topology = std::move(topology); }
    bool hasValidatedTopology() const { return m_topology.validation.passed; }
    int getGenerationSeed() const { return m_topology.validation.usedSeed; }
    int getRequestedGenerationSeed() const { return m_topology.validation.requestedSeed; }

    // Rift locations
    std::vector<Vec2> getRiftPositions() const { return m_riftPositions; }
    void addRiftPosition(Vec2 pos) { m_riftPositions.push_back(pos); }
    int getRiftRequiredDimension(int index) const;
    bool isRiftActiveInDimension(int index, int dimension) const;

    // Enemy spawn points
    struct SpawnPoint { Vec2 position; int enemyType; int dimension; };
    std::vector<SpawnPoint> getEnemySpawns() const { return m_enemySpawns; }
    void addEnemySpawn(Vec2 pos, int type, int dim) { m_enemySpawns.push_back({pos, type, dim}); }

    // Hazard checks
    bool isInLaserBeam(float worldX, float worldY, int dimension) const;
    bool isOnConveyor(int tileX, int tileY, int dimension, int& direction) const;

    // New tile type checks
    bool isIceTile(int tileX, int tileY, int dimension) const;
    bool isGravityWell(int tileX, int tileY, int dimension) const;
    Vec2 getTeleporterDestination(int tileX, int tileY, int dimension) const;
    void updateTiles(float dt);  // Update crumbling timers etc.
    void triggerCrumble(int tileX, int tileY, int dimension);

    // Dimension puzzle: activate switch to open paired gate
    bool activateDimSwitch(int pairId, int switchDim);
    bool isDimSwitchAt(int tileX, int tileY, int dimension) const;

    // Secret rooms
    std::vector<SecretRoom>& getSecretRooms() { return m_secretRooms; }
    const std::vector<SecretRoom>& getSecretRooms() const { return m_secretRooms; }
    void addSecretRoom(const SecretRoom& room) { m_secretRooms.push_back(room); }

    // Random events
    std::vector<RandomEvent>& getRandomEvents() { return m_randomEvents; }
    const std::vector<RandomEvent>& getRandomEvents() const { return m_randomEvents; }
    void addRandomEvent(const RandomEvent& event) { m_randomEvents.push_back(event); }

    // NPCs
    std::vector<NPCData>& getNPCs() { return m_npcs; }
    const std::vector<NPCData>& getNPCs() const { return m_npcs; }
    void addNPC(const NPCData& npc) { m_npcs.push_back(npc); }

    void clear();

private:
    int m_width, m_height, m_tileSize;
    std::vector<Tile> m_tilesA; // Dimension A
    std::vector<Tile> m_tilesB; // Dimension B
    Vec2 m_spawnPoint;
    Vec2 m_exitPoint;
    bool m_exitActive = false;
    std::vector<Vec2> m_riftPositions;
    LevelTopology m_topology;
    std::vector<SpawnPoint> m_enemySpawns;
    std::vector<SecretRoom> m_secretRooms;
    std::vector<RandomEvent> m_randomEvents;
    std::vector<NPCData> m_npcs;

    // Laser emitter cache (built on first access)
    mutable bool m_laserCacheDirty = true;
    struct EmitterPos { int x, y, dim; };
    mutable std::vector<EmitterPos> m_laserEmitters;
    void buildLaserCache() const;

    SDL_Texture* m_tileset = nullptr;
    int m_tilesetCols = 16;  // Tiles per row in tileset

    int index(int x, int y) const { return y * m_width + x; }

    // Tileset rendering
    void renderTilesetTile(SDL_Renderer* renderer, SDL_Rect destRect,
                           int tilesetRow, int tilesetCol, const Tile& tile);
    int getAutoTileIndex(int tx, int ty, int dim) const;

    // Tile rendering helpers
    void renderSolidTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile,
                         int tx, int ty, int dim) const;
    void renderOneWayTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile) const;
    void renderSpikeTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile) const;
    void renderFireTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderConveyorTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderLaserEmitter(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderLaserBeams(SDL_Renderer* renderer, const Camera& camera, int dim, Uint32 ticks) const;
    void renderRift(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks,
                    int requiredDimension, bool activeInCurrentDimension) const;
    void renderExit(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks) const;
    void renderIceTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderGravityWell(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderTeleporter(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderCrumblingTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderDimSwitch(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderDimGate(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
};
