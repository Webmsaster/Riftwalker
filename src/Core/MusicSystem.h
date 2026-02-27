#pragma once

// 5-layer dynamic music system
// Layers are volume-controlled based on game state
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

private:
    float lerp(float current, float target, float speed, float dt);

    float m_baseVol = 1.0f;      // Always on (dimension drone)
    float m_rhythmVol = 0.0f;    // >= 1 enemy nearby
    float m_intensityVol = 0.0f; // >= 3 enemies or boss
    float m_dangerVol = 0.0f;    // Low HP or high entropy
    float m_bossVol = 0.0f;      // Boss active
    float m_time = 0.0f;
};
