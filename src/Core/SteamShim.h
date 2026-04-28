#pragma once
// Thin Steamworks shim — header-only API used throughout the codebase.
// Real Steamworks SDK calls live behind ENABLE_STEAMWORKS.
//
// Setup for full Steamworks build:
//   1. Download Steamworks SDK from https://partner.steamgames.com/
//   2. Extract steam_api64.lib + steam_api64.dll into vendor/steamworks/
//   3. Place sdk/public/steam/*.h alongside the lib
//   4. Configure: cmake -DENABLE_STEAMWORKS=ON ..
//   5. Drop steam_appid.txt next to the exe with your appID
//
// Without ENABLE_STEAMWORKS, all functions are no-ops — game runs
// stand-alone (Itch.io / DRM-free distribution).

#include <string>

namespace Steam {

// Initialize Steam. Returns true if Steam is running and we connected.
// Always returns false in the no-Steamworks build.
bool init();

// Shut down. Safe to call even if init() failed.
void shutdown();

// Pump Steam callbacks once per frame. No-op without Steamworks.
void runCallbacks();

// Set/clear/store an achievement by API name. Mirrors the in-game
// AchievementSystem so unlocks propagate to Steam automatically.
void setAchievement(const std::string& apiName);

// Cloud save: write/read the named file. Falls back to local FS in
// the no-Steamworks build, so save logic doesn't branch.
bool cloudWrite(const std::string& name, const void* data, int sizeBytes);
int  cloudRead (const std::string& name, void* outData, int maxBytes);

// Status query for HUD/diagnostics (e.g. "Connected to Steam: yes/no").
bool isOnline();

} // namespace Steam
