#include "Core/MusicSystem.h"
#include "Core/SoundGenerator.h"
#include "Core/Game.h"
#include <algorithm>
#include <cmath>

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

    // Crossfade management for procedural tracks
    if (m_crossfading) {
        m_crossfadeTimer += dt;
        float progress = std::min(m_crossfadeTimer / m_crossfadeDuration, 1.0f);

        // Volume calculation respecting g_musicVolume
        float musicVol = g_musicVolume * 128.0f * 0.7f; // 70% of music vol for tracks

        // Fade in active track
        if (m_activeChannel >= 0) {
            int vol = static_cast<int>(progress * musicVol);
            Mix_Volume(m_activeChannel, vol);
        }
        // Fade out old track
        if (m_fadingChannel >= 0) {
            int vol = static_cast<int>((1.0f - progress) * musicVol);
            Mix_Volume(m_fadingChannel, vol);
        }

        if (progress >= 1.0f) {
            // Crossfade complete - stop and free fading track
            if (m_fadingChannel >= 0) {
                Mix_HaltChannel(m_fadingChannel);
                m_fadingChannel = -1;
            }
            if (m_fadingTrack) {
                Mix_FreeChunk(m_fadingTrack);
                m_fadingTrack = nullptr;
            }
            m_crossfading = false;
        }
    } else {
        // Maintain volume tracking with g_musicVolume changes
        if (m_activeChannel >= 0 && Mix_Playing(m_activeChannel)) {
            float musicVol = g_musicVolume * 128.0f * 0.7f;
            Mix_Volume(m_activeChannel, static_cast<int>(musicVol));
        }
    }
}

float MusicSystem::getCombatIntensity() const {
    return std::max({m_rhythmVol * 0.3f, m_intensityVol * 0.6f, m_bossVol * 0.8f, m_dangerVol * 0.5f});
}

void MusicSystem::startTrack(Mix_Chunk* newTrack) {
    if (!newTrack) return;

    // Move current active track to fading slot
    if (m_fadingChannel >= 0) {
        Mix_HaltChannel(m_fadingChannel);
    }
    if (m_fadingTrack) {
        Mix_FreeChunk(m_fadingTrack);
    }
    m_fadingTrack = m_activeTrack;
    m_fadingChannel = m_activeChannel;

    // Start new track on the other channel
    m_useChannelA = !m_useChannelA;
    int channel = m_useChannelA ? MUSIC_CHANNEL_A : MUSIC_CHANNEL_B;

    m_activeTrack = newTrack;
    m_activeChannel = Mix_PlayChannel(channel, newTrack, -1); // Loop forever
    if (m_activeChannel >= 0) {
        Mix_Volume(m_activeChannel, 0); // Start silent, crossfade will bring it in
    }

    // Start crossfade
    m_crossfading = true;
    m_crossfadeTimer = 0;
}

void MusicSystem::generateAndPlayThemeTrack() {
    Mix_Chunk* track = SoundGenerator::themeMusic(m_currentThemeId, m_currentDimension);
    startTrack(track);
}

void MusicSystem::generateAndPlayBossTrack() {
    Mix_Chunk* track = SoundGenerator::bossMusic(m_bossPhase);
    startTrack(track);
}

void MusicSystem::setTheme(int themeId, int dimension) {
    if (themeId == m_currentThemeId && dimension == m_currentDimension && !m_bossActive) return;

    m_currentThemeId = themeId;
    m_currentDimension = dimension;
    m_bossActive = false;

    generateAndPlayThemeTrack();
}

void MusicSystem::setBossActive(bool active, int phase) {
    if (active == m_bossActive && (!active || phase == m_bossPhase)) return;

    m_bossActive = active;
    m_bossPhase = phase;

    if (active) {
        generateAndPlayBossTrack();
    } else {
        // Return to theme music
        generateAndPlayThemeTrack();
    }
}

void MusicSystem::setDimension(int dimension) {
    if (dimension == m_currentDimension) return;
    m_currentDimension = dimension;

    if (!m_bossActive) {
        // Regenerate theme track with new dimension's detuning
        generateAndPlayThemeTrack();
    }
}

void MusicSystem::setBossPhase(int phase) {
    if (phase == m_bossPhase || !m_bossActive) return;
    m_bossPhase = phase;
    generateAndPlayBossTrack();
}

void MusicSystem::stopAll() {
    if (m_activeChannel >= 0) {
        Mix_HaltChannel(m_activeChannel);
        m_activeChannel = -1;
    }
    if (m_fadingChannel >= 0) {
        Mix_HaltChannel(m_fadingChannel);
        m_fadingChannel = -1;
    }
    m_crossfading = false;
    m_crossfadeTimer = 0;
}

void MusicSystem::cleanup() {
    stopAll();
    if (m_activeTrack) {
        Mix_FreeChunk(m_activeTrack);
        m_activeTrack = nullptr;
    }
    if (m_fadingTrack) {
        Mix_FreeChunk(m_fadingTrack);
        m_fadingTrack = nullptr;
    }
    m_currentThemeId = -1;
    m_currentDimension = 1;
    m_bossActive = false;
    m_bossPhase = 1;
}
