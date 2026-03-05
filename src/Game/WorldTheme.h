#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

struct ThemeColors {
    SDL_Color solid{100, 100, 100, 255};
    SDL_Color oneWay{80, 80, 80, 200};
    SDL_Color spike{200, 50, 50, 255};
    SDL_Color background{20, 20, 30, 255};
    SDL_Color accent{150, 150, 200, 255};
    SDL_Color enemyPrimary{200, 60, 60, 255};
    SDL_Color enemySecondary{180, 40, 40, 255};
    SDL_Color laser{255, 40, 40, 255};
    SDL_Color fire{255, 120, 30, 255};
    SDL_Color conveyor{100, 100, 120, 255};
};

enum class ThemeID {
    VictorianClockwork,
    DeepOcean,
    NeonCity,
    AncientRuins,
    CrystalCavern,
    BioMechanical,
    FrozenWasteland,
    VolcanicCore,
    FloatingIslands,
    VoidRealm,
    SpaceWestern,
    Biopunk
};

struct WorldTheme {
    ThemeID id;
    std::string name;
    ThemeColors colors;

    // Generation parameters
    float platformDensity = 0.3f;   // How many platforms
    float enemyDensity = 0.15f;     // How many enemies
    float hazardDensity = 0.1f;     // How many spikes/traps
    float openness = 0.5f;          // 0 = tight corridors, 1 = open areas
    bool hasWater = false;
    bool hasLava = false;

    static std::vector<WorldTheme> getAllThemes();
    static const std::vector<WorldTheme>& getAllThemesRef();
    static WorldTheme getTheme(ThemeID id);
    static std::pair<WorldTheme, WorldTheme> getRandomPair(int seed);
};
