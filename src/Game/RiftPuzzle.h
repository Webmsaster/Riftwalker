#pragma once
#include "Core/Camera.h"
#include <SDL2/SDL.h>
#include <vector>
#include <functional>

enum class PuzzleType {
    Timing,     // Hit button at right moment in a cycle
    Sequence,   // Repeat a color/direction sequence
    Alignment   // Align two dimensions by rotating pieces
};

enum class PuzzleState {
    Inactive,
    Active,
    Success,
    Failed
};

class RiftPuzzle {
public:
    RiftPuzzle(PuzzleType type, int difficulty);

    void activate();
    void update(float dt);
    void render(SDL_Renderer* renderer, int screenW, int screenH);
    void handleInput(int action); // 0=up, 1=right, 2=down, 3=left, 4=confirm

    PuzzleState getState() const { return m_state; }
    PuzzleType getType() const { return m_type; }
    bool isActive() const { return m_state == PuzzleState::Active; }

    std::function<void(bool success)> onComplete;

private:
    void initTiming();
    void initSequence();
    void initAlignment();

    void updateTiming(float dt);
    void updateSequence(float dt);
    void updateAlignment(float dt);

    void renderTiming(SDL_Renderer* renderer, int cx, int cy);
    void renderSequence(SDL_Renderer* renderer, int cx, int cy);
    void renderAlignment(SDL_Renderer* renderer, int cx, int cy);

    PuzzleType m_type;
    PuzzleState m_state = PuzzleState::Inactive;
    int m_difficulty;
    float m_timer = 0;
    float m_timeLimit = 10.0f;

    // Timing puzzle
    float m_cycleSpeed = 2.0f;
    float m_cyclePos = 0;  // 0-1
    float m_sweetSpotStart = 0.4f;
    float m_sweetSpotEnd = 0.6f;
    int m_timingHits = 0;
    int m_timingRequired = 3;

    // Sequence puzzle
    std::vector<int> m_sequence;
    int m_sequenceIndex = 0;
    float m_showTimer = 0;
    bool m_showingSequence = true;
    std::vector<int> m_playerInput;

    // Alignment puzzle
    int m_currentRotation = 0;
    int m_targetRotation = 0;
};
