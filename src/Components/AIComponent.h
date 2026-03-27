#pragma once
#include "ECS/Component.h"
#include "Core/Camera.h"

enum class AIState {
    Idle,
    Patrol,
    Chase,
    Attack,
    Flee,
    Stunned,
    Juggled,
    Dead
};

enum class EnemyType {
    Walker,     // Basic ground enemy, patrols
    Flyer,      // Flying enemy, swoops down
    Turret,     // Stationary, shoots projectiles
    Charger,    // Fast ground enemy, charges at player
    Phaser,     // Dimension-switching enemy
    Exploder,   // Runs at player, explodes on contact or death
    Shielder,   // Has shield that blocks frontal attacks
    Crawler,    // Wall/ceiling clinger, drops on player
    Summoner,   // Spawns minions from distance
    Sniper,     // Long-range telegraphed shots, keeps distance
    Teleporter, // Blinks behind player for surprise attacks
    Reflector,  // Frontal shield that reflects projectiles, cycles on/off
    Leech,      // Slow drainer, steals HP from player to heal self
    Swarmer,    // Tiny, fast, spawns in groups of 3 — overwhelms via numbers
    GravityWell,// Floating enemy, pulls player toward it with gravity field
    Mimic,      // Disguised as crate until player approaches, then ambushes
    Boss,       // Level boss with multiple phases
    COUNT
};

enum class EnemyElement {
    None = 0,   // No element
    Fire,       // Extra burn damage, orange tint
    Ice,        // Slows player on hit, blue tint
    Electric    // Chain damage to nearby enemies on death, yellow tint
};

enum class EliteModifier {
    None = 0,
    Berserker,   // Red: +50% DMG, +30% Speed
    Shielded,    // Blue: Regenerating shield (30 HP every 5s)
    Teleporter,  // Purple: Teleports behind player every 3s
    Splitter,    // Green: Splits into 2 smaller copies on death
    Vampiric,    // DarkRed: Heals 20% of damage dealt
    Explosive,   // Orange: Explodes on death (40 DMG, 80px radius)
    FireAura,    // Flame aura: deals burn DoT to player in 100px proximity
    HealAura,    // Green aura: heals nearby enemies 3 HP/s within 120px
    ShieldAura   // Cyan aura: nearby enemies within 100px take 30% less damage
};

struct AIComponent : public Component {
    AIState state = AIState::Patrol;
    EnemyType enemyType = EnemyType::Walker;
    EnemyElement element = EnemyElement::None;
    bool isMiniBoss = false;

    // Elite system
    bool isElite = false;
    EliteModifier eliteMod = EliteModifier::None;
    float eliteGlowTimer = 0;
    float eliteShieldHP = 0;       // Shielded: current shield HP
    float eliteShieldRegenTimer = 0; // Shielded: regen cooldown
    float eliteTeleportTimer = 0;  // Teleporter: teleport cooldown
    float eliteVampHealAccum = 0;  // Vampiric: accumulated heal
    float healAuraTimer = 0;       // HealAura: cooldown to avoid per-frame O(n) scan

    // Detection
    float detectRange = 200.0f;
    float attackRange = 50.0f;
    float loseRange = 300.0f;

    // Patrol
    Vec2 patrolStart;
    Vec2 patrolEnd;
    bool patrolForward = true;
    float patrolSpeed = 80.0f;

    // Chase
    float chaseSpeed = 120.0f;

    // Attack
    float attackCooldown = 1.0f;
    float attackTimer = 0.0f;
    float attackWindup = 0.35f;  // wind-up delay before first melee attack (visual telegraph)

    // Stun
    float stunDuration = 0.5f;
    float stunTimer = 0.0f;

    // Flyer specific
    float flyHeight = 100.0f;
    float swoopSpeed = 250.0f;

    // Charger specific
    float chargeSpeed = 350.0f;
    float chargeTime = 0.8f;
    float chargeWindup = 0.5f;

    // Phaser specific
    float phaseCooldown = 3.0f;
    float phaseTimer = 0.0f;

    // Exploder specific
    float explodeRadius = 80.0f;
    float explodeDamage = 35.0f;
    bool exploded = false;

    // Shielder specific
    bool shieldUp = true;
    float shieldAngle = 0; // direction shield faces

    // Crawler specific
    bool onCeiling = false;
    bool dropping = false;
    float dropSpeed = 400.0f;
    float clingTimer = 0;

    // Summoner specific
    float summonCooldown = 6.0f;
    float summonTimer = 0;
    int maxMinions = 3;
    int activeMinions = 0;

    // Sniper specific
    float sniperRange = 400.0f;
    float telegraphTimer = 0;
    float telegraphDuration = 0.8f;
    bool isTelegraphing = false;
    float retreatSpeed = 120.0f;
    float preferredRange = 300.0f;

    // Boss specific
    int bossType = 0;          // 0=Rift Guardian, 1=Void Wyrm
    int bossPhase = 1;         // 1=normal, 2=enraged, 3=desperate
    float bossAttackTimer = 0; // timer for boss attack patterns
    int bossAttackPattern = 0; // current attack pattern index
    float bossLeapTimer = 0;   // ground slam cooldown
    bool bossEnraged = false;
    float bossShieldTimer = 0;      // shield burst cooldown
    float bossShieldActiveTimer = 0; // remaining shield duration
    float bossTeleportTimer = 0;    // teleport cooldown

    // Boss telegraph system
    float bossTelegraphTimer = 0;      // countdown until telegraphed attack fires
    int bossTelegraphAttack = -1;      // which attack pattern is being telegraphed (-1 = none)
    Vec2 bossTelegraphDir;             // stored direction for telegraphed attack

    // Void Wyrm specific
    float wyrmOrbitAngle = 0;       // current orbit angle around player
    float wyrmOrbitRadius = 150.0f; // orbit distance
    float wyrmDiveTimer = 0;        // divebomb cooldown
    bool wyrmDiving = false;        // currently diving
    Vec2 wyrmDiveTarget;            // dive target position
    float wyrmPoisonTimer = 0;      // poison cloud cooldown
    float wyrmBarrageTimer = 0;     // barrage cooldown
    int wyrmBarrageCount = 0;       // shots remaining in barrage

    // Dimensional Architect boss specific
    float archSwapTimer = 0;         // tile-swap cooldown
    float archRiftTimer = 0;         // rift-zone cooldown
    float archConstructTimer = 0;    // construct beam cooldown
    float archCollapseTimer = 0;     // arena collapse cooldown
    bool archConstructing = false;   // beam active
    float archBeamAngle = 0;         // construct beam direction
    float archHoverY = 0;            // hover offset (sine wave)
    int archSwapSize = 3;            // tile swap area size (grows per phase)

    // Temporal Weaver boss specific
    float twPhaseTimer = 0;          // Attack phase timer
    float twSlowZoneTimer = 0;       // Time Slow Zone cooldown
    float twSweepTimer = 0;          // Clock Hand Sweep cooldown
    float twRewindTimer = 0;         // Time Rewind cooldown
    float twStopTimer = 0;           // Time Stop cooldown
    float twStormTimer = 0;          // Temporal Storm timer
    bool twCloneActive = false;      // Temporal Clone spawned
    int twCloneCount = 0;            // Number of active clones
    float twHoverY = 0;              // Hover offset (sine wave)

    // Void Sovereign boss specific
    float vsOrbTimer = 0;            // Void Orbs cooldown
    float vsSlamTimer = 0;           // Rift Slam cooldown
    float vsTeleportTimer = 0;       // Teleport cooldown
    float vsDimLockTimer = 0;        // Dimension Lock cooldown
    float vsDimLockActive = 0;       // Remaining dim-lock duration
    float vsStormTimer = 0;          // Void Storm cooldown
    float vsLaserTimer = 0;          // Reality Tear laser cooldown
    float vsLaserAngle = 0;          // Laser sweep angle
    bool vsLaserActive = false;      // Laser is firing
    float vsAutoSwitchTimer = 0;     // Phase 3: auto dim-switch timer
    bool vsForceDimSwitch = false;   // Phase 3: flag for PlayState to trigger dim-switch
    int vsCloneCount = 0;            // Shadow Clones active
    float vsCloneSpawnTimer = 0;     // Clone respawn cooldown
    float vsArenaShink = 0;          // Arena shrink amount (Phase 2)
    float vsStormActive = 0;         // Void Storm remaining duration
    Vec2 vsStormSafe1;               // Void Storm safe zone 1
    Vec2 vsStormSafe2;               // Void Storm safe zone 2
    float vsVoidKernPulse = 0;       // Visual: pulsing core

    // Entropy Incarnate boss specific (bossType 5)
    float eiPulseTimer = 0;          // Entropy Pulse AoE cooldown
    bool eiPulseFired = false;       // Flag: entropy pulse just fired (consumed by PlayState)
    float eiTendrilTimer = 0;        // Void Tendrils cooldown
    float eiTeleportTimer = 0;       // Teleport cooldown
    float eiStormActive = 0;         // Entropy Storm aura remaining duration
    float eiStormTimer = 0;          // Entropy Storm cooldown
    float eiDimLockTimer = 0;        // Forced Dimension Lock cooldown
    float eiDimLockActive = 0;       // Remaining dim-lock duration
    float eiMissileTimer = 0;        // Entropy Missiles cooldown
    float eiMinionTimer = 0;         // Minion spawn cooldown
    int eiMinionCount = 0;           // Active entropy minions
    float eiShatterTimer = 0;        // Phase 3: Dimension Shatter cooldown
    bool eiForceDimSwitch = false;   // Phase 3: flag for PlayState to trigger dim-switch
    float eiOverloadHealAccum = 0;   // Phase 3: Entropy Overload heal accumulator
    bool eiFinalStandTriggered = false; // Phase 3: Final Stand at 10% HP
    bool eiFinalStandEntropyApplied = false; // Final Stand entropy applied by PlayState
    float eiCorePulse = 0;           // Visual: pulsing core animation timer
    float eiHoverY = 0;              // Hover offset (sine wave)

    // Aggro alert: "!" indicator when enemy first notices player
    float aggroAlertTimer = 0;

    // Environmental hazard damage cooldown (per-enemy)
    float hazardDmgCooldown = 0;

    // Spawn animation: brief invulnerability + scale-up + glow when enemy appears
    float spawnTimer = 0;       // >0 = still spawning in (no AI, invulnerable)
    float spawnTimerInitial = 0; // initial value of spawnTimer (for computing progress)

    // Dimension behavior modifiers (computed by AISystem each frame for dim-0 enemies)
    float dimSpeedMod = 1.0f;   // Movement speed multiplier
    float dimDamageMod = 1.0f;  // Damage multiplier
    float dimDetectMod = 1.0f;  // Detection range multiplier

    // Juggle state
    float juggleTimer = 0;
    int juggleHitCount = 0;
    float juggleDamageBonus = 0.15f; // +15% per juggle hit

    // Adaptive behavior
    int timesHit = 0;
    bool isEnraged = false;
    int attackMissCount = 0;

    // Dodge/React
    float dodgeCooldown = 0;
    float dodgeTimer = 0;
    Vec2 dodgeDirection{0, 0};

    // Synergy awareness (computed per-frame)
    bool synergySummonerBuff = false;
    float summonerBuffSpeedMult = 1.0f;
    float summonerBuffDamageMult = 1.0f;
    Entity* synergyProtectTarget = nullptr;

    // Charger bounce
    float chargeMissTimer = 0;
    bool chargerBounced = false;

    // Walker lunge
    float walkerLungeTimer = 0;

    // Shielder bash
    float shielderBashCooldown = 0;

    // Teleporter specific
    float teleportCooldown = 3.0f;
    float teleportTimer = 0;
    float afterimageTimer = 0;
    Vec2 afterimagePos{0, 0};
    bool justTeleported = false;

    // Reflector specific
    bool reflectorShieldUp = true;
    float reflectorShieldTimer = 0;
    float reflectorShieldCooldown = 4.0f;
    float reflectorShieldDownTime = 1.0f;

    // Leech specific
    float leechDrainRate = 2.0f;
    float leechDrainTimer = 0;

    // Swarmer specific
    float swarmerFlockTimer = 0;      // regroup cooldown
    float swarmerZigzagPhase = 0;     // zigzag movement offset

    // GravityWell specific
    float gravPullRadius = 120.0f;    // pull range
    float gravPullForce = 200.0f;     // pull strength
    float gravPulseTimer = 0;         // visual pulse
    float gravHoverY = 0;             // hover offset

    // Mimic specific
    bool mimicRevealed = false;       // true once player is close enough
    float mimicRevealRange = 60.0f;   // range to trigger reveal
    float mimicLungeTimer = 0;        // cooldown for lunge attack

    // Element weapon debuffs (applied by player's weapon buffs)
    float burnTimer = 0;       // fire weapon DoT remaining
    float burnDmgTick = 0;     // burn damage tick timer
    float freezeTimer = 0;     // ice weapon slow remaining (50% speed reduction)

    Vec2 targetPosition;
    bool facingRight = true;

    void stun(float duration) {
        state = AIState::Stunned;
        stunTimer = duration > 0 ? duration : stunDuration;
    }

    void launch() {
        state = AIState::Juggled;
        juggleTimer = 3.0f; // max 3s airtime
        juggleHitCount = 0;
    }
};
