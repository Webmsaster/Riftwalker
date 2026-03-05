#include "Game/DailyRun.h"
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <cstring>

int DailyRun::getTodaySeed() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    return (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday;
}

std::string DailyRun::getTodayDate() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    return std::string(buf);
}

void DailyRun::addRecord(int score, int kills, int difficulty) {
    DailyRecord rec;
    rec.score = score;
    rec.kills = kills;
    rec.difficulty = difficulty;
    auto date = getTodayDate();
    strncpy(rec.date, date.c_str(), sizeof(rec.date) - 1);
    rec.date[sizeof(rec.date) - 1] = '\0';

    m_records.push_back(rec);
    std::sort(m_records.begin(), m_records.end(), [](const DailyRecord& a, const DailyRecord& b) {
        return a.score > b.score;
    });
    if (static_cast<int>(m_records.size()) > MAX_RECORDS) {
        m_records.resize(MAX_RECORDS);
    }
}

int DailyRun::getTodayBest() const {
    auto today = getTodayDate();
    int best = 0;
    for (auto& r : m_records) {
        if (std::string(r.date) == today && r.score > best) {
            best = r.score;
        }
    }
    return best;
}

void DailyRun::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return;
    for (auto& r : m_records) {
        f << r.date << " " << r.score << " " << r.kills << " " << r.difficulty << "\n";
    }
}

void DailyRun::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    m_records.clear();
    DailyRecord rec;
    std::string dateStr;
    while (f >> dateStr >> rec.score >> rec.kills >> rec.difficulty) {
        strncpy(rec.date, dateStr.c_str(), sizeof(rec.date) - 1);
        rec.date[sizeof(rec.date) - 1] = '\0';
        m_records.push_back(rec);
        if (static_cast<int>(m_records.size()) >= MAX_RECORDS) break;
    }
}
