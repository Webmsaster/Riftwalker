#include "ResourceManager.h"
#include <SDL2/SDL_image.h>
#include <sys/stat.h>
#include <stdexcept>

// Helper: check if a file exists on disk
static bool fileExists(const std::string& path) {
    struct stat s;
    return stat(path.c_str(), &s) == 0;
}

// Helper: draw a filled rectangle onto an SDL_Surface pixel buffer (ARGB8888)
static void surfaceFillRect(SDL_Surface* surf, int x, int y, int w, int h, Uint32 color) {
    SDL_Rect r = {x, y, w, h};
    SDL_FillRect(surf, &r, color);
}

// Helper: create a 32x32 SDL_Surface with a solid color body and a darker border
static SDL_Surface* makePlaceholderSurface(int w, int h,
                                            Uint8 br, Uint8 bg, Uint8 bb,   // body color
                                            Uint8 dr, Uint8 dg, Uint8 db) { // detail color
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return nullptr;
    SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);

    Uint32 body    = SDL_MapRGBA(surf->format, br, bg, bb, 255);
    Uint32 detail  = SDL_MapRGBA(surf->format, dr, dg, db, 255);
    Uint32 outline = SDL_MapRGBA(surf->format, 20, 20, 20, 200);

    // Fill body
    SDL_FillRect(surf, nullptr, body);

    // Draw detail band (horizontal stripe in middle third)
    int bh = h / 4;
    surfaceFillRect(surf, 2, h / 2 - bh / 2, w - 4, bh, detail);

    // Draw dark outline (1px border)
    SDL_Rect top    = {0, 0,     w, 1};
    SDL_Rect bottom = {0, h - 1, w, 1};
    SDL_Rect left   = {0, 0,     1, h};
    SDL_Rect right  = {w - 1, 0, 1, h};
    SDL_FillRect(surf, &top,    outline);
    SDL_FillRect(surf, &bottom, outline);
    SDL_FillRect(surf, &left,   outline);
    SDL_FillRect(surf, &right,  outline);

    return surf;
}

ResourceManager& ResourceManager::instance() {
    static ResourceManager inst;
    return inst;
}

void ResourceManager::init(SDL_Renderer* renderer) {
    m_renderer = renderer;
}

void ResourceManager::shutdown() {
    clearAll();
}

ResourceManager::~ResourceManager() {
    shutdown();
}

SDL_Texture* ResourceManager::getTexture(const std::string& path) {
    auto it = m_textures.find(path);
    if (it != m_textures.end()) return it->second;
    if (!m_renderer) return nullptr;

    SDL_Texture* tex = IMG_LoadTexture(m_renderer, path.c_str());
    if (!tex) {
        SDL_Log("Failed to load texture %s: %s", path.c_str(), IMG_GetError());
        return nullptr;
    }
    // Force linear filtering per texture for smooth scaling (hint alone is unreliable)
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    m_textures[path] = tex;
    return tex;
}

Mix_Chunk* ResourceManager::getSound(const std::string& path) {
    auto it = m_sounds.find(path);
    if (it != m_sounds.end()) return it->second;

    Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    if (!chunk) {
        SDL_Log("Failed to load sound %s: %s", path.c_str(), Mix_GetError());
        return nullptr;
    }
    m_sounds[path] = chunk;
    return chunk;
}

Mix_Music* ResourceManager::getMusic(const std::string& path) {
    auto it = m_music.find(path);
    if (it != m_music.end()) return it->second;

    Mix_Music* mus = Mix_LoadMUS(path.c_str());
    if (!mus) {
        SDL_Log("Failed to load music %s: %s", path.c_str(), Mix_GetError());
        return nullptr;
    }
    m_music[path] = mus;
    return mus;
}

TTF_Font* ResourceManager::getFont(const std::string& path, int size) {
    std::string key = path + ":" + std::to_string(size);
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) return it->second;

    TTF_Font* font = TTF_OpenFont(path.c_str(), size);
    if (!font) {
        SDL_Log("Failed to load font %s: %s", path.c_str(), TTF_GetError());
        return nullptr;
    }
    m_fonts[key] = font;
    return font;
}

SDL_Texture* ResourceManager::createColorTexture(const std::string& name, int w, int h,
                                                   Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) return it->second;
    if (!m_renderer) return nullptr;

    SDL_Texture* tex = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, w, h);
    if (!tex) return nullptr;

    SDL_SetRenderTarget(m_renderer, tex);
    SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
    SDL_RenderClear(m_renderer);
    SDL_SetRenderTarget(m_renderer, nullptr);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    m_textures[name] = tex;
    return tex;
}

void ResourceManager::ensurePlaceholderTextures(const std::string& baseDir) {
    // Placeholder definitions: filename, w, h, body RGB, detail RGB
    struct PlaceholderDef {
        const char* filename;
        int w, h;
        Uint8 br, bg, bb; // body
        Uint8 dr, dg, db; // detail stripe
    };

    static const PlaceholderDef defs[] = {
        // Entity placeholders (32x48 for player, 32x32 for enemies, 16x16 for pickups)
        { "player.png",         32, 48,  60, 120, 220,  40,  80, 180 }, // blue player
        { "walker.png",         32, 32, 200,  40,  40, 150,  20,  20 }, // red walker
        { "flyer.png",          32, 32, 220, 200,  30, 160, 140,  20 }, // yellow flyer
        { "charger.png",        32, 32, 200,  80,  30, 150,  50,  20 }, // orange charger
        { "turret.png",         32, 32,  80,  80,  80,  50,  50,  50 }, // grey turret
        { "phaser.png",         32, 32, 120,  50, 200,  80,  30, 150 }, // purple phaser
        { "exploder.png",       32, 32, 220, 120,  30, 170,  80,  20 }, // orange-red exploder
        { "shielder.png",       32, 32,  50, 160, 200,  30, 110, 150 }, // cyan shielder
        { "crawler.png",        32, 24, 100, 180,  60,  70, 130,  40 }, // green crawler
        { "summoner.png",       32, 32, 160,  60, 160, 110,  40, 110 }, // magenta summoner
        { "sniper.png",         32, 32,  60, 100, 180,  40,  70, 130 }, // blue-grey sniper
        { "teleporter.png",     32, 32,  90,  40, 140,  60,  25, 100 }, // purple teleporter
        { "reflector.png",      32, 32, 140, 160, 190, 100, 120, 150 }, // silver reflector
        { "leech.png",          32, 24,  50, 100,  40,  30,  70,  25 }, // dark green leech
        { "swarmer.png",        16, 16, 255, 180,  50, 200, 130,  30 }, // amber swarmer
        { "gravitywell.png",    32, 32, 100,  50, 180,  60,  30, 120 }, // purple gravity well
        { "mimic.png",          32, 32, 140,  95,  45, 100,  65,  25 }, // brown mimic
        // Boss placeholders (64x64)
        { "rift_guardian.png",  64, 64, 160,  60, 220, 100,  30, 160 }, // violet boss
        { "void_wyrm.png",      64, 64,  40, 160, 120,  20, 100,  80 }, // teal boss
        { "dimensional_architect.png", 64, 64, 80, 80, 200, 50, 50, 150 }, // blue boss
        { "temporal_weaver.png",64, 64, 220, 180,  40, 160, 130,  20 }, // gold boss
        { "void_sovereign.png", 96, 96, 100,  20, 160,  60,  10, 100 }, // dark-purple boss
        { "entropy_incarnate.png",64,64,180,  30,  30, 130,  20,  20 }, // crimson boss
        // Special entity placeholders
        { "rift.png",           32, 32, 120,  40, 200,  80,  20, 150 }, // purple rift
        { "exit.png",           32, 32,  40, 200,  80,  20, 140,  50 }, // green exit
        // Tile placeholder (16x16 tileset grid cell)
        { "tile_solid.png",     16, 16,  90,  80,  70,  60,  54,  46 }, // grey-brown solid
    };

    for (auto& def : defs) {
        std::string path = baseDir + "/" + def.filename;

        // Skip if already cached
        if (m_textures.count(path)) continue;

        // If PNG already on disk, just load it
        if (fileExists(path)) {
            SDL_Texture* tex = IMG_LoadTexture(m_renderer, path.c_str());
            if (tex) { m_textures[path] = tex; }
            continue;
        }

        // Generate surface, save as PNG, then load as texture
        SDL_Surface* surf = makePlaceholderSurface(def.w, def.h,
                                                    def.br, def.bg, def.bb,
                                                    def.dr, def.dg, def.db);
        if (!surf) continue;

        if (IMG_SavePNG(surf, path.c_str()) != 0) {
            SDL_Log("ResourceManager: could not save placeholder %s: %s", path.c_str(), IMG_GetError());
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
        SDL_FreeSurface(surf);

        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            m_textures[path] = tex;
        }
    }
}

void ResourceManager::clearAll() {
    for (auto& [k, v] : m_textures) SDL_DestroyTexture(v);
    for (auto& [k, v] : m_sounds) Mix_FreeChunk(v);
    for (auto& [k, v] : m_music) Mix_FreeMusic(v);
    for (auto& [k, v] : m_fonts) TTF_CloseFont(v);
    m_textures.clear();
    m_sounds.clear();
    m_music.clear();
    m_fonts.clear();
}
