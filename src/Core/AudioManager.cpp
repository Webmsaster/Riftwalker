#include "AudioManager.h"
#include "ResourceManager.h"
#include "SoundGenerator.h"
#include "MusicSystem.h"
#include <SDL2/SDL.h>

AudioManager& AudioManager::instance() {
    static AudioManager inst;
    return inst;
}

void AudioManager::init() {
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048) < 0) {
        SDL_Log("SDL_mixer init failed: %s", Mix_GetError());
        return;
    }
    Mix_AllocateChannels(18); // 0-9: SFX, 10: theme ambient, 11-14: music layers, 15: ambient, 16-17: procedural music
    m_initialized = true;

    generateSounds();

    // Generate ambient loops
    m_ambientA = SoundGenerator::ambientDimA();
    m_ambientB = SoundGenerator::ambientDimB();

    // Generate dynamic music layers
    m_layerRhythm = SoundGenerator::musicRhythm();
    m_layerIntensity = SoundGenerator::musicIntensity();
    m_layerDanger = SoundGenerator::musicDanger();
    m_layerBoss = SoundGenerator::musicBoss();
}

// SFX enum name table for .wav file loading
static const char* sfxNames[] = {
    "PlayerJump", "PlayerDash", "PlayerLand",
    "MeleeSwing", "MeleeHit", "RangedShot",
    "EnemyHit", "EnemyDeath",
    "PlayerHurt", "PlayerDeath",
    "DimensionSwitch",
    "RiftRepair", "RiftFail",
    "Pickup",
    "MenuSelect", "MenuConfirm",
    "LevelComplete",
    "CollapseWarning", "SuitEntropyCritical",
    "SpikeDamage",
    "BossMultiShot", "BossShieldBurst", "BossTeleport",
    "CrawlerDrop", "SummonerSummon", "SniperTelegraph",
    "FireBurn", "LaserHit",
    "WyrmDive", "WyrmPoison", "WyrmBarrage",
    "IceFreeze", "ElectricChain",
    "Parry", "ParryCounter", "ChargedAttackRelease",
    "CriticalHit", "ComboMilestone",
    "AirJuggleLaunch", "EnemyStun",
    "GroundSlam",
    "RiftShieldActivate", "RiftShieldAbsorb", "RiftShieldReflect", "RiftShieldBurst",
    "PhaseStrikeTeleport", "PhaseStrikeHit",
    "BreakableWall", "SecretRoomDiscover",
    "ShrineActivate", "ShrineBlessing", "ShrineCurse",
    "MerchantGreet", "AnomalySpawn",
    "ArchTileSwap", "ArchConstruct", "ArchRiftOpen", "ArchCollapse", "ArchBeam",
    "VoidSovereignOrb", "VoidSovereignSlam", "VoidSovereignTeleport",
    "VoidSovereignDimLock", "VoidSovereignStorm", "VoidSovereignLaser",
    "EntropyPulse", "EntropyTendril", "EntropyMissile", "EntropyShatter",
    "LoreDiscover",
    "ChargeReady",
    "Heartbeat",
    "VolumePreview",
    "ShockTrap",
};

// Procedural fallback generators (same order as SFX enum)
static Mix_Chunk* (*sfxGenerators[])() = {
    SoundGenerator::playerJump, SoundGenerator::playerDash, SoundGenerator::playerLand,
    SoundGenerator::meleeSwing, SoundGenerator::meleeHit, SoundGenerator::rangedShot,
    SoundGenerator::enemyHit, SoundGenerator::enemyDeath,
    SoundGenerator::playerHurt, SoundGenerator::playerDeath,
    SoundGenerator::dimensionSwitch,
    SoundGenerator::riftRepair, SoundGenerator::riftFail,
    SoundGenerator::pickup,
    SoundGenerator::menuSelect, SoundGenerator::menuConfirm,
    SoundGenerator::levelComplete,
    SoundGenerator::collapseWarning, SoundGenerator::suitEntropyCritical,
    SoundGenerator::spikeDamage,
    SoundGenerator::bossMultiShot, SoundGenerator::bossShieldBurst, SoundGenerator::bossTeleport,
    SoundGenerator::crawlerDrop, SoundGenerator::summonerSummon, SoundGenerator::sniperTelegraph,
    SoundGenerator::fireBurn, SoundGenerator::laserHit,
    SoundGenerator::wyrmDive, SoundGenerator::wyrmPoison, SoundGenerator::wyrmBarrage,
    SoundGenerator::iceFreeze, SoundGenerator::electricChain,
    SoundGenerator::parry, SoundGenerator::parryCounter, SoundGenerator::chargedAttackRelease,
    SoundGenerator::criticalHit, SoundGenerator::comboMilestone,
    SoundGenerator::airJuggleLaunch, SoundGenerator::enemyStun,
    SoundGenerator::groundSlam,
    SoundGenerator::riftShieldActivate, SoundGenerator::riftShieldAbsorb,
    SoundGenerator::riftShieldReflect, SoundGenerator::riftShieldBurst,
    SoundGenerator::phaseStrikeTeleport, SoundGenerator::phaseStrikeHit,
    SoundGenerator::breakableWall, SoundGenerator::secretRoomDiscover,
    SoundGenerator::shrineActivate, SoundGenerator::shrineBlessing, SoundGenerator::shrineCurse,
    SoundGenerator::merchantGreet, SoundGenerator::anomalySpawn,
    SoundGenerator::archTileSwap, SoundGenerator::archConstruct, SoundGenerator::archRiftOpen,
    SoundGenerator::archCollapse, SoundGenerator::archBeam,
    SoundGenerator::voidSovereignOrb, SoundGenerator::voidSovereignSlam,
    SoundGenerator::voidSovereignTeleport, SoundGenerator::voidSovereignDimLock,
    SoundGenerator::voidSovereignStorm, SoundGenerator::voidSovereignLaser,
    SoundGenerator::entropyPulse, SoundGenerator::entropyTendril,
    SoundGenerator::entropyMissile, SoundGenerator::entropyShatter,
    SoundGenerator::loreDiscover,
    SoundGenerator::chargeReady,
    SoundGenerator::heartbeat,
    SoundGenerator::volumePreview,
    SoundGenerator::shockTrap,
};

void AudioManager::generateSounds() {
    int wavLoaded = 0;
    for (int i = 0; i < static_cast<int>(SFX::COUNT); i++) {
        // Try loading .wav file first
        std::string path = "assets/sounds/" + std::string(sfxNames[i]) + ".wav";
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (chunk) {
            m_sfx[i] = chunk;
            wavLoaded++;
        } else {
            // Fall back to procedural generation
            m_sfx[i] = sfxGenerators[i]();
        }
    }
    if (wavLoaded > 0) {
        SDL_Log("Audio: Loaded %d/%d SFX from .wav files", wavLoaded, static_cast<int>(SFX::COUNT));
    }
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

        stopMusicLayers();
        if (m_layerRhythm) { Mix_FreeChunk(m_layerRhythm); m_layerRhythm = nullptr; }
        if (m_layerIntensity) { Mix_FreeChunk(m_layerIntensity); m_layerIntensity = nullptr; }
        if (m_layerDanger) { Mix_FreeChunk(m_layerDanger); m_layerDanger = nullptr; }
        if (m_layerBoss) { Mix_FreeChunk(m_layerBoss); m_layerBoss = nullptr; }

        stopThemeAmbient();
        Mix_CloseAudio();
        m_initialized = false;
    }
}

void AudioManager::playMusic(const std::string& path, int loops, int fadeMs) {
    if (!m_initialized || m_muted) return;
    Mix_Music* music = ResourceManager::instance().getMusic(path);
    if (music) {
        // Fade out any currently playing music, then fade in the new track
        if (Mix_PlayingMusic() && fadeMs > 0) {
            Mix_FadeOutMusic(fadeMs / 2);
        }
        Mix_FadeInMusic(music, loops, fadeMs);
        Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
    }
}

void AudioManager::stopMusic(int fadeMs) {
    if (!m_initialized) return;
    if (fadeMs > 0 && Mix_PlayingMusic()) {
        Mix_FadeOutMusic(fadeMs);
    } else {
        Mix_HaltMusic();
    }
}

void AudioManager::pauseMusic() {
    if (!m_initialized) return;
    Mix_PauseMusic();
}

void AudioManager::resumeMusic() {
    if (!m_initialized) return;
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
        stopMusicLayers();
        Mix_HaltChannel(10); // ThemeAmbient
        Mix_HaltChannel(15); // Ambient loop
    } else {
        Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
        startMusicLayers();
    }
}

void AudioManager::startMusicLayers() {
    if (!m_initialized || m_layersActive) return;

    Mix_Chunk* layers[] = {m_layerRhythm, m_layerIntensity, m_layerDanger, m_layerBoss};
    int channels[] = {14, 13, 12, 11};

    for (int i = 0; i < 4; i++) {
        if (!layers[i]) continue;
        m_layerChannels[i] = Mix_PlayChannel(channels[i], layers[i], -1);
        if (m_layerChannels[i] >= 0) {
            Mix_Volume(m_layerChannels[i], 0); // Start silent
        }
    }
    m_layersActive = true;
}

void AudioManager::stopMusicLayers() {
    if (!m_layersActive) return;
    for (int i = 0; i < 4; i++) {
        if (m_layerChannels[i] >= 0) {
            Mix_HaltChannel(m_layerChannels[i]);
            m_layerChannels[i] = -1;
        }
    }
    m_layersActive = false;
}

void AudioManager::updateMusicLayers(const MusicSystem& music) {
    if (!m_initialized || m_muted || !m_layersActive) return;

    float volumes[] = {
        music.getRhythmVolume(),
        music.getIntensityVolume(),
        music.getDangerVolume(),
        music.getBossVolume()
    };

    float maxLayerVol = m_musicVolume * m_masterVolume * 0.5f; // Music layers at 50% of music vol

    for (int i = 0; i < 4; i++) {
        if (m_layerChannels[i] >= 0) {
            int vol = static_cast<int>(volumes[i] * maxLayerVol);
            Mix_Volume(m_layerChannels[i], vol);
        }
    }
}

void AudioManager::playThemeAmbient(int themeId) {
    if (!m_initialized || m_muted) return;
    if (themeId == m_currentThemeId && m_themeAmbientChannel >= 0 && Mix_Playing(m_themeAmbientChannel)) return;

    stopThemeAmbient();

    m_themeAmbientChunk = SoundGenerator::themeAmbient(themeId);
    if (!m_themeAmbientChunk) return;

    m_themeAmbientChannel = Mix_PlayChannel(10, m_themeAmbientChunk, -1);
    if (m_themeAmbientChannel >= 0) {
        Mix_Volume(m_themeAmbientChannel, static_cast<int>(m_musicVolume * m_masterVolume * 0.4f));
    }
    m_currentThemeId = themeId;
}

void AudioManager::stopThemeAmbient() {
    if (m_themeAmbientChannel >= 0) {
        Mix_HaltChannel(m_themeAmbientChannel);
        m_themeAmbientChannel = -1;
    }
    if (m_themeAmbientChunk) {
        Mix_FreeChunk(m_themeAmbientChunk);
        m_themeAmbientChunk = nullptr;
    }
    m_currentThemeId = -1;
}
