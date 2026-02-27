#pragma once
#include "Tile.h"
#include "SecretRoom.h"
#include "RandomEvent.h"
#include "Core/Camera.h"
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
    bool inBounds(int x, int y) const;

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

    // Rift locations
    std::vector<Vec2> getRiftPositions() const { return m_riftPositions; }
    void addRiftPosition(Vec2 pos) { m_riftPositions.push_back(pos); }

    // Enemy spawn points
    struct SpawnPoint { Vec2 position; int enemyType; int dimension; };
    std::vector<SpawnPoint> getEnemySpawns() const { return m_enemySpawns; }
    void addEnemySpawn(Vec2 pos, int type, int dim) { m_enemySpawns.push_back({pos, type, dim}); }

    // Hazard checks
    bool isInLaserBeam(float worldX, float worldY, int dimension) const;
    bool isOnConveyor(int tileX, int tileY, int dimension, int& direction) const;

    // Secret rooms
    std::vector<SecretRoom>& getSecretRooms() { return m_secretRooms; }
    const std::vector<SecretRoom>& getSecretRooms() const { return m_secretRooms; }
    void addSecretRoom(const SecretRoom& room) { m_secretRooms.push_back(room); }

    // Random events
    std::vector<RandomEvent>& getRandomEvents() { return m_randomEvents; }
    const std::vector<RandomEvent>& getRandomEvents() const { return m_randomEvents; }
    void addRandomEvent(const RandomEvent& event) { m_randomEvents.push_back(event); }

    void clear();

private:
    int m_width, m_height, m_tileSize;
    std::vector<Tile> m_tilesA; // Dimension A
    std::vector<Tile> m_tilesB; // Dimension B
    Vec2 m_spawnPoint;
    Vec2 m_exitPoint;
    std::vector<Vec2> m_riftPositions;
    std::vector<SpawnPoint> m_enemySpawns;
    std::vector<SecretRoom> m_secretRooms;
    std::vector<RandomEvent> m_randomEvents;

    int index(int x, int y) const { return y * m_width + x; }

    // Tile rendering helpers
    void renderSolidTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile,
                         int tx, int ty, int dim) const;
    void renderOneWayTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile) const;
    void renderSpikeTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile) const;
    void renderFireTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderConveyorTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderLaserEmitter(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const;
    void renderLaserBeams(SDL_Renderer* renderer, const Camera& camera, int dim, Uint32 ticks) const;
    void renderRift(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks) const;
    void renderExit(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks) const;
};
