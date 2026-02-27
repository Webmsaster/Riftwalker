#pragma once
#include <string>
#include <vector>
#include <functional>

struct Achievement {
    std::string id;
    std::string name;
    std::string description;
    bool unlocked = false;
};

// Notification popup data
struct AchievementNotification {
    std::string name;
    float timer = 0;
    float duration = 3.0f;
};

class AchievementSystem {
public:
    void init();

    // Check and unlock - returns true if newly unlocked
    bool unlock(const std::string& id);
    bool isUnlocked(const std::string& id) const;

    // Update notification timers
    void update(float dt);

    // Getters
    const std::vector<Achievement>& getAll() const { return m_achievements; }
    int getUnlockedCount() const;
    int getTotalCount() const { return static_cast<int>(m_achievements.size()); }

    // Active notification (for HUD)
    const AchievementNotification* getActiveNotification() const;

    // Persistence
    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

private:
    std::vector<Achievement> m_achievements;
    AchievementNotification m_notification;
};
