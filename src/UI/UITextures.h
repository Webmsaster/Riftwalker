#pragma once
#include <SDL2/SDL.h>
#include <algorithm>
#include "Core/ResourceManager.h"

// 9-slice texture rendering: stretches center while preserving corner/edge detail
inline void renderNineSlice(SDL_Renderer* renderer, SDL_Texture* tex,
                            const SDL_Rect& dst, int border) {
    int tw, th;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    int sb = std::min(border, std::min(tw / 3, th / 3));
    int db = std::min(border, std::min(dst.w / 3, dst.h / 3));

    struct Slice { SDL_Rect src, d; };
    Slice slices[9] = {
        {{0, 0, sb, sb},                     {dst.x, dst.y, db, db}},
        {{sb, 0, tw - sb * 2, sb},           {dst.x + db, dst.y, dst.w - db * 2, db}},
        {{tw - sb, 0, sb, sb},               {dst.x + dst.w - db, dst.y, db, db}},
        {{0, sb, sb, th - sb * 2},           {dst.x, dst.y + db, db, dst.h - db * 2}},
        {{sb, sb, tw - sb * 2, th - sb * 2}, {dst.x + db, dst.y + db, dst.w - db * 2, dst.h - db * 2}},
        {{tw - sb, sb, sb, th - sb * 2},     {dst.x + dst.w - db, dst.y + db, db, dst.h - db * 2}},
        {{0, th - sb, sb, sb},               {dst.x, dst.y + dst.h - db, db, db}},
        {{sb, th - sb, tw - sb * 2, sb},     {dst.x + db, dst.y + dst.h - db, dst.w - db * 2, db}},
        {{tw - sb, th - sb, sb, sb},         {dst.x + dst.w - db, dst.y + dst.h - db, db, db}},
    };
    for (auto& s : slices) {
        if (s.d.w > 0 && s.d.h > 0 && s.src.w > 0 && s.src.h > 0)
            SDL_RenderCopy(renderer, tex, &s.src, &s.d);
    }
}

// Render a dark panel background with 9-slice. Returns true if texture was used.
inline bool renderPanelBg(SDL_Renderer* renderer, const SDL_Rect& dst,
                          Uint8 alpha = 255, const char* texName = "assets/textures/ui/panel_dark.png") {
    SDL_Texture* tex = ResourceManager::instance().getTexture(texName);
    if (!tex) return false;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(tex, alpha);
    renderNineSlice(renderer, tex, dst, 16);
    SDL_SetTextureAlphaMod(tex, 255);
    return true;
}
