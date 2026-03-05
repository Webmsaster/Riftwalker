#include "RiftPuzzle.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

RiftPuzzle::RiftPuzzle(PuzzleType type, int difficulty)
    : m_type(type), m_difficulty(difficulty)
{
    switch (type) {
        case PuzzleType::Timing: initTiming(); break;
        case PuzzleType::Sequence: initSequence(); break;
        case PuzzleType::Alignment: initAlignment(); break;
    }
}

void RiftPuzzle::initTiming() {
    m_cycleSpeed = 1.5f + m_difficulty * 0.8f; // Faster scaling
    float halfSize = 0.10f - m_difficulty * 0.015f;
    halfSize = std::max(halfSize, 0.04f); // Minimum 4% sweet spot
    m_sweetSpotStart = 0.5f - halfSize;
    m_sweetSpotEnd = 0.5f + halfSize;
    m_timingRequired = 3 + m_difficulty; // More hits required
    m_timeLimit = 8.0f + m_difficulty * 1.5f;
}

void RiftPuzzle::initSequence() {
    int seqLen = 3 + m_difficulty;
    m_sequence.clear();
    for (int i = 0; i < seqLen; i++) {
        m_sequence.push_back(std::rand() % 4); // 0-3 directions
    }
    m_timeLimit = 5.0f + seqLen * 2.0f;
}

void RiftPuzzle::initAlignment() {
    m_targetRotation = 0;
    m_currentRotation = (1 + std::rand() % 7) * 45; // 45-315 degrees
    m_timeLimit = 8.0f + m_difficulty * 2.0f;
}

void RiftPuzzle::activate() {
    m_state = PuzzleState::Active;
    m_timer = 0;
    m_timingHits = 0;
    m_sequenceIndex = 0;
    m_showingSequence = (m_type == PuzzleType::Sequence);
    m_showTimer = 0;
    m_playerInput.clear();
}

void RiftPuzzle::update(float dt) {
    if (m_state != PuzzleState::Active) return;

    m_timer += dt;
    if (m_timer >= m_timeLimit) {
        m_state = PuzzleState::Failed;
        if (onComplete) onComplete(false);
        return;
    }

    switch (m_type) {
        case PuzzleType::Timing: updateTiming(dt); break;
        case PuzzleType::Sequence: updateSequence(dt); break;
        case PuzzleType::Alignment: updateAlignment(dt); break;
    }
}

void RiftPuzzle::updateTiming(float dt) {
    m_cyclePos += m_cycleSpeed * dt;
    if (m_cyclePos > 1.0f) m_cyclePos -= 1.0f;
}

void RiftPuzzle::updateSequence(float dt) {
    if (m_showingSequence) {
        m_showTimer += dt;
        float showDuration = 0.8f;
        int showIndex = static_cast<int>(m_showTimer / showDuration);
        if (showIndex >= static_cast<int>(m_sequence.size())) {
            m_showingSequence = false;
            m_showTimer = 0;
        }
    }
}

void RiftPuzzle::updateAlignment(float dt) {
    // Nothing to update continuously
}

void RiftPuzzle::handleInput(int action) {
    if (m_state != PuzzleState::Active) return;

    switch (m_type) {
        case PuzzleType::Timing: {
            if (action == 4) { // confirm
                if (m_cyclePos >= m_sweetSpotStart && m_cyclePos <= m_sweetSpotEnd) {
                    m_timingHits++;
                    if (m_timingHits >= m_timingRequired) {
                        m_state = PuzzleState::Success;
                        if (onComplete) onComplete(true);
                    }
                } else {
                    m_timingHits = std::max(0, m_timingHits - 1);
                }
            }
            break;
        }
        case PuzzleType::Sequence: {
            if (m_showingSequence) return;
            if (action >= 0 && action <= 3) {
                m_playerInput.push_back(action);
                int idx = static_cast<int>(m_playerInput.size()) - 1;
                if (idx >= static_cast<int>(m_sequence.size()) || m_playerInput[idx] != m_sequence[idx]) {
                    m_state = PuzzleState::Failed;
                    if (onComplete) onComplete(false);
                    return;
                }
                if (m_playerInput.size() == m_sequence.size()) {
                    m_state = PuzzleState::Success;
                    if (onComplete) onComplete(true);
                }
            }
            break;
        }
        case PuzzleType::Alignment: {
            if (action == 1) m_currentRotation = (m_currentRotation + 45) % 360;
            else if (action == 3) m_currentRotation = (m_currentRotation - 45 + 360) % 360;

            if (m_currentRotation == m_targetRotation) {
                m_state = PuzzleState::Success;
                if (onComplete) onComplete(true);
            }
            break;
        }
    }
}

void RiftPuzzle::render(SDL_Renderer* renderer, int screenW, int screenH) {
    if (m_state != PuzzleState::Active) return;

    int cx = screenW / 2;
    int cy = screenH / 2;

    // Dark overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect overlay = {0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &overlay);

    // Puzzle frame
    int frameW = 300, frameH = 200;
    SDL_Rect frame = {cx - frameW / 2, cy - frameH / 2, frameW, frameH};
    SDL_SetRenderDrawColor(renderer, 30, 20, 50, 240);
    SDL_RenderFillRect(renderer, &frame);
    SDL_SetRenderDrawColor(renderer, 120, 60, 200, 255);
    SDL_RenderDrawRect(renderer, &frame);

    // Timer bar
    float timePercent = 1.0f - m_timer / m_timeLimit;
    SDL_Rect timerBar = {cx - frameW / 2 + 5, cy - frameH / 2 + 5,
                         static_cast<int>((frameW - 10) * timePercent), 6};
    Uint8 timerR = static_cast<Uint8>(255 * (1.0f - timePercent));
    Uint8 timerG = static_cast<Uint8>(255 * timePercent);
    SDL_SetRenderDrawColor(renderer, timerR, timerG, 50, 255);
    SDL_RenderFillRect(renderer, &timerBar);

    switch (m_type) {
        case PuzzleType::Timing: renderTiming(renderer, cx, cy); break;
        case PuzzleType::Sequence: renderSequence(renderer, cx, cy); break;
        case PuzzleType::Alignment: renderAlignment(renderer, cx, cy); break;
    }
}

void RiftPuzzle::renderTiming(SDL_Renderer* renderer, int cx, int cy) {
    // Oscillating bar
    int barW = 240, barH = 20;
    int barX = cx - barW / 2;
    int barY = cy;

    // Background bar
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_Rect bar = {barX, barY, barW, barH};
    SDL_RenderFillRect(renderer, &bar);

    // Sweet spot
    int ssX = barX + static_cast<int>(m_sweetSpotStart * barW);
    int ssW = static_cast<int>((m_sweetSpotEnd - m_sweetSpotStart) * barW);
    SDL_SetRenderDrawColor(renderer, 50, 200, 50, 200);
    SDL_Rect sweetSpot = {ssX, barY, ssW, barH};
    SDL_RenderFillRect(renderer, &sweetSpot);

    // Cursor
    int cursorX = barX + static_cast<int>(m_cyclePos * barW);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect cursor = {cursorX - 2, barY - 4, 4, barH + 8};
    SDL_RenderFillRect(renderer, &cursor);

    // Hit counter
    for (int i = 0; i < m_timingRequired; i++) {
        SDL_Rect dot = {cx - m_timingRequired * 10 + i * 20, cy - 30, 12, 12};
        if (i < m_timingHits) {
            SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
        }
        SDL_RenderFillRect(renderer, &dot);
    }
}

void RiftPuzzle::renderSequence(SDL_Renderer* renderer, int cx, int cy) {
    // Direction colors: up=blue, right=green, down=red, left=yellow
    SDL_Color dirColors[4] = {
        {80, 80, 255, 255},   // up
        {80, 255, 80, 255},   // right
        {255, 80, 80, 255},   // down
        {255, 255, 80, 255}   // left
    };

    if (m_showingSequence) {
        int showIndex = static_cast<int>(m_showTimer / 0.8f);
        showIndex = std::min(showIndex, static_cast<int>(m_sequence.size()) - 1);

        // Show current direction
        auto& c = dirColors[m_sequence[showIndex]];
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_Rect indicator = {cx - 25, cy - 25, 50, 50};
        SDL_RenderFillRect(renderer, &indicator);
    } else {
        // Show 4 direction buttons
        int btnSize = 30;
        SDL_Rect buttons[4] = {
            {cx - btnSize / 2, cy - 50, btnSize, btnSize},       // up
            {cx + 30, cy - btnSize / 2, btnSize, btnSize},       // right
            {cx - btnSize / 2, cy + 20, btnSize, btnSize},       // down
            {cx - 60, cy - btnSize / 2, btnSize, btnSize}        // left
        };

        for (int i = 0; i < 4; i++) {
            auto& c = dirColors[i];
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 200);
            SDL_RenderFillRect(renderer, &buttons[i]);
        }

        // Show progress
        for (int i = 0; i < static_cast<int>(m_playerInput.size()); i++) {
            SDL_Rect dot = {cx - static_cast<int>(m_sequence.size()) * 8 + i * 16, cy + 60, 10, 10};
            SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
            SDL_RenderFillRect(renderer, &dot);
        }
    }
}

void RiftPuzzle::renderAlignment(SDL_Renderer* renderer, int cx, int cy) {
    // Target (stationary circle outline)
    int radius = 50;
    SDL_SetRenderDrawColor(renderer, 100, 255, 100, 100);
    for (int i = 0; i < 8; i++) {
        float angle = i * 45.0f * 3.14159f / 180.0f;
        int x1 = cx + static_cast<int>(radius * std::cos(angle));
        int y1 = cy + static_cast<int>(radius * std::sin(angle));
        SDL_Rect dot = {x1 - 3, y1 - 3, 6, 6};
        SDL_RenderFillRect(renderer, &dot);
    }

    // Current rotation indicator
    float angle = m_currentRotation * 3.14159f / 180.0f;
    int indX = cx + static_cast<int>(radius * std::cos(angle));
    int indY = cy + static_cast<int>(radius * std::sin(angle));
    SDL_SetRenderDrawColor(renderer, 200, 100, 255, 255);
    SDL_Rect indicator = {indX - 6, indY - 6, 12, 12};
    SDL_RenderFillRect(renderer, &indicator);

    // Target indicator at 0 degrees
    float tAngle = m_targetRotation * 3.14159f / 180.0f;
    int tX = cx + static_cast<int>(radius * std::cos(tAngle));
    int tY = cy + static_cast<int>(radius * std::sin(tAngle));
    SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
    SDL_Rect target = {tX - 8, tY - 8, 16, 16};
    SDL_RenderDrawRect(renderer, &target);
}
