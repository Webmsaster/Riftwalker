#include "Core/Game.h"
#include "Game/LevelGenerator.h"
#include "Game/Tile.h"
#include "Game/WorldTheme.h"
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

// Global flags for automated testing modes
bool g_autoSmokeTest = false;
bool g_autoPlaytest = false;
int g_forcedRunSeed = -1;

// Playtest configuration (set via CLI args)
int g_playtestRuns = 10;              // --playtest-runs N
int g_playtestClassLock = -1;         // --playtest-class 0/1/2 (-1=rotate)
int g_playtestProfile = 0;           // --playtest-profile 0=balanced,1=aggressive,2=defensive,3=speedrun
char g_playtestOutputFile[256] = "playtest_report.log"; // --playtest-output FILE
bool g_smokeRegression = false;
int g_smokeTargetFloor = 5;
float g_smokeMaxRuntime = 600.0f;
int g_smokeCompletedFloor = 0;
int g_smokeFailureCode = 0;
char g_smokeFailureReason[256] = {};

// Global settings (persisted via riftwalker_settings.cfg)
// Defaults tuned for first-time players: volumes conservative, visuals full
float g_sfxVolume      = 0.8f;  // 0.0 - 1.0 (80% — audible without being harsh)
float g_musicVolume    = 0.6f;  // 0.0 - 1.0 (60% — music shouldn't overpower SFX)
float g_shakeIntensity = 1.0f;  // 0.0 - 2.0 (multiplier)
float g_hudOpacity     = 0.9f;  // 0.5 - 1.0 (90% — slightly transparent for immersion)

// Null SDL log callback - prevents all SDL_Log from writing to console/stderr
static void nullLogCallback(void*, int, SDL_LogPriority, const char*) {}

namespace {
constexpr int kValidationRetryAttempts = 512;

struct GeneratorTestConfig {
    bool enabled = false;
    bool runDeterministic = true;
    bool runNegative = true;
    bool runProperty = true;
    int minDifficulty = 1;
    int maxDifficulty = 5;
    int propertyCount = 100;
    int propertySeedStart = 1;
    int negativeSeedScanLimit = 2048;
    std::vector<int> deterministicSeeds{1, 42, 1337, 9001, 123456};
};

struct PropertyStats {
    int total = 0;
    int passed = 0;
    int retried = 0;
    int exhausted = 0;
    std::array<int, 8> failureBits{};
    std::vector<int> exhaustedSeeds;
};

bool parseIntArg(const char* value, int& out) {
    if (!value || !*value) return false;
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (!end || *end != '\0') return false;
    out = static_cast<int>(parsed);
    return true;
}

std::vector<int> parseSeedList(const char* value) {
    std::vector<int> seeds;
    if (!value || !*value) return seeds;

    const char* cursor = value;
    while (*cursor) {
        char* end = nullptr;
        long parsed = std::strtol(cursor, &end, 10);
        if (end == cursor) break;
        seeds.push_back(static_cast<int>(parsed));
        cursor = end;
        if (*cursor == ',') cursor++;
    }
    return seeds;
}

void configureGenerator(LevelGenerator& generator, int themeSeed) {
    auto themes = WorldTheme::getRandomPair(themeSeed);
    generator.setThemes(themes.first, themes.second);
}

std::string failureMaskToString(int failureMask) {
    if (failureMask == LevelValidationFailure_None) {
        return "none";
    }

    struct FailureName {
        int bit;
        const char* name;
    };

    static const FailureName kNames[] = {
        {LevelValidationFailure_Spawn, "spawn"},
        {LevelValidationFailure_Exit, "exit"},
        {LevelValidationFailure_RiftAnchor, "rift_anchor"},
        {LevelValidationFailure_RiftReachability, "rift_reach"},
        {LevelValidationFailure_ExitReachability, "exit_reach"},
        {LevelValidationFailure_MainPath, "main_path"},
        {LevelValidationFailure_DimSwitch, "dim_switch"},
        {LevelValidationFailure_DimPuzzle, "dim_puzzle"}
    };

    std::string result;
    for (const auto& entry : kNames) {
        if ((failureMask & entry.bit) == 0) continue;
        if (!result.empty()) result += ",";
        result += entry.name;
    }
    return result;
}

void recordFailureBits(int failureMask, std::array<int, 8>& counters) {
    static const int kBits[] = {
        LevelValidationFailure_Spawn,
        LevelValidationFailure_Exit,
        LevelValidationFailure_RiftAnchor,
        LevelValidationFailure_RiftReachability,
        LevelValidationFailure_ExitReachability,
        LevelValidationFailure_MainPath,
        LevelValidationFailure_DimSwitch,
        LevelValidationFailure_DimPuzzle
    };

    for (int i = 0; i < 8; i++) {
        if ((failureMask & kBits[i]) != 0) {
            counters[i]++;
        }
    }
}

bool findLevelWithFeature(int difficulty,
                          int scanLimit,
                          const std::function<bool(const Level&)>& predicate,
                          Level& outLevel,
                          int& outSeed) {
    for (int seed = 1; seed <= scanLimit; seed++) {
        LevelGenerator generator;
        configureGenerator(generator, seed);
        Level level = generator.generate(difficulty, seed);
        if (!level.hasValidatedTopology()) continue;
        if (!predicate(level)) continue;
        outLevel = level;
        outSeed = seed;
        return true;
    }
    return false;
}

bool runDeterministicTests(const GeneratorTestConfig& config) {
    bool ok = true;
    std::printf("[GEN-TEST] Deterministische Seeds\n");

    for (int difficulty = config.minDifficulty; difficulty <= config.maxDifficulty; difficulty++) {
        for (int seed : config.deterministicSeeds) {
            LevelGenerator generator;
            configureGenerator(generator, seed);

            Level level = generator.generate(difficulty, seed);
            const auto& topo = level.getTopology();
            bool passed = topo.validation.passed;
            int retries = topo.validation.usedSeed - topo.validation.requestedSeed;

            std::printf("  diff=%d seed=%d used=%d retries=%d passed=%d mask=%s\n",
                        difficulty,
                        seed,
                        topo.validation.usedSeed,
                        retries,
                        passed ? 1 : 0,
                        failureMaskToString(topo.validation.failureMask).c_str());

            if (!passed) {
                ok = false;
            }
        }
    }

    return ok;
}

bool runNegativeTests(const GeneratorTestConfig& config) {
    bool ok = true;
    std::printf("[GEN-TEST] Negative Faelle\n");

    {
        Level level(8, 8, 32);
        int seed = -1;
        bool found = findLevelWithFeature(
            3,
            config.negativeSeedScanLimit,
            [](const Level& candidate) {
                for (const auto& edge : candidate.getTopology().edges) {
                    if (edge.type == LevelGraphEdgeType::VerticalShaft && edge.validated) {
                        return true;
                    }
                }
                return false;
            },
            level,
            seed);

        if (!found) {
            std::printf("  shaft_break: no sample level found\n");
            ok = false;
        } else {
            auto topo = level.getTopology();
            bool mutated = false;
            int brokenFromRoom = -1;
            int brokenToRoom = -1;
            Tile solid;
            solid.type = TileType::Solid;

            for (const auto& edge : topo.edges) {
                if (edge.type != LevelGraphEdgeType::VerticalShaft || !edge.validated) continue;
                const auto& fromRoom = topo.rooms[edge.fromRoom];
                const auto& toRoom = topo.rooms[edge.toRoom];
                int x1 = fromRoom.x + fromRoom.w - 1;
                int y1 = fromRoom.y + fromRoom.h / 2;
                int x2 = toRoom.x;
                int y2 = toRoom.y + toRoom.h / 2;
                int midX = (x1 + x2) / 2;
                int shaftTop = std::min(y1, y2) - 1;
                int shaftBottom = std::max(y1, y2) + 4;

                auto blockHorizontal = [&](int fromX, int toX, int atY, int dim) {
                    int lo = std::min(fromX, toX);
                    int hi = std::max(fromX, toX);
                    for (int x = lo; x <= hi; x++) {
                        for (int dy = 0; dy < 4; dy++) {
                            int y = atY + dy;
                            if (!level.inBounds(x, y)) continue;
                            level.setTile(x, y, dim, solid);
                            mutated = true;
                        }
                    }
                };

                for (int dim = 1; dim <= 2; dim++) {
                    blockHorizontal(x1, midX, y1, dim);
                    blockHorizontal(midX, x2, y2, dim);
                    for (int x = midX; x <= midX + 1; x++) {
                        for (int y = shaftTop; y <= shaftBottom; y++) {
                            if (!level.inBounds(x, y)) continue;
                            level.setTile(x, y, dim, solid);
                            mutated = true;
                        }
                    }
                }

                if (mutated) {
                    brokenFromRoom = edge.fromRoom;
                    brokenToRoom = edge.toRoom;
                    break;
                }
            }

            LevelGenerator generator;
            configureGenerator(generator, seed);
            bool passed = generator.validateLevelForTesting(level, topo);
            bool pairStillValidated = false;
            for (const auto& edge : topo.edges) {
                if (edge.fromRoom == brokenFromRoom &&
                    edge.toRoom == brokenToRoom &&
                    edge.validated) {
                    pairStillValidated = true;
                    break;
                }
            }
            std::printf("  shaft_break: seed=%d mutated=%d passed=%d mask=%s\n",
                        seed,
                        mutated ? 1 : 0,
                        passed ? 1 : 0,
                        failureMaskToString(topo.validation.failureMask).c_str());
            if (!mutated ||
                passed ||
                pairStillValidated ||
                (topo.validation.failureMask & LevelValidationFailure_MainPath) == 0) {
                ok = false;
            }
        }
    }

    {
        Level level(8, 8, 32);
        int seed = -1;
        bool found = findLevelWithFeature(
            2,
            config.negativeSeedScanLimit,
            [](const Level& candidate) {
                return !candidate.getTopology().rifts.empty();
            },
            level,
            seed);

        if (!found) {
            std::printf("  rift_support_break: no sample level found\n");
            ok = false;
        } else {
            auto topo = level.getTopology();
            const auto& rift = topo.rifts.front();
            Tile empty;
            empty.type = TileType::Empty;
            for (int dim = 1; dim <= 2; dim++) {
                for (int below = 1; below <= 5; below++) {
                    if (level.inBounds(rift.tileX, rift.tileY + below)) {
                        level.setTile(rift.tileX, rift.tileY + below, dim, empty);
                    }
                }
            }

            LevelGenerator generator;
            configureGenerator(generator, seed);
            bool passed = generator.validateLevelForTesting(level, topo);
            std::printf("  rift_support_break: seed=%d passed=%d mask=%s\n",
                        seed,
                        passed ? 1 : 0,
                        failureMaskToString(topo.validation.failureMask).c_str());
            if (passed || (topo.validation.failureMask & LevelValidationFailure_RiftAnchor) == 0) {
                ok = false;
            }
        }
    }

    {
        Level level(8, 8, 32);
        int seed = -1;
        bool found = findLevelWithFeature(
            3,
            config.negativeSeedScanLimit,
            [](const Level& candidate) {
                return !candidate.getTopology().switches.empty();
            },
            level,
            seed);

        if (!found) {
            std::printf("  dim_puzzle_break: no sample level found\n");
            ok = false;
        } else {
            auto topo = level.getTopology();
            const auto& sw = topo.switches.front();
            Tile empty;
            empty.type = TileType::Empty;
            level.setTile(sw.switchTileX, sw.switchTileY, sw.switchDimension, empty);

            LevelGenerator generator;
            configureGenerator(generator, seed);
            bool passed = generator.validateLevelForTesting(level, topo);
            std::printf("  dim_puzzle_break: seed=%d passed=%d mask=%s\n",
                        seed,
                        passed ? 1 : 0,
                        failureMaskToString(topo.validation.failureMask).c_str());
            if (passed || (topo.validation.failureMask & LevelValidationFailure_DimPuzzle) == 0) {
                ok = false;
            }
        }
    }

    return ok;
}

bool runPropertyTests(const GeneratorTestConfig& config) {
    bool ok = true;
    std::printf("[GEN-TEST] Property Seeds\n");

    for (int difficulty = config.minDifficulty; difficulty <= config.maxDifficulty; difficulty++) {
        PropertyStats stats;

        for (int offset = 0; offset < config.propertyCount; offset++) {
            int requestedSeed = config.propertySeedStart + offset;
            LevelGenerator generator;
            configureGenerator(generator, requestedSeed);

            bool expectedPass = false;
            int expectedUsedSeed = requestedSeed;

            for (int attempt = 0; attempt < kValidationRetryAttempts; attempt++) {
                LevelTopology topo;
                Level candidate = generator.generateCandidateForTesting(difficulty, requestedSeed + attempt, topo);
                topo.validation.requestedSeed = requestedSeed;
                topo.validation.usedSeed = requestedSeed + attempt;
                topo.validation.attempt = attempt;

                bool candidatePass = generator.validateLevelForTesting(candidate, topo);
                if (candidatePass) {
                    expectedPass = true;
                    expectedUsedSeed = requestedSeed + attempt;
                    if (attempt > 0) {
                        stats.retried++;
                    }
                    break;
                }

                recordFailureBits(topo.validation.failureMask, stats.failureBits);
            }

            Level level = generator.generate(difficulty, requestedSeed);
            const auto& finalTopo = level.getTopology();

            stats.total++;
            if (finalTopo.validation.passed) {
                stats.passed++;
            } else {
                stats.exhausted++;
                stats.exhaustedSeeds.push_back(requestedSeed);
            }

            bool seedMatch = finalTopo.validation.usedSeed == expectedUsedSeed;
            if (finalTopo.validation.passed != expectedPass || (expectedPass && !seedMatch)) {
                std::printf("  mismatch: diff=%d seed=%d expectedPass=%d actualPass=%d expectedUsed=%d actualUsed=%d\n",
                            difficulty,
                            requestedSeed,
                            expectedPass ? 1 : 0,
                            finalTopo.validation.passed ? 1 : 0,
                            expectedUsedSeed,
                            finalTopo.validation.usedSeed);
                ok = false;
            }
        }

        float retryRate = (stats.total > 0)
            ? (100.0f * static_cast<float>(stats.retried) / static_cast<float>(stats.total))
            : 0.0f;

        std::printf("  diff=%d total=%d passed=%d retried=%d retryRate=%.1f%% exhausted=%d\n",
                    difficulty,
                    stats.total,
                    stats.passed,
                    stats.retried,
                    retryRate,
                    stats.exhausted);

        static const char* kFailureNames[8] = {
            "spawn", "exit", "rift_anchor", "rift_reach",
            "exit_reach", "main_path", "dim_switch", "dim_puzzle"
        };
        std::printf("    failureBits:");
        bool printedBit = false;
        for (int i = 0; i < 8; i++) {
            if (stats.failureBits[i] == 0) continue;
            std::printf(" %s=%d", kFailureNames[i], stats.failureBits[i]);
            printedBit = true;
        }
        if (!printedBit) {
            std::printf(" none");
        }
        std::printf("\n");

        if (!stats.exhaustedSeeds.empty()) {
            std::printf("    exhaustedSeeds:");
            int limit = std::min(static_cast<int>(stats.exhaustedSeeds.size()), 12);
            for (int i = 0; i < limit; i++) {
                std::printf(" %d", stats.exhaustedSeeds[i]);
            }
            if (static_cast<int>(stats.exhaustedSeeds.size()) > limit) {
                std::printf(" ...");
            }
            std::printf("\n");
        }

        if (stats.exhausted > 0) {
            ok = false;
        }
    }

    return ok;
}

bool runGeneratorValidationTests(const GeneratorTestConfig& config) {
    bool ok = true;

    if (config.runDeterministic) {
        ok = runDeterministicTests(config) && ok;
    }
    if (config.runNegative) {
        ok = runNegativeTests(config) && ok;
    }
    if (config.runProperty) {
        ok = runPropertyTests(config) && ok;
    }

    std::printf("[GEN-TEST] result=%s\n", ok ? "PASS" : "FAIL");
    return ok;
}
} // namespace

int main(int argc, char* argv[]) {
    // Set working directory to exe location so assets resolve correctly
    // regardless of where the exe is launched from
    if (SDL_Init(0) == 0) {
        char* basePath = SDL_GetBasePath();
        if (basePath) {
#ifdef _WIN32
            _chdir(basePath);
#else
            chdir(basePath);
#endif
            SDL_free(basePath);
        }
    }

    GeneratorTestConfig generatorTests;
    g_smokeCompletedFloor = 0;
    g_smokeFailureCode = 0;
    g_smokeFailureReason[0] = '\0';

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--smoke") == 0) {
            g_autoSmokeTest = true;
            continue;
        }
        if (std::strcmp(argv[i], "--smoke-regression") == 0) {
            g_autoSmokeTest = true;
            g_smokeRegression = true;
            continue;
        }
        if (std::strcmp(argv[i], "--playtest") == 0) {
            g_autoPlaytest = true;
            continue;
        }
        if (std::strncmp(argv[i], "--playtest-runs=", 16) == 0) {
            g_playtestRuns = std::max(1, std::atoi(argv[i] + 16));
            continue;
        }
        if (std::strncmp(argv[i], "--playtest-class=", 17) == 0) {
            const char* cls = argv[i] + 17;
            if (std::strcmp(cls, "voidwalker") == 0 || std::strcmp(cls, "0") == 0) g_playtestClassLock = 0;
            else if (std::strcmp(cls, "berserker") == 0 || std::strcmp(cls, "1") == 0) g_playtestClassLock = 1;
            else if (std::strcmp(cls, "phantom") == 0 || std::strcmp(cls, "2") == 0) g_playtestClassLock = 2;
            else g_playtestClassLock = -1; // "all" or invalid = rotate
            continue;
        }
        if (std::strncmp(argv[i], "--playtest-profile=", 19) == 0) {
            const char* prof = argv[i] + 19;
            if (std::strcmp(prof, "aggressive") == 0 || std::strcmp(prof, "1") == 0) g_playtestProfile = 1;
            else if (std::strcmp(prof, "defensive") == 0 || std::strcmp(prof, "2") == 0) g_playtestProfile = 2;
            else if (std::strcmp(prof, "speedrun") == 0 || std::strcmp(prof, "3") == 0) g_playtestProfile = 3;
            else g_playtestProfile = 0; // "balanced" or default
            continue;
        }
        if (std::strncmp(argv[i], "--playtest-output=", 18) == 0) {
            std::strncpy(g_playtestOutputFile, argv[i] + 18, sizeof(g_playtestOutputFile) - 1);
            g_playtestOutputFile[sizeof(g_playtestOutputFile) - 1] = '\0';
            continue;
        }
        if (std::strcmp(argv[i], "--validate-generator") == 0) {
            generatorTests.enabled = true;
            continue;
        }
        if (std::strcmp(argv[i], "--skip-deterministic") == 0) {
            generatorTests.runDeterministic = false;
            continue;
        }
        if (std::strcmp(argv[i], "--skip-negative") == 0) {
            generatorTests.runNegative = false;
            continue;
        }
        if (std::strcmp(argv[i], "--skip-property") == 0) {
            generatorTests.runProperty = false;
            continue;
        }
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            g_forcedRunSeed = std::atoi(argv[++i]);
            continue;
        }
        if (std::strncmp(argv[i], "--seed=", 7) == 0) {
            g_forcedRunSeed = std::atoi(argv[i] + 7);
            continue;
        }
        if (std::strncmp(argv[i], "--smoke-target-floor=", 21) == 0) {
            int targetFloor = 0;
            if (parseIntArg(argv[i] + 21, targetFloor)) {
                g_smokeTargetFloor = std::max(1, targetFloor);
            }
            continue;
        }
        if (std::strncmp(argv[i], "--smoke-max-runtime=", 20) == 0) {
            int runtimeSeconds = 0;
            if (parseIntArg(argv[i] + 20, runtimeSeconds)) {
                g_smokeMaxRuntime = static_cast<float>(std::max(1, runtimeSeconds));
            }
            continue;
        }
        if (std::strncmp(argv[i], "--difficulty=", 13) == 0) {
            int difficulty = 0;
            if (parseIntArg(argv[i] + 13, difficulty)) {
                generatorTests.minDifficulty = difficulty;
                generatorTests.maxDifficulty = difficulty;
            }
            continue;
        }
        if (std::strncmp(argv[i], "--max-difficulty=", 17) == 0) {
            parseIntArg(argv[i] + 17, generatorTests.maxDifficulty);
            continue;
        }
        if (std::strncmp(argv[i], "--property-count=", 17) == 0) {
            parseIntArg(argv[i] + 17, generatorTests.propertyCount);
            continue;
        }
        if (std::strncmp(argv[i], "--property-start=", 17) == 0) {
            parseIntArg(argv[i] + 17, generatorTests.propertySeedStart);
            continue;
        }
        if (std::strncmp(argv[i], "--negative-scan-limit=", 22) == 0) {
            parseIntArg(argv[i] + 22, generatorTests.negativeSeedScanLimit);
            continue;
        }
        if (std::strncmp(argv[i], "--seeds=", 8) == 0) {
            auto parsedSeeds = parseSeedList(argv[i] + 8);
            if (!parsedSeeds.empty()) {
                generatorTests.deterministicSeeds = std::move(parsedSeeds);
            }
            continue;
        }
    }

    if (generatorTests.minDifficulty > generatorTests.maxDifficulty) {
        std::swap(generatorTests.minDifficulty, generatorTests.maxDifficulty);
    }

    if (generatorTests.enabled) {
        return runGeneratorValidationTests(generatorTests) ? 0 : 1;
    }

    if (g_autoSmokeTest || g_autoPlaytest) {
#ifdef _WIN32
        const char* devNull = "NUL";
#else
        const char* devNull = "/dev/null";
#endif
        (void)freopen(devNull, "w", stderr);
        (void)freopen(devNull, "w", stdout);
    }

    unsigned int globalRandomSeed = static_cast<unsigned int>(std::time(nullptr));
    if (g_smokeRegression && g_forcedRunSeed >= 0) {
        globalRandomSeed = static_cast<unsigned int>(g_forcedRunSeed);
    }
    std::srand(globalRandomSeed);

    Game game;
    if (!game.init()) {
        return 1;
    }

    if (g_autoSmokeTest || g_autoPlaytest) {
        SDL_LogSetOutputFunction(nullLogCallback, nullptr);
    }

    game.run();
    game.shutdown();

    if (g_smokeRegression) {
        if (g_smokeFailureCode != 0) {
            return g_smokeFailureCode;
        }
        if (g_smokeCompletedFloor < g_smokeTargetFloor) {
            FILE* smokeFile = std::fopen("smoke_test.log", "a");
            if (smokeFile) {
                std::fprintf(smokeFile,
                             "[SMOKE] RESULT status=FAIL code=6 completedFloor=%d targetFloor=%d reason=insufficient_progress\n",
                             g_smokeCompletedFloor,
                             g_smokeTargetFloor);
                std::fclose(smokeFile);
            }
            return 6;
        }
        return 0;
    }

    return 0;
}
