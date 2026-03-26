#include "ScreenCapture.h"
#include <SDL2/SDL_image.h>
#include <ctime>
#include <cstdio>
#include <direct.h>

namespace ScreenCapture {

static void ensureDirectory(const std::string& path) {
    // Create parent directories (simple: handle one level of nesting)
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
        // Try to create parent of dir first (for nested dirs like screenshots/playtest/)
        size_t pos2 = dir.find_last_of("/\\");
        if (pos2 != std::string::npos) {
            std::string parentDir = dir.substr(0, pos2);
            _mkdir(parentDir.c_str());
        }
        _mkdir(dir.c_str());
    }
}

bool captureScreenshot(SDL_Renderer* renderer, const std::string& filepath) {
    // Use actual renderer output size (may differ from logical size due to HiDPI)
    int rw, rh;
    SDL_GetRendererOutputSize(renderer, &rw, &rh);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, rw, rh, 32,
                                                          SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        SDL_Log("ScreenCapture: Failed to create surface: %s", SDL_GetError());
        return false;
    }

    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch) != 0) {
        SDL_Log("ScreenCapture: Failed to read pixels: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return false;
    }

    ensureDirectory(filepath);

    int result = IMG_SavePNG(surface, filepath.c_str());
    SDL_FreeSurface(surface);

    if (result != 0) {
        SDL_Log("ScreenCapture: Failed to save PNG: %s", IMG_GetError());
        return false;
    }

    SDL_Log("ScreenCapture: Saved %s (%dx%d)", filepath.c_str(), rw, rh);
    return true;
}

std::string generateFilename(const std::string& prefix) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "screenshots/%s_%04d%02d%02d_%02d%02d%02d.png",
                  prefix.c_str(),
                  t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                  t->tm_hour, t->tm_min, t->tm_sec);
    return std::string(buf);
}

std::string generatePlaytestFilename(int run, int floor) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "screenshots/playtest/run%d_floor%d_%02d%02d%02d.png",
                  run, floor, t->tm_hour, t->tm_min, t->tm_sec);
    return std::string(buf);
}

} // namespace ScreenCapture
