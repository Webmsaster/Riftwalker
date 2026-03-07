#pragma once
#include <SDL2/SDL_mixer.h>
#include <cmath>
#include <cstdlib>
#include <vector>

// Procedural sound effect generator - creates Mix_Chunk* at runtime
// No asset files needed!
class SoundGenerator {
public:
    static Mix_Chunk* sineWave(float freq, float duration, int volume = 64);
    static Mix_Chunk* squareWave(float freq, float duration, int volume = 64);
    static Mix_Chunk* noise(float duration, int volume = 40);

    // Game-specific sounds
    static Mix_Chunk* playerJump();
    static Mix_Chunk* playerDash();
    static Mix_Chunk* playerLand();
    static Mix_Chunk* meleeSwing();
    static Mix_Chunk* meleeHit();
    static Mix_Chunk* rangedShot();
    static Mix_Chunk* enemyHit();
    static Mix_Chunk* enemyDeath();
    static Mix_Chunk* playerHurt();
    static Mix_Chunk* playerDeath();
    static Mix_Chunk* dimensionSwitch();
    static Mix_Chunk* riftRepair();
    static Mix_Chunk* riftFail();
    static Mix_Chunk* pickup();
    static Mix_Chunk* menuSelect();
    static Mix_Chunk* menuConfirm();
    static Mix_Chunk* levelComplete();
    static Mix_Chunk* collapseWarning();
    static Mix_Chunk* suitEntropyCritical();
    static Mix_Chunk* spikeDamage();

    // Boss special attack sounds
    static Mix_Chunk* bossMultiShot();
    static Mix_Chunk* bossShieldBurst();
    static Mix_Chunk* bossTeleport();

    // New enemy sounds
    static Mix_Chunk* crawlerDrop();
    static Mix_Chunk* summonerSummon();
    static Mix_Chunk* sniperTelegraph();

    // Environmental hazard sounds
    static Mix_Chunk* fireBurn();
    static Mix_Chunk* laserHit();

    // Void Wyrm boss sounds
    static Mix_Chunk* wyrmDive();
    static Mix_Chunk* wyrmPoison();
    static Mix_Chunk* wyrmBarrage();

    // Combat enhancement sounds
    static Mix_Chunk* iceFreeze();
    static Mix_Chunk* electricChain();
    static Mix_Chunk* parry();
    static Mix_Chunk* parryCounter();
    static Mix_Chunk* chargedAttackRelease();
    static Mix_Chunk* criticalHit();
    static Mix_Chunk* comboMilestone();
    static Mix_Chunk* airJuggleLaunch();
    static Mix_Chunk* enemyStun();

    // Player ability sounds
    static Mix_Chunk* groundSlam();
    static Mix_Chunk* riftShieldActivate();
    static Mix_Chunk* riftShieldAbsorb();
    static Mix_Chunk* riftShieldReflect();
    static Mix_Chunk* riftShieldBurst();
    static Mix_Chunk* phaseStrikeTeleport();
    static Mix_Chunk* phaseStrikeHit();

    // Secret room & event sounds
    static Mix_Chunk* breakableWall();
    static Mix_Chunk* secretRoomDiscover();
    static Mix_Chunk* shrineActivate();
    static Mix_Chunk* shrineBlessing();
    static Mix_Chunk* shrineCurse();
    static Mix_Chunk* merchantGreet();
    static Mix_Chunk* anomalySpawn();

    // Dimensional Architect boss sounds
    static Mix_Chunk* archTileSwap();
    static Mix_Chunk* archConstruct();
    static Mix_Chunk* archRiftOpen();
    static Mix_Chunk* archCollapse();
    static Mix_Chunk* archBeam();

    // Void Sovereign boss sounds
    static Mix_Chunk* voidSovereignOrb();
    static Mix_Chunk* voidSovereignSlam();
    static Mix_Chunk* voidSovereignTeleport();
    static Mix_Chunk* voidSovereignDimLock();
    static Mix_Chunk* voidSovereignStorm();
    static Mix_Chunk* voidSovereignLaser();

    // Lore
    static Mix_Chunk* loreDiscover();

    // Charge attack ready cue
    static Mix_Chunk* chargeReady();

    // Low HP warning heartbeat
    static Mix_Chunk* heartbeat();

    // Ambient music loops (~4 seconds each)
    static Mix_Chunk* ambientDimA(); // Cool, ethereal drone
    static Mix_Chunk* ambientDimB(); // Warm, ominous drone

private:
    struct Sample {
        std::vector<Sint16> data;
        int sampleRate = 44100;
    };

    static Sample generate(float duration);
    static void addSine(Sample& s, float freq, float startVol, float endVol,
                        float startTime = 0, float endTime = -1);
    static void addSquare(Sample& s, float freq, float startVol, float endVol,
                          float startTime = 0, float endTime = -1);
    static void addNoise(Sample& s, float startVol, float endVol,
                         float startTime = 0, float endTime = -1);
    static void addSweep(Sample& s, float startFreq, float endFreq,
                         float startVol, float endVol,
                         float startTime = 0, float endTime = -1);
    static Mix_Chunk* toChunk(const Sample& s);
};
