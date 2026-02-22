#include "Core/Game.h"
#include <cstdlib>
#include <ctime>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

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
