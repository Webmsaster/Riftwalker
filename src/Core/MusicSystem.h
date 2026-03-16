#pragma once
#include <SDL2/SDL_mixer.h>

// 5-layer dynamic music system + procedural track management
// Layers are volume-controlled based on game state
// Procedural tracks crossfade when theme/boss state changes
class MusicSystem {
public:
    void update(float dt, int nearEnemyCount, bool bossActive, float hpPercent, float entropy);

    // Get current volume targets (0-1) for each layer
    float getBaseVolume() const { return m_baseVol; }
    float getRhythmVolume() const { return m_rhythmVol; }
    float getIntensityVolume() const { return m_intensityVol; }
    float getDangerVolume() const { return m_dangerVol; }
    float getBossVolume() const { return m_bossVol; }

    // For the AudioManager to use when mixing ambient
    float getCombatIntensity() const;

    // Procedural music track management
    void setTheme(int themeId, int dimension);  // Start theme music
    void setBossActive(bool active, int phase = 1); // Switch to/from boss music
    void setDimension(int dimension);           // Dimension shift effect
    void stopAll();                             // Stop all tracks
    void cleanup();                             // Free all chunks

    // Crossfade state
    float getCrossfadeProgress() const { return m_crossfadeTimer / m_crossfadeDuration; }
    bool isCrossfading() const { return m_crossfading; }

    // Boss phase tracking
    int getBossPhase() const { return m_bossPhase; }
    void setBossPhase(int phase);

private:
    float lerp(float current, float target, float speed, float dt);

    float m_baseVol = 1.0f;      // Always on (dimension drone)
    float m_rhythmVol = 0.0f;    // >= 1 enemy nearby
    float m_intensityVol = 0.0f; // >= 3 enemies or boss
    float m_dangerVol = 0.0f;    // Low HP or high entropy
    float m_bossVol = 0.0f;      // Boss active
    float m_time = 0.0f;

    // Procedural music track state
    int m_currentThemeId = -1;
    int m_currentDimension = 1;
    bool m_bossActive = false;
    int m_bossPhase = 1;

    // Crossfade management
    // Two track slots: active (playing) and fading (crossfade out)
    Mix_Chunk* m_activeTrack = nullptr;
    Mix_Chunk* m_fadingTrack = nullptr;
    int m_activeChannel = -1;
    int m_fadingChannel = -1;

    bool m_crossfading = false;
    float m_crossfadeTimer = 0;
    float m_crossfadeDuration = 0.5f;

    // Channel assignment (uses channels 16-17)
    static constexpr int MUSIC_CHANNEL_A = 16;
    static constexpr int MUSIC_CHANNEL_B = 17;
    bool m_useChannelA = true; // Toggle between channels

    void startTrack(Mix_Chunk* newTrack); // Internal: crossfade to new track
    void generateAndPlayThemeTrack();
    void generateAndPlayBossTrack();
};
