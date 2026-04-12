#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <set>
#include <string>
#include <unordered_map>
#include "Game/WeaponSystem.h"

class Player;
class SuitEntropy;
class DimensionManager;
class Level;
class EntityManager;
class CombatSystem;
struct AbilityComponent;

class HUD {
public:
    ~HUD() {
        clearTextCache();
        if (m_hudTarget) SDL_DestroyTexture(m_hudTarget);
        if (m_ngPlusTex) SDL_DestroyTexture(m_ngPlusTex);
    }

    void render(SDL_Renderer* renderer, TTF_Font* font,
                const Player* player, const SuitEntropy* entropy,
                const DimensionManager* dimMgr,
                int screenW, int screenH, int fps, int riftShards);

    void setCombatSystem(const CombatSystem* cs) { m_combatSystem = cs; }
    void setFloor(int floor) { m_currentFloor = floor; }
    void setKillCount(int kills) { m_killCount = kills; }
    void setNGPlusTier(int tier) { m_ngPlusTier = tier; }
    void setRunTime(float t) { m_runTime = t; }

    // Minimap toggle (M key)
    void toggleMinimap() { m_showMinimap = !m_showMinimap; }
    bool isMinimapVisible() const { return m_showMinimap; }

    void renderMinimap(SDL_Renderer* renderer, const Level* level,
                       const Player* player, const DimensionManager* dimMgr,
                       int screenW, int screenH,
                       EntityManager* entities = nullptr,
                       const std::set<int>* repairedRifts = nullptr);

    // Damage flash overlay
    void triggerDamageFlash() { m_damageFlash = 0.3f; }
    void updateFlash(float dt);
    void renderFlash(SDL_Renderer* renderer, int screenW, int screenH);

private:
    // Text texture cache entry — shared by renderText() and getOrBuildText()
    struct CachedText {
        SDL_Texture* texture = nullptr;
        int w = 0, h = 0;
    };

    // Sub-renderers extracted from render() for readability
    void renderAbilityBar(SDL_Renderer* renderer, TTF_Font* font,
                          const Player* player, const DimensionManager* dimMgr,
                          int screenW, int screenH, int startY);
    void renderCombatOverlay(SDL_Renderer* renderer, TTF_Font* font,
                             const Player* player, int screenW, int screenH);
    void renderWeaponPanel(SDL_Renderer* renderer, TTF_Font* font,
                           const Player* player, int screenW, int screenH);

    void renderBar(SDL_Renderer* renderer, int x, int y, int w, int h,
                   float percent, SDL_Color fillColor, SDL_Color bgColor);
    void renderText(SDL_Renderer* renderer, TTF_Font* font,
                    const char* text, int x, int y, SDL_Color color);
    // Cached text lookup/build — returns pointer to cached entry or nullptr.
    // Use when you need per-frame blend/color/alpha mod control (combo, bonus).
    CachedText* getOrBuildText(SDL_Renderer* renderer, TTF_Font* font,
                                const char* text, SDL_Color color);

    float m_damageFlash = 0;
    const CombatSystem* m_combatSystem = nullptr;
    int m_currentFloor = 1;
    int m_killCount = 0;
    bool m_showMinimap = true;  // M key toggle
    int m_ngPlusTier = 0;  // NG+ tier shown as gold number in top-right
    float m_runTime = 0.0f; // Run timer (seconds), set each frame

    // Ability ready flash: brief glow when cooldown finishes
    float m_abilityReadyFlash[4] = {};  // melee, ranged, dash, dim-switch
    bool m_abilityWasOnCooldown[4] = {};

    // Weapon mastery tier-up flash (gold pulse when tier increases)
    float m_masteryFlash[2] = {};  // [0]=melee, [1]=ranged — counts down from 0.6
    MasteryTier m_prevMasteryTier[2] = {};  // track previous tier to detect changes

    // Cached NG+ tier texture (recreated only when tier changes)
    SDL_Texture* m_ngPlusTex = nullptr;
    int m_ngPlusTexW = 0, m_ngPlusTexH = 0;
    int m_cachedNGTier = -1;

    // Off-screen texture for HUD opacity (lazily created, freed on resize)
    SDL_Texture* m_hudTarget = nullptr;
    int m_hudTargetW = 0;
    int m_hudTargetH = 0;

    // Cached SDL_GetTicks() — set once per render() call, used by all pulse/animation math
    Uint32 m_frameTicks = 0;

    // Text texture cache: avoids per-frame TTF_RenderText + SDL_CreateTextureFromSurface
    std::unordered_map<std::string, CachedText> m_textCache;
    static constexpr size_t MAX_TEXT_CACHE = 128;
    void clearTextCache();
};
