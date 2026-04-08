#include "NPCSystem.h"
#include "Core/Localization.h"

NPCData NPCSystem::createNPC(NPCType type, Vec2 pos, int dimension) {
    NPCData npc;
    npc.type = type;
    npc.position = pos;
    npc.dimension = dimension;

    // Legacy fields — dialog is rendered via NPCSystem::getGreeting()/getDialogOptions() instead
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
        case NPCType::Blacksmith:
            npc.name = "Rift Blacksmith";
            npc.greeting = "Bring me shards and I'll forge your weapons anew.";
            npc.dialogLine = "Rift-tempered steel cuts deeper.";
            break;
        case NPCType::FortuneTeller:
            npc.name = "Fortune Teller";
            npc.greeting = "The rifts whisper to me... I see what lies ahead.";
            npc.dialogLine = "For a small offering, I can reveal hidden paths.";
            break;
        case NPCType::VoidMerchant:
            npc.name = "Void Merchant";
            npc.greeting = "Rare artifacts from between dimensions... interested?";
            npc.dialogLine = "Each relic carries the echo of a fallen traveler.";
            break;
        default: break;
    }
    return npc;
}

const char* NPCSystem::getName(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar: return LOC("npc.scholar");
        case NPCType::DimRefugee: return LOC("npc.refugee");
        case NPCType::LostEngineer: return LOC("npc.engineer");
        case NPCType::EchoOfSelf: return LOC("npc.echo");
        case NPCType::Blacksmith: return LOC("npc.blacksmith");
        case NPCType::FortuneTeller: return LOC("npc.fortune");
        case NPCType::VoidMerchant: return LOC("npc.merchant");
        default: return "Unknown";
    }
}

const char* NPCSystem::getGreeting(NPCType type, int storyStage) {
    switch (type) {
        case NPCType::RiftScholar:
            if (storyStage >= 2) return LOC("npc.scholar.g2");
            if (storyStage >= 1) return LOC("npc.scholar.g1");
            return LOC("npc.scholar.g0");
        case NPCType::DimRefugee:
            if (storyStage >= 2) return LOC("npc.refugee.g2");
            if (storyStage >= 1) return LOC("npc.refugee.g1");
            return LOC("npc.refugee.g0");
        case NPCType::LostEngineer:
            if (storyStage >= 2) return LOC("npc.engineer.g2");
            if (storyStage >= 1) return LOC("npc.engineer.g1");
            return LOC("npc.engineer.g0");
        case NPCType::EchoOfSelf:
            if (storyStage >= 2) return LOC("npc.echo.g2");
            if (storyStage >= 1) return LOC("npc.echo.g1");
            return LOC("npc.echo.g0");
        case NPCType::Blacksmith:
            if (storyStage >= 2) return LOC("npc.blacksmith.g2");
            if (storyStage >= 1) return LOC("npc.blacksmith.g1");
            return LOC("npc.blacksmith.g0");
        case NPCType::FortuneTeller:
            if (storyStage >= 2) return LOC("npc.fortune.g2");
            if (storyStage >= 1) return LOC("npc.fortune.g1");
            return LOC("npc.fortune.g0");
        case NPCType::VoidMerchant:
            if (storyStage >= 2) return LOC("npc.merchant.g2");
            if (storyStage >= 1) return LOC("npc.merchant.g1");
            return LOC("npc.merchant.g0");
        default: return "";
    }
}

const char* NPCSystem::getStoryLine(NPCType type, int storyStage) {
    switch (type) {
        case NPCType::RiftScholar:
            if (storyStage >= 2) return LOC("npc.scholar.d2");
            if (storyStage >= 1) return LOC("npc.scholar.d1");
            return LOC("npc.scholar.d0");
        case NPCType::DimRefugee:
            if (storyStage >= 2) return LOC("npc.refugee.d2");
            if (storyStage >= 1) return LOC("npc.refugee.d1");
            return LOC("npc.refugee.d0");
        case NPCType::LostEngineer:
            if (storyStage >= 2) return LOC("npc.engineer.d2");
            if (storyStage >= 1) return LOC("npc.engineer.d1");
            return LOC("npc.engineer.d0");
        case NPCType::EchoOfSelf:
            if (storyStage >= 2) return LOC("npc.echo.d2");
            if (storyStage >= 1) return LOC("npc.echo.d1");
            return LOC("npc.echo.d0");
        case NPCType::Blacksmith:
            if (storyStage >= 2) return LOC("npc.smith.d2");
            if (storyStage >= 1) return LOC("npc.smith.d1");
            return LOC("npc.smith.d0");
        case NPCType::FortuneTeller:
            if (storyStage >= 2) return LOC("npc.fortune.d2");
            if (storyStage >= 1) return LOC("npc.fortune.d1");
            return LOC("npc.fortune.d0");
        case NPCType::VoidMerchant:
            if (storyStage >= 2) return LOC("npc.merchant.d2");
            if (storyStage >= 1) return LOC("npc.merchant.d1");
            return LOC("npc.merchant.d0");
        default: return "";
    }
}

std::vector<const char*> NPCSystem::getDialogOptions(NPCType type, int storyStage, bool hasActiveQuest) {
    switch (type) {
        case NPCType::RiftScholar:
            if (storyStage >= 2)
                return {LOC("npc.scholar.s2.opt"), LOC("npc.opt.leave")};
            if (storyStage >= 1)
                return {LOC("npc.scholar.s1.opt1"), LOC("npc.scholar.s1.opt2"), LOC("npc.opt.leave")};
            if (!hasActiveQuest)
                return {LOC("npc.scholar.s0.opt1"), LOC("npc.scholar.s0.opt2"),
                        LOC("npc.scholar.quest"), LOC("npc.opt.leave")};
            return {LOC("npc.scholar.s0.opt1"), LOC("npc.scholar.s0.opt2"), LOC("npc.opt.leave")};
        case NPCType::DimRefugee:
            if (storyStage >= 2)
                return {LOC("npc.refugee.s2.opt"), LOC("npc.opt.leave")};
            if (storyStage >= 1)
                return {LOC("npc.refugee.s1.opt1"), LOC("npc.refugee.s1.opt2"), LOC("npc.opt.leave")};
            return {LOC("npc.refugee.s0.opt1"), LOC("npc.refugee.s0.opt2"), LOC("npc.opt.leave")};
        case NPCType::LostEngineer:
            if (storyStage >= 2)
                return {LOC("npc.engineer.s2.opt"), LOC("npc.opt.leave")};
            if (storyStage >= 1)
                return {LOC("npc.engineer.s1.opt1"), LOC("npc.engineer.s1.opt2"), LOC("npc.opt.leave")};
            if (!hasActiveQuest)
                return {LOC("npc.engineer.s0.opt"),
                        LOC("npc.engineer.quest"), LOC("npc.opt.leave")};
            return {LOC("npc.engineer.s0.opt"), LOC("npc.opt.leave")};
        case NPCType::EchoOfSelf:
            return {LOC("npc.opt.fight"), LOC("npc.opt.not_yet")};
        case NPCType::Blacksmith:
            if (storyStage >= 2)
                return {LOC("npc.smith.s2.opt"), LOC("npc.opt.leave")};
            if (storyStage >= 1)
                return {LOC("npc.smith.s1.opt1"), LOC("npc.smith.s1.opt2"),
                        LOC("npc.smith.s1.opt3"), LOC("npc.opt.leave")};
            return {LOC("npc.smith.s0.opt1"), LOC("npc.smith.s0.opt2"), LOC("npc.opt.leave")};
        case NPCType::FortuneTeller:
            if (storyStage >= 2)
                return {LOC("npc.fortune.s2.opt1"), LOC("npc.fortune.s2.opt2"), LOC("npc.opt.leave")};
            if (storyStage >= 1)
                return {LOC("npc.fortune.s1.opt1"), LOC("npc.fortune.s1.opt2"), LOC("npc.opt.leave")};
            return {LOC("npc.fortune.s0.opt"), LOC("npc.opt.read_fortune"), LOC("npc.opt.leave")};
        case NPCType::VoidMerchant:
            if (storyStage >= 2)
                return {LOC("npc.merch.s2.opt1"), LOC("npc.merch.s2.opt2"), LOC("npc.opt.leave")};
            if (storyStage >= 1)
                return {LOC("npc.merch.s1.opt1"), LOC("npc.merch.s1.opt2"), LOC("npc.opt.leave")};
            return {LOC("npc.merch.s0.opt"), LOC("npc.opt.browse"), LOC("npc.opt.leave")};
        default:
            return {LOC("npc.opt.leave")};
    }
}

const char* NPCSystem::getQuestOffer(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar:    return LOC("npc.quest.scholar");
        case NPCType::LostEngineer:   return LOC("npc.quest.engineer");
        case NPCType::FortuneTeller:  return LOC("npc.quest.fortune");
        default: return "";
    }
}

const char* NPCSystem::getQuestProgress(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar:    return LOC("npc.qprog.scholar");
        case NPCType::LostEngineer:   return LOC("npc.qprog.engineer");
        case NPCType::FortuneTeller:  return LOC("npc.qprog.fortune");
        default: return "";
    }
}

const char* NPCSystem::getQuestComplete(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar:    return LOC("npc.qdone.scholar");
        case NPCType::LostEngineer:   return LOC("npc.qdone.engineer");
        case NPCType::FortuneTeller:  return LOC("npc.qdone.fortune");
        default: return "";
    }
}
