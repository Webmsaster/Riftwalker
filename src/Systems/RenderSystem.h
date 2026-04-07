#pragma once
#include "ECS/EntityManager.h"
#include "Core/Camera.h"
#include <SDL2/SDL.h>
#include <string>

class RenderSystem {
public:
    void render(SDL_Renderer* renderer, EntityManager& entities,
                const Camera& camera, int currentDimension, float dimBlendAlpha = 0.0f,
                float interpolation = 1.0f);

private:
    void renderEntity(SDL_Renderer* renderer, Entity& entity, const Camera& camera,
                      float alpha = 1.0f, float interpolation = 1.0f);

    // Custom procedural rendering per entity type
    void renderPlayer(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderWalker(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderFlyer(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderTurret(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderCharger(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderPhaser(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderExploder(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderShielder(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderCrawler(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderSummoner(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderSniper(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderTeleporter(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderReflector(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderLeech(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderSwarmer(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderGravityWell(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderMimic(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderBoss(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderVoidWyrm(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderDimensionalArchitect(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderTemporalWeaver(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderVoidSovereign(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderEntropyIncarnate(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderPickup(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderProjectile(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderPlayerTurret(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);
    void renderShockTrap(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);

    // Sprite-based rendering (returns true if sprite was rendered)
    bool renderSprite(SDL_Renderer* renderer, SDL_Rect rect, Entity& entity, float alpha);

    // Shared HP bar rendering for all enemies (color gradient, lag bar, elite marker)
    void renderEnemyHPBar(SDL_Renderer* renderer, int x, int y, int w, Entity& entity, float alpha, bool isBoss = false);

    // Helper
    void fillRect(SDL_Renderer* r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca);
    void drawLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca);

    // Per-frame cached SDL_GetTicks() to avoid redundant system calls
    Uint32 m_frameTicks = 0;

    // Reusable render sort buffer (avoids per-frame allocation)
    struct RenderEntry {
        Entity* entity;
        int layer;
        float alpha;
    };
    std::vector<RenderEntry> m_renderEntries;
};
