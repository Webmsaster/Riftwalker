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
        case PuzzleType::Pattern: initPattern(); break;
    }
}

void RiftPuzzle::initTiming() {
    m_cycleSpeed = 1.2f + m_difficulty * 0.4f; // Gentler speed scaling
    float halfSize = 0.15f - m_difficulty * 0.012f;
    halfSize = std::max(halfSize, 0.07f); // Minimum 7% sweet spot (was 4%)
    m_sweetSpotStart = 0.5f - halfSize;
    m_sweetSpotEnd = 0.5f + halfSize;
    m_timingRequired = 2 + m_difficulty / 2; // Fewer hits required
    m_timeLimit = 10.0f + m_difficulty * 2.0f; // More time
}

void RiftPuzzle::initSequence() {
    int seqLen = 3 + m_difficulty / 2; // Shorter sequences (was 3 + difficulty)
    seqLen = std::min(seqLen, 6); // Cap at 6 max
    m_sequence.clear();
    for (int i = 0; i < seqLen; i++) {
        m_sequence.push_back(std::rand() % 4); // 0-3 directions
    }
    m_timeLimit = 6.0f + seqLen * 2.5f; // More time per step
}

void RiftPuzzle::initAlignment() {
    m_targetRotation = 0;
    m_currentRotation = (1 + std::rand() % 7) * 45; // 45-315 degrees
    m_timeLimit = 8.0f + m_difficulty * 2.0f;
}

void RiftPuzzle::initPattern() {
    // Clear grids
    for (int y = 0; y < PATTERN_GRID; y++)
        for (int x = 0; x < PATTERN_GRID; x++) {
            m_patternTarget[y][x] = false;
            m_patternPlayer[y][x] = false;
        }
    // Number of cells to memorize scales with difficulty: 2-5
    m_patternCellCount = std::min(2 + m_difficulty / 3, 4); // Fewer cells, cap at 4
    // Place random cells
    int placed = 0;
    while (placed < m_patternCellCount) {
        int rx = std::rand() % PATTERN_GRID;
        int ry = std::rand() % PATTERN_GRID;
        if (!m_patternTarget[ry][rx]) {
            m_patternTarget[ry][rx] = true;
            placed++;
        }
    }
    m_patternShowing = true;
    m_patternShowTimer = 0;
    m_patternShowDuration = std::max(2.5f, 4.0f - m_difficulty * 0.15f); // Longer reveal time
    m_patternCursorX = 1;
    m_patternCursorY = 1;
    m_timeLimit = 10.0f + m_difficulty * 2.0f; // More time
}

void RiftPuzzle::activate() {
    m_state = PuzzleState::Active;
    m_timer = 0;
    m_timingHits = 0;
    m_sequenceIndex = 0;
    m_showingSequence = (m_type == PuzzleType::Sequence);
    m_patternShowing = (m_type == PuzzleType::Pattern);
    m_patternShowTimer = 0;
    m_showTimer = 0;
    m_playerInput.clear();
    // Clear player grid on activate
    for (int y = 0; y < PATTERN_GRID; y++)
        for (int x = 0; x < PATTERN_GRID; x++)
            m_patternPlayer[y][x] = false;
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
        case PuzzleType::Pattern: updatePattern(dt); break;
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

void RiftPuzzle::updatePattern(float dt) {
    if (m_patternShowing) {
        m_patternShowTimer += dt;
        if (m_patternShowTimer >= m_patternShowDuration) {
            m_patternShowing = false;
        }
    }
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
                    // Wrong input: reset progress instead of instant fail
                    m_playerInput.clear();
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
        case PuzzleType::Pattern: {
            if (m_patternShowing) return; // can't interact during reveal
            // Navigate cursor
            if (action == 0 && m_patternCursorY > 0) m_patternCursorY--;
            else if (action == 2 && m_patternCursorY < PATTERN_GRID - 1) m_patternCursorY++;
            else if (action == 3 && m_patternCursorX > 0) m_patternCursorX--;
            else if (action == 1 && m_patternCursorX < PATTERN_GRID - 1) m_patternCursorX++;
            else if (action == 4) {
                // Toggle cell
                m_patternPlayer[m_patternCursorY][m_patternCursorX] =
                    !m_patternPlayer[m_patternCursorY][m_patternCursorX];
                // Check if player has selected enough cells
                int selected = 0;
                for (int y = 0; y < PATTERN_GRID; y++)
                    for (int x = 0; x < PATTERN_GRID; x++)
                        if (m_patternPlayer[y][x]) selected++;
                if (selected == m_patternCellCount) {
                    // Check if pattern matches
                    bool match = true;
                    for (int y = 0; y < PATTERN_GRID; y++)
                        for (int x = 0; x < PATTERN_GRID; x++)
                            if (m_patternTarget[y][x] != m_patternPlayer[y][x])
                                match = false;
                    if (match) {
                        m_state = PuzzleState::Success;
                        if (onComplete) onComplete(true);
                    } else {
                        m_state = PuzzleState::Failed;
                        if (onComplete) onComplete(false);
                    }
                }
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
    float timePercent = 1.0f - m_timer / std::max(0.01f, m_timeLimit);
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
        case PuzzleType::Pattern: renderPattern(renderer, cx, cy); break;
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

void RiftPuzzle::renderPattern(SDL_Renderer* renderer, int cx, int cy) {
    int cellSize = 40;
    int gap = 4;
    int gridPx = PATTERN_GRID * (cellSize + gap) - gap;
    int startX = cx - gridPx / 2;
    int startY = cy - gridPx / 2;

    for (int y = 0; y < PATTERN_GRID; y++) {
        for (int x = 0; x < PATTERN_GRID; x++) {
            SDL_Rect cell = {
                startX + x * (cellSize + gap),
                startY + y * (cellSize + gap),
                cellSize, cellSize
            };

            if (m_patternShowing) {
                // Reveal phase: show target pattern
                if (m_patternTarget[y][x]) {
                    // Pulsing glow effect
                    float pulse = 0.7f + 0.3f * std::sin(m_patternShowTimer * 4.0f);
                    Uint8 brightness = static_cast<Uint8>(200 * pulse);
                    SDL_SetRenderDrawColor(renderer, 120, brightness, 255, 255);
                    SDL_RenderFillRect(renderer, &cell);
                } else {
                    SDL_SetRenderDrawColor(renderer, 40, 35, 55, 255);
                    SDL_RenderFillRect(renderer, &cell);
                }
            } else {
                // Input phase
                if (m_patternPlayer[y][x]) {
                    SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
                    SDL_RenderFillRect(renderer, &cell);
                } else {
                    SDL_SetRenderDrawColor(renderer, 40, 35, 55, 255);
                    SDL_RenderFillRect(renderer, &cell);
                }
                // Cursor highlight
                if (x == m_patternCursorX && y == m_patternCursorY) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
                    SDL_RenderDrawRect(renderer, &cell);
                    // Inner border for visibility
                    SDL_Rect inner = {cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2};
                    SDL_RenderDrawRect(renderer, &inner);
                }
            }

            // Subtle grid border
            SDL_SetRenderDrawColor(renderer, 80, 60, 120, 150);
            SDL_RenderDrawRect(renderer, &cell);
        }
    }

    // Selection counter below grid
    if (!m_patternShowing) {
        int selected = 0;
        for (int y = 0; y < PATTERN_GRID; y++)
            for (int x = 0; x < PATTERN_GRID; x++)
                if (m_patternPlayer[y][x]) selected++;
        int dotY = startY + gridPx + 12;
        for (int i = 0; i < m_patternCellCount; i++) {
            SDL_Rect dot = {cx - m_patternCellCount * 10 + i * 20, dotY, 12, 12};
            if (i < selected) {
                SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 50, 45, 70, 255);
            }
            SDL_RenderFillRect(renderer, &dot);
        }
    }
}
