#pragma once
#include <vector>
#include <cstdint>

struct TrailPoint {
    float x = 0, y = 0;
    float life = 0;
    float maxLife = 1.0f;  // non-zero default prevents NaN in life/maxLife alpha math
    float size = 0;
    uint8_t r = 255, g = 255, b = 255, a = 255;
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
