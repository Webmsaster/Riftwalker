#include "Camera.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

float Vec2::length() const {
    return std::sqrt(x * x + y * y);
}

Vec2 Vec2::normalized() const {
    float len = length();
    if (len < 0.0001f) return {0, 0};
    return {x / len, y / len};
}

Camera::Camera(int screenW, int screenH)
    : m_screenW(screenW), m_screenH(screenH) {}

void Camera::follow(Vec2 target, float dt, Vec2 velocity) {
    // Calculate look-ahead offset based on movement direction
    Vec2 targetLookAhead = {0, 0};
    float speed = velocity.length();
    if (speed > 30.0f) {
        Vec2 dir = velocity.normalized();
        targetLookAhead = dir * lookAheadStrength;
        // Reduce vertical look-ahead (less useful in platformer)
        targetLookAhead.y *= 0.3f;
    }

    // Smoothly interpolate look-ahead
    Vec2 lookDiff = targetLookAhead - m_lookAheadSmooth;
    m_lookAheadSmooth += lookDiff * (3.0f * dt);

    m_target = target + m_lookAheadSmooth;

    // Smooth camera follow
    Vec2 diff = m_target - m_position;
    m_position += diff * (m_smoothSpeed * dt);
}

void Camera::shake(float intensity, float duration) {
    m_shakeIntensity = intensity * shakeMultiplier;
    m_shakeDuration = duration;
    m_shakeTimer = 0;
}

void Camera::update(float dt) {
    m_shakeOffset = {0, 0};
    if (m_shakeTimer < m_shakeDuration) {
        m_shakeTimer += dt;
        float progress = m_shakeTimer / m_shakeDuration;
        float currentIntensity = m_shakeIntensity * (1.0f - progress);
        m_shakeOffset.x = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
        m_shakeOffset.y = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
    }

    // Clamp to bounds
    if (m_hasBounds) {
        float halfW = m_screenW / (2.0f * zoom);
        float halfH = m_screenH / (2.0f * zoom);
        m_position.x = std::clamp(m_position.x, m_minX + halfW, m_maxX - halfW);
        m_position.y = std::clamp(m_position.y, m_minY + halfH, m_maxY - halfH);
    }
}

Vec2 Camera::worldToScreen(Vec2 worldPos) const {
    Vec2 screen;
    screen.x = (worldPos.x - m_position.x) * zoom + m_screenW / 2.0f + m_shakeOffset.x;
    screen.y = (worldPos.y - m_position.y) * zoom + m_screenH / 2.0f + m_shakeOffset.y;
    return screen;
}

SDL_Rect Camera::worldToScreen(SDL_FRect worldRect) const {
    Vec2 screen = worldToScreen(Vec2{worldRect.x, worldRect.y});
    return {
        static_cast<int>(screen.x),
        static_cast<int>(screen.y),
        static_cast<int>(worldRect.w * zoom),
        static_cast<int>(worldRect.h * zoom)
    };
}

void Camera::setBounds(float minX, float minY, float maxX, float maxY) {
    m_hasBounds = true;
    m_minX = minX; m_minY = minY;
    m_maxX = maxX; m_maxY = maxY;
}
