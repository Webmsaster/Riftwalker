#pragma once
#include "Level.h"
#include "SecretRoom.h"
#include "RandomEvent.h"
#include "WorldTheme.h"
#include <random>

struct RoomTemplate {
    int width, height;
    std::vector<std::string> layout; // '#' = solid, '.' = empty, '-' = one-way, '^' = spike
                                     // 'S' = spawn, 'E' = exit, 'R' = rift, 'X' = enemy
};

class LevelGenerator {
public:
    LevelGenerator();

    Level generate(int difficulty, int seed);
    Level generateCandidateForTesting(int difficulty, int seed, LevelTopology& topology);
    bool validateLevelForTesting(const Level& level, LevelTopology& topology) const;
    bool revalidateLevel(Level& level) const;

    void setThemes(const WorldTheme& themeA, const WorldTheme& themeB);

private:
    Level generateCandidate(int difficulty, int seed, LevelTopology& topology);
    bool validateGeneratedLevel(const Level& level, LevelTopology& topology) const;
    void generateRoom(Level& level, int roomX, int roomY, int roomW, int roomH,
                      int dim, const WorldTheme& theme);
    void applyTemplate(Level& level, int startX, int startY,
                       const RoomTemplate& tmpl, int dim, const WorldTheme& theme);
    void connectRooms(Level& level, int x1, int y1, int x2, int y2, int dim,
                      const WorldTheme& theme);
    void addPlatforms(Level& level, int startX, int startY, int w, int h,
                      int dim, const WorldTheme& theme);
    void addEnemySpawns(Level& level, int startX, int startY, int w, int h,
                        int dim, int difficulty, const WorldTheme& theme);
    void addRifts(Level& level, const std::vector<struct LGRoom>& rooms, int floor, int count,
                  std::vector<LevelGraphRift>* outRifts = nullptr);
    void placeBorders(Level& level, int dim, const WorldTheme& theme);
    RoomTemplate getRandomRoom(int difficulty);
    void placeSecretRooms(Level& level, const std::vector<struct LGRoom>& rooms, int difficulty);
    void placeRandomEvents(Level& level, const std::vector<struct LGRoom>& rooms, int difficulty);
    void placeNPCs(Level& level, const std::vector<struct LGRoom>& rooms, int difficulty);
    void placeDimPuzzles(Level& level, const std::vector<struct LGRoom>& rooms, int difficulty,
                         std::vector<LevelGraphSwitch>* outSwitches = nullptr);

    WorldTheme m_themeA;
    WorldTheme m_themeB;
    std::mt19937 m_rng;

    static const std::vector<RoomTemplate>& getRoomTemplates();
};
