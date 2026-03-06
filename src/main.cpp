#include "Core/Game.h"
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <SDL2/SDL.h>

// Global flag: start in smoke test mode via --smoke CLI arg
bool g_autoSmokeTest = false;

// Null SDL log callback — prevents all SDL_Log from writing to console/stderr
static void nullLogCallback(void*, int, SDL_LogPriority, const char*) {}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--smoke") == 0)
            g_autoSmokeTest = true;
    }

    // Smoke test: suppress ALL console output at process level to prevent
    // Windows console buffer blocking (SDL_Log writes to stderr → buffer fills → hang)
    if (g_autoSmokeTest) {
        freopen("NUL", "w", stderr);
        freopen("NUL", "w", stdout);
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Game game;
    if (!game.init()) {
        return 1;
    }

    // After SDL_Init: kill SDL's log output function entirely in smoke mode
    if (g_autoSmokeTest) {
        SDL_LogSetOutputFunction(nullLogCallback, nullptr);
    }

    game.run();
    game.shutdown();

    return 0;
}
