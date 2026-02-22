#include "WorldTheme.h"
#include <cstdlib>

std::vector<WorldTheme> WorldTheme::getAllThemes() {
    return {
        {ThemeID::VictorianClockwork, "Victorian Clockwork",
         {{139, 115, 85, 255}, {110, 90, 70, 200}, {180, 60, 30, 255},
          {25, 18, 12, 255}, {185, 155, 100, 255}, {160, 80, 40, 255}, {140, 60, 30, 255}},
         0.35f, 0.15f, 0.1f, 0.4f, false, false},

        {ThemeID::DeepOcean, "Deep Ocean",
         {{30, 60, 100, 255}, {20, 50, 80, 200}, {80, 200, 200, 255},
          {5, 10, 25, 255}, {40, 120, 180, 255}, {100, 40, 60, 255}, {80, 30, 50, 255}},
         0.25f, 0.2f, 0.05f, 0.6f, true, false},

        {ThemeID::NeonCity, "Neon City",
         {{50, 50, 70, 255}, {40, 40, 55, 200}, {255, 50, 100, 255},
          {10, 8, 20, 255}, {0, 255, 200, 255}, {255, 0, 128, 255}, {200, 0, 100, 255}},
         0.4f, 0.2f, 0.15f, 0.45f, false, false},

        {ThemeID::AncientRuins, "Ancient Ruins",
         {{120, 110, 90, 255}, {100, 90, 70, 200}, {150, 50, 50, 255},
          {20, 18, 15, 255}, {170, 140, 80, 255}, {140, 70, 50, 255}, {120, 50, 40, 255}},
         0.3f, 0.15f, 0.12f, 0.5f, false, false},

        {ThemeID::CrystalCavern, "Crystal Cavern",
         {{60, 40, 80, 255}, {50, 30, 70, 200}, {200, 100, 255, 255},
          {15, 8, 25, 255}, {180, 120, 255, 255}, {120, 50, 150, 255}, {100, 40, 130, 255}},
         0.3f, 0.12f, 0.08f, 0.55f, false, false},

        {ThemeID::BioMechanical, "Bio-Mechanical",
         {{70, 80, 60, 255}, {55, 65, 45, 200}, {200, 220, 50, 255},
          {15, 18, 10, 255}, {150, 200, 80, 255}, {180, 60, 80, 255}, {150, 40, 60, 255}},
         0.35f, 0.2f, 0.15f, 0.4f, false, false},

        {ThemeID::FrozenWasteland, "Frozen Wasteland",
         {{150, 170, 200, 255}, {130, 150, 180, 200}, {100, 200, 255, 255},
          {15, 20, 35, 255}, {180, 210, 255, 255}, {80, 100, 180, 255}, {60, 80, 160, 255}},
         0.25f, 0.1f, 0.08f, 0.65f, false, false},

        {ThemeID::VolcanicCore, "Volcanic Core",
         {{80, 40, 30, 255}, {65, 30, 20, 200}, {255, 100, 30, 255},
          {20, 8, 5, 255}, {255, 150, 50, 255}, {200, 80, 30, 255}, {180, 60, 20, 255}},
         0.3f, 0.18f, 0.2f, 0.45f, false, true},

        {ThemeID::FloatingIslands, "Floating Islands",
         {{90, 130, 90, 255}, {70, 110, 70, 200}, {255, 200, 100, 255},
          {30, 40, 60, 255}, {120, 200, 120, 255}, {180, 80, 80, 255}, {160, 60, 60, 255}},
         0.2f, 0.12f, 0.05f, 0.8f, false, false},

        {ThemeID::VoidRealm, "Void Realm",
         {{30, 20, 40, 255}, {20, 15, 35, 200}, {255, 50, 255, 255},
          {5, 2, 10, 255}, {200, 50, 200, 255}, {150, 30, 150, 255}, {130, 20, 130, 255}},
         0.2f, 0.25f, 0.18f, 0.5f, false, false},

        {ThemeID::SpaceWestern, "Space Western",
         {{140, 100, 60, 255}, {120, 80, 45, 200}, {255, 180, 50, 255},
          {15, 10, 8, 255}, {220, 160, 80, 255}, {200, 100, 50, 255}, {180, 80, 40, 255}},
         0.3f, 0.18f, 0.1f, 0.55f, false, false},

        {ThemeID::Biopunk, "Biopunk",
         {{40, 70, 50, 255}, {30, 60, 40, 200}, {100, 255, 100, 255},
          {8, 15, 10, 255}, {80, 220, 80, 255}, {200, 50, 80, 255}, {180, 40, 60, 255}},
         0.35f, 0.22f, 0.12f, 0.4f, false, false},
    };
}

WorldTheme WorldTheme::getTheme(ThemeID id) {
    auto themes = getAllThemes();
    for (auto& t : themes) {
        if (t.id == id) return t;
    }
    return themes[0];
}

std::pair<WorldTheme, WorldTheme> WorldTheme::getRandomPair(int seed) {
    auto themes = getAllThemes();
    std::srand(seed);
    int a = std::rand() % themes.size();
    int b;
    do { b = std::rand() % themes.size(); } while (b == a);
    return {themes[a], themes[b]};
}
