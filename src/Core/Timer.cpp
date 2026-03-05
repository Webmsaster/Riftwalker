#include "Timer.h"

Timer::Timer()
    : m_deltaTime(0.0f)
    , m_totalTime(0.0f)
    , m_accumulator(0.0f)
    , m_fps(0)
    , m_frameCount(0)
    , m_fpsTimer(0.0f)
{
    m_frequency = SDL_GetPerformanceFrequency();
    if (m_frequency == 0) m_frequency = 1;
    m_lastTime = SDL_GetPerformanceCounter();
}

void Timer::reset() {
    m_lastTime = SDL_GetPerformanceCounter();
    m_deltaTime = 0.0f;
    m_accumulator = 0.0f;
}

void Timer::update() {
    Uint64 currentTime = SDL_GetPerformanceCounter();
    m_deltaTime = static_cast<float>(currentTime - m_lastTime) / static_cast<float>(m_frequency);
    m_lastTime = currentTime;

    // Clamp delta time to prevent spiral of death
    if (m_deltaTime > 0.25f) m_deltaTime = 0.25f;

    m_totalTime += m_deltaTime;
    m_accumulator += m_deltaTime;

    // FPS counter
    m_frameCount++;
    m_fpsTimer += m_deltaTime;
    if (m_fpsTimer >= 1.0f) {
        m_fps = m_frameCount;
        m_frameCount = 0;
        m_fpsTimer -= 1.0f;
    }
}

bool Timer::shouldStep() {
    if (m_accumulator >= FIXED_TIMESTEP) {
        m_accumulator -= FIXED_TIMESTEP;
        return true;
    }
    return false;
}
