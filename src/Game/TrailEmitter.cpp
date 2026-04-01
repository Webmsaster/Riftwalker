#include "Game/TrailEmitter.h"
#include <SDL2/SDL.h>
#include <algorithm>

void TrailSystem::emit(float x, float y, float size, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float lifetime) {
    if (static_cast<int>(m_points.size()) >= MAX_POINTS) return;
    m_points.push_back({x, y, 0.0f, lifetime, size, r, g, b, a});
}

void TrailSystem::update(float dt) {
    for (auto& p : m_points) p.life += dt;
    m_points.erase(
        std::remove_if(m_points.begin(), m_points.end(),
            [](const TrailPoint& p) { return p.life >= p.maxLife; }),
        m_points.end());
}

void TrailSystem::render(SDL_Renderer* renderer) const {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (auto& p : m_points) {
        float alpha = (p.maxLife > 0) ? 1.0f - (p.life / p.maxLife) : 0.0f;
        float s = p.size * alpha;
        if (s < 0.5f) continue;
        int is = static_cast<int>(s);
        int px = static_cast<int>(p.x - s * 0.5f);
        int py = static_cast<int>(p.y - s * 0.5f);
        uint8_t a = static_cast<uint8_t>(p.a * alpha);

        // Outer glow halo (larger, dimmer)
        if (is >= 2) {
            SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, static_cast<uint8_t>(a * 0.25f));
            SDL_Rect glow = {px - 2, py - 2, is + 4, is + 4};
            SDL_RenderFillRect(renderer, &glow);
        }

        // Core trail point
        SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, a);
        SDL_Rect rect = {px, py, is, is};
        SDL_RenderFillRect(renderer, &rect);

        // Bright center (for larger trail points)
        if (is >= 3 && a > 60) {
            uint8_t br = static_cast<uint8_t>(std::min(255, p.r + 80));
            uint8_t bg = static_cast<uint8_t>(std::min(255, p.g + 80));
            uint8_t bb = static_cast<uint8_t>(std::min(255, p.b + 60));
            SDL_SetRenderDrawColor(renderer, br, bg, bb, static_cast<uint8_t>(a * 0.6f));
            SDL_Rect core = {px + is / 2, py + is / 2, 1, 1};
            SDL_RenderFillRect(renderer, &core);
        }
    }
}

void TrailSystem::clear() {
    m_points.clear();
}
