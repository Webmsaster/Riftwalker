#include "RelicSystem.h"
#include "Game/Player.h"
#include "Game/RelicSynergy.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include <cstdlib>
#include <algorithm>

// === Balance Safety Rails ===
static constexpr float MAX_ATK_SPEED_MULT = 1.6f;       // Hard cap on attack speed
static constexpr float MAX_DMG_MULT = 4.0f;             // Hard cap on damage multiplier
static constexpr float MAX_VOID_HUNGER_BONUS = 0.40f;   // +40% cap from VoidHunger
static constexpr float MIN_SWITCH_COOLDOWN = 0.20f;     // Floor for dimension switch CD (seconds)
static constexpr float VOID_RESONANCE_ICD = 0.8f;       // Seconds between VoidResonance 2x procs
static constexpr float DIM_RESIDUE_SPAWN_ICD = 1.2f;    // Seconds between DimResidue zone spawns
static constexpr int   MAX_RESIDUE_ZONES = 3;            // Max concurrent DimResidue zones

static std::vector<RelicData> s_relicData;

static void initRelicData() {
    if (!s_relicData.empty()) return;
    s_relicData.resize(static_cast<int>(RelicID::COUNT));

    auto& d = s_relicData;
    d[static_cast<int>(RelicID::IronHeart)]      = {RelicID::IronHeart, "Iron Heart", "+15 Max HP", RelicTier::Common, SDL_Color{200, 200, 200, 255}};
    d[static_cast<int>(RelicID::SwiftBoots)]      = {RelicID::SwiftBoots, "Swift Boots", "+10% Move Speed", RelicTier::Common, SDL_Color{100, 255, 200, 255}};
    d[static_cast<int>(RelicID::ShardMagnet)]     = {RelicID::ShardMagnet, "Shard Magnet", "+50% Pickup Radius", RelicTier::Common, SDL_Color{180, 130, 255, 255}};
    d[static_cast<int>(RelicID::ThornMail)]       = {RelicID::ThornMail, "Thorn Mail", "8 DMG Reflection", RelicTier::Common, SDL_Color{200, 100, 50, 255}};
    d[static_cast<int>(RelicID::QuickHands)]      = {RelicID::QuickHands, "Quick Hands", "+15% Attack Speed", RelicTier::Common, SDL_Color{255, 230, 100, 255}};
    d[static_cast<int>(RelicID::LuckyCoin)]       = {RelicID::LuckyCoin, "Lucky Coin", "+10% Shard Drop", RelicTier::Common, SDL_Color{255, 215, 0, 255}};
    d[static_cast<int>(RelicID::BloodFrenzy)]     = {RelicID::BloodFrenzy, "Blood Frenzy", "+25% DMG under 30% HP", RelicTier::Rare, SDL_Color{200, 30, 30, 255}};
    d[static_cast<int>(RelicID::EchoStrike)]      = {RelicID::EchoStrike, "Echo Strike", "20% Double Hit", RelicTier::Rare, SDL_Color{120, 200, 255, 255}};
    d[static_cast<int>(RelicID::PhaseCloak)]      = {RelicID::PhaseCloak, "Phase Cloak", "1s Invis after Switch", RelicTier::Rare, SDL_Color{100, 50, 200, 255}};
    d[static_cast<int>(RelicID::EntropyAnchor)]   = {RelicID::EntropyAnchor, "Entropy Anchor", "-40% Entropy (Boss)", RelicTier::Rare, SDL_Color{50, 180, 120, 255}};
    d[static_cast<int>(RelicID::ChainLightning)]  = {RelicID::ChainLightning, "Chain Lightning", "Kill: 15 DMG Bolt", RelicTier::Rare, SDL_Color{255, 255, 80, 255}};
    d[static_cast<int>(RelicID::SoulSiphon)]      = {RelicID::SoulSiphon, "Soul Siphon", "Kills Heal 3 HP", RelicTier::Rare, SDL_Color{80, 255, 120, 255}};
    d[static_cast<int>(RelicID::BerserkerCore)]   = {RelicID::BerserkerCore, "Berserker Core", "+50% DMG, -30% HP", RelicTier::Legendary, SDL_Color{255, 50, 50, 255}};
    d[static_cast<int>(RelicID::TimeDilator)]     = {RelicID::TimeDilator, "Time Dilator", "-30% Ability CD", RelicTier::Legendary, SDL_Color{80, 180, 255, 255}};
    d[static_cast<int>(RelicID::DimensionalEcho)] = {RelicID::DimensionalEcho, "Dimensional Echo", "Hit Both Dimensions", RelicTier::Legendary, SDL_Color{200, 100, 255, 255}};
    d[static_cast<int>(RelicID::PhoenixFeather)]  = {RelicID::PhoenixFeather, "Phoenix Feather", "1x Auto-Revive", RelicTier::Legendary, SDL_Color{255, 180, 50, 255}};
    d[static_cast<int>(RelicID::VoidHunger)]      = {RelicID::VoidHunger, "Void Hunger", "Kill: +1% DMG (Run)", RelicTier::Legendary, SDL_Color{80, 0, 120, 255}};
    d[static_cast<int>(RelicID::ChaosOrb)]        = {RelicID::ChaosOrb, "Chaos Orb", "Random Effect / 30s", RelicTier::Legendary, SDL_Color{255, 100, 200, 255}};
    // Cursed Relics
    d[static_cast<int>(RelicID::CursedBlade)]    = {RelicID::CursedBlade, "Cursed Blade", "+40% Melee, -20% Ranged", RelicTier::Rare, SDL_Color{180, 30, 60, 255}};
    d[static_cast<int>(RelicID::GlassHeart)]     = {RelicID::GlassHeart, "Glass Heart", "+50% Max HP, 1.6x DMG taken", RelicTier::Rare, SDL_Color{220, 180, 200, 255}};
    d[static_cast<int>(RelicID::TimeTax)]        = {RelicID::TimeTax, "Time Tax", "-50% Ability CD, costs 5 HP", RelicTier::Rare, SDL_Color{100, 180, 220, 255}};
    d[static_cast<int>(RelicID::EntropySponge)]  = {RelicID::EntropySponge, "Entropy Sponge", "No passive entropy, kills +5%", RelicTier::Legendary, SDL_Color{40, 180, 80, 255}};
    d[static_cast<int>(RelicID::VoidPact)]       = {RelicID::VoidPact, "Void Pact", "Kill heals 5 HP, max 60% HP", RelicTier::Legendary, SDL_Color{80, 0, 100, 255}};
    d[static_cast<int>(RelicID::ChaosRift)]      = {RelicID::ChaosRift, "Chaos Rift", "10th kill: buff, 5th hit: spike", RelicTier::Legendary, SDL_Color{200, 60, 200, 255}};
    // Dimension-Switch Relics
    d[static_cast<int>(RelicID::RiftConduit)]    = {RelicID::RiftConduit, "Rift Conduit", "Switch: +10% ATK Speed 3s (x3)", RelicTier::Common, SDL_Color{120, 180, 255, 255}};
    d[static_cast<int>(RelicID::DualityGem)]     = {RelicID::DualityGem, "Duality Gem", "Dim A: +25% DMG, Dim B: +25% Armor", RelicTier::Rare, SDL_Color{180, 100, 220, 255}};
    d[static_cast<int>(RelicID::DimResidue)]     = {RelicID::DimResidue, "Dim. Residue", "Switch: leave 15 DMG/s zone 2s", RelicTier::Rare, SDL_Color{200, 80, 150, 255}};
    d[static_cast<int>(RelicID::RiftMantle)]     = {RelicID::RiftMantle, "Rift Mantle", "-50% Switch CD, costs 5% Max HP", RelicTier::Legendary, SDL_Color{100, 220, 180, 255}};
    d[static_cast<int>(RelicID::StabilityMatrix)] = {RelicID::StabilityMatrix, "Stability Matrix", "+3% DMG/s in dim (max 30%)", RelicTier::Common, SDL_Color{200, 200, 100, 255}};
    d[static_cast<int>(RelicID::VoidResonance)]  = {RelicID::VoidResonance, "Void Resonance", "Cross-dim kill: 2x DMG", RelicTier::Legendary, SDL_Color{150, 50, 200, 255}};
}

const RelicData& RelicSystem::getRelicData(RelicID id) {
    initRelicData();
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < static_cast<int>(s_relicData.size())) return s_relicData[idx];
    static RelicData empty;
    return empty;
}

const std::vector<RelicData>& RelicSystem::getAllRelics() {
    initRelicData();
    return s_relicData;
}

std::vector<RelicID> RelicSystem::generateChoice(int difficulty, const std::vector<ActiveRelic>& owned) {
    initRelicData();
    // Build pool excluding already-owned relics
    std::vector<RelicID> pool;
    for (int i = 1; i < static_cast<int>(RelicID::COUNT); i++) {
        RelicID id = static_cast<RelicID>(i);
        bool alreadyOwned = false;
        for (auto& r : owned) {
            if (r.id == id) { alreadyOwned = true; break; }
        }
        if (alreadyOwned) continue;

        // Weight by tier: Common=3, Rare=2, Legendary=1
        auto& data = s_relicData[i];
        int weight = 1;
        switch (data.tier) {
            case RelicTier::Common: weight = 3; break;
            case RelicTier::Rare: weight = 2; break;
            case RelicTier::Legendary: weight = 1; break;
        }
        // Higher difficulty = more legendaries
        if (data.tier == RelicTier::Legendary && difficulty >= 3) weight++;
        if (data.tier == RelicTier::Common && difficulty >= 5) weight = std::max(1, weight - 1);

        for (int w = 0; w < weight; w++) pool.push_back(id);
    }

    // Pick 3 unique relics
    std::vector<RelicID> choices;
    for (int i = 0; i < 3 && !pool.empty(); i++) {
        int idx = std::rand() % pool.size();
        RelicID pick = pool[idx];
        // Remove all instances of this relic from pool
        pool.erase(std::remove(pool.begin(), pool.end(), pick), pool.end());
        choices.push_back(pick);
    }
    return choices;
}

RelicID RelicSystem::generateDrop(int difficulty, const std::vector<ActiveRelic>& owned) {
    auto choices = generateChoice(difficulty, owned);
    if (choices.empty()) return RelicID::None;
    return choices[std::rand() % choices.size()];
}

void RelicSystem::applyStatEffects(RelicComponent& relics, Player& player,
                                    HealthComponent& hp, CombatComponent& combat) {
    // Recalculate stats from scratch based on active relics
    float hpBonus = 0;
    float speedMult = 1.0f;

    for (auto& r : relics.relics) {
        switch (r.id) {
            case RelicID::IronHeart:
                hpBonus += 15.0f;
                break;
            case RelicID::SwiftBoots:
                speedMult += 0.10f;
                break;
            case RelicID::BerserkerCore:
                hpBonus -= hp.maxHP * 0.30f;
                break;
            case RelicID::GlassHeart:
                hpBonus += hp.maxHP * 0.50f;
                break;
            default: break;
        }
    }

    hp.maxHP += hpBonus;
    hp.currentHP = std::min(hp.currentHP, hp.maxHP);
    if (hp.currentHP <= 0 && hpBonus < 0) hp.currentHP = 1.0f;
    player.moveSpeed *= speedMult;
}

float RelicSystem::getDamageMultiplier(const RelicComponent& relics, float currentHPPercent, int currentDimension) {
    float mult = 1.0f;
    for (auto& r : relics.relics) {
        switch (r.id) {
            case RelicID::BloodFrenzy:
                if (currentHPPercent < 0.3f) mult += 0.25f;
                break;
            case RelicID::BerserkerCore:
                mult += 0.50f;
                break;
            case RelicID::VoidHunger:
                mult += std::min(relics.voidHungerBonus, MAX_VOID_HUNGER_BONUS);
                break;
            case RelicID::DualityGem:
                // Dimension A (1): +25% DMG (DualNature: always active)
                if (currentDimension == 1 || RelicSynergy::isDualNatureActive(relics))
                    mult += 0.25f;
                break;
            case RelicID::StabilityMatrix: {
                // +3% DMG per second (RiftMaster: +4%/s), max 30%/40%
                float rate = RelicSynergy::getStabilityDmgPerSec(relics);
                float maxBonus = RelicSynergy::isRiftMasterActive(relics) ? 0.40f : 0.30f;
                mult += std::min(relics.stabilityTimer * rate, maxBonus);
                break;
            }
            default: break;
        }
    }
    // ChaosRift buff: type 1 = +40% DMG for 15s after 10 kills
    if (relics.hasRelic(RelicID::ChaosRift) && relics.chaosRiftBuffTimer > 0 && relics.chaosRiftBuffType == 1) {
        mult += 0.40f;
    }
    // PhaseHunter synergy: 2x damage on first attack after dimension switch
    mult *= RelicSynergy::getPhaseHunterDamageMult(relics);
    return std::min(mult, MAX_DMG_MULT);
}

float RelicSystem::getAttackSpeedMultiplier(const RelicComponent& relics) {
    float mult = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::QuickHands) mult += 0.15f;
    }
    // RiftConduit: +10% attack speed per stack (max 3 stacks)
    if (relics.hasRelic(RelicID::RiftConduit) && relics.riftConduitTimer > 0) {
        mult += relics.riftConduitStacks * 0.10f;
    }
    return std::min(mult, MAX_ATK_SPEED_MULT);
}

float RelicSystem::getAbilityCooldownMultiplier(const RelicComponent& relics) {
    float mult = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::TimeDilator) mult -= 0.30f;
    }
    return std::max(0.1f, mult);
}

bool RelicSystem::rollEchoStrike(const RelicComponent& relics) {
    for (auto& r : relics.relics) {
        if (r.id == RelicID::EchoStrike) {
            float chance = RelicSynergy::getEchoStrikeChance(relics);
            return (std::rand() % 100) < static_cast<int>(chance * 100);
        }
    }
    return false;
}

float RelicSystem::getThornDamage(const RelicComponent& relics) {
    float dmg = 0;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::ThornMail) dmg += 8.0f;
    }
    return dmg;
}

float RelicSystem::getKillHeal(const RelicComponent& relics) {
    float heal = 0;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::SoulSiphon) heal += 3.0f;
    }
    return heal;
}

float RelicSystem::getChainLightningDamage(const RelicComponent& relics) {
    for (auto& r : relics.relics) {
        if (r.id == RelicID::ChainLightning) return 15.0f;
    }
    return 0;
}

bool RelicSystem::hasDimensionalEcho(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::DimensionalEcho);
}

bool RelicSystem::hasPhoenixFeather(const RelicComponent& relics) {
    for (auto& r : relics.relics) {
        if (r.id == RelicID::PhoenixFeather && !r.consumed) return true;
    }
    return false;
}

void RelicSystem::consumePhoenixFeather(RelicComponent& relics) {
    for (auto& r : relics.relics) {
        if (r.id == RelicID::PhoenixFeather && !r.consumed) {
            r.consumed = true;
            return;
        }
    }
}

float RelicSystem::getShardDropMultiplier(const RelicComponent& relics) {
    float mult = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::LuckyCoin) {
            // CursedFortune synergy: +30% instead of +10%
            float synergyBonus = RelicSynergy::getShardDropBonus(relics);
            mult += (synergyBonus > 0) ? synergyBonus : 0.10f;
        }
    }
    return mult;
}

float RelicSystem::getShardMagnetMultiplier(const RelicComponent& relics) {
    float mult = 1.0f;
    for (auto& r : relics.relics) {
        if (r.id == RelicID::ShardMagnet) mult += 0.50f;
    }
    return mult;
}

float RelicSystem::getEntropyMultiplier(const RelicComponent& relics, bool isBossFight) {
    float mult = 1.0f;
    if (isBossFight) {
        for (auto& r : relics.relics) {
            if (r.id == RelicID::EntropyAnchor) mult -= 0.40f;
        }
    }
    return std::max(0.1f, mult);
}

void RelicSystem::updateTimedEffects(RelicComponent& relics, float dt) {
    // ChaosRift: tick down buff timer
    if (relics.chaosRiftBuffTimer > 0) {
        relics.chaosRiftBuffTimer -= dt;
    }
    // PhaseHunter: expire buff after timeout
    if (relics.phaseHunterBuffActive && relics.phaseHunterBuffTimer > 0) {
        relics.phaseHunterBuffTimer -= dt;
        if (relics.phaseHunterBuffTimer <= 0) {
            relics.phaseHunterBuffActive = false;
        }
    }
    // ChaosOrb: random relic effect every 30s
    if (relics.hasRelic(RelicID::ChaosOrb)) {
        relics.chaosOrbTimer -= dt;
        if (relics.chaosOrbTimer <= 0) {
            relics.chaosOrbTimer = 30.0f;
            // Pick a random common/rare effect
            int effects[] = {0, 1, 2, 3, 4, 5}; // IronHeart through LuckyCoin effects
            relics.chaosOrbCurrentEffect = effects[std::rand() % 6];
        }
    }
}

void RelicSystem::onEnemyKill(RelicComponent& relics) {
    for (auto& r : relics.relics) {
        if (r.id == RelicID::VoidHunger) {
            relics.voidHungerBonus += RelicSynergy::getVoidHungerBonusPerKill(relics);
            relics.voidHungerBonus = std::min(relics.voidHungerBonus, MAX_VOID_HUNGER_BONUS);
        }
    }
    // ChaosRift: track kills for buff trigger
    if (relics.hasRelic(RelicID::ChaosRift)) {
        relics.chaosRiftKillCount++;
        if (relics.chaosRiftKillCount >= 10) {
            relics.chaosRiftKillCount = 0;
            relics.chaosRiftBuffTimer = 15.0f;
            relics.chaosRiftBuffType = std::rand() % 4; // 0=speed, 1=dmg, 2=armor, 3=regen
        }
    }
}

void RelicSystem::onDimensionSwitch(RelicComponent& relics, HealthComponent* hp) {
    if (relics.hasRelic(RelicID::PhaseCloak)) {
        relics.phaseCloakTimer = 1.0f;
    }
    // PhaseHunter synergy: activate 2x damage buff after dimension switch (3s window)
    if (RelicSynergy::isActive(relics, SynergyID::PhaseHunter)) {
        relics.phaseHunterBuffActive = true;
        relics.phaseHunterBuffTimer = 3.0f;
    }
    // RiftConduit: stack attack speed buff on switch
    if (relics.hasRelic(RelicID::RiftConduit)) {
        relics.riftConduitStacks = std::min(relics.riftConduitStacks + 1, 3);
        relics.riftConduitTimer = 3.0f;
    }
    // StabilityMatrix: reset timer on switch (RiftMaster: stacks persist)
    if (relics.hasRelic(RelicID::StabilityMatrix) && !RelicSynergy::isRiftMasterActive(relics)) {
        relics.stabilityTimer = 0;
    }
    // RiftMantle: costs 5% max HP per switch (DualNature: no cost)
    if (relics.hasRelic(RelicID::RiftMantle) && hp && !RelicSynergy::isDualNatureActive(relics)) {
        float cost = hp->maxHP * 0.05f;
        hp->takeDamage(cost);
        if (hp->currentHP < 1.0f) hp->currentHP = 1.0f; // Don't kill player
    }
}

float RelicSystem::getSwitchCooldownMult(const RelicComponent& relics) {
    float baseCooldown = 0.5f; // DimensionManager default
    float mult = relics.hasRelic(RelicID::RiftMantle) ? 0.5f : 1.0f;
    // Enforce hard minimum switch cooldown
    float effective = baseCooldown * mult;
    if (effective < MIN_SWITCH_COOLDOWN) {
        mult = MIN_SWITCH_COOLDOWN / baseCooldown;
    }
    return mult;
}

bool RelicSystem::isCursed(RelicID id) {
    return id == RelicID::CursedBlade || id == RelicID::GlassHeart ||
           id == RelicID::TimeTax || id == RelicID::EntropySponge ||
           id == RelicID::VoidPact || id == RelicID::ChaosRift;
}

float RelicSystem::getCursedMeleeMult(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::CursedBlade) ? 1.4f : 1.0f;
}

float RelicSystem::getCursedRangedMult(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::CursedBlade) ? 0.8f : 1.0f;
}

float RelicSystem::getDamageTakenMult(const RelicComponent& relics, int currentDimension) {
    float mult = 1.0f;
    if (relics.hasRelic(RelicID::GlassHeart)) {
        // FortifiedSoul synergy: reduce GlassHeart penalty
        float synergyMult = RelicSynergy::getGlassHeartDamageMult(relics);
        mult = (synergyMult > 0) ? synergyMult : 1.6f;
    }
    // DualityGem: Dimension B (2): +25% Armor (DualNature: always active)
    if (relics.hasRelic(RelicID::DualityGem) &&
        (currentDimension == 2 || RelicSynergy::isDualNatureActive(relics))) {
        mult *= 0.75f;
    }
    // ChaosRift buff: type 2 = -30% damage taken for 15s
    if (relics.hasRelic(RelicID::ChaosRift) && relics.chaosRiftBuffTimer > 0 && relics.chaosRiftBuffType == 2) {
        mult *= 0.70f;
    }
    return mult;
}

float RelicSystem::getAbilityCDMultCursed(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::TimeTax) ? 0.5f : 1.0f;
}

float RelicSystem::getAbilityHPCost(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::TimeTax) ? 5.0f : 0.0f;
}

bool RelicSystem::hasNoPassiveEntropy(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::EntropySponge);
}

float RelicSystem::getKillEntropyGain(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::EntropySponge) ? 5.0f : 0.0f;
}

float RelicSystem::getVoidPactHeal(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::VoidPact) ? 5.0f : 0.0f;
}

float RelicSystem::getVoidPactMaxHPPercent(const RelicComponent& relics) {
    return relics.hasRelic(RelicID::VoidPact) ? 0.6f : 1.0f;
}

float RelicSystem::getVoidResonanceICD() { return VOID_RESONANCE_ICD; }
float RelicSystem::getDimResidueSpawnICD() { return DIM_RESIDUE_SPAWN_ICD; }
int   RelicSystem::getMaxResidueZones() { return MAX_RESIDUE_ZONES; }
