#include "ResourceManager.h"
#include <stdexcept>

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

    SDL_Texture* tex = IMG_LoadTexture(m_renderer, path.c_str());
    if (!tex) {
        SDL_Log("Failed to load texture %s: %s", path.c_str(), IMG_GetError());
        return nullptr;
    }
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
