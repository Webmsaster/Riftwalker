#include "Level.h"
#include "Core/ResourceManager.h"
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
    if (!inBounds(x, y)) {
        empty = Tile{}; // Reset to prevent stale mutations from previous OOB accesses
        return empty;
    }
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
    m_laserCacheDirty = true;
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

bool Level::isDimensionExclusive(int x, int y, int dimension) const {
    if (!inBounds(x, y)) return false;
    int otherDim = (dimension == 1) ? 2 : 1;
    const auto& thisTile = getTile(x, y, dimension);
    const auto& otherTile = getTile(x, y, otherDim);
    return (thisTile.isSolid() || thisTile.isOneWay()) && otherTile.type == TileType::Empty;
}

bool Level::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

bool Level::loadTileset(const std::string& path) {
    m_tileset = ResourceManager::instance().getTexture(path);
    if (m_tileset) {
        SDL_Log("Tileset loaded: %s", path.c_str());
    }
    return m_tileset != nullptr;
}

int Level::getRiftRequiredDimension(int index) const {
    if (index < 0 || index >= static_cast<int>(m_topology.rifts.size())) {
        return 0;
    }
    int requiredDimension = m_topology.rifts[index].requiredDimension;
    return (requiredDimension == 1 || requiredDimension == 2) ? requiredDimension : 0;
}

bool Level::isRiftActiveInDimension(int index, int dimension) const {
    int requiredDimension = getRiftRequiredDimension(index);
    return requiredDimension == 0 || requiredDimension == dimension;
}

int Level::getAutoTileIndex(int tx, int ty, int dim) const {
    int mask = 0;
    if (isSolid(tx, ty - 1, dim)) mask |= 1;  // top
    if (isSolid(tx + 1, ty, dim)) mask |= 2;  // right
    if (isSolid(tx, ty + 1, dim)) mask |= 4;  // bottom
    if (isSolid(tx - 1, ty, dim)) mask |= 8;  // left
    return mask;
}

void Level::renderTilesetTile(SDL_Renderer* renderer, SDL_Rect destRect,
                               int tilesetRow, int tilesetCol, const Tile& tile) {
    SDL_Rect srcRect = {
        tilesetCol * m_tileSize,
        tilesetRow * m_tileSize,
        m_tileSize, m_tileSize
    };

    // Apply theme color tinting
    SDL_SetTextureColorMod(m_tileset, tile.color.r, tile.color.g, tile.color.b);
    SDL_SetTextureAlphaMod(m_tileset, tile.color.a);
    SDL_SetTextureBlendMode(m_tileset, SDL_BLENDMODE_BLEND);

    SDL_RenderCopy(renderer, m_tileset, &srcRect, &destRect);
}

void Level::render(SDL_Renderer* renderer, const Camera& camera,
                   int currentDim, float blendAlpha) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Vec2 camPos = camera.getPosition();
    int screenW = camera.getViewWidth();
    int screenH = camera.getViewHeight();

    int startX = std::max(0, static_cast<int>((camPos.x - screenW / 2) / m_tileSize) - 1);
    int startY = std::max(0, static_cast<int>((camPos.y - screenH / 2) / m_tileSize) - 1);
    int endX = std::min(m_width - 1, static_cast<int>((camPos.x + screenW / 2) / m_tileSize) + 1);
    int endY = std::min(m_height - 1, static_cast<int>((camPos.y + screenH / 2) / m_tileSize) + 1);

    Uint32 ticks = SDL_GetTicks();

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
            } else if (tile.type == TileType::Fire) {
                renderFireTile(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::Conveyor) {
                renderConveyorTile(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::LaserEmitter) {
                renderLaserEmitter(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::Breakable) {
                // Solid base with crack lines as hint
                SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, tile.color.a);
                SDL_RenderFillRect(renderer, &sr);
                // Subtle crack lines
                Uint8 cr = static_cast<Uint8>(std::min(255, tile.color.r + 40));
                Uint8 cg = static_cast<Uint8>(std::min(255, tile.color.g + 30));
                Uint8 cb = static_cast<Uint8>(std::min(255, tile.color.b + 20));
                SDL_SetRenderDrawColor(renderer, cr, cg, cb, 80);
                // Diagonal cracks
                SDL_RenderDrawLine(renderer, sr.x + 3, sr.y + 2, sr.x + sr.w / 2, sr.y + sr.h / 2);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y + sr.h / 2, sr.x + sr.w - 4, sr.y + sr.h - 3);
                SDL_RenderDrawLine(renderer, sr.x + sr.w - 5, sr.y + 4, sr.x + sr.w / 3, sr.y + sr.h - 5);
                // Subtle shimmer
                float shimmer = 0.3f + 0.2f * std::sin(ticks * 0.004f + x * 2.1f + y * 3.7f);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(15 * shimmer));
                SDL_Rect shimmerRect = {sr.x + 2, sr.y + 2, sr.w - 4, sr.h - 4};
                SDL_RenderFillRect(renderer, &shimmerRect);
            } else if (tile.type == TileType::ShrineBase) {
                // Shrine base: glowing pedestal
                SDL_SetRenderDrawColor(renderer, 60, 40, 80, 200);
                SDL_RenderFillRect(renderer, &sr);
                float glow = 0.5f + 0.5f * std::sin(ticks * 0.003f + x);
                Uint8 ga = static_cast<Uint8>(60 + 40 * glow);
                SDL_SetRenderDrawColor(renderer, 180, 120, 255, ga);
                SDL_Rect inner = {sr.x + 4, sr.y + 4, sr.w - 8, sr.h - 8};
                SDL_RenderFillRect(renderer, &inner);
                SDL_SetRenderDrawColor(renderer, 220, 180, 255, static_cast<Uint8>(30 * glow));
                SDL_RenderDrawRect(renderer, &sr);
            } else if (tile.type == TileType::Ice) {
                renderIceTile(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::GravityWell) {
                renderGravityWell(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::Teleporter) {
                renderTeleporter(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::Crumbling) {
                renderCrumblingTile(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::DimSwitch) {
                renderDimSwitch(renderer, sr, tile, ticks);
            } else if (tile.type == TileType::DimGate) {
                renderDimGate(renderer, sr, tile, ticks);
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

            // Dimension-exclusive indicator: pulsing border on tiles that don't exist in the other dimension
            if (isDimensionExclusive(x, y, currentDim) &&
                (tile.isSolid() || tile.isOneWay())) {
                float pulse = 0.5f + 0.5f * std::sin(ticks * 0.004f + x * 1.3f + y * 2.1f);
                Uint8 dimAlpha = static_cast<Uint8>(40 + 50 * pulse);
                // Color matches current dimension (blue=dim1, red=dim2)
                if (currentDim == 1) {
                    SDL_SetRenderDrawColor(renderer, 100, 160, 255, dimAlpha);
                } else {
                    SDL_SetRenderDrawColor(renderer, 255, 100, 100, dimAlpha);
                }
                SDL_RenderDrawRect(renderer, &sr);
                // Inner glow line
                SDL_Rect innerGlow = {sr.x + 1, sr.y + 1, sr.w - 2, sr.h - 2};
                Uint8 innerAlpha = static_cast<Uint8>(20 + 25 * pulse);
                if (currentDim == 1) {
                    SDL_SetRenderDrawColor(renderer, 100, 160, 255, innerAlpha);
                } else {
                    SDL_SetRenderDrawColor(renderer, 255, 100, 100, innerAlpha);
                }
                SDL_RenderDrawRect(renderer, &innerGlow);
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

    // Render laser beams (after tiles, before rifts)
    renderLaserBeams(renderer, camera, currentDim, ticks);

    // Render rifts (animated portals)
    for (int i = 0; i < static_cast<int>(m_riftPositions.size()); i++) {
        const auto& riftPos = m_riftPositions[i];
        SDL_FRect worldRect = {riftPos.x, riftPos.y,
                               static_cast<float>(m_tileSize), static_cast<float>(m_tileSize * 2)};
        SDL_Rect sr = camera.worldToScreen(worldRect);

        int requiredDimension = getRiftRequiredDimension(i);
        renderRift(renderer, sr, ticks, requiredDimension,
                   isRiftActiveInDimension(i, currentDim));
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
    // Tileset rendering: use auto-tile index for correct variant
    if (m_tileset) {
        int autoIdx = getAutoTileIndex(tx, ty, dim);
        // Row 0 of tileset, column = auto-tile index (0-15)
        SDL_Rect srcRect = { autoIdx * m_tileSize, 0, m_tileSize, m_tileSize };
        SDL_SetTextureColorMod(m_tileset, tile.color.r, tile.color.g, tile.color.b);
        SDL_SetTextureAlphaMod(m_tileset, tile.color.a);
        SDL_SetTextureBlendMode(m_tileset, SDL_BLENDMODE_BLEND);
        SDL_RenderCopy(renderer, m_tileset, &srcRect, &sr);
        return;
    }

    // Procedural fallback: Check neighbors for edge-aware rendering
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

void Level::renderRift(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks,
                       int requiredDimension, bool activeInCurrentDimension) const {
    float time = ticks * 0.003f;
    SDL_Color accent = (requiredDimension == 2)
        ? SDL_Color{255, 90, 145, 255}
        : SDL_Color{90, 180, 255, 255};
    SDL_Color core = (requiredDimension == 2)
        ? SDL_Color{70, 12, 30, 220}
        : SDL_Color{12, 24, 48, 220};
    float visibility = activeInCurrentDimension ? 1.0f : 0.45f;

    // Outer glow (pulsing)
    float pulse = 0.5f + 0.5f * std::sin(time);
    int glowSize = 4 + static_cast<int>(pulse * 4);
    SDL_Rect glow = {sr.x - glowSize, sr.y - glowSize,
                     sr.w + glowSize * 2, sr.h + glowSize * 2};
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b,
                           static_cast<Uint8>((30 + 20 * pulse) * visibility));
    SDL_RenderFillRect(renderer, &glow);

    // Rift border (animated)
    for (int i = 0; i < 3; i++) {
        float offset = std::sin(time + i * 0.5f) * 2;
        SDL_Rect border = {sr.x + i + static_cast<int>(offset), sr.y + i,
                           sr.w - i * 2, sr.h - i * 2};
        Uint8 r = static_cast<Uint8>(std::clamp(
            static_cast<int>(accent.r + 35 * std::sin(time + i)), 0, 255));
        Uint8 g = static_cast<Uint8>(std::clamp(
            static_cast<int>(accent.g * 0.55f + 20 * std::sin(time * 0.9f + i)), 0, 255));
        Uint8 b = static_cast<Uint8>(std::clamp(
            static_cast<int>(accent.b + 35 * std::sin(time * 1.3f + i)), 0, 255));
        SDL_SetRenderDrawColor(renderer, r, g, b,
                               static_cast<Uint8>((200 - i * 40) * visibility));
        SDL_RenderDrawRect(renderer, &border);
    }

    // Inner void (dark center with sparkles)
    SDL_Rect inner = {sr.x + 3, sr.y + 3, sr.w - 6, sr.h - 6};
    SDL_SetRenderDrawColor(renderer, core.r, core.g, core.b,
                           static_cast<Uint8>(core.a * visibility));
    SDL_RenderFillRect(renderer, &inner);

    // Animated sparkles inside
    for (int i = 0; i < 6; i++) {
        float sx = std::sin(time * 1.5f + i * 1.1f) * (inner.w / 2 - 2);
        float sy = std::sin(time * 1.2f + i * 1.7f) * (inner.h / 2 - 2);
        int px = inner.x + inner.w / 2 + static_cast<int>(sx);
        int py = inner.y + inner.h / 2 + static_cast<int>(sy);

        Uint8 r = static_cast<Uint8>(std::clamp(
            static_cast<int>(accent.r + 55 * std::sin(time + i * 0.8f)), 0, 255));
        Uint8 g = static_cast<Uint8>(std::clamp(
            static_cast<int>(accent.g * 0.6f + 25 * std::sin(time * 1.1f + i)), 0, 255));
        Uint8 b = static_cast<Uint8>(std::clamp(
            static_cast<int>(accent.b + 40 * std::sin(time * 0.7f + i)), 0, 255));
        SDL_SetRenderDrawColor(renderer, r, g, b,
                               static_cast<Uint8>(220 * visibility));
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
        SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b,
                               static_cast<Uint8>(150 * pulse * visibility));
        SDL_RenderDrawLine(renderer, tx1, ty1, tx2, ty2);
    }

    if (!activeInCurrentDimension) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                               static_cast<Uint8>(40 + 20 * pulse));
        SDL_Rect ghost = {sr.x + 1, sr.y + 1, sr.w - 2, sr.h - 2};
        SDL_RenderDrawRect(renderer, &ghost);
    }
}

void Level::renderExit(SDL_Renderer* renderer, SDL_Rect sr, Uint32 ticks) const {
    float time = ticks * 0.002f;
    float pulse = 0.5f + 0.5f * std::sin(time);

    if (m_exitActive) {
        // ACTIVE EXIT: bright green, energetic particles

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

        // Corner accents (bright green)
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
    } else {
        // LOCKED EXIT: dim red/gray, slow pulse, no particles

        // Dim outer glow (red-gray)
        float slowPulse = 0.5f + 0.5f * std::sin(time * 0.5f);
        int glowSize = 2 + static_cast<int>(slowPulse * 2);
        SDL_Rect glow = {sr.x - glowSize, sr.y - glowSize,
                         sr.w + glowSize * 2, sr.h + glowSize * 2};
        SDL_SetRenderDrawColor(renderer, 80, 30, 30, static_cast<Uint8>(15 + 10 * slowPulse));
        SDL_RenderFillRect(renderer, &glow);

        // Gateway frame (dim gray-red)
        SDL_SetRenderDrawColor(renderer, 100, 60, 60, 180);
        SDL_RenderDrawRect(renderer, &sr);
        SDL_Rect inner1 = {sr.x + 1, sr.y + 1, sr.w - 2, sr.h - 2};
        SDL_SetRenderDrawColor(renderer, 80, 40, 40, 150);
        SDL_RenderDrawRect(renderer, &inner1);

        // Inner portal (dark, locked look)
        SDL_Rect inner = {sr.x + 3, sr.y + 3, sr.w - 6, sr.h - 6};
        SDL_SetRenderDrawColor(renderer, 20, 15, 15, 200);
        SDL_RenderFillRect(renderer, &inner);

        // Lock symbol: horizontal bar across middle
        int barY = inner.y + inner.h / 2 - 2;
        SDL_Rect lockBar = {inner.x + 2, barY, inner.w - 4, 4};
        SDL_SetRenderDrawColor(renderer, 120, 60, 60, static_cast<Uint8>(150 + 50 * slowPulse));
        SDL_RenderFillRect(renderer, &lockBar);

        // Corner accents (dim red)
        int accent = 4;
        SDL_SetRenderDrawColor(renderer, 120, 50, 50, 150);
        SDL_RenderDrawLine(renderer, sr.x, sr.y, sr.x + accent, sr.y);
        SDL_RenderDrawLine(renderer, sr.x, sr.y, sr.x, sr.y + accent);
        SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y, sr.x + sr.w - 1 - accent, sr.y);
        SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y, sr.x + sr.w - 1, sr.y + accent);
        SDL_RenderDrawLine(renderer, sr.x, sr.y + sr.h - 1, sr.x + accent, sr.y + sr.h - 1);
        SDL_RenderDrawLine(renderer, sr.x, sr.y + sr.h - 1, sr.x, sr.y + sr.h - 1 - accent);
        SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y + sr.h - 1, sr.x + sr.w - 1 - accent, sr.y + sr.h - 1);
        SDL_RenderDrawLine(renderer, sr.x + sr.w - 1, sr.y + sr.h - 1, sr.x + sr.w - 1, sr.y + sr.h - 1 - accent);
    }
}

void Level::renderFireTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    float time = ticks * 0.005f;

    // Base lava/fire glow
    SDL_SetRenderDrawColor(renderer, tile.color.r / 2, tile.color.g / 4, 0, 255);
    SDL_RenderFillRect(renderer, &sr);

    // Animated flame tongues
    for (int i = 0; i < 4; i++) {
        float phase = time + i * 1.5f;
        float flicker = 0.4f + 0.6f * std::abs(std::sin(phase));
        int flameH = 4 + static_cast<int>(flicker * (sr.h / 2));
        int flameX = sr.x + (i * sr.w) / 4 + sr.w / 8;
        int flameW = sr.w / 5;

        Uint8 r = static_cast<Uint8>(tile.color.r * flicker);
        Uint8 g = static_cast<Uint8>(tile.color.g * flicker * 0.7f);
        SDL_SetRenderDrawColor(renderer, r, g, 0, static_cast<Uint8>(200 * flicker));
        SDL_Rect flame = {flameX, sr.y + sr.h - flameH, flameW, flameH};
        SDL_RenderFillRect(renderer, &flame);

        // Bright tip
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, static_cast<Uint8>(180 * flicker));
        SDL_Rect tip = {flameX + 1, sr.y + sr.h - flameH, flameW - 2, 3};
        SDL_RenderFillRect(renderer, &tip);
    }

    // Ember particles
    for (int i = 0; i < 3; i++) {
        float px = std::sin(time * 1.3f + i * 2.1f) * sr.w * 0.3f;
        float py = std::fmod(time * 0.8f + i * 1.3f, 1.0f);
        int ex = sr.x + sr.w / 2 + static_cast<int>(px);
        int ey = sr.y + sr.h - static_cast<int>(py * sr.h);
        Uint8 ea = static_cast<Uint8>((1.0f - py) * 200);
        SDL_SetRenderDrawColor(renderer, 255, 200, 50, ea);
        SDL_Rect ember = {ex, ey, 2, 2};
        SDL_RenderFillRect(renderer, &ember);
    }
}

void Level::renderConveyorTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    // Base belt
    SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, 255);
    SDL_RenderFillRect(renderer, &sr);

    // Top surface
    Uint8 hiR = std::min(255, tile.color.r + 30);
    Uint8 hiG = std::min(255, tile.color.g + 30);
    Uint8 hiB = std::min(255, tile.color.b + 30);
    SDL_SetRenderDrawColor(renderer, hiR, hiG, hiB, 255);
    SDL_Rect top = {sr.x, sr.y, sr.w, 3};
    SDL_RenderFillRect(renderer, &top);

    // Animated arrows showing direction
    float time = ticks * 0.003f;
    bool goRight = (tile.variant == 0);
    int arrowCount = 3;
    SDL_SetRenderDrawColor(renderer, 200, 200, 60, 180);

    for (int i = 0; i < arrowCount; i++) {
        float phase = std::fmod(time + i * 0.33f, 1.0f);
        if (!goRight) phase = 1.0f - phase;
        int ax = sr.x + static_cast<int>(phase * sr.w);
        int ay = sr.y + sr.h / 2;
        // Arrow chevron
        if (goRight) {
            SDL_RenderDrawLine(renderer, ax - 3, ay - 3, ax, ay);
            SDL_RenderDrawLine(renderer, ax, ay, ax - 3, ay + 3);
        } else {
            SDL_RenderDrawLine(renderer, ax + 3, ay - 3, ax, ay);
            SDL_RenderDrawLine(renderer, ax, ay, ax + 3, ay + 3);
        }
    }
}

void Level::renderLaserEmitter(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    // Dark metallic block
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_RenderFillRect(renderer, &sr);

    // Emitter lens (glowing center)
    float time = ticks * 0.004f;
    // Laser toggles: 2s on, 1s off
    float cycle = std::fmod(time, 3.0f);
    bool active = cycle < 2.0f;

    Uint8 glow = active ? 255 : 80;
    SDL_SetRenderDrawColor(renderer, glow, 20, 20, 255);
    SDL_Rect lens = {sr.x + sr.w / 2 - 3, sr.y + sr.h / 2 - 3, 6, 6};
    SDL_RenderFillRect(renderer, &lens);

    // Metallic border highlight
    SDL_SetRenderDrawColor(renderer, 80, 80, 90, 200);
    SDL_RenderDrawRect(renderer, &sr);
}

void Level::buildLaserCache() const {
    m_laserEmitters.clear();
    for (int d = 1; d <= 2; d++) {
        const auto& tiles = (d == 2) ? m_tilesB : m_tilesA;
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                if (tiles[y * m_width + x].type == TileType::LaserEmitter) {
                    m_laserEmitters.push_back({x, y, d});
                }
            }
        }
    }
    m_laserCacheDirty = false;
}

void Level::renderLaserBeams(SDL_Renderer* renderer, const Camera& camera, int dim, Uint32 ticks) const {
    float time = ticks * 0.004f;
    // Laser toggles: 2s on, 1s off
    float cycle = std::fmod(time, 3.0f);
    bool active = cycle < 2.0f;
    if (!active) return;

    if (m_laserCacheDirty) buildLaserCache();

    float pulse = 0.7f + 0.3f * std::sin(time * 8.0f);

    for (auto& ep : m_laserEmitters) {
        if (ep.dim != dim) continue;
        int x = ep.x, y = ep.y;
        const Tile& tile = getTile(x, y, dim);

        int dx = 0, dy = 0;
        if (tile.variant == 0) dx = 1;
        else if (tile.variant == 1) dx = -1;
        else if (tile.variant == 2) dy = 1;
        else dy = -1;

        int bx = x + dx, by = y + dy;
        while (inBounds(bx, by)) {
            const Tile& bt = getTile(bx, by, dim);
            if (bt.isSolid()) break;

            SDL_FRect worldRect = {
                static_cast<float>(bx * m_tileSize),
                static_cast<float>(by * m_tileSize),
                static_cast<float>(m_tileSize),
                static_cast<float>(m_tileSize)
            };
            SDL_Rect sr = camera.worldToScreen(worldRect);

            Uint8 a = static_cast<Uint8>(200 * pulse);
            if (dx != 0) {
                SDL_SetRenderDrawColor(renderer, 255, 30, 30, a);
                SDL_Rect beam = {sr.x, sr.y + sr.h / 2 - 1, sr.w, 3};
                SDL_RenderFillRect(renderer, &beam);
                SDL_SetRenderDrawColor(renderer, 255, 80, 80, static_cast<Uint8>(60 * pulse));
                SDL_Rect glow = {sr.x, sr.y + sr.h / 2 - 4, sr.w, 9};
                SDL_RenderFillRect(renderer, &glow);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 30, 30, a);
                SDL_Rect beam = {sr.x + sr.w / 2 - 1, sr.y, 3, sr.h};
                SDL_RenderFillRect(renderer, &beam);
                SDL_SetRenderDrawColor(renderer, 255, 80, 80, static_cast<Uint8>(60 * pulse));
                SDL_Rect glow = {sr.x + sr.w / 2 - 4, sr.y, 9, sr.h};
                SDL_RenderFillRect(renderer, &glow);
            }

            bx += dx;
            by += dy;
        }
    }
}

bool Level::isInLaserBeam(float worldX, float worldY, int dimension) const {
    float time = SDL_GetTicks() * 0.004f;
    float cycle = std::fmod(time, 3.0f);
    if (cycle >= 2.0f) return false; // laser off

    if (m_laserCacheDirty) buildLaserCache();

    int playerTX = static_cast<int>(worldX) / m_tileSize;
    int playerTY = static_cast<int>(worldY) / m_tileSize;

    for (auto& ep : m_laserEmitters) {
        if (dimension != 0 && ep.dim != dimension) continue;
        int x = ep.x, y = ep.y;
        const Tile& tile = getTile(x, y, ep.dim);

        int dx = 0, dy = 0;
        if (tile.variant == 0) dx = 1;
        else if (tile.variant == 1) dx = -1;
        else if (tile.variant == 2) dy = 1;
        else dy = -1;

        int bx = x + dx, by = y + dy;
        while (inBounds(bx, by)) {
            const Tile& bt = getTile(bx, by, dimension);
            if (bt.isSolid()) break;
            if (bx == playerTX && by == playerTY) return true;
            bx += dx;
            by += dy;
        }
    }
    return false;
}

bool Level::isOnConveyor(int tileX, int tileY, int dimension, int& direction) const {
    if (!inBounds(tileX, tileY)) return false;
    const Tile& tile = getTile(tileX, tileY, dimension);
    if (tile.type != TileType::Conveyor) return false;
    direction = (tile.variant == 0) ? 1 : -1; // 1=right, -1=left
    return true;
}

void Level::clear() {
    std::fill(m_tilesA.begin(), m_tilesA.end(), Tile{});
    std::fill(m_tilesB.begin(), m_tilesB.end(), Tile{});
    m_riftPositions.clear();
    m_topology = {};
    m_enemySpawns.clear();
    m_secretRooms.clear();
    m_randomEvents.clear();
    m_npcs.clear();
    m_laserCacheDirty = true;
}

bool Level::isIceTile(int tileX, int tileY, int dimension) const {
    if (!inBounds(tileX, tileY)) return false;
    return getTile(tileX, tileY, dimension).type == TileType::Ice;
}

bool Level::isGravityWell(int tileX, int tileY, int dimension) const {
    if (!inBounds(tileX, tileY)) return false;
    return getTile(tileX, tileY, dimension).type == TileType::GravityWell;
}

Vec2 Level::getTeleporterDestination(int tileX, int tileY, int dimension) const {
    if (!inBounds(tileX, tileY)) return {0, 0};
    const Tile& src = getTile(tileX, tileY, dimension);
    if (src.type != TileType::Teleporter) return {0, 0};
    int pairID = src.variant;

    // Find matching teleporter with same pair ID
    auto& tiles = (dimension == 2) ? m_tilesB : m_tilesA;
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            if (x == tileX && y == tileY) continue;
            const Tile& t = tiles[y * m_width + x];
            if (t.type == TileType::Teleporter && t.variant == pairID) {
                return {static_cast<float>(x * m_tileSize + m_tileSize / 2),
                        static_cast<float>(y * m_tileSize)};
            }
        }
    }
    return {0, 0};
}

void Level::triggerCrumble(int tileX, int tileY, int dimension) {
    if (!inBounds(tileX, tileY)) return;
    Tile& tile = getTile(tileX, tileY, dimension);
    if (tile.type != TileType::Crumbling || tile.crumbling || tile.crumbled) return;
    tile.crumbling = true;
    tile.crumbleTimer = 1.5f;
}

void Level::updateTiles(float dt) {
    auto updateTileVec = [&](std::vector<Tile>& tiles) {
        for (auto& tile : tiles) {
            if (tile.type != TileType::Crumbling) continue;
            if (tile.crumbling && !tile.crumbled) {
                tile.crumbleTimer -= dt;
                if (tile.crumbleTimer <= 0) {
                    tile.crumbled = true;
                    tile.crumbling = false;
                    tile.respawnTimer = 5.0f;
                }
            }
            if (tile.crumbled) {
                tile.respawnTimer -= dt;
                if (tile.respawnTimer <= 0) {
                    tile.crumbled = false;
                    tile.crumbleTimer = 0;
                }
            }
        }
    };
    updateTileVec(m_tilesA);
    updateTileVec(m_tilesB);
}

void Level::renderIceTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    // Icy blue surface
    SDL_SetRenderDrawColor(renderer, 140, 200, 240, 220);
    SDL_RenderFillRect(renderer, &sr);
    // Frost pattern
    float shimmer = 0.5f + 0.5f * std::sin(ticks * 0.003f);
    SDL_SetRenderDrawColor(renderer, 200, 230, 255, static_cast<Uint8>(60 + 40 * shimmer));
    SDL_Rect frost = {sr.x + 2, sr.y + 2, sr.w - 4, sr.h / 3};
    SDL_RenderFillRect(renderer, &frost);
    // Highlight line at top
    SDL_SetRenderDrawColor(renderer, 220, 240, 255, 150);
    SDL_RenderDrawLine(renderer, sr.x + 2, sr.y + 1, sr.x + sr.w - 2, sr.y + 1);
}

void Level::renderGravityWell(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    // Dark purple background
    SDL_SetRenderDrawColor(renderer, 40, 20, 60, 200);
    SDL_RenderFillRect(renderer, &sr);
    // Swirling rings
    int cx = sr.x + sr.w / 2;
    int cy = sr.y + sr.h / 2;
    float time = ticks * 0.004f;
    for (int ring = 0; ring < 3; ring++) {
        float r = 4.0f + ring * 4.0f + std::sin(time + ring) * 2.0f;
        Uint8 ra = static_cast<Uint8>(120 - ring * 30);
        for (int a = 0; a < 12; a++) {
            float angle = time * (1.0f + ring * 0.3f) + a * (6.283185f / 12);
            int px = cx + static_cast<int>(std::cos(angle) * r);
            int py = cy + static_cast<int>(std::sin(angle) * r);
            SDL_SetRenderDrawColor(renderer, 160, 80, 220, ra);
            SDL_Rect dot = {px, py, 2, 2};
            SDL_RenderFillRect(renderer, &dot);
        }
    }
}

void Level::renderTeleporter(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    // Glowing portal
    float time = ticks * 0.005f;
    float pulse = 0.5f + 0.5f * std::sin(time);
    // Base
    SDL_SetRenderDrawColor(renderer, 30, 60, 30, 200);
    SDL_RenderFillRect(renderer, &sr);
    // Inner glow
    Uint8 ga = static_cast<Uint8>(100 + 100 * pulse);
    SDL_Rect inner = {sr.x + 4, sr.y + 4, sr.w - 8, sr.h - 8};
    SDL_SetRenderDrawColor(renderer, 50, 220, 100, ga);
    SDL_RenderFillRect(renderer, &inner);
    // Center bright
    SDL_Rect center = {sr.x + sr.w / 2 - 4, sr.y + sr.h / 2 - 4, 8, 8};
    SDL_SetRenderDrawColor(renderer, 100, 255, 150, static_cast<Uint8>(180 + 60 * pulse));
    SDL_RenderFillRect(renderer, &center);
    // Border
    SDL_SetRenderDrawColor(renderer, 80, 255, 120, static_cast<Uint8>(120 * pulse));
    SDL_RenderDrawRect(renderer, &sr);
}

void Level::renderCrumblingTile(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    if (tile.crumbled) return; // Don't render broken tiles

    // Base color with crumble shake
    int offX = 0, offY = 0;
    Uint8 alpha = 220;
    if (tile.crumbling) {
        float shake = (1.0f - tile.crumbleTimer / 1.5f);
        offX = static_cast<int>((std::rand() % 3 - 1) * shake * 2);
        offY = static_cast<int>((std::rand() % 3 - 1) * shake * 2);
        alpha = static_cast<Uint8>(220 * (tile.crumbleTimer / 1.5f));
    }
    SDL_Rect shaken = {sr.x + offX, sr.y + offY, sr.w, sr.h};
    SDL_SetRenderDrawColor(renderer, tile.color.r, tile.color.g, tile.color.b, alpha);
    SDL_RenderFillRect(renderer, &shaken);
    // Crack pattern when crumbling
    if (tile.crumbling) {
        float t = 1.0f - tile.crumbleTimer / 1.5f;
        Uint8 ca = static_cast<Uint8>(200 * t);
        SDL_SetRenderDrawColor(renderer, 255, 200, 100, ca);
        SDL_RenderDrawLine(renderer, shaken.x + 3, shaken.y + 3,
                           shaken.x + shaken.w - 3, shaken.y + shaken.h - 3);
        SDL_RenderDrawLine(renderer, shaken.x + shaken.w - 3, shaken.y + 3,
                           shaken.x + 3, shaken.y + shaken.h - 3);
    }
}

void Level::renderDimSwitch(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    bool activated = tile.crumbled; // reuse crumbled as "activated" flag
    float pulse = 0.5f + 0.5f * std::sin(ticks * 0.005f);

    // Base plate
    if (activated) {
        SDL_SetRenderDrawColor(renderer, 60, 180, 60, 200);
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 80, 120, 200);
    }
    SDL_Rect plate = {sr.x + 2, sr.y + sr.h / 2, sr.w - 4, sr.h / 2};
    SDL_RenderFillRect(renderer, &plate);

    // Diamond symbol on top
    int cx = sr.x + sr.w / 2;
    int cy = sr.y + sr.h / 4;
    Uint8 symAlpha = activated ? static_cast<Uint8>(200) : static_cast<Uint8>(100 + 80 * pulse);
    Uint8 symR = activated ? 100 : 180;
    Uint8 symG = activated ? 255 : static_cast<Uint8>(120 + 60 * pulse);
    Uint8 symB = activated ? 100 : 220;
    SDL_SetRenderDrawColor(renderer, symR, symG, symB, symAlpha);
    SDL_RenderDrawLine(renderer, cx, cy - 6, cx + 6, cy);
    SDL_RenderDrawLine(renderer, cx + 6, cy, cx, cy + 6);
    SDL_RenderDrawLine(renderer, cx, cy + 6, cx - 6, cy);
    SDL_RenderDrawLine(renderer, cx - 6, cy, cx, cy - 6);

    // Pair ID indicator dot
    SDL_Rect idDot = {sr.x + sr.w - 6, sr.y + 2, 4, 4};
    Uint8 idHue = static_cast<Uint8>((tile.variant * 67) % 256);
    SDL_SetRenderDrawColor(renderer, idHue, 255 - idHue, 150, 200);
    SDL_RenderFillRect(renderer, &idDot);
}

void Level::renderDimGate(SDL_Renderer* renderer, SDL_Rect sr, const Tile& tile, Uint32 ticks) const {
    bool open = tile.crumbled;
    if (open) {
        // Open gate: faint outline only
        SDL_SetRenderDrawColor(renderer, 80, 200, 80, 60);
        SDL_RenderDrawRect(renderer, &sr);
        return;
    }

    float pulse = 0.5f + 0.5f * std::sin(ticks * 0.003f);

    // Solid barrier with energy pattern
    SDL_SetRenderDrawColor(renderer, 100, 50, 50, 220);
    SDL_RenderFillRect(renderer, &sr);

    // Horizontal energy lines
    Uint8 lineAlpha = static_cast<Uint8>(120 + 80 * pulse);
    SDL_SetRenderDrawColor(renderer, 255, 80, 80, lineAlpha);
    for (int ly = sr.y + 4; ly < sr.y + sr.h; ly += 6) {
        SDL_RenderDrawLine(renderer, sr.x + 2, ly, sr.x + sr.w - 2, ly);
    }

    // Lock symbol in center
    int cx = sr.x + sr.w / 2;
    int cy = sr.y + sr.h / 2;
    SDL_SetRenderDrawColor(renderer, 255, 200, 60, static_cast<Uint8>(180 + 50 * pulse));
    SDL_Rect lockBody = {cx - 4, cy - 2, 8, 6};
    SDL_RenderFillRect(renderer, &lockBody);
    SDL_RenderDrawLine(renderer, cx - 3, cy - 2, cx - 3, cy - 5);
    SDL_RenderDrawLine(renderer, cx + 3, cy - 2, cx + 3, cy - 5);
    SDL_RenderDrawLine(renderer, cx - 3, cy - 5, cx + 3, cy - 5);

    // Pair ID indicator
    Uint8 idHue = static_cast<Uint8>((tile.variant * 67) % 256);
    SDL_SetRenderDrawColor(renderer, idHue, 255 - idHue, 150, 200);
    SDL_Rect idDot = {sr.x + sr.w - 6, sr.y + 2, 4, 4};
    SDL_RenderFillRect(renderer, &idDot);

    // Border
    SDL_SetRenderDrawColor(renderer, 200, 60, 60, static_cast<Uint8>(150 + 60 * pulse));
    SDL_RenderDrawRect(renderer, &sr);
}

bool Level::isDimSwitchAt(int tileX, int tileY, int dimension) const {
    if (!inBounds(tileX, tileY)) return false;
    const Tile& tile = getTile(tileX, tileY, dimension);
    return tile.type == TileType::DimSwitch && !tile.crumbled;
}

bool Level::activateDimSwitch(int pairId, int switchDim) {
    // Mark the switch as activated
    int gateDim = (switchDim == 1) ? 2 : 1;
    auto& gateTiles = (gateDim == 1) ? m_tilesA : m_tilesB;
    auto& switchTiles = (switchDim == 1) ? m_tilesA : m_tilesB;

    bool activated = false;

    // Activate matching switch tiles
    for (auto& tile : switchTiles) {
        if (tile.type == TileType::DimSwitch && tile.variant == pairId && !tile.crumbled) {
            tile.crumbled = true;
            activated = true;
        }
    }

    // Open matching gate tiles
    for (auto& tile : gateTiles) {
        if (tile.type == TileType::DimGate && tile.variant == pairId && !tile.crumbled) {
            tile.crumbled = true;
        }
    }

    return activated;
}
