#pragma once
#include "GameState.h"

// NG+ tier selection screen shown after DifficultySelect when NG+ is unlocked.
// Players choose Normal (0) or any NG+ tier they have unlocked (1-5).
class NGPlusSelectState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void handleConfirm();
    int m_selected = 0;   // currently highlighted tier (0=Normal, 1-5=NG+X)
    int m_maxTier = 0;    // highest unlocked tier
    float m_time = 0;
};
