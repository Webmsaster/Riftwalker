#pragma once
#include <fstream>
#include <string>
#include <functional>
#include <cstdio>
#include <SDL2/SDL.h>

// Atomic save: write to .tmp, flush, then rename. Old file becomes .bak.
// Prevents data loss if game crashes mid-write.
inline bool atomicSave(const std::string& path,
                       const std::function<void(std::ofstream&)>& writer) {
    std::string tmpPath = path + ".tmp";
    {
        std::ofstream f(tmpPath);
        if (!f) {
            SDL_Log("Save failed: cannot create %s", tmpPath.c_str());
            return false;
        }
        writer(f);
        f.flush();
        if (!f.good()) {
            SDL_Log("Save failed: write error for %s", tmpPath.c_str());
            std::remove(tmpPath.c_str());
            return false;
        }
    }
    // Rotate: target -> .bak, .tmp -> target
    std::remove((path + ".bak").c_str());
    std::rename(path.c_str(), (path + ".bak").c_str());
    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        SDL_Log("Save failed: rename %s -> %s", tmpPath.c_str(), path.c_str());
        std::rename((path + ".bak").c_str(), path.c_str());
        return false;
    }
    return true;
}

// Open file with .bak fallback if primary is missing or empty.
inline std::ifstream openWithBackupFallback(const std::string& path) {
    std::ifstream file(path);
    if (file.is_open() && file.peek() != std::ifstream::traits_type::eof()) return file;
    file.close();
    file.open(path + ".bak");
    if (file.is_open()) SDL_Log("Using backup save: %s.bak", path.c_str());
    return file;
}
