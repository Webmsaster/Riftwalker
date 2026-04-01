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

void Camera::follow(const Vec2& target, float dt, const Vec2& velocity, bool grounded) {
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

    // Horizontal: always smooth follow
    float diffX = m_target.x - m_position.x;
    m_position.x += diffX * (m_smoothSpeed * dt);

    // Vertical dead zone: only follow when player leaves the band
    float diffY = m_target.y - m_position.y;
    if (grounded) {
        // On ground: gently re-center vertically (smooth return to center)
        m_position.y += diffY * (3.0f * dt);
    } else if (diffY > verticalDeadZone) {
        // Player below dead zone: follow to keep them at zone edge
        float overshoot = diffY - verticalDeadZone;
        m_position.y += overshoot * (m_smoothSpeed * 1.5f * dt);
    } else if (diffY < -verticalDeadZone) {
        // Player above dead zone: follow to keep them at zone edge
        float overshoot = diffY + verticalDeadZone;
        m_position.y += overshoot * (m_smoothSpeed * 1.5f * dt);
    }
    // Inside dead zone + airborne: camera stays still vertically
}

void Camera::flash(float duration, Uint8 r, Uint8 g, Uint8 b) {
    m_flashDuration = duration;
    m_flashTimer = 0;
    m_flashColor = {r, g, b, 255};
}

float Camera::getFlashAlpha() const {
    if (m_flashDuration <= 0 || m_flashTimer >= m_flashDuration) return 0;
    float progress = m_flashTimer / m_flashDuration;
    // Quick fade-out: starts bright, decays fast
    return (1.0f - progress) * (1.0f - progress);
}

void Camera::shake(float intensity, float duration) {
    float scaled = intensity * shakeMultiplier;
    // Keep the stronger shake — don't let weaker calls overwrite a big hit
    float remaining = m_shakeIntensity * std::max(0.0f, 1.0f - m_shakeTimer / std::max(m_shakeDuration, 0.001f));
    if (scaled >= remaining) {
        m_shakeIntensity = scaled;
        m_shakeDuration = duration;
        m_shakeTimer = 0;
    }
}

void Camera::update(float dt) {
    // Update screen flash timer
    if (m_flashDuration > 0 && m_flashTimer < m_flashDuration) {
        m_flashTimer += dt;
    }

    // Smooth zoom transition
    if (std::abs(zoom - zoomTarget) > 0.01f) {
        float diff = zoomTarget - zoom;
        zoom += diff * zoomSpeed * dt;
    } else {
        zoom = zoomTarget;
    }

    m_shakeOffset = {0, 0};
    if (m_shakeDuration > 0 && m_shakeTimer < m_shakeDuration) {
        m_shakeTimer += dt;
        float progress = m_shakeTimer / m_shakeDuration;
        // Exponential decay for natural falloff (fast start, smooth tail)
        float decay = std::exp(-4.0f * progress);
        float currentIntensity = m_shakeIntensity * decay;
        // Sinusoidal oscillation for smoother motion (not pure random)
        float freq = 25.0f; // High frequency for snappy feel
        float phase = m_shakeTimer * freq;
        float randX = std::sin(phase) * 0.7f + (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * 0.3f;
        float randY = std::cos(phase * 1.3f) * 0.7f + (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * 0.3f;
        m_shakeOffset.x = randX * currentIntensity;
        m_shakeOffset.y = randY * currentIntensity;
    }

    // Clamp to bounds
    if (m_hasBounds) {
        float halfW = m_screenW / (2.0f * zoom);
        float halfH = m_screenH / (2.0f * zoom);
        float clampMinX = m_minX + halfW;
        float clampMaxX = m_maxX - halfW;
        float clampMinY = m_minY + halfH;
        float clampMaxY = m_maxY - halfH;
        if (clampMinX > clampMaxX) clampMinX = clampMaxX = (clampMinX + clampMaxX) * 0.5f;
        if (clampMinY > clampMaxY) clampMinY = clampMaxY = (clampMinY + clampMaxY) * 0.5f;
        m_position.x = std::clamp(m_position.x, clampMinX, clampMaxX);
        m_position.y = std::clamp(m_position.y, clampMinY, clampMaxY);
    }
}

Vec2 Camera::worldToScreen(const Vec2& worldPos) const {
    Vec2 screen;
    screen.x = (worldPos.x - m_position.x) * zoom + m_screenW / 2.0f + m_shakeOffset.x;
    screen.y = (worldPos.y - m_position.y) * zoom + m_screenH / 2.0f + m_shakeOffset.y;
    return screen;
}

SDL_Rect Camera::worldToScreen(SDL_FRect worldRect) const {
    Vec2 screen = worldToScreen(Vec2{worldRect.x, worldRect.y});
    // Compute right/bottom edges first, then derive w/h for pixel-perfect alignment
    float right = screen.x + worldRect.w * zoom;
    float bottom = screen.y + worldRect.h * zoom;
    int x = static_cast<int>(std::round(screen.x));
    int y = static_cast<int>(std::round(screen.y));
    int w = static_cast<int>(std::round(right)) - x;
    int h = static_cast<int>(std::round(bottom)) - y;
    // Ensure minimum 1px size
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return {x, y, w, h};
}

void Camera::setBounds(float minX, float minY, float maxX, float maxY) {
    m_hasBounds = true;
    m_minX = minX; m_minY = minY;
    m_maxX = maxX; m_maxY = maxY;
}
