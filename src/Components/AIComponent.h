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
    Boss        // Level boss with multiple phases
};

struct AIComponent : public Component {
    AIState state = AIState::Patrol;
    EnemyType enemyType = EnemyType::Walker;

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
    int bossPhase = 1;         // 1=normal, 2=enraged, 3=desperate
    float bossAttackTimer = 0; // timer for boss attack patterns
    int bossAttackPattern = 0; // current attack pattern index
    float bossLeapTimer = 0;   // ground slam cooldown
    bool bossEnraged = false;
    float bossShieldTimer = 0;      // shield burst cooldown
    float bossShieldActiveTimer = 0; // remaining shield duration
    float bossTeleportTimer = 0;    // teleport cooldown

    Vec2 targetPosition;
    bool facingRight = true;

    void stun(float duration) {
        state = AIState::Stunned;
        stunTimer = duration > 0 ? duration : stunDuration;
    }
};
