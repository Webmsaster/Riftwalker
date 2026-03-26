// visual_diff — Compare two PNG screenshots and output diff statistics.
// Uses pixelmatch-cpp17 for perceptual pixel comparison.
//
// Usage:
//   visual_diff reference.png actual.png [diff_output.png] [--threshold 0.1]
//
// Exit codes:
//   0 = images match (diff pixels below 0.1% of total)
//   1 = images differ
//   2 = error (missing files, dimension mismatch, etc.)

#define SDL_MAIN_HANDLED
#include <pixelmatch/pixelmatch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <optional>

struct Image {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int stride = 0; // in pixels
};

static std::optional<Image> loadPng(const char* path) {
    SDL_Surface* raw = IMG_Load(path);
    if (!raw) {
        std::fprintf(stderr, "ERROR: Cannot load '%s': %s\n", path, IMG_GetError());
        return std::nullopt;
    }

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(raw);
    if (!rgba) {
        std::fprintf(stderr, "ERROR: Cannot convert '%s': %s\n", path, SDL_GetError());
        return std::nullopt;
    }

    Image img;
    img.width = rgba->w;
    img.height = rgba->h;
    img.stride = rgba->pitch / 4;
    img.pixels.resize(rgba->pitch * rgba->h);
    std::memcpy(img.pixels.data(), rgba->pixels, img.pixels.size());
    SDL_FreeSurface(rgba);
    return img;
}

static bool savePng(const char* path, const uint8_t* data, int w, int h, int stridePixels) {
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint8_t*>(data), w, h, 32, stridePixels * 4,
        SDL_PIXELFORMAT_ABGR8888);
    if (!surf) return false;
    int result = IMG_SavePNG(surf, path);
    SDL_FreeSurface(surf);
    return result == 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: visual_diff <reference.png> <actual.png> [diff.png] [--threshold N]\n");
        return 2;
    }

    const char* refPath = argv[1];
    const char* actPath = argv[2];
    const char* diffPath = nullptr;
    float threshold = 0.1f;
    float maxDiffPercent = 0.1f; // Pass if below this percentage

    for (int i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
            threshold = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--max-diff") == 0 && i + 1 < argc) {
            maxDiffPercent = static_cast<float>(std::atof(argv[++i]));
        } else if (!diffPath) {
            diffPath = argv[i];
        }
    }

    // Initialize SDL2_image
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::fprintf(stderr, "ERROR: IMG_Init failed: %s\n", IMG_GetError());
        return 2;
    }

    auto ref = loadPng(refPath);
    auto act = loadPng(actPath);
    if (!ref || !act) {
        IMG_Quit();
        return 2;
    }

    if (ref->width != act->width || ref->height != act->height) {
        std::fprintf(stderr, "ERROR: Dimension mismatch: %dx%d vs %dx%d\n",
                     ref->width, ref->height, act->width, act->height);
        IMG_Quit();
        return 2;
    }

    // Prepare diff buffer
    std::vector<uint8_t> diffBuf(ref->pixels.size());

    pixelmatch::Options opts;
    opts.threshold = threshold;
    opts.includeAA = true;
    opts.diffColor = {255, 0, 0, 255};   // Red for differences
    opts.aaColor = {255, 255, 0, 255};    // Yellow for anti-aliasing

    int numDiff = pixelmatch::pixelmatch(
        ref->pixels, act->pixels, diffBuf,
        static_cast<size_t>(ref->width),
        static_cast<size_t>(ref->height),
        static_cast<size_t>(ref->stride),
        opts
    );

    int totalPixels = ref->width * ref->height;
    float diffPercent = (totalPixels > 0) ? (100.0f * numDiff / totalPixels) : 0;

    std::printf("Compared: %s vs %s\n", refPath, actPath);
    std::printf("Size: %dx%d (%d pixels)\n", ref->width, ref->height, totalPixels);
    std::printf("Different pixels: %d (%.2f%%)\n", numDiff, diffPercent);
    std::printf("Threshold: %.2f\n", threshold);

    if (diffPath && numDiff > 0) {
        if (savePng(diffPath, diffBuf.data(), ref->width, ref->height, ref->stride)) {
            std::printf("Diff image saved: %s\n", diffPath);
        }
    }

    // Pass if below max-diff threshold
    bool pass = diffPercent < maxDiffPercent;
    std::printf("Result: %s\n", pass ? "PASS" : "FAIL");

    IMG_Quit();
    return pass ? 0 : 1;
}
