#pragma once

enum class ChallengeID {
    None = 0,
    Speedrun,       // 10-minute timer, faster enemies, score multiplier for time left
    GlassCannon,    // 1 HP, 3x DMG, no healing
    BossRush,       // Only boss fights, no exploration
    EndlessRift,    // Infinite levels, scaling difficulty, leaderboard score
    DimensionLock,  // No dimension switching
    COUNT
};

enum class MutatorID {
    None = 0,
    BigHeadMode,    // All hitboxes 50% larger
    FragileWorld,   // All tiles can be broken
    ShardStorm,     // 3x shard drops, 2x enemy count
    DimensionFlux,  // Auto dimension switch every 15s
    BulletHell,     // All enemies shoot projectiles
    LowGravity,     // Halved gravity, higher jumps
    COUNT
};

struct ChallengeData {
    ChallengeID id = ChallengeID::None;
    const char* name = "";
    const char* description = "";
    float enemyHPMult = 1.0f;
    float enemyDMGMult = 1.0f;
    float enemySpeedMult = 1.0f;
    float playerDMGMult = 1.0f;
    float playerMaxHP = -1;  // -1 = use default
    float timeLimit = 0;     // 0 = no limit
    bool noHealing = false;
    bool noDimSwitch = false;
    bool bossOnly = false;
    bool endless = false;
};

struct MutatorData {
    MutatorID id = MutatorID::None;
    const char* name = "";
    const char* description = "";
};

class ChallengeMode {
public:
    static void init();
    static const ChallengeData& getChallengeData(ChallengeID id);
    static const MutatorData& getMutatorData(MutatorID id);
    static int getChallengeCount();
    static int getMutatorCount();
};

// Global active challenge + mutators for current run
inline ChallengeID g_activeChallenge = ChallengeID::None;
inline MutatorID g_activeMutators[2] = {MutatorID::None, MutatorID::None};
