#include "Core/Game.h"
#include <cstdlib>
#include <ctime>
#include <cstring>

// Global flag: start in smoke test mode via --smoke CLI arg
bool g_autoSmokeTest = false;

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--smoke") == 0)
            g_autoSmokeTest = true;
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Game game;
    if (!game.init()) {
        SDL_Log("Failed to initialize game!");
        return 1;
    }

    game.run();
    game.shutdown();

    return 0;
}
