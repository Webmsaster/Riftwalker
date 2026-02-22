#pragma once
#include <SDL2/SDL.h>

class Timer {
public:
    Timer();

    void reset();
    void update();
    float getDeltaTime() const { return m_deltaTime; }
    float getTotalTime() const { return m_totalTime; }
    int getFPS() const { return m_fps; }

    // Fixed timestep
    static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
    static constexpr int MAX_STEPS_PER_FRAME = 5;

    bool shouldStep();
    float getAccumulator() const { return m_accumulator; }
    float getInterpolation() const { return m_accumulator / FIXED_TIMESTEP; }

private:
    Uint64 m_lastTime;
    Uint64 m_frequency;
    float m_deltaTime;
    float m_totalTime;
    float m_accumulator;

    // FPS counter
    int m_fps;
    int m_frameCount;
    float m_fpsTimer;
};
