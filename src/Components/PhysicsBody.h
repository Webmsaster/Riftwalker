#pragma once
#include "ECS/Component.h"
#include "Core/Camera.h"

struct PhysicsBody : public Component {
    Vec2 velocity{0, 0};
    Vec2 acceleration{0, 0};
    float gravity = 980.0f;    // pixels/s^2
    float maxFallSpeed = 600.0f;
    float friction = 0.0f;     // ground friction
    float airResistance = 0.0f;
    float bounciness = 0.0f;

    // Apex hang-time: reduced gravity near jump peak
    float apexThreshold = 0.0f;       // velocity.y range considered "near apex" (0 = disabled)
    float apexGravityMultiplier = 1.0f; // gravity multiplier when near apex

    bool useGravity = true;
    bool onGround = false;
    bool onWallLeft = false;
    bool onWallRight = false;
    bool onCeiling = false;
    bool wasOnGround = false;
    bool onIce = false;

    // Landing impact: captures fall speed before collision zeros it
    float landingImpactSpeed = 0.0f;

    // Wall impact: captures horizontal speed when hitting a wall (for bounce/particles)
    float wallImpactSpeed = 0.0f;

    // Coyote time
    float coyoteTime = 0.1f;
    float coyoteTimer = 0.0f;
    bool canCoyoteJump() const { return coyoteTimer > 0.0f; }

    void applyForce(const Vec2& force) {
        acceleration += force;
    }

    void applyImpulse(const Vec2& impulse) {
        velocity += impulse;
    }
};
