#include "NPCSystem.h"

NPCData NPCSystem::createNPC(NPCType type, Vec2 pos, int dimension) {
    NPCData npc;
    npc.type = type;
    npc.position = pos;
    npc.dimension = dimension;

    switch (type) {
        case NPCType::RiftScholar:
            npc.name = "Rift Scholar";
            npc.greeting = "Ah, a traveler! Let me share what I know...";
            npc.dialogLine = "The dimensional rifts grow unstable. Beware the Weaver's time zones.";
            break;
        case NPCType::DimRefugee:
            npc.name = "Dimensional Refugee";
            npc.greeting = "Please... I need help. Perhaps we can trade?";
            npc.dialogLine = "I can offer shards for your health, or swap a relic.";
            break;
        case NPCType::LostEngineer:
            npc.name = "Lost Engineer";
            npc.greeting = "Your weapons look battered. Let me take a look.";
            npc.dialogLine = "I can temporarily boost your weapon's power by 30%.";
            break;
        case NPCType::EchoOfSelf:
            npc.name = "Echo of Self";
            npc.greeting = "You face... yourself. Defeat me for a great reward.";
            npc.dialogLine = "A mirror match begins!";
            break;
        default: break;
    }
    return npc;
}

const char* NPCSystem::getName(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar: return "Rift Scholar";
        case NPCType::DimRefugee: return "Dim. Refugee";
        case NPCType::LostEngineer: return "Lost Engineer";
        case NPCType::EchoOfSelf: return "Echo of Self";
        default: return "Unknown";
    }
}

const char* NPCSystem::getGreeting(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar: return "Knowledge is power in the rift.";
        case NPCType::DimRefugee: return "Let's make a deal...";
        case NPCType::LostEngineer: return "I can fix that for you.";
        case NPCType::EchoOfSelf: return "Face yourself!";
        default: return "";
    }
}

std::vector<const char*> NPCSystem::getDialogOptions(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar:
            return {"[Listen to tip]", "[Ask about enemies]", "[Leave]"};
        case NPCType::DimRefugee:
            return {"[Trade 20 HP for 50 Shards]", "[Swap random Relic]", "[Leave]"};
        case NPCType::LostEngineer:
            return {"[Upgrade weapon (+30%)]", "[Leave]"};
        case NPCType::EchoOfSelf:
            return {"[Fight!]", "[Not yet...]"};
        default:
            return {"[Leave]"};
    }
}
