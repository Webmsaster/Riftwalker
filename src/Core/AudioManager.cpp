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

    // Generate dynamic music layers
    m_layerRhythm = SoundGenerator::musicRhythm();
    m_layerIntensity = SoundGenerator::musicIntensity();
    m_layerDanger = SoundGenerator::musicDanger();
    m_layerBoss = SoundGenerator::musicBoss();
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
    m_sfx[static_cast<int>(SFX::FireBurn)]             = SoundGenerator::fireBurn();
    m_sfx[static_cast<int>(SFX::LaserHit)]             = SoundGenerator::laserHit();
    m_sfx[static_cast<int>(SFX::WyrmDive)]             = SoundGenerator::wyrmDive();
    m_sfx[static_cast<int>(SFX::WyrmPoison)]           = SoundGenerator::wyrmPoison();
    m_sfx[static_cast<int>(SFX::WyrmBarrage)]          = SoundGenerator::wyrmBarrage();
    m_sfx[static_cast<int>(SFX::IceFreeze)]            = SoundGenerator::iceFreeze();
    m_sfx[static_cast<int>(SFX::ElectricChain)]        = SoundGenerator::electricChain();
    m_sfx[static_cast<int>(SFX::Parry)]                = SoundGenerator::parry();
    m_sfx[static_cast<int>(SFX::ParryCounter)]         = SoundGenerator::parryCounter();
    m_sfx[static_cast<int>(SFX::ChargedAttackRelease)] = SoundGenerator::chargedAttackRelease();
    m_sfx[static_cast<int>(SFX::CriticalHit)]          = SoundGenerator::criticalHit();
    m_sfx[static_cast<int>(SFX::ComboMilestone)]       = SoundGenerator::comboMilestone();
    m_sfx[static_cast<int>(SFX::AirJuggleLaunch)]      = SoundGenerator::airJuggleLaunch();
    m_sfx[static_cast<int>(SFX::EnemyStun)]            = SoundGenerator::enemyStun();
    m_sfx[static_cast<int>(SFX::GroundSlam)]           = SoundGenerator::groundSlam();
    m_sfx[static_cast<int>(SFX::RiftShieldActivate)]   = SoundGenerator::riftShieldActivate();
    m_sfx[static_cast<int>(SFX::RiftShieldAbsorb)]     = SoundGenerator::riftShieldAbsorb();
    m_sfx[static_cast<int>(SFX::RiftShieldReflect)]    = SoundGenerator::riftShieldReflect();
    m_sfx[static_cast<int>(SFX::RiftShieldBurst)]      = SoundGenerator::riftShieldBurst();
    m_sfx[static_cast<int>(SFX::PhaseStrikeTeleport)]  = SoundGenerator::phaseStrikeTeleport();
    m_sfx[static_cast<int>(SFX::PhaseStrikeHit)]       = SoundGenerator::phaseStrikeHit();
    m_sfx[static_cast<int>(SFX::BreakableWall)]        = SoundGenerator::breakableWall();
    m_sfx[static_cast<int>(SFX::SecretRoomDiscover)]   = SoundGenerator::secretRoomDiscover();
    m_sfx[static_cast<int>(SFX::ShrineActivate)]       = SoundGenerator::shrineActivate();
    m_sfx[static_cast<int>(SFX::ShrineBlessing)]       = SoundGenerator::shrineBlessing();
    m_sfx[static_cast<int>(SFX::ShrineCurse)]          = SoundGenerator::shrineCurse();
    m_sfx[static_cast<int>(SFX::MerchantGreet)]        = SoundGenerator::merchantGreet();
    m_sfx[static_cast<int>(SFX::AnomalySpawn)]         = SoundGenerator::anomalySpawn();
    m_sfx[static_cast<int>(SFX::ArchTileSwap)]         = SoundGenerator::archTileSwap();
    m_sfx[static_cast<int>(SFX::ArchConstruct)]        = SoundGenerator::archConstruct();
    m_sfx[static_cast<int>(SFX::ArchRiftOpen)]         = SoundGenerator::archRiftOpen();
    m_sfx[static_cast<int>(SFX::ArchCollapse)]         = SoundGenerator::archCollapse();
    m_sfx[static_cast<int>(SFX::ArchBeam)]             = SoundGenerator::archBeam();
    m_sfx[static_cast<int>(SFX::VoidSovereignOrb)]      = SoundGenerator::voidSovereignOrb();
    m_sfx[static_cast<int>(SFX::VoidSovereignSlam)]      = SoundGenerator::voidSovereignSlam();
    m_sfx[static_cast<int>(SFX::VoidSovereignTeleport)]  = SoundGenerator::voidSovereignTeleport();
    m_sfx[static_cast<int>(SFX::VoidSovereignDimLock)]   = SoundGenerator::voidSovereignDimLock();
    m_sfx[static_cast<int>(SFX::VoidSovereignStorm)]     = SoundGenerator::voidSovereignStorm();
    m_sfx[static_cast<int>(SFX::VoidSovereignLaser)]     = SoundGenerator::voidSovereignLaser();
    m_sfx[static_cast<int>(SFX::LoreDiscover)]           = SoundGenerator::loreDiscover();
    m_sfx[static_cast<int>(SFX::ChargeReady)]            = SoundGenerator::chargeReady();
    m_sfx[static_cast<int>(SFX::Heartbeat)]              = SoundGenerator::heartbeat();
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

void AudioManager::playMusic(const std::string& path, int loops) {
    if (!m_initialized || m_muted) return;
    Mix_Music* music = ResourceManager::instance().getMusic(path);
    if (music) {
        Mix_PlayMusic(music, loops);
        Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
    }
}

void AudioManager::stopMusic() {
    if (!m_initialized) return;
    Mix_HaltMusic();
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
    } else {
        Mix_VolumeMusic(static_cast<int>(m_musicVolume * m_masterVolume));
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
