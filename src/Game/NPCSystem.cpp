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
        case NPCType::Blacksmith:
            npc.name = "Rift Blacksmith";
            npc.greeting = "Bring me shards and I'll forge your weapons anew.";
            npc.dialogLine = "Rift-tempered steel cuts deeper.";
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
        case NPCType::Blacksmith: return "Blacksmith";
        default: return "Unknown";
    }
}

const char* NPCSystem::getGreeting(NPCType type, int storyStage) {
    switch (type) {
        case NPCType::RiftScholar:
            if (storyStage >= 2) return "You again! I've deciphered the final texts.";
            if (storyStage >= 1) return "Welcome back! I've found something crucial...";
            return "Knowledge is power in the rift.";
        case NPCType::DimRefugee:
            if (storyStage >= 2) return "My friend! I can finally repay your kindness.";
            if (storyStage >= 1) return "You came back! I found supplies to share.";
            return "Let's make a deal...";
        case NPCType::LostEngineer:
            if (storyStage >= 2) return "My masterwork is complete. This one's on me.";
            if (storyStage >= 1) return "I've been studying rift energy. Better results!";
            return "I can fix that for you.";
        case NPCType::EchoOfSelf:
            if (storyStage >= 2) return "One final test. Prove your mastery.";
            if (storyStage >= 1) return "Stronger now... Can you match me again?";
            return "Face yourself!";
        case NPCType::Blacksmith:
            if (storyStage >= 2) return "For you, my finest work. Free of charge.";
            if (storyStage >= 1) return "Your weapons have seen battle. I can do more now.";
            return "Shards fuel my forge. Let me improve your gear.";
        default: return "";
    }
}

const char* NPCSystem::getStoryLine(NPCType type, int storyStage) {
    switch (type) {
        case NPCType::RiftScholar:
            if (storyStage >= 2)
                return "The truth: the Sovereign was once a guardian. The rifts\nare the dimensions healing themselves. You are the catalyst.";
            if (storyStage >= 1)
                return "Ancient texts reveal the rifts aren't natural.\nSomeone — or something — tore the dimensions apart.";
            return "The dimensional rifts grow unstable. Each repair\nweakens the Void Sovereign's grip on reality.";
        case NPCType::DimRefugee:
            if (storyStage >= 2)
                return "You've helped me survive this long. I gathered\nrift components — take this gift, no strings attached.";
            if (storyStage >= 1)
                return "I found medical supplies between the dimensions.\nLet me patch you up — and I have a better deal too.";
            return "I'm trapped between dimensions. Trade me some health\nand I'll give you rift shards I've collected.";
        case NPCType::LostEngineer:
            if (storyStage >= 2)
                return "I cracked it! Rift-infused metal permanently bonds\nwith weapons. This upgrade lasts your whole run.";
            if (storyStage >= 1)
                return "Rift energy amplifies weapons more than I thought!\nLonger effect, plus I can boost your attack speed.";
            return "Let me recalibrate your weapon with rift harmonics.\nShould boost damage output for a while.";
        case NPCType::EchoOfSelf:
            if (storyStage >= 2) return "Your ultimate echo. Defeat it for the greatest reward.";
            if (storyStage >= 1) return "Your echo is stronger now, adapted from last time.";
            return "A mirror match begins! Defeat your echo for a reward.";
        case NPCType::Blacksmith:
            if (storyStage >= 2)
                return "I've perfected my rift-forging technique.\nAccept my masterwork — both weapons, permanently enhanced.";
            if (storyStage >= 1)
                return "With more rift energy, I can now upgrade range\nand attack speed too. Better prices for a returning customer.";
            return "My forge burns with rift energy. I can sharpen your\nmelee or reinforce your ranged weapon — for a price.";
        default: return "";
    }
}

std::vector<const char*> NPCSystem::getDialogOptions(NPCType type, int storyStage) {
    switch (type) {
        case NPCType::RiftScholar:
            if (storyStage >= 2)
                return {"[Learn the truth (+40 Shards, heal)]", "[Leave]"};
            if (storyStage >= 1)
                return {"[Listen (+25 Shards, -entropy)]", "[Ask about the Sovereign]", "[Leave]"};
            return {"[Listen to tip (+15 Shards)]", "[Ask about enemies]", "[Leave]"};
        case NPCType::DimRefugee:
            if (storyStage >= 2)
                return {"[Accept gift (free Relic)]", "[Leave]"};
            if (storyStage >= 1)
                return {"[Free healing (+30 HP)]", "[Trade 15 HP for 60 Shards]", "[Leave]"};
            return {"[Trade 20 HP for 50 Shards]", "[Trade 30 HP for 80 Shards]", "[Leave]"};
        case NPCType::LostEngineer:
            if (storyStage >= 2)
                return {"[Permanent upgrade (+25% DMG)]", "[Leave]"};
            if (storyStage >= 1)
                return {"[Upgrade weapon (+40% DMG, 60s)]", "[Tune attacks (+20% speed, 45s)]", "[Leave]"};
            return {"[Upgrade weapon (+30% DMG, 45s)]", "[Leave]"};
        case NPCType::EchoOfSelf:
            return {"[Fight!]", "[Not yet...]"};
        case NPCType::Blacksmith:
            if (storyStage >= 2)
                return {"[Accept masterwork (free +30% both)]", "[Leave]"};
            if (storyStage >= 1)
                return {"[Sharpen melee +25% DMG (35 shards)]", "[Reinforce ranged +25% DMG (35 shards)]",
                        "[Hone speed +15% ATK SPD (45 shards)]", "[Leave]"};
            return {"[Sharpen melee +20% DMG (40 shards)]", "[Reinforce ranged +20% DMG (40 shards)]", "[Leave]"};
        default:
            return {"[Leave]"};
    }
}
