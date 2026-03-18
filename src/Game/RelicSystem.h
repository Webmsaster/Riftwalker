#pragma once
#include "Components/RelicComponent.h"
#include <vector>
#include <functional>

class EntityManager;
class Player;
struct HealthComponent;
struct CombatComponent;

class RelicSystem {
public:
    // Get static relic definitions
    static const RelicData& getRelicData(RelicID id);
    static const std::vector<RelicData>& getAllRelics();

    // Generate a relic choice (count relics from pool, weighted by tier)
    static std::vector<RelicID> generateChoice(int difficulty, const std::vector<ActiveRelic>& owned, int count = 3);

    // Generate a cursed-only relic choice (for secret rooms / special encounters)
    static std::vector<RelicID> generateCursedChoice(int difficulty, const std::vector<ActiveRelic>& owned);

    // Generate a single random relic drop
    static RelicID generateDrop(int difficulty, const std::vector<ActiveRelic>& owned);

    // Apply stat-based relic effects to player (called once when relic picked up)
    static void applyStatEffects(RelicComponent& relics, Player& player,
                                  HealthComponent& hp, CombatComponent& combat);

    // Get damage multiplier from relics (called per attack)
    static float getDamageMultiplier(const RelicComponent& relics, float currentHPPercent, int currentDimension = 0);

    // Get attack speed multiplier
    static float getAttackSpeedMultiplier(const RelicComponent& relics);

    // Get ability cooldown multiplier
    static float getAbilityCooldownMultiplier(const RelicComponent& relics);

    // Check for echo strike (double hit chance)
    static bool rollEchoStrike(const RelicComponent& relics);

    // Get thorn damage (reflected on hit)
    static float getThornDamage(const RelicComponent& relics);

    // Get lifesteal from kills (SoulSiphon)
    static float getKillHeal(const RelicComponent& relics);

    // Get chain lightning damage on kill
    static float getChainLightningDamage(const RelicComponent& relics);

    // Check for dimensional echo (attacks hit other dimension)
    static bool hasDimensionalEcho(const RelicComponent& relics);

    // Check/consume phoenix feather
    static bool hasPhoenixFeather(const RelicComponent& relics);
    static void consumePhoenixFeather(RelicComponent& relics);

    // Get shard drop multiplier
    static float getShardDropMultiplier(const RelicComponent& relics);

    // Get shard magnet radius multiplier
    static float getShardMagnetMultiplier(const RelicComponent& relics);

    // Get entropy multiplier (for boss fights)
    static float getEntropyMultiplier(const RelicComponent& relics, bool isBossFight);

    // Update timed effects (ChaosOrb, PhaseCloak, VoidHunger)
    static void updateTimedEffects(RelicComponent& relics, float dt);

    // On kill: update VoidHunger bonus
    static void onEnemyKill(RelicComponent& relics);

    // On dimension switch: trigger PhaseCloak, RiftConduit, etc.
    static void onDimensionSwitch(RelicComponent& relics, HealthComponent* hp = nullptr);

    // Get dimension switch cooldown multiplier (RiftMantle)
    static float getSwitchCooldownMult(const RelicComponent& relics);

    // Balance safety rails (internal cooldowns / caps)
    static float getVoidResonanceICD();    // Seconds between 2x procs
    static float getDimResidueSpawnICD();  // Seconds between zone spawns
    static int   getMaxResidueZones();     // Max concurrent zones

    // Cursed relic queries
    static bool isCursed(RelicID id);
    static float getCursedMeleeMult(const RelicComponent& relics);
    static float getCursedRangedMult(const RelicComponent& relics);
    static float getDamageTakenMult(const RelicComponent& relics, int currentDimension = 0);
    static float getAbilityCDMultCursed(const RelicComponent& relics);
    static float getAbilityHPCost(const RelicComponent& relics);
    static bool hasNoPassiveEntropy(const RelicComponent& relics);
    static float getKillEntropyGain(const RelicComponent& relics);
    static float getVoidPactHeal(const RelicComponent& relics);
    static float getVoidPactMaxHPPercent(const RelicComponent& relics);

    // New cursed relic queries
    static float getBloodPactKillHPCost(const RelicComponent& relics);     // BloodPact: HP drained per kill
    static float getEntropySiphonKillReduction(const RelicComponent& relics); // EntropySiphon: entropy reduced per kill
    static float getEntropySiphonGainMult(const RelicComponent& relics);   // EntropySiphon: 1.5x entropy gain mult
    static bool hasVampiricEdge(const RelicComponent& relics);             // VampiricEdge: blocks natural healing
    static float getVampiricEdgeKillHeal(const RelicComponent& relics);    // VampiricEdge: HP healed per kill
    static float getBerserkersCurseDamageMult(const RelicComponent& relics, float hpPercent); // stacks per missing 10% HP
    static bool hasBerserkersCurse(const RelicComponent& relics);          // BerserkersCurse: no shields
    static float getTimeDistortionSpeedMult(const RelicComponent& relics); // +30% move+attack speed
    static float getTimeDistortionEntropyDecayMult(const RelicComponent& relics); // 50% slower entropy decay
    static float getChaosCoreStatMult(const RelicComponent& relics);       // ChaosCore: +25% all stats
    static float getSoulLeechShardMult(const RelicComponent& relics);      // SoulLeech: 2x shard drops
    static float getSoulLeechLevelHPCost(const RelicComponent& relics);    // SoulLeech: -5 HP per transition
};
