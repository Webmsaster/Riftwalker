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

// Max dimension for saved screenshots. Large renders (2K/4K) are scaled down
// so saved PNGs stay under ~2000px — prevents "image dimension limit" errors
// when reviewing multiple screenshots via AI tools, and keeps disk usage lower.
static constexpr int MAX_SCREENSHOT_DIM = 1920;

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

    // Downscale if either dimension exceeds MAX_SCREENSHOT_DIM
    SDL_Surface* saveSurface = surface;
    bool ownsSaveSurface = false;
    if (rw > MAX_SCREENSHOT_DIM || rh > MAX_SCREENSHOT_DIM) {
        float sx = static_cast<float>(MAX_SCREENSHOT_DIM) / rw;
        float sy = static_cast<float>(MAX_SCREENSHOT_DIM) / rh;
        float scale = (sx < sy) ? sx : sy;
        int nw = static_cast<int>(rw * scale);
        int nh = static_cast<int>(rh * scale);

        SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, nw, nh, 32,
                                                             SDL_PIXELFORMAT_ARGB8888);
        if (scaled) {
            // SDL_BlitScaled uses linear scaling for smooth downsampling
            SDL_Rect srcRect = {0, 0, rw, rh};
            SDL_Rect dstRect = {0, 0, nw, nh};
            if (SDL_BlitScaled(surface, &srcRect, scaled, &dstRect) == 0) {
                saveSurface = scaled;
                ownsSaveSurface = true;
            } else {
                SDL_Log("ScreenCapture: BlitScaled failed, saving at full size: %s", SDL_GetError());
                SDL_FreeSurface(scaled);
            }
        }
    }

    ensureDirectory(filepath);

    int result = IMG_SavePNG(saveSurface, filepath.c_str());
    int savedW = saveSurface->w;
    int savedH = saveSurface->h;
    if (ownsSaveSurface) SDL_FreeSurface(saveSurface);
    SDL_FreeSurface(surface);

    if (result != 0) {
        SDL_Log("ScreenCapture: Failed to save PNG: %s", IMG_GetError());
        return false;
    }

    SDL_Log("ScreenCapture: Saved %s (%dx%d)", filepath.c_str(), savedW, savedH);
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
