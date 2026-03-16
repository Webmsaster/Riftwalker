#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Player;
class SuitEntropy;
class DimensionManager;
class Level;
class EntityManager;
class CombatSystem;
struct AbilityComponent;

class HUD {
public:
    void render(SDL_Renderer* renderer, TTF_Font* font,
                const Player* player, const SuitEntropy* entropy,
                const DimensionManager* dimMgr,
                int screenW, int screenH, int fps, int riftShards);

    void setCombatSystem(const CombatSystem* cs) { m_combatSystem = cs; }
    void setFloor(int floor) { m_currentFloor = floor; }
    void setKillCount(int kills) { m_killCount = kills; }

    void renderMinimap(SDL_Renderer* renderer, const Level* level,
                       const Player* player, const DimensionManager* dimMgr,
                       int screenW, int screenH,
                       EntityManager* entities = nullptr);

    // Damage flash overlay
    void triggerDamageFlash() { m_damageFlash = 0.3f; }
    void updateFlash(float dt);
    void renderFlash(SDL_Renderer* renderer, int screenW, int screenH);

private:
    void renderBar(SDL_Renderer* renderer, int x, int y, int w, int h,
                   float percent, SDL_Color fillColor, SDL_Color bgColor);
    void renderText(SDL_Renderer* renderer, TTF_Font* font,
                    const char* text, int x, int y, SDL_Color color);

    float m_damageFlash = 0;
    const CombatSystem* m_combatSystem = nullptr;
    int m_currentFloor = 1;
    int m_killCount = 0;

    // Ability ready flash: brief glow when cooldown finishes
    float m_abilityReadyFlash[4] = {};  // melee, ranged, dash, dim-switch
    bool m_abilityWasOnCooldown[4] = {};

    // Off-screen texture for HUD opacity (lazily created, freed on resize)
    SDL_Texture* m_hudTarget = nullptr;
    int m_hudTargetW = 0;
    int m_hudTargetH = 0;
};
