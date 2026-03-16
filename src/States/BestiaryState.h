#pragma once
#include "GameState.h"
#include "Game/Bestiary.h"
#include <SDL2/SDL_ttf.h>

class BestiaryState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    // Detail panel renderers
    void renderDiscoveredDetail(SDL_Renderer* renderer, TTF_Font* font,
                                const BestiaryEntry& entry, int typeIdx, bool isBoss);
    void renderUndiscoveredDetail(SDL_Renderer* renderer, TTF_Font* font,
                                  int typeIdx, bool isBoss);

    // Enemy preview (colored, animated procedural shapes)
    void renderEnemyPreview(SDL_Renderer* renderer, int cx, int cy, int type, bool isBoss);
    // Silhouette for undiscovered entries
    void renderSilhouette(SDL_Renderer* renderer, int cx, int cy, int type, bool isBoss);

    // Per-type preview shapes
    void renderWalkerPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderFlyerPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderTurretPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderChargerPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderPhaserPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderExploderPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderShielderPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderCrawlerPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderSummonerPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderSniperPreview(SDL_Renderer* renderer, int cx, int cy, float hover);
    void renderBossPreview(SDL_Renderer* renderer, int cx, int cy, int bossType);

    int   m_selected     = 0;
    int   m_totalEntries = 0;
    float m_time         = 0;
    int   m_scrollOffset = 0;
};
