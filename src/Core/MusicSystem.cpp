#include "Core/MusicSystem.h"
#include <algorithm>

float MusicSystem::lerp(float current, float target, float speed, float dt) {
    float diff = target - current;
    float step = speed * dt;
    if (diff > step) return current + step;
    if (diff < -step) return current - step;
    return target;
}

void MusicSystem::update(float dt, int nearEnemyCount, bool bossActive, float hpPercent, float entropy) {
    m_time += dt;
    float fadeSpeed = 2.0f; // 0.5s transition

    // Base: always on
    m_baseVol = 1.0f;

    // Rhythm: fades in when enemies are nearby
    float rhythmTarget = (nearEnemyCount >= 1) ? 1.0f : 0.0f;
    m_rhythmVol = lerp(m_rhythmVol, rhythmTarget, fadeSpeed, dt);

    // Intensity: 3+ enemies or boss
    float intensityTarget = (nearEnemyCount >= 3 || bossActive) ? 1.0f : 0.0f;
    m_intensityVol = lerp(m_intensityVol, intensityTarget, fadeSpeed, dt);

    // Danger: low HP or high entropy
    float dangerTarget = (hpPercent < 0.3f || entropy > 80.0f) ? 1.0f : 0.0f;
    m_dangerVol = lerp(m_dangerVol, dangerTarget, fadeSpeed, dt);

    // Boss theme: boss active
    float bossTarget = bossActive ? 1.0f : 0.0f;
    m_bossVol = lerp(m_bossVol, bossTarget, fadeSpeed * 0.5f, dt);

    // Clamp all
    m_baseVol = std::clamp(m_baseVol, 0.0f, 1.0f);
    m_rhythmVol = std::clamp(m_rhythmVol, 0.0f, 1.0f);
    m_intensityVol = std::clamp(m_intensityVol, 0.0f, 1.0f);
    m_dangerVol = std::clamp(m_dangerVol, 0.0f, 1.0f);
    m_bossVol = std::clamp(m_bossVol, 0.0f, 1.0f);
}

float MusicSystem::getCombatIntensity() const {
    return std::max({m_rhythmVol * 0.3f, m_intensityVol * 0.6f, m_bossVol * 0.8f, m_dangerVol * 0.5f});
}
