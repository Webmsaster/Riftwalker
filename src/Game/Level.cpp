#include "Level.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

Level::Level(int width, int height, int tileSize)
    : m_width(width), m_height(height), m_tileSize(tileSize)
{
    m_tilesA.resize(width * height);
    m_tilesB.resize(width * height);
}

Tile& Level::getTile(int x, int y, int dimension) {
    static Tile empty;
    if (!inBounds(x, y)) return empty;
    return (dimension == 2) ? m_tilesB[index(x, y)] : m_tilesA[index(x, y)];
}

const Tile& Level::getTile(int x, int y, int dimension) const {
    static const Tile empty;
    if (!inBounds(x, y)) return empty;
    return (dimension == 2) ? m_tilesB[index(x, y)] : m_tilesA[index(x, y)];
}

void Level::setTile(int x, int y, int dimension, const Tile& tile) {
    if (!inBounds(x, y)) return;
    if (dimension == 2) m_tilesB[index(x, y)] = tile;
    else m_tilesA[index(x, y)] = tile;
}

bool Level::isSolid(int x, int y, int dimension) const {
    if (!inBounds(x, y)) return true;
    const auto& tile = getTile(x, y, dimension == 0 ? 1 : dimension);
    return tile.isSolid();
}

bool Level::isOneWay(int x, int y, int dimension) const {
    if (!inBounds(x, y)) return false;
    const auto& tile = getTile(x, y, dimension == 0 ? 1 : dimension);
    return tile.isOneWay();
}

bool Level::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

void Level::render(SDL_Renderer* renderer, const Camera& camera,
                   int currentDim, float blendAlpha) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Vec2 camPos = camera.getPosition();
    int screenW = 1280;
    int screenH = 720;

    int startX = std::max(0, static_cast<int>((camPos.x - screenW / 2) / m_tileSize) - 1);
    int startY = std::max(0, static_cast<int>((camPos.y - screenH / 2) / m_tileSize) - 1);
    int endX = std::min(m_width - 1, static_cast<int>((camPos.x + screenW / 2) / m_tileSize) + 1);
    int endY = std::min(m_height - 1, static_cast<int>((camPos.y + screenH / 2) / m_tileSize) + 1);

    // Render current dimension tiles with edge-aware rendering
    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            const Tile& tile = getTile(x, y, currentDim);
            if (tile.type == TileType::Empty) continue;

            SDL_FRect worldRect = {
                static_cast<float>(x * m_tileSize),
                static_cast<float>(y * m_tileSize),
                static_cast<float>(m_tileSize),
                static_cast<float>(m_tileSize)
            };
            SDL_Rect sr = camera.worldToScreen(worldRect);

            if (tile.type == TileType::Solid) {
                renderSolidTile(renderer, sr, tile, x, y, currentDim);
            } else if (tile.type == TileType::OneWay) {
                renderOneWayTile(renderer, sr, tile);
            } else if (tile.type == TileType::Spike) {
                renderSpikeTile(renderer, sr, tile);
            } else if (tile.type == TileType::Decoration) {
                // Subtle accent dot/cross
                Uint8 da = tile.color.a;
                SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, da);
                int dcx = sr.x + sr.w / 2;
                int dcy = sr.y + sr.h / 2;
                SDL_Rect dot = {dcx - 2, dcy - 2, 4, 4};
                SDL_RenderFillRect(renderer, &dot);
            } else {
                SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, tile.color.a);
                SDL_RenderFillRect(renderer, &sr);
            }
        }
    }

    // Ghost overlay of other dimension
    if (blendAlpha > 0.01f) {
        int otherDim = (currentDim == 1) ? 2 : 1;
        for (int y = startY; y <= endY; y++) {
            for (int x = startX; x <= endX; x++) {
                const Tile& tile = getTile(x, y, otherDim);
                if (tile.type == TileType::Empty) continue;
                const Tile& currentTile = getTile(x, y, currentDim);
                if (currentTile.type != TileType::Empty) continue;

                SDL_FRect worldRect = {
                    static_cast<float>(x * m_tileSize),
                    static_cast<float>(y * m_tileSize),
                    static_cast<float>(m_tileSize),
                    static_cast<float>(m_tileSize)
                };
                SDL_Rect sr = camera.worldToScreen(worldRect);

                Uint8 a = static_cast<Uint8>(blendAlpha * 80);
                // Ghost tiles with dashed outline
                SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, static_cast<Uint8>(a * 0.5f));
                SDL_RenderFillRect(renderer, &sr);
                SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, a);
                SDL_RenderDrawRect(renderer, &sr);
            }
        }
    }

    // Render rifts (animated portals)
    Uint32 ticks = SDL_GetTicks();
    for (auto& riftPos : m_riftPositions) {
        SDL_FRect worldRect = {riftPos.x, riftPos.y,
                               static_cast<float>(m_tileSize), static_cast<float>(m_tileSize * 2)};
        SDL_Rect sr = camera.worldToScreen(worldRect);

        renderRift(renderer, sr, ticks);
    }

    // Render exit (animated gateway)
    {
        SDL_FRect worldRect = {m_exitPoint.x, m_exitPoint.y,
                               static_cast<float>(m_tileSize), static_cast<float>(m_tileSize * 2)};
        SDL_Rect sr = camera.worldToScreen(worldRect);
        renderExit(renderer, sr, ticks);
    }
}

void Level::renderSolidTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile,
                             int tx, int ty, int dim) const {
    // Check neighbors for edge-aware rendering
    bool emptyAbove = !isSolid(tx, ty - 1, dim);
    bool emptyBelow = !isSolid(tx, ty + 1, dim);
    bool emptyLeft  = !isSolid(tx - 1, ty, dim);
    bool emptyRight = !isSolid(tx + 1, ty, dim);

    // Base fill
    SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, 255);
    SDL_RenderFillRect(renderer, &sr);

    // Inner shading (darker towards bottom-right)
    Uint8 shadR = tile.color.r > 25 ? tile.color.r - 25 : 0;
    Uint8 shadG = tile.color.g > 25 ? tile.color.g - 25 : 0;
    Uint8 shadB = tile.color.b > 25 ? tile.color.b - 25 : 0;
    SDL_Rect shade = {sr.x + sr.w / 2, sr.y + sr.h / 2, sr.w / 2, sr.h / 2};
    SDL_SetRenderDrawColor(renderer, shadR, shadG, shadB, 60);
    SDL_RenderFillRect(renderer, &shade);

    // Highlight on exposed edges (brighter)
    Uint8 hiR = std::min(255, tile.color.r + 35);
    Uint8 hiG = std::min(255, tile.color.g + 35);
    Uint8 hiB = std::min(255, tile.color.b + 35);

    if (emptyAbove) {
        SDL_SetRenderDrawColor(renderer, hiR, hiG, hiB, 200);
        SDL_Rect top = {sr.x, sr.y, sr.w, 2};
        SDL_RenderFillRect(renderer, &top);
    }
    if (emptyLeft) {
        SDL_SetRenderDrawColor(renderer, hiR, hiG, hiB, 150);
        SDL_Rect left = {sr.x, sr.y, 2, sr.h};
        SDL_RenderFillRect(renderer, &left);
    }

    // Shadow on exposed bottom/right edges
    Uint8 shR = tile.color.r > 40 ? tile.color.r - 40 : 0;
    Uint8 shG = tile.color.g > 40 ? tile.color.g - 40 : 0;
    Uint8 shB = tile.color.b > 40 ? tile.color.b - 40 : 0;

    if (emptyBelow) {
        SDL_SetRenderDrawColor(renderer, shR, shG, shB, 200);
        SDL_Rect bot = {sr.x, sr.y + sr.h - 2, sr.w, 2};
        SDL_RenderFillRect(renderer, &bot);
    }
    if (emptyRight) {
        SDL_SetRenderDrawColor(renderer, shR, shG, shB, 150);
        SDL_Rect right = {sr.x + sr.w - 2, sr.y, 2, sr.h};
        SDL_RenderFillRect(renderer, &right);
    }

    // Subtle grid line (very faint)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 20);
    SDL_RenderDrawRect(renderer, &sr);
}

void Level::renderOneWayTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile) const {
    // Top surface line (thick)
    SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, 255);
    SDL_Rect topBar = {sr.x, sr.y, sr.w, 4};
    SDL_RenderFillRect(renderer, &topBar);

    // Highlight on top
    Uint8 hiR = std::min(255, tile.color.r + 50);
    Uint8 hiG = std::min(255, tile.color.g + 50);
    Uint8 hiB = std::min(255, tile.color.b + 50);
    SDL_SetRenderDrawColor(renderer, hiR, hiG, hiB, 200);
    SDL_Rect highlight = {sr.x, sr.y, sr.w, 2};
    SDL_RenderFillRect(renderer, &highlight);

    // Dashed support lines below
    SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, 80);
    for (int i = 0; i < sr.w; i += 6) {
        SDL_RenderDrawLine(renderer, sr.x + i, sr.y + 4, sr.x + i, sr.y + sr.h);
    }
}

void Level::renderSpikeTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile) const {
    // Draw triangular spikes
    int spikeCount = 3;
    int spikeW = sr.w / spikeCount;

    for (int i = 0; i < spikeCount; i++) {
        int baseX = sr.x + i * spikeW;
        int tipX = baseX + spikeW / 2;
        int tipY = sr.y + 2;
        int baseY = sr.y + sr.h;

        // Spike body (triangle approximation with lines)
        for (int row = 0; row < sr.h - 2; row++) {
            float t = static_cast<float>(row) / (sr.h - 2);
            int halfW = static_cast<int>(spikeW * 0.5f * t);
            int cy = tipY + row;

            Uint8 r = static_cast<Uint8>(tile.color.r * (1.0f - t * 0.3f));
            Uint8 g = static_cast<Uint8>(tile.color.g * (1.0f - t * 0.3f));
            Uint8 b = static_cast<Uint8>(tile.color.b * (1.0f - t * 0.3f));
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_RenderDrawLine(renderer, tipX - halfW, cy, tipX + halfW, cy);
        }

        // Bright tip
        SDL_SetRenderDrawColor(renderer, 255, 220, 200, 255);
        SDL_Rect tip = {tipX - 1, tipY, 2, 3};
        SDL_RenderFillRect(renderer, &tip);
    }
}

void Level::renderRift(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks) const {
    float time = ticks * 0.003f;

    // Outer glow (pulsing)
    float pulse = 0.5f + 0.5f * std::sin(time);
    int glowSize = 4 + static_cast<int>(pulse * 4);
    SDL_Rect glow = {sr.x - glowSize, sr.y - glowSize,
                     sr.w + glowSize * 2, sr.h + glowSize * 2};
    SDL_SetRenderDrawColor(renderer, 120, 40, 200, static_cast<Uint8>(30 + 20 * pulse));
    SDL_RenderFillRect(renderer, &glow);

    // Rift border (animated)
    for (int i = 0; i < 3; i++) {
        float offset = std::sin(time + i * 0.5f) * 2;
        SDL_Rect border = {sr.x + i + static_cast<int>(offset), sr.y + i,
                           sr.w - i * 2, sr.h - i * 2};
        Uint8 r = static_cast<Uint8>(140 + 60 * std::sin(time + i));
        Uint8 b = static_cast<Uint8>(200 + 55 * std::sin(time * 1.3f + i));
        SDL_SetRenderDrawColor(renderer, r, 50, b, static_cast<Uint8>(200 - i * 40));
        SDL_RenderDrawRect(renderer, &border);
    }

    // Inner void (dark center with sparkles)
    SDL_Rect inner = {sr.x + 3, sr.y + 3, sr.w - 6, sr.h - 6};
    SDL_SetRenderDrawColor(renderer, 20, 10, 40, 220);
    SDL_RenderFillRect(renderer, &inner);

    // Animated sparkles inside
    for (int i = 0; i < 6; i++) {
        float sx = std::sin(time * 1.5f + i * 1.1f) * (inner.w / 2 - 2);
        float sy = std::sin(time * 1.2f + i * 1.7f) * (inner.h / 2 - 2);
        int px = inner.x + inner.w / 2 + static_cast<int>(sx);
        int py = inner.y + inner.h / 2 + static_cast<int>(sy);

        Uint8 r = static_cast<Uint8>(150 + 105 * std::sin(time + i * 0.8f));
        Uint8 b = static_cast<Uint8>(200 + 55 * std::sin(time * 0.7f + i));
        SDL_SetRenderDrawColor(renderer, r, 80, b, 220);
        SDL_Rect spark = {px - 1, py - 1, 2, 2};
        SDL_RenderFillRect(renderer, &spark);
    }

    // Energy tendrils from edges
    for (int i = 0; i < 4; i++) {
        float angle = time * 0.8f + i * 1.57f;
        int tx1 = sr.x + sr.w / 2 + static_cast<int>(std::cos(angle) * sr.w * 0.4f);
        int ty1 = sr.y + sr.h / 2 + static_cast<int>(std::sin(angle) * sr.h * 0.4f);
        int tx2 = sr.x + sr.w / 2 + static_cast<int>(std::cos(angle) * (sr.w * 0.6f + pulse * 8));
        int ty2 = sr.y + sr.h / 2 + static_cast<int>(std::sin(angle) * (sr.h * 0.6f + pulse * 8));
        SDL_SetRenderDrawColor(renderer, 180, 100, 255, static_cast<Uint8>(150 * pulse));
        SDL_RenderDrawLine(renderer, tx1, ty1, tx2, ty2);
    }
}

void Level::renderExit(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks) const {
    float time = ticks * 0.002f;
    float pulse = 0.5f + 0.5f * std::sin(time);

    // Outer glow (green)
    int glowSize = 3 + static_cast<int>(pulse * 3);
    SDL_Rect glow = {sr.x - glowSize, sr.y - glowSize,
                     sr.w + glowSize * 2, sr.h + glowSize * 2};
    SDL_SetRenderDrawColor(renderer, 40, 180, 60, static_cast<Uint8>(25 + 15 * pulse));
    SDL_RenderFillRect(renderer, &glow);

    // Gateway frame
    SDL_SetRenderDrawColor(renderer, 60, 200, 80, 230);
    SDL_RenderDrawRect(renderer, &sr);
    SDL_Rect inner1 = {sr.x + 1, sr.y + 1, sr.w - 2, sr.h - 2};
    SDL_SetRenderDrawColor(renderer, 40, 160, 60, 200);
    SDL_RenderDrawRect(renderer, &inner1);

    // Inner portal (dark green with bright sparkles)
    SDL_Rect inner = {sr.x + 3, sr.y + 3, sr.w - 6, sr.h - 6};
    SDL_SetRenderDrawColor(renderer, 15, 40, 20, 200);
    SDL_RenderFillRect(renderer, &inner);

    // Upward particle lines (energy flowing up)
    for (int i = 0; i < 5; i++) {
        float phase = time * 2.0f + i * 1.2f;
        float yOff = std::fmod(phase, 1.0f);
        int px = inner.x + 2 + (i * (inner.w - 4)) / 4;
        int py = inner.y + inner.h - static_cast<int>(yOff * inner.h);
        int len = 3 + static_cast<int>(pulse * 3);

        Uint8 g = static_cast<Uint8>(150 + 105 * std::sin(time + i));
        SDL_SetRenderDrawColor(renderer, 50, g, 80, static_cast<Uint8>(200 * (1.0f - yOff)));
        SDL_Rect particle = {px, py, 2, len};
        SDL_RenderFillRect(renderer, &particle);
    }

    // Corner accents
    int accent = 4;
    SDL_SetRenderDrawColor(renderer, 100, 255, 120, 200);
    SDL_RenderDrawLine(renderer, sr.x, sr.y, sr.x + accent, sr.y);
    SDL_RenderDrawLine(renderer, sr.x, sr.y, sr.x, sr.y + accent);
    SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y, sr.x + sr.w - 1 - accent, sr.y);
    SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y, sr.x + sr.w - 1, sr.y + accent);
    SDL_RenderDrawLine(renderer, sr.x, sr.y + sr.h - 1, sr.x + accent, sr.y + sr.h - 1);
    SDL_RenderDrawLine(renderer, sr.x, sr.y + sr.h - 1, sr.x, sr.y + sr.h - 1 - accent);
    SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y + sr.h - 1, sr.x + sr.w - 1 - accent, sr.y + sr.h - 1);
    SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y + sr.h - 1, sr.x + sr.w - 1, sr.y + sr.h - 1 - accent);
}

void Level::clear() {
    std::fill(m_tilesA.begin(), m_tilesA.end(), Tile{});
    std::fill(m_tilesB.begin(), m_tilesB.end(), Tile{});
    m_riftPositions.clear();
    m_enemySpawns.clear();
}
