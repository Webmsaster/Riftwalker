#pragma once
#include <SDL2/SDL.h>
#include <string>

// Lightweight screenshot utility using SDL_RenderReadPixels + IMG_SavePNG.
// Call captureScreenshot() BEFORE SDL_RenderPresent().
namespace ScreenCapture {

// Save current renderer contents as PNG.
// Automatically queries actual output size (handles HiDPI).
// Returns true on success. Creates directories if needed.
bool captureScreenshot(SDL_Renderer* renderer, const std::string& filepath);

// Generate a timestamped filename: screenshots/riftwalker_YYYYMMDD_HHMMSS.png
std::string generateFilename(const std::string& prefix = "screenshot");

// Generate playtest filename: screenshots/playtest/run_N_floor_F_HHMMSS.png
std::string generatePlaytestFilename(int run, int floor);

} // namespace ScreenCapture
