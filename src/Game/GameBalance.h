#pragma once
// GameBalance.h — Centralized reference for all gameplay balance constants.
// These values document the tuned state after playtest sessions.
// Actual values are used directly in Player.cpp, Enemy.cpp, ItemDrop.cpp etc.
// This header provides named constants for shared/cross-system references.

namespace Balance {

// === Player ===
namespace Player {
    constexpr float BaseHP           = 120.0f;
    constexpr float InvincibilityTime = 1.2f;
    constexpr float MoveSpeed        = 250.0f;
    constexpr float JumpForce        = -420.0f;
    constexpr float DashSpeed        = 500.0f;
    constexpr float DashDuration     = 0.2f;
    constexpr float DashCooldown     = 0.5f;
    constexpr float WallSlideSpeed   = 60.0f;
    constexpr float WallJumpForceX   = 300.0f;
    constexpr float WallJumpForceY   = -380.0f;
    constexpr float Gravity          = 980.0f;

    // Attack base damage (before weapon modifiers)
    constexpr float MeleeDamage      = 25.0f;
    constexpr float RangedDamage     = 15.0f;
    constexpr float DashDamage       = 30.0f;
    constexpr float ChargedDamage    = 50.0f;

    // Buff multipliers
    constexpr float SpeedBoostMult   = 1.5f;
    constexpr float DamageBoostMult  = 1.5f;
}

// === Enemies ===
namespace Enemy {
    // HP values per enemy type
    constexpr float WalkerHP     = 40.0f;
    constexpr float FlyerHP      = 25.0f;
    constexpr float TurretHP     = 40.0f;
    constexpr float ChargerHP    = 45.0f;
    constexpr float PhaserHP     = 30.0f;
    constexpr float ExploderHP   = 20.0f;
    constexpr float ShielderHP   = 65.0f;
    constexpr float CrawlerHP    = 20.0f;
    constexpr float SummonerHP   = 60.0f;
    constexpr float SniperHP     = 35.0f;
    constexpr float TeleporterHP = 35.0f;
    constexpr float ReflectorHP  = 55.0f;
    constexpr float LeechHP      = 80.0f;
    constexpr float SwarmerHP    = 15.0f;
    constexpr float GravityWellHP = 50.0f;
    constexpr float MimicHP      = 55.0f;
    constexpr float MinionHP     = 10.0f;

    // Damage values
    constexpr float WalkerDmg    = 10.0f;
    constexpr float FlyerDmg     = 10.0f;
    constexpr float ChargerDmg   = 18.0f;
    constexpr float ExploderDmg  = 30.0f;
    constexpr float SniperDmg    = 16.0f;
    constexpr float MimicDmg     = 18.0f;

    // MiniBoss multipliers
    constexpr float MiniBossHPMult    = 2.5f;
    constexpr float MiniBossArmorAdd  = 0.2f;
    constexpr float MiniBossArmorCap  = 0.6f;
    constexpr float MiniBossDmgMult   = 1.3f;
    constexpr float MiniBossDetectMult = 1.3f;
    constexpr float MiniBossSpeedMult = 1.15f;

    // Elite multipliers
    constexpr float EliteHPMult       = 1.3f;
    constexpr float EliteDmgMult      = 1.15f;
    constexpr float BerserkerDmgMult  = 1.5f;
    constexpr float BerserkerSpeedMult = 1.3f;
    constexpr float EliteShieldHP     = 30.0f;
}

// === Bosses ===
namespace Boss {
    // Base HP formulas: baseHP + difficulty * scaling
    constexpr float VoidWyrmBaseHP    = 150.0f;
    constexpr float VoidWyrmHPPerDiff = 40.0f;
    constexpr float RiftGuardBaseHP   = 180.0f;
    constexpr float RiftGuardHPPerDiff = 50.0f;
}

// === Item Drops ===
namespace Drops {
    constexpr float HealthOrbHeal    = 30.0f;
    constexpr int   BaseShardValue   = 5;
    constexpr int   ShardPerDifficulty = 3;

    // Drop rate percentages (out of 100)
    constexpr int HealthDropRate     = 25;
    constexpr int ShardDropRate      = 30;
    constexpr int ShieldDropRate     = 10;
    constexpr int SpeedDropRate      = 8;
    constexpr int DamageDropRate     = 7;

    // Buff durations (seconds)
    constexpr float ShieldDuration   = 8.0f;
    constexpr float SpeedDuration    = 6.0f;
    constexpr float DamageDuration   = 8.0f;
}

// === Suit Entropy ===
namespace Entropy {
    constexpr float MaxEntropy       = 100.0f;
    constexpr float PassiveDecay     = 0.15f;
    constexpr float PassiveGain      = 0.2f;
    constexpr float SwitchCost       = 0.5f;
    constexpr float DamageCostMult   = 0.1f;
    constexpr float RepairReduction  = 30.0f;
    constexpr float ForceSwitchInterval = 8.0f;
}

} // namespace Balance
