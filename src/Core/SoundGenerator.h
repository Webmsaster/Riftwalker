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
