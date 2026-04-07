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
        case NPCType::FortuneTeller:
            if (storyStage >= 2)
                return "My visions have never been clearer. I can reveal every\nsecret on this floor — and grant you foresight against the boss.";
            if (storyStage >= 1)
                return "The dimensional threads are easier to read now.\nI can show you hidden rooms and warn of ambushes ahead.";
            return "I sense hidden chambers nearby. For a small offering,\nI'll reveal their locations on your map.";
        case NPCType::VoidMerchant:
            if (storyStage >= 2)
                return "My finest acquisition: a relic from the Sovereign's own\nvault. Normally priceless — but for you, a special rate.";
            if (storyStage >= 1)
                return "I've expanded my collection. These relics carry\nstronger dimensional resonance — worth every shard.";
            return "Each of these relics was pried from the void between\ndimensions. They carry great power — at a fair price.";
        default: return "";
    }
}

std::vector<const char*> NPCSystem::getDialogOptions(NPCType type, int storyStage, bool hasActiveQuest) {
    switch (type) {
        case NPCType::RiftScholar:
            if (storyStage >= 2)
                return {"[Learn the truth (+40 Shards, heal)]", "[Leave]"};
            if (storyStage >= 1)
                return {"[Listen (+25 Shards, -entropy)]", "[Ask about the Sovereign]", "[Leave]"};
            // Stage 0: offer quest option alongside normal options
            if (!hasActiveQuest)
                return {"[Listen to tip (+15 Shards)]", "[Ask about enemies]",
                        "[Accept quest: Hunt 10 creatures]", "[Leave]"};
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
            // Stage 0: offer quest option alongside normal options
            if (!hasActiveQuest)
                return {"[Upgrade weapon (+30% DMG, 45s)]",
                        "[Accept quest: Repair 3 rifts]", "[Leave]"};
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
        case NPCType::FortuneTeller:
            if (storyStage >= 2)
                return {"[Reveal all secrets (free)]", "[Boss foresight (+20% DMG vs boss)]", "[Leave]"};
            if (storyStage >= 1)
                return {"[Reveal hidden rooms (20 shards)]", "[Reveal ambushes (15 shards)]", "[Leave]"};
            return {"[Reveal secrets (30 shards)]", "[Read my fortune]", "[Leave]"};
        case NPCType::VoidMerchant:
            if (storyStage >= 2)
                return {"[Buy legendary relic (80 shards)]", "[Buy random relic (40 shards)]", "[Leave]"};
            if (storyStage >= 1)
                return {"[Buy relic (60 shards)]", "[Buy random relic (35 shards)]", "[Leave]"};
            return {"[Buy relic (50 shards)]", "[Browse wares]", "[Leave]"};
        default:
            return {"[Leave]"};
    }
}

const char* NPCSystem::getQuestOffer(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar:
            return "I need data on the rift creatures. Defeat 10 enemies\nand I'll reward you with rift shards.";
        case NPCType::LostEngineer:
            return "Help me calibrate the rift sensors. Repair 3 rifts\non this floor and I'll pay you — plus stabilize your suit.";
        case NPCType::FortuneTeller:
            return "The void hides many secrets. Find 2 secret rooms\non this floor, and I'll share a powerful vision.";
        default: return "";
    }
}

const char* NPCSystem::getQuestProgress(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar:
            return "Keep hunting those creatures. I can sense more data flowing in...";
        case NPCType::LostEngineer:
            return "The sensors are picking up your repairs. Keep going!";
        case NPCType::FortuneTeller:
            return "I sense you've found some hidden places... keep searching.";
        default: return "";
    }
}

const char* NPCSystem::getQuestComplete(NPCType type) {
    switch (type) {
        case NPCType::RiftScholar:
            return "Excellent work! The data you gathered is invaluable.\nHere are your shards, as promised.";
        case NPCType::LostEngineer:
            return "Sensors fully calibrated! Your suit readings look better too.\nTake this payment — you've earned it.";
        case NPCType::FortuneTeller:
            return "Your discoveries have amplified my visions tenfold!\nAs promised — a glimpse of what awaits on the next floor.";
        default: return "";
    }
}
