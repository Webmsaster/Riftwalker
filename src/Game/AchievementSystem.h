#pragma once
#include <string>
#include <vector>
#include <functional>

struct Achievement {
    std::string id;
    std::string name;
    std::string description;
    std::string rewardDesc;   // Short reward text shown on unlock
    bool unlocked = false;
};

// Accumulated bonuses from all unlocked achievements
struct AchievementBonuses {
    float maxHPBonus = 0;           // Flat HP added
    float meleeDamageMult = 1.0f;   // Multiplicative
    float rangedDamageMult = 1.0f;
    float moveSpeedMult = 1.0f;
    float dashCooldownMult = 1.0f;
    float switchCooldownMult = 1.0f;
    float shardDropMult = 1.0f;
    float critChanceBonus = 0;      // Flat added
    float comboDamageBonus = 0;     // Flat added
    float armorBonus = 0;
    float dotDurationMult = 1.0f;   // < 1.0 = shorter DOTs
    float allDamageMult = 1.0f;
};

// Notification popup data
struct AchievementNotification {
    std::string name;
    std::string rewardText;
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

    // Accumulated bonuses from all unlocked achievements
    AchievementBonuses getUnlockedBonuses() const;

    // Active notification (for HUD)
    const AchievementNotification* getActiveNotification() const;

    // Persistence
    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

private:
    std::vector<Achievement> m_achievements;
    AchievementNotification m_notification;
};
