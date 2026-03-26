#include "Game/DailyRun.h"
#include "Game/ChallengeMode.h"
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>

int DailyRun::getTodaySeed() {
    time_t now = time(nullptr);
    struct tm tBuf = {};
#ifdef _WIN32
    localtime_s(&tBuf, &now);
#else
    localtime_r(&now, &tBuf);
#endif
    struct tm* t = &tBuf;
    return (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday;
}

MutatorID DailyRun::getDailyMutator() {
    // Derive a deterministic mutator from the daily seed
    int seed = getTodaySeed();
    int mutCount = static_cast<int>(MutatorID::COUNT) - 1; // exclude None
    if (mutCount <= 0) return MutatorID::None;
    int pick = ((seed * 2654435761u) >> 16) % mutCount; // hash for better distribution
    return static_cast<MutatorID>(pick + 1); // +1 to skip None
}

std::string DailyRun::getTodayDate() {
    time_t now = time(nullptr);
    struct tm tBuf2 = {};
#ifdef _WIN32
    localtime_s(&tBuf2, &now);
#else
    localtime_r(&now, &tBuf2);
#endif
    struct tm* t = &tBuf2;
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    return std::string(buf);
}

void DailyRun::addEntry(const DailyLeaderboardEntry& entry) {
    m_previousBest = getTodayBest();
    m_entries.push_back(entry);
    prune();
}

void DailyRun::addRecord(int score, int kills, int difficulty) {
    (void)difficulty;
    DailyLeaderboardEntry e;
    e.score = score;
    e.kills = kills;
    auto date = getTodayDate();
    snprintf(e.date, sizeof(e.date), "%s", date.c_str());
    addEntry(e);
}

int DailyRun::getTodayBest() const {
    auto today = getTodayDate();
    int best = 0;
    for (const auto& e : m_entries) {
        if (std::string(e.date) == today && e.score > best)
            best = e.score;
    }
    return best;
}

int DailyRun::getTodayRank(int score) const {
    auto today = getTodayDate();
    int rank = 1;
    for (const auto& e : m_entries) {
        if (std::string(e.date) == today && e.score > score)
            rank++;
    }
    for (const auto& e : m_entries) {
        if (std::string(e.date) == today && e.score == score)
            return rank;
    }
    return 0;
}

std::vector<DailyRun::DaySummary> DailyRun::getDaySummaries() const {
    std::map<std::string, int> bestPerDay;
    for (const auto& e : m_entries) {
        std::string d(e.date);
        auto it = bestPerDay.find(d);
        if (it == bestPerDay.end() || e.score > it->second)
            bestPerDay[d] = e.score;
    }
    std::vector<DaySummary> result;
    result.reserve(bestPerDay.size());
    for (auto& [date, best] : bestPerDay)
        result.push_back({date, best});
    std::sort(result.begin(), result.end(),
              [](const DaySummary& a, const DaySummary& b) { return a.date > b.date; });
    return result;
}

void DailyRun::prune() {
    std::sort(m_entries.begin(), m_entries.end(),
              [](const DailyLeaderboardEntry& a, const DailyLeaderboardEntry& b) {
                  int dc = std::string(b.date).compare(std::string(a.date));
                  if (dc != 0) return dc < 0;
                  return a.score > b.score;
              });

    std::vector<std::string> keptDates;
    for (const auto& e : m_entries) {
        std::string d(e.date);
        bool found = false;
        for (auto& kd : keptDates) { if (kd == d) { found = true; break; } }
        if (!found) {
            if (static_cast<int>(keptDates.size()) >= MAX_DAYS_KEPT) break;
            keptDates.push_back(d);
        }
    }

    std::map<std::string, int> countPerDay;
    std::vector<DailyLeaderboardEntry> kept;
    kept.reserve(m_entries.size());
    for (const auto& e : m_entries) {
        std::string d(e.date);
        bool inKept = false;
        for (auto& kd : keptDates) { if (kd == d) { inKept = true; break; } }
        if (!inKept) continue;
        if (countPerDay[d] >= MAX_PER_DAY) continue;
        countPerDay[d]++;
        kept.push_back(e);
    }
    m_entries = std::move(kept);
}

void DailyRun::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return;
    f << "v2\n";
    for (const auto& e : m_entries) {
        f << e.date        << " "
          << e.score       << " "
          << e.floors      << " "
          << e.kills       << " "
          << e.rifts       << " "
          << e.shards      << " "
          << e.bestCombo   << " "
          << e.runTime     << " "
          << e.playerClass << " "
          << e.deathCause  << "\n";
    }
}

void DailyRun::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    m_entries.clear();

    std::string firstToken;
    f >> firstToken;
    if (firstToken == "v2") {
        DailyLeaderboardEntry e;
        std::string dateStr;
        while (f >> dateStr >> e.score >> e.floors >> e.kills >> e.rifts
                  >> e.shards >> e.bestCombo >> e.runTime >> e.playerClass >> e.deathCause) {
            snprintf(e.date, sizeof(e.date), "%s", dateStr.c_str());
            m_entries.push_back(e);
        }
    } else {
        // Legacy v1: date score kills difficulty
        DailyLeaderboardEntry e;
        int difficulty = 0;
        if (f >> e.score >> e.kills >> difficulty) {
            snprintf(e.date, sizeof(e.date), "%s", firstToken.c_str());
            m_entries.push_back(e);
        }
        std::string dateStr;
        while (f >> dateStr >> e.score >> e.kills >> difficulty) {
            e = DailyLeaderboardEntry{};
            snprintf(e.date, sizeof(e.date), "%s", dateStr.c_str());
            m_entries.push_back(e);
        }
    }
    prune();
}
