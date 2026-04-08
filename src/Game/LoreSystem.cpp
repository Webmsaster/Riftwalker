#include "Game/LoreSystem.h"
#include "Core/SaveUtils.h"
#include "Core/Localization.h"
#include <fstream>
#include <cstdio>
#include <cstring>

void LoreSystem::init() {
    m_fragments.clear();
    m_fragments.resize(static_cast<int>(LoreID::COUNT));

    auto add = [&](LoreID id, const std::string& title, const std::string& text) {
        int idx = static_cast<int>(id);
        m_fragments[idx].id = id;
        m_fragments[idx].title = title;
        m_fragments[idx].text = text;
    };

    add(LoreID::TheRift, "The Rift",
        "Between worlds lies a wound in reality itself. The Rift tears through "
        "dimensions like a river of void, connecting places that should never touch. "
        "Those who enter rarely return unchanged.");

    add(LoreID::DimensionOrigin, "Echoes of Origin",
        "The dimensions were once one. A cataclysm split reality into layers, each "
        "a distorted mirror of the other. The boundaries grow thinner with each "
        "passing cycle, and the creatures between grow bolder.");

    add(LoreID::WalkerScourge, "The Walker Scourge",
        "Walkers are the most common denizens of the Rift. Mindless husks driven "
        "by an insatiable hunger, they wander the fractured corridors endlessly. "
        "Some say they were once explorers like you.");

    add(LoreID::VoidHunger, "Void Hunger",
        "Entropy is the Rift's breath. As it rises, reality frays at the edges. "
        "Colors bleed, sounds distort, and the boundary between dimensions grows "
        "paper-thin. To survive high entropy is to dance on the edge of oblivion.");

    add(LoreID::AscendedOnes, "The Ascended",
        "Those who prove themselves worthy in the Rift are granted Ascension - "
        "a chance to begin anew with greater power. Each Ascension strips away "
        "weakness, but the Rift responds in kind, growing ever more hostile.");

    add(LoreID::ForgottenCraft, "Forgotten Craft",
        "The shopkeepers of the Rift are enigmatic figures. They exist outside "
        "normal space, their stalls appearing at crossroads between dimensions. "
        "No one knows what they do with the shards they collect.");

    add(LoreID::BossMemory1, "Guardian's Echo",
        "The first guardian you defeated was merely a sentinel - a construct of "
        "the Rift designed to test intruders. Its destruction sent ripples through "
        "every connected dimension.");

    add(LoreID::BossMemory2, "The Wyrm's Lament",
        "The Void Wyrm was ancient beyond measure, a creature born in the space "
        "between spaces. Its death cry resonated across all dimensions, awakening "
        "things that had long slumbered.");

    add(LoreID::BossMemory3, "Architect's Design",
        "The Dimensional Architect sought to rebuild reality according to its own "
        "blueprint. Its towers and walls were attempts to stitch the Rift closed - "
        "or perhaps to tear it wider. Its true purpose died with it.");

    add(LoreID::BossMemory4, "Weaver's Thread",
        "The Temporal Weaver existed in all moments simultaneously. Defeating it "
        "was not a matter of strength, but of breaking the loop it had woven "
        "around itself. Time flows freely now - for better or worse.");

    add(LoreID::SovereignTruth, "The Sovereign's Truth",
        "The Void Sovereign was not a ruler but a prisoner. Chained to the deepest "
        "layer of the Rift, it spent eons trying to break free. In defeating it, "
        "you may have released something far worse - or perhaps set it free at last.");

    add(LoreID::FinalRevelation, "Final Revelation",
        "You are the Rift. Every dimension switch, every enemy slain, every cycle "
        "completed - you were not exploring the wound in reality. You were the wound, "
        "learning to heal itself. The Rift ends where you began.");

    add(LoreID::SwarmNature, "The Swarm's Nature",
        "Rift Swarmers are not individuals but fragments of a single hive-mind that "
        "shattered when the dimensions split. Each amber insect carries a sliver of "
        "collective memory. In great enough numbers, they almost remember what they were.");

    add(LoreID::GravityAnomaly, "Gravity Anomalies",
        "Where the dimensional fabric is thinnest, gravity wells form spontaneously — "
        "living singularities that feed on spatial distortion. Ancient rift scholars "
        "called them 'the void's hunger given form.' They do not chase. They wait.");

    add(LoreID::MimicDeception, "The Art of Deception",
        "Mimics were once ordinary matter, reshaped by prolonged exposure to unstable "
        "rift energy. They learned to fold their forms into familiar shapes — crates, "
        "chests, rubble. The perfect camouflage for a dimension where nothing is certain.");

    add(LoreID::EntropyIncarnate, "Entropy Made Flesh",
        "Entropy Incarnate is not a creature but a concept given form — the inevitable "
        "heat death of all dimensions condensed into a single entity. Defeating it does "
        "not destroy entropy. It merely delays the end, buying reality one more cycle.");

    add(LoreID::DimensionTheory, "Dimensional Theory",
        "The Fortune Tellers of the Rift possess a rare gift: they can perceive all "
        "dimensional layers simultaneously. What appears as prophecy is simply seeing "
        "what already exists in adjacent realities. The future is just another dimension.");

    add(LoreID::RiftEcology, "Rift Ecology",
        "The Rift is not merely a wound — it is an ecosystem. Walkers graze on "
        "dimensional energy. Flyers hunt Walkers. Bosses regulate population. Even "
        "the shards you collect are nutrients in this alien food chain. You are apex predator.");

    add(LoreID::VoidCommerce, "Void Commerce",
        "The Void Merchants travel between all known rifts, trading relics gathered "
        "from fallen travelers. Their economy runs on dimensional shards — crystallized "
        "reality that serves as universal currency across every fractured world.");

    add(LoreID::ClassLegacy, "Legacy of the Four",
        "Voidwalker, Berserker, Phantom, Technomancer — four paths through the Rift, "
        "each born from a different philosophy of survival. The Voidwalker embraces "
        "the between. The Berserker defies it. The Phantom slips through. The Technomancer "
        "rewrites it. Together, they are the Rift's full vocabulary.");
}

void LoreSystem::discover(LoreID id) {
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(m_fragments.size())) {
        if (!m_fragments[idx].discovered) {
            m_fragments[idx].discovered = true;
            // Pick localized title (falls back to hardcoded EN if key missing)
            char titleKey[32];
            std::snprintf(titleKey, sizeof(titleKey), "lore.%d.title", idx);
            const char* locTitle = LOC(titleKey);
            m_notification.title = (std::strcmp(locTitle, titleKey) == 0)
                ? m_fragments[idx].title
                : std::string(locTitle);
            m_notification.timer = m_notification.duration;
        }
    }
}

void LoreSystem::updateNotification(float dt) {
    if (m_notification.timer > 0) {
        m_notification.timer -= dt;
    }
}

const LoreNotification* LoreSystem::getActiveNotification() const {
    if (m_notification.timer > 0) return &m_notification;
    return nullptr;
}

bool LoreSystem::isDiscovered(LoreID id) const {
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(m_fragments.size())) {
        return m_fragments[idx].discovered;
    }
    return false;
}

int LoreSystem::discoveredCount() const {
    int count = 0;
    for (auto& f : m_fragments) {
        if (f.discovered) count++;
    }
    return count;
}

void LoreSystem::save(const std::string& path) const {
    atomicSave(path, [&](std::ofstream& file) {
        for (auto& f : m_fragments) {
            file << static_cast<int>(f.id) << " " << (f.discovered ? 1 : 0) << "\n";
        }
    });
}

void LoreSystem::load(const std::string& path) {
    std::ifstream file = openWithBackupFallback(path);
    if (!file.is_open()) return;
    int id, disc;
    while (file >> id >> disc) {
        if (id >= 0 && id < static_cast<int>(m_fragments.size())) {
            m_fragments[id].discovered = (disc != 0);
        }
    }
}
