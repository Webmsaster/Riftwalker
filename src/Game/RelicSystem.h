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

    // Generate a relic choice (3 relics from pool, weighted by tier)
    static std::vector<RelicID> generateChoice(int difficulty, const std::vector<ActiveRelic>& owned);

    // Generate a single random relic drop
    static RelicID generateDrop(int difficulty, const std::vector<ActiveRelic>& owned);

    // Apply stat-based relic effects to player (called once when relic picked up)
    static void applyStatEffects(RelicComponent& relics, Player& player,
                                  HealthComponent& hp, CombatComponent& combat);

    // Get damage multiplier from relics (called per attack)
    static float getDamageMultiplier(const RelicComponent& relics, float currentHPPercent);

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

    // On dimension switch: trigger PhaseCloak
    static void onDimensionSwitch(RelicComponent& relics);
};
