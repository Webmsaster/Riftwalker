#pragma once
#include <string>
#include <vector>
#include <array>

enum class LoreID {
    TheRift,            // Secret room discovery
    DimensionOrigin,    // First dimension switch
    WalkerScourge,      // Kill 50 Walkers
    VoidHunger,         // Survive Entropy > 90%
    AscendedOnes,       // First Ascension
    ForgottenCraft,     // NPC shop interaction
    BossMemory1,        // Kill first boss
    BossMemory2,        // Kill Void Wyrm
    BossMemory3,        // Kill Dimensional Architect
    BossMemory4,        // Kill Temporal Weaver
    SovereignTruth,     // Kill Void Sovereign
    FinalRevelation,    // Complete ending
    SwarmNature,        // Encounter Swarmers
    GravityAnomaly,     // Encounter GravityWell
    MimicDeception,     // Kill a Mimic
    EntropyIncarnate,   // Kill Entropy Incarnate boss
    DimensionTheory,    // Use Fortune Teller
    RiftEcology,        // Discover 5+ secret rooms
    VoidCommerce,       // Use Void Merchant
    ClassLegacy,        // Win with all 4 classes
    COUNT
};

struct LoreFragment {
    LoreID id;
    std::string title;
    std::string text;
    bool discovered = false;
};

struct LoreNotification {
    std::string title;
    float timer = 0;
    float duration = 4.0f;
};

class LoreSystem {
public:
    void init();
    void discover(LoreID id);
    bool isDiscovered(LoreID id) const;
    int discoveredCount() const;
    int totalCount() const { return static_cast<int>(LoreID::COUNT); }
    const std::vector<LoreFragment>& getFragments() const { return m_fragments; }

    void updateNotification(float dt);
    const LoreNotification* getActiveNotification() const;

    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    std::vector<LoreFragment> m_fragments;
    LoreNotification m_notification;
};
