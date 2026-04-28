#include "SteamShim.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <fstream>

#ifdef ENABLE_STEAMWORKS
// Real Steamworks SDK headers — include only when the SDK is present.
#include "steam/steam_api.h"
#endif

namespace Steam {

#ifdef ENABLE_STEAMWORKS

namespace {
    bool s_inited = false;
}

bool init() {
    if (!SteamAPI_Init()) {
        SDL_Log("SteamAPI_Init failed — running without Steam features.");
        return false;
    }
    s_inited = true;
    SDL_Log("Connected to Steam (user: %s)",
        SteamFriends() ? SteamFriends()->GetPersonaName() : "?");
    return true;
}

void shutdown() {
    if (s_inited) {
        SteamAPI_Shutdown();
        s_inited = false;
    }
}

void runCallbacks() {
    if (s_inited) SteamAPI_RunCallbacks();
}

void setAchievement(const std::string& apiName) {
    if (!s_inited) return;
    auto* userStats = SteamUserStats();
    if (!userStats) return;
    userStats->SetAchievement(apiName.c_str());
    userStats->StoreStats();
}

bool cloudWrite(const std::string& name, const void* data, int sizeBytes) {
    if (!s_inited) return false;
    auto* remote = SteamRemoteStorage();
    if (!remote) return false;
    return remote->FileWrite(name.c_str(), data, sizeBytes);
}

int cloudRead(const std::string& name, void* outData, int maxBytes) {
    if (!s_inited) return 0;
    auto* remote = SteamRemoteStorage();
    if (!remote) return 0;
    if (!remote->FileExists(name.c_str())) return 0;
    return remote->FileRead(name.c_str(), outData, maxBytes);
}

bool isOnline() { return s_inited; }

#else // !ENABLE_STEAMWORKS — no-op stubs

bool init()       { return false; }
void shutdown()   {}
void runCallbacks() {}
void setAchievement(const std::string&) {}

bool cloudWrite(const std::string& name, const void* data, int sizeBytes) {
    // Local fallback: write to <cwd>/<name>.dat so the API works even
    // without Steam (Itch.io / DRM-free build still gets save data).
    std::ofstream f(name + ".dat", std::ios::binary);
    if (!f) return false;
    f.write(static_cast<const char*>(data), sizeBytes);
    return f.good();
}

int cloudRead(const std::string& name, void* outData, int maxBytes) {
    std::ifstream f(name + ".dat", std::ios::binary);
    if (!f) return 0;
    f.read(static_cast<char*>(outData), maxBytes);
    return static_cast<int>(f.gcount());
}

bool isOnline() { return false; }

#endif

} // namespace Steam
