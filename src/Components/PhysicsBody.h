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

    bool useGravity = true;
    bool onGround = false;
    bool onWallLeft = false;
    bool onWallRight = false;
    bool onCeiling = false;
    bool wasOnGround = false;

    // Coyote time
    float coyoteTime = 0.1f;
    float coyoteTimer = 0.0f;
    bool canCoyoteJump() const { return coyoteTimer > 0.0f; }

    void applyForce(Vec2 force) {
        acceleration += force;
    }

    void applyImpulse(Vec2 impulse) {
        velocity += impulse;
    }
};
