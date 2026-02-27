#include "Game/TrailEmitter.h"
#include <SDL2/SDL.h>

void TrailSystem::emit(float x, float y, float size, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float lifetime) {
    if (static_cast<int>(m_points.size()) >= MAX_POINTS) return;
    m_points.push_back({x, y, 0.0f, lifetime, size, r, g, b, a});
}

void TrailSystem::update(float dt) {
    for (auto it = m_points.begin(); it != m_points.end(); ) {
        it->life += dt;
        if (it->life >= it->maxLife) {
            it = m_points.erase(it);
        } else {
            ++it;
        }
    }
}

void TrailSystem::render(SDL_Renderer* renderer) const {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (auto& p : m_points) {
        float alpha = (p.maxLife > 0) ? 1.0f - (p.life / p.maxLife) : 0.0f;
        float s = p.size * alpha;
        if (s < 0.5f) continue;
        int is = static_cast<int>(s);
        SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, static_cast<uint8_t>(p.a * alpha));
        SDL_Rect rect = {static_cast<int>(p.x - s * 0.5f), static_cast<int>(p.y - s * 0.5f), is, is};
        SDL_RenderFillRect(renderer, &rect);
    }
}

void TrailSystem::clear() {
    m_points.clear();
}
