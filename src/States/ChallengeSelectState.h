#pragma once
#include "GameState.h"
#include "Game/ChallengeMode.h"
#include <SDL2/SDL_ttf.h>

class ChallengeSelectState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    void renderChallengeCard(SDL_Renderer* renderer, TTF_Font* font,
                             const ChallengeData& data, int x, int y, int w, int h, bool selected);
    void renderMutatorToggle(SDL_Renderer* renderer, TTF_Font* font,
                             const MutatorData& data, int x, int y, int w, int h, bool active, bool focused);

    int m_selectedChallenge = 0;
    int m_selectedMutatorSlot = -1; // -1 = in challenge list, 0+ = in mutator list
    bool m_inMutatorSection = false;
    int m_focusedMutator = 0;
    float m_time = 0;
};
