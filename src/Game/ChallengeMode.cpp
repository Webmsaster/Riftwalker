#include "ChallengeMode.h"
#include "Core/SaveUtils.h"
#include <vector>
#include <fstream>

static std::vector<ChallengeData> s_challenges;
static std::vector<MutatorData> s_mutators;
static bool s_init = false;

void ChallengeMode::init() {
    if (s_init) return;
    s_init = true;

    s_challenges.resize(static_cast<int>(ChallengeID::COUNT));
    s_challenges[static_cast<int>(ChallengeID::None)] = {ChallengeID::None, "Normal", "Standard run"};
    s_challenges[static_cast<int>(ChallengeID::Speedrun)] = {
        ChallengeID::Speedrun, "Speedrun", "10 min timer, fast enemies",
        1.0f, 1.0f, 1.3f, 1.0f, -1, 600.0f, false, false, false, false
    };
    s_challenges[static_cast<int>(ChallengeID::GlassCannon)] = {
        ChallengeID::GlassCannon, "Glass Cannon", "1 HP, 3x DMG, no healing",
        1.0f, 1.0f, 1.0f, 3.0f, 1.0f, 0, true, false, false, false
    };
    s_challenges[static_cast<int>(ChallengeID::BossRush)] = {
        ChallengeID::BossRush, "Boss Rush", "Only boss fights",
        1.2f, 1.0f, 1.0f, 1.0f, -1, 0, false, false, true, false
    };
    s_challenges[static_cast<int>(ChallengeID::EndlessRift)] = {
        ChallengeID::EndlessRift, "Endless Rift", "Infinite levels, rising difficulty",
        1.0f, 1.0f, 1.0f, 1.0f, -1, 0, false, false, false, true
    };
    s_challenges[static_cast<int>(ChallengeID::DimensionLock)] = {
        ChallengeID::DimensionLock, "Dimension Lock", "No dimension switching",
        1.0f, 1.0f, 1.0f, 1.0f, -1, 0, false, true, false, false
    };

    s_mutators.resize(static_cast<int>(MutatorID::COUNT));
    s_mutators[static_cast<int>(MutatorID::None)] = {MutatorID::None, "None", "No effect"};
    s_mutators[static_cast<int>(MutatorID::BigHeadMode)] = {MutatorID::BigHeadMode, "Big Head", "All hitboxes +50%"};
    s_mutators[static_cast<int>(MutatorID::FragileWorld)] = {MutatorID::FragileWorld, "Fragile World", "All tiles breakable"};
    s_mutators[static_cast<int>(MutatorID::ShardStorm)] = {MutatorID::ShardStorm, "Shard Storm", "3x shards, 2x enemies"};
    s_mutators[static_cast<int>(MutatorID::DimensionFlux)] = {MutatorID::DimensionFlux, "Dim Flux", "Auto-switch every 15s"};
    s_mutators[static_cast<int>(MutatorID::BulletHell)] = {MutatorID::BulletHell, "Bullet Hell", "All enemies shoot"};
    s_mutators[static_cast<int>(MutatorID::LowGravity)] = {MutatorID::LowGravity, "Low Gravity", "Half gravity, high jumps"};
}

const ChallengeData& ChallengeMode::getChallengeData(ChallengeID id) {
    init();
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(s_challenges.size())) return s_challenges[idx];
    return s_challenges[0];
}

const MutatorData& ChallengeMode::getMutatorData(MutatorID id) {
    init();
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(s_mutators.size())) return s_mutators[idx];
    return s_mutators[0];
}

int ChallengeMode::getChallengeCount() { return static_cast<int>(ChallengeID::COUNT); }
int ChallengeMode::getMutatorCount() { return static_cast<int>(MutatorID::COUNT); }

ChallengeBest ChallengeMode::bestScores[static_cast<int>(ChallengeID::COUNT)] = {};

void ChallengeMode::recordResult(ChallengeID id, int score, float time, int floor, int kills) {
    int idx = static_cast<int>(id);
    if (idx <= 0 || idx >= static_cast<int>(ChallengeID::COUNT)) return;
    auto& b = bestScores[idx];
    b.completed = true;
    if (score > b.bestScore) b.bestScore = score;
    if (time > 0 && (b.bestTime <= 0 || time < b.bestTime)) b.bestTime = time;
    if (floor > b.bestFloor) b.bestFloor = floor;
    if (kills > b.bestKills) b.bestKills = kills;
}

void ChallengeMode::save(const std::string& filepath) {
    atomicSave(filepath, [](std::ofstream& f) {
        f << "CHALLENGE_SCORES_V1\n";
        for (int i = 1; i < static_cast<int>(ChallengeID::COUNT); i++) {
            auto& b = bestScores[i];
            f << i << " " << (b.completed ? 1 : 0) << " "
              << b.bestScore << " " << b.bestTime << " "
              << b.bestFloor << " " << b.bestKills << "\n";
        }
    });
}

void ChallengeMode::load(const std::string& filepath) {
    init();
    std::ifstream f(filepath);
    if (!f.is_open() || f.peek() == std::ifstream::traits_type::eof()) {
        f.close();
        f.open(filepath + ".bak");
        if (f.is_open()) SDL_Log("Using backup challenge save: %s.bak", filepath.c_str());
    }
    if (!f) return;
    std::string header;
    std::getline(f, header);
    if (header != "CHALLENGE_SCORES_V1") return;
    int id, comp, score, floor, kills;
    float time;
    while (f >> id >> comp >> score >> time >> floor >> kills) {
        if (id > 0 && id < static_cast<int>(ChallengeID::COUNT)) {
            auto& b = bestScores[id];
            b.completed = (comp != 0);
            b.bestScore = std::max(0, score);
            b.bestTime = std::max(0.0f, time);
            b.bestFloor = std::max(0, floor);
            b.bestKills = std::max(0, kills);
        }
    }
}
