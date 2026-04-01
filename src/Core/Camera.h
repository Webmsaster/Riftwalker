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

    void follow(const Vec2& target, float dt, const Vec2& velocity = {0, 0}, bool grounded = false);
    void shake(float intensity, float duration);
    void flash(float duration, Uint8 r = 255, Uint8 g = 255, Uint8 b = 255);
    void update(float dt);

    float lookAheadStrength = 60.0f;
    float verticalDeadZone = 40.0f;  // pixels above/below center before camera follows vertically

    Vec2 worldToScreen(const Vec2& worldPos) const;
    SDL_Rect worldToScreen(SDL_FRect worldRect) const;
    Vec2 getPosition() const { return m_position; }
    void setPosition(const Vec2& pos) { m_position = pos; }
    void setBounds(float minX, float minY, float maxX, float maxY);

    float zoom = 5.0f;  // 5.0x zoom: doubled for 2560x1440 logical res (was 2.5 at 1280x720)
    float zoomTarget = 5.0f; // Target zoom for smooth transitions
    float zoomSpeed = 2.0f;  // Lerp speed (units/sec)

    int getViewWidth() const { return m_screenW; }
    int getViewHeight() const { return m_screenH; }

    // Frustum test: returns true if a world-space rect is potentially visible (with margin)
    bool isInView(float wx, float wy, float ww, float wh) const {
        Vec2 scr = worldToScreen(Vec2(wx, wy));
        float margin = 128.0f; // extra margin for particles/auras extending beyond hitbox (scaled for 2K)
        return scr.x + ww * zoom + margin > 0 && scr.x - margin < m_screenW
            && scr.y + wh * zoom + margin > 0 && scr.y - margin < m_screenH;
    }

    // Screen shake intensity multiplier (0.0 = off, 1.0 = full)
    float shakeMultiplier = 1.0f;

    // Screen flash (white flash on boss death, etc.)
    float getFlashAlpha() const;
    SDL_Color getFlashColor() const { return m_flashColor; }

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

    // Screen flash
    float m_flashTimer = 0;
    float m_flashDuration = 0;
    SDL_Color m_flashColor = {255, 255, 255, 255};

    // Bounds
    bool m_hasBounds = false;
    float m_minX, m_minY, m_maxX, m_maxY;
};
