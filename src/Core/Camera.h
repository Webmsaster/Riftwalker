#pragma once
#include <SDL2/SDL.h>

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    float length() const;
    Vec2 normalized() const;
};

class Camera {
public:
    Camera(int screenW, int screenH);

    void follow(Vec2 target, float dt, Vec2 velocity = {0, 0});
    void shake(float intensity, float duration);
    void update(float dt);

    float lookAheadStrength = 60.0f;

    Vec2 worldToScreen(Vec2 worldPos) const;
    SDL_Rect worldToScreen(SDL_FRect worldRect) const;
    Vec2 getPosition() const { return m_position; }
    void setPosition(Vec2 pos) { m_position = pos; }
    void setBounds(float minX, float minY, float maxX, float maxY);

    float zoom = 1.0f;

    // Screen shake intensity multiplier (0.0 = off, 1.0 = full)
    float shakeMultiplier = 1.0f;

private:
    Vec2 m_position;
    Vec2 m_target;
    int m_screenW, m_screenH;
    float m_smoothSpeed = 5.0f;

    // Screen shake
    float m_shakeIntensity = 0;
    float m_shakeDuration = 0;
    float m_shakeTimer = 0;
    Vec2 m_shakeOffset;

    // Look-ahead
    Vec2 m_lookAhead;
    Vec2 m_lookAheadSmooth;

    // Bounds
    bool m_hasBounds = false;
    float m_minX, m_minY, m_maxX, m_maxY;
};
