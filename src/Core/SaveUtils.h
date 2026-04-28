#pragma once
#include <fstream>
#include <string>
#include <functional>
#include <cstdio>
#include <SDL2/SDL.h>

// Save-format version. Bumped when on-disk save layout changes
// in a way that older code can't tolerate. Older saves with a
// LOWER format_version are still loadable (forward compatible);
// HIGHER format_versions fall back to defaults (the user is on
// an older binary than the saves).
constexpr int kSaveFormatVersion = 1;

// Helper: write the format-version line at the top of a save file.
// Pass this into atomicSave's writer callback before any other content
// so loaders see it first. Backward-compatible: existing loaders that
// do `while (cfg >> key >> value)` simply read it as another key/value.
inline void writeSaveHeader(std::ofstream& f) {
    f << "format_version " << kSaveFormatVersion << "\n";
}

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
