#pragma once
#include <vector>
#include <cstdint>

struct TrailPoint {
    float x, y;
    float life;
    float maxLife;
    float size;
    uint8_t r, g, b, a;
};

class TrailSystem {
public:
    void emit(float x, float y, float size, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float lifetime = 0.3f);
    void update(float dt);
    void render(struct SDL_Renderer* renderer) const;
    void clear();
    int count() const { return static_cast<int>(m_points.size()); }

private:
    static constexpr int MAX_POINTS = 500;
    std::vector<TrailPoint> m_points;
};
