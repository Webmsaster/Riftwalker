#pragma once
#include <string>
#include <vector>
#include <ctime>

struct DailyRecord {
    int score = 0;
    int kills = 0;
    int difficulty = 0;
    char date[12] = {};
};

// Global flag for daily run mode
inline bool g_dailyRunActive = false;

class DailyRun {
public:
    static int getTodaySeed();
    static std::string getTodayDate();

    void addRecord(int score, int kills, int difficulty);
    const std::vector<DailyRecord>& getRecords() const { return m_records; }
    int getTodayBest() const;

    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    std::vector<DailyRecord> m_records; // Top 10
    static constexpr int MAX_RECORDS = 10;
};
