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
    static std::vector<const char*> getDialogOptions(NPCType type, int storyStage = 0);
};
