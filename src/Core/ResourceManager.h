#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <memory>

class ResourceManager {
public:
    static ResourceManager& instance();

    void init(SDL_Renderer* renderer);
    void shutdown();

    SDL_Texture* getTexture(const std::string& path);
    Mix_Chunk* getSound(const std::string& path);
    Mix_Music* getMusic(const std::string& path);
    TTF_Font* getFont(const std::string& path, int size);

    // Create colored rectangle textures (runtime, no disk I/O)
    SDL_Texture* createColorTexture(const std::string& name, int w, int h,
                                     Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);

    // Generate placeholder PNG sprites to disk (if missing) and cache them.
    // Safe to call multiple times — skips files that already exist.
    void ensurePlaceholderTextures(const std::string& baseDir = "assets/textures/placeholders");

    void clearAll();

private:
    ResourceManager() = default;
    ~ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    SDL_Renderer* m_renderer = nullptr;
    std::unordered_map<std::string, SDL_Texture*> m_textures;
    std::unordered_map<std::string, Mix_Chunk*> m_sounds;
    std::unordered_map<std::string, Mix_Music*> m_music;
    std::unordered_map<std::string, TTF_Font*> m_fonts;
};
