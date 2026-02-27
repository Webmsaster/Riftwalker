#pragma once
#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>

enum class SFX {
    PlayerJump, PlayerDash, PlayerLand,
    MeleeSwing, MeleeHit, RangedShot,
    EnemyHit, EnemyDeath,
    PlayerHurt, PlayerDeath,
    DimensionSwitch,
    RiftRepair, RiftFail,
    Pickup,
    MenuSelect, MenuConfirm,
    LevelComplete,
    CollapseWarning, SuitEntropyCritical,
    SpikeDamage,
    BossMultiShot, BossShieldBurst, BossTeleport,
    CrawlerDrop, SummonerSummon, SniperTelegraph,
    COUNT
};

class AudioManager {
public:
    static AudioManager& instance();

    void init();
    void shutdown();
    void generateSounds(); // create all procedural SFX

    // Music
    void playMusic(const std::string& path, int loops = -1);
    void stopMusic();
    void pauseMusic();
    void resumeMusic();
    void setMusicVolume(int volume); // 0-128

    // Sound effects (file-based)
    void playSound(const std::string& path, int loops = 0);

    // Sound effects (procedural, by enum)
    void play(SFX sfx);

    void setSFXVolume(int volume); // 0-128

    // Ambient music (procedural loops)
    void playAmbient(int dimension); // 1 or 2
    void stopAmbient();

    // Master
    void setMasterVolume(float volume); // 0.0-1.0
    float getMasterVolume() const { return m_masterVolume; }
    int getMusicVolume() const { return m_musicVolume; }
    int getSFXVolume() const { return m_sfxVolume; }
    bool isMuted() const { return m_muted; }
    void toggleMute();

private:
    AudioManager() = default;

    float m_masterVolume = 1.0f;
    int m_musicVolume = 64;
    int m_sfxVolume = 64;
    bool m_muted = false;
    bool m_initialized = false;

    Mix_Chunk* m_sfx[static_cast<int>(SFX::COUNT)] = {};

    // Ambient loops
    Mix_Chunk* m_ambientA = nullptr;
    Mix_Chunk* m_ambientB = nullptr;
    int m_ambientChannel = -1;
    int m_currentAmbientDim = 0;
};
