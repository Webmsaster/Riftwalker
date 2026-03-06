#include "Core/Game.h"
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <SDL2/SDL.h>

// Global flags for automated testing modes
bool g_autoSmokeTest = false;
bool g_autoPlaytest = false;

// Null SDL log callback — prevents all SDL_Log from writing to console/stderr
static void nullLogCallback(void*, int, SDL_LogPriority, const char*) {}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--smoke") == 0)
            g_autoSmokeTest = true;
        if (std::strcmp(argv[i], "--playtest") == 0)
            g_autoPlaytest = true;
    }

    // Automated modes: suppress ALL console output at process level
    if (g_autoSmokeTest || g_autoPlaytest) {
        freopen("NUL", "w", stderr);
        freopen("NUL", "w", stdout);
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Game game;
    if (!game.init()) {
        return 1;
    }

    // After SDL_Init: kill SDL's log output function entirely in automated modes
    if (g_autoSmokeTest || g_autoPlaytest) {
        SDL_LogSetOutputFunction(nullLogCallback, nullptr);
    }

    game.run();
    game.shutdown();

    return 0;
}
