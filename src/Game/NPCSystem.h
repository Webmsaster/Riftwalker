#pragma once
#include "Core/Camera.h"
#include <string>
#include <vector>

enum class NPCType {
    RiftScholar = 0,    // Gives lore + random tip
    DimRefugee,         // Trade offer (HP for shards, relic swap)
    LostEngineer,       // Repair/upgrade weapon temporarily
    EchoOfSelf,         // Mirror fight: defeat your copy for reward
    Blacksmith,         // Upgrade weapons for shards (per-run)
    COUNT
};

// Simple kill/collect quest offered by NPCs
struct NPCQuest {
    std::string description;
    int targetKills = 0;      // Kill N enemies
    int targetRifts = 0;      // Repair N rifts
    int currentKills = 0;
    int currentRifts = 0;
    int shardReward = 0;
    float entropyReduction = 0; // Bonus entropy reduction on completion
    bool active = false;
    bool completed = false;
    NPCType questGiver = NPCType::RiftScholar;
};

struct NPCData {
    NPCType type = NPCType::RiftScholar;
    Vec2 position{0, 0};
    bool interacted = false;
    int dimension = 0;
    float bobTimer = 0;

    // Dialog
    const char* name = "";
    const char* greeting = "";
    const char* dialogLine = "";
};

class NPCSystem {
public:
    static NPCData createNPC(NPCType type, Vec2 pos, int dimension);
    static const char* getName(NPCType type);
    // Stage-based dialog (storyStage 0-2 based on prior encounters this run)
    static const char* getGreeting(NPCType type, int storyStage = 0);
    static const char* getStoryLine(NPCType type, int storyStage);
    static std::vector<const char*> getDialogOptions(NPCType type, int storyStage = 0, bool hasActiveQuest = false);
    // Quest dialog for NPCs that offer quests
    static const char* getQuestOffer(NPCType type);
    static const char* getQuestProgress(NPCType type);
    static const char* getQuestComplete(NPCType type);
};
