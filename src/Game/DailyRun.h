#pragma once
#include <string>
#include <vector>
#include <ctime>

// Forward declare MutatorID to avoid pulling in ChallengeMode.h
enum class MutatorID;

// Calculates score using the standard formula.
// floors * 1000 + kills * 50 + rifts * 200 + shards * 2 + bestCombo * 100 - deathPenalty
// deathPenalty is 0 for victory (deathCause==5), 500 otherwise.
inline int calculateRunScore(int floors, int kills, int rifts, int shards,
                              int bestCombo, int deathCause) {
    int penalty = (deathCause == 5) ? 0 : 500;
    int victoryBonus = (deathCause == 5) ? 5000 : 0;
    return floors * 1000 + kills * 50 + rifts * 200 + shards * 2
           + bestCombo * 100 + victoryBonus - penalty;
}

// Legacy minimal record (kept for backward compat)
struct DailyRecord {
    int score = 0;
    int kills = 0;
    int difficulty = 0;
    char date[12] = {};
};

// Full leaderboard entry
struct DailyLeaderboardEntry {
    int   score       = 0;
    int   floors      = 0;
    int   kills       = 0;
    int   rifts       = 0;
    int   shards      = 0;
    int   bestCombo   = 0;
    float runTime     = 0.f;  // seconds
    int   playerClass = 0;    // PlayerClass as int
    int   deathCause  = 0;
    char  date[12]    = {};   // "YYYY-MM-DD"
};

// Global flag for daily run mode
inline bool g_dailyRunActive = false;

class DailyRun {
public:
    static int getTodaySeed();
    static std::string getTodayDate();
    static MutatorID getDailyMutator();

    // Add a full entry; keeps top 10 per date and last 7 days of dates.
    void addEntry(const DailyLeaderboardEntry& entry);

    // Legacy minimal add
    void addRecord(int score, int kills, int difficulty);

    // All entries (across all tracked dates)
    const std::vector<DailyLeaderboardEntry>& getEntries() const { return m_entries; }

    // Best score for today
    int getTodayBest() const;

    // Previous best score recorded before last addEntry call
    int getPreviousBest() const { return m_previousBest; }

    // Rank of a given score among today's entries (1-based; 0 if not ranked)
    int getTodayRank(int score) const;

    // Compact per-day summary for "previous days" display
    struct DaySummary { std::string date; int bestScore = 0; };
    std::vector<DaySummary> getDaySummaries() const;

    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    std::vector<DailyLeaderboardEntry> m_entries;
    int m_previousBest = 0;

    static constexpr int MAX_PER_DAY   = 10;
    static constexpr int MAX_DAYS_KEPT = 7;

    void prune();
};
