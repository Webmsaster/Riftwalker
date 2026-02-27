#include "AudioManager.h"
#include "ResourceManager.h"
#include "SoundGenerator.h"
#include <SDL2/SDL.h>

AudioManager& AudioManager::instance() {
    static AudioManager inst;
    return inst;
}

void AudioManager::init() {
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 2048) < 0) {
        SDL_Log("SDL_mixer init failed: %s", Mix_GetError());
        return;
    }
    Mix_AllocateChannels(16);
    m_initialized = true;

    generateSounds();

    // Generate ambient loops
    m_ambientA = SoundGenerator::ambientDimA();
    m_ambientB = SoundGenerator::ambientDimB();
}

void AudioManager::generateSounds() {
    m_sfx[static_cast<int>(SFX::PlayerJump)]    = SoundGenerator::playerJump();
    m_sfx[static_cast<int>(SFX::PlayerDash)]    = SoundGenerator::playerDash();
    m_sfx[static_cast<int>(SFX::PlayerLand)]    = SoundGenerator::playerLand();
    m_sfx[static_cast<int>(SFX::MeleeSwing)]    = SoundGenerator::meleeSwing();
    m_sfx[static_cast<int>(SFX::MeleeHit)]      = SoundGenerator::meleeHit();
    m_sfx[static_cast<int>(SFX::RangedShot)]    = SoundGenerator::rangedShot();
    m_sfx[static_cast<int>(SFX::EnemyHit)]      = SoundGenerator::enemyHit();
    m_sfx[static_cast<int>(SFX::EnemyDeath)]    = SoundGenerator::enemyDeath();
    m_sfx[static_cast<int>(SFX::PlayerHurt)]    = SoundGenerator::playerHurt();
    m_sfx[static_cast<int>(SFX::PlayerDeath)]   = SoundGenerator::playerDeath();
    m_sfx[static_cast<int>(SFX::DimensionSwitch)] = SoundGenerator::dimensionSwitch();
    m_sfx[static_cast<int>(SFX::RiftRepair)]    = SoundGenerator::riftRepair();
    m_sfx[static_cast<int>(SFX::RiftFail)]      = SoundGenerator::riftFail();
    m_sfx[static_cast<int>(SFX::Pickup)]        = SoundGenerator::pickup();
    m_sfx[static_cast<int>(SFX::MenuSelect)]    = SoundGenerator::menuSelect();
    m_sfx[static_cast<int>(SFX::MenuConfirm)]   = SoundGenerator::menuConfirm();
    m_sfx[static_cast<int>(SFX::LevelComplete)] = SoundGenerator::levelComplete();
    m_sfx[static_cast<int>(SFX::CollapseWarning)] = SoundGenerator::collapseWarning();
    m_sfx[static_cast<int>(SFX::SuitEntropyCritical)] = SoundGenerator::suitEntropyCritical();
    m_sfx[static_cast<int>(SFX::SpikeDamage)]   = SoundGenerator::spikeDamage();
    m_sfx[static_cast<int>(SFX::BossMultiShot)]    = SoundGenerator::bossMultiShot();
    m_sfx[static_cast<int>(SFX::BossShieldBurst)]   = SoundGenerator::bossShieldBurst();
    m_sfx[static_cast<int>(SFX::BossTeleport)]      = SoundGenerator::bossTeleport();
    m_sfx[static_cast<int>(SFX::CrawlerDrop)]        = SoundGenerator::crawlerDrop();
    m_sfx[static_cast<int>(SFX::SummonerSummon)]     = SoundGenerator::summonerSummon();
    m_sfx[static_cast<int>(SFX::SniperTelegraph)]    = SoundGenerator::sniperTelegraph();
}

void AudioManager::shutdown() {
    if (m_initialized) {
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
        for (int i = 0; i < static_cast<int>(SFX::COUNT); i++) {
            if (m_sfx[i]) {
                Mix_FreeChunk(m_sfx[i]);
                m_sfx[i] = nullptr;
            }
        }
        if (m_ambientA) { Mix_FreeChunk(m_ambientA); m_ambientA = nullptr; }
        if (m_ambientB) { Mix_FreeChunk(m_ambientB); m_ambientB = nullptr; }
        m_ambientChannel = -1;
        m_currentAmbientDim = 0;
        Mix_CloseAudio();
        m_initialized = false;
    }
}

void AudioManager::playMusic(const std::string& path, int loops) {
    if (!m_initialized || m_muted) return;
    Mix_Music* music = ResourceManager::instance().getMusic(path);
    if (music) {
        Mix_PlayMusic(music, loops);
        Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
    }
}

void AudioManager::stopMusic() {
    Mix_HaltMusic();
}

void AudioManager::pauseMusic() {
    Mix_PauseMusic();
}

void AudioManager::resumeMusic() {
    Mix_ResumeMusic();
}

void AudioManager::setMusicVolume(int volume) {
    m_musicVolume = volume;
    Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
}

void AudioManager::playSound(const std::string& path, int loops) {
    if (!m_initialized || m_muted) return;
    Mix_Chunk* chunk = ResourceManager::instance().getSound(path);
    if (chunk) {
        int channel = Mix_PlayChannel(-1, chunk, loops);
        if (channel >= 0) {
            Mix_Volume(channel, static_cast<int>(m_sfxVolume * m_masterVolume));
        }
    }
}

void AudioManager::play(SFX sfx) {
    if (!m_initialized || m_muted) return;
    int idx = static_cast<int>(sfx);
    if (idx >= 0 && idx < static_cast<int>(SFX::COUNT) && m_sfx[idx]) {
        int channel = Mix_PlayChannel(-1, m_sfx[idx], 0);
        if (channel >= 0) {
            Mix_Volume(channel, static_cast<int>(m_sfxVolume * m_masterVolume));
        }
    }
}

void AudioManager::setSFXVolume(int volume) {
    m_sfxVolume = volume;
}

void AudioManager::playAmbient(int dimension) {
    if (!m_initialized || m_muted) return;
    if (dimension == m_currentAmbientDim && m_ambientChannel >= 0 && Mix_Playing(m_ambientChannel)) return;

    Mix_Chunk* chunk = (dimension == 1) ? m_ambientA : m_ambientB;
    if (!chunk) return;

    // Stop previous ambient
    if (m_ambientChannel >= 0) {
        Mix_HaltChannel(m_ambientChannel);
    }

    // Play on channel 15 (reserved), loop forever
    m_ambientChannel = Mix_PlayChannel(15, chunk, -1);
    if (m_ambientChannel >= 0) {
        Mix_Volume(m_ambientChannel, static_cast<int>(m_musicVolume * m_masterVolume * 0.6f));
    }
    m_currentAmbientDim = dimension;
}

void AudioManager::stopAmbient() {
    if (m_ambientChannel >= 0) {
        Mix_HaltChannel(m_ambientChannel);
        m_ambientChannel = -1;
        m_currentAmbientDim = 0;
    }
}

void AudioManager::setMasterVolume(float volume) {
    m_masterVolume = volume;
    Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
}

void AudioManager::toggleMute() {
    m_muted = !m_muted;
    if (m_muted) {
        Mix_VolumeMusic(0);
    } else {
        Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
    }
}
