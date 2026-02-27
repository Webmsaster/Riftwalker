#include "BestiaryState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Game/Bestiary.h"
#include "Components/AIComponent.h"
#include <cmath>
#include <cstdio>

void BestiaryState::enter() {
    m_selected = 0;
    m_time = 0;
    m_scrollOffset = 0;
    // Count: regular enemies + bosses
    m_totalEntries = static_cast<int>(EnemyType::COUNT) + 4; // 4 bosses
}

void BestiaryState::handleEvent(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) return;

    switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
            m_selected = (m_selected - 1 + m_totalEntries) % m_totalEntries;
            AudioManager::instance().play(SFX::MenuSelect);
            break;
        case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
            m_selected = (m_selected + 1) % m_totalEntries;
            AudioManager::instance().play(SFX::MenuSelect);
            break;
        case SDL_SCANCODE_ESCAPE:
            AudioManager::instance().play(SFX::MenuConfirm);
            game->changeState(StateID::Menu);
            break;
        default: break;
    }

    // Scroll to keep selected in view
    int visibleRows = 8;
    if (m_selected < m_scrollOffset) m_scrollOffset = m_selected;
    if (m_selected >= m_scrollOffset + visibleRows) m_scrollOffset = m_selected - visibleRows + 1;
}

void BestiaryState::update(float dt) {
    m_time += dt;
}

void BestiaryState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 10, 8, 20, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Grid
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 20);
    for (int x = 0; x < 1280; x += 80) SDL_RenderDrawLine(renderer, x, 0, x, 720);
    for (int y = 0; y < 720; y += 80) SDL_RenderDrawLine(renderer, 0, y, 1280, y);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Title
    {
        SDL_Color c = {180, 100, 100, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "B E S T I A R Y", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int tw = static_cast<int>(s->w * 1.5f);
                int th = static_cast<int>(s->h * 1.5f);
                SDL_Rect r = {640 - tw / 2, 30, tw, th};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Discovery counter
    {
        char countText[32];
        std::snprintf(countText, sizeof(countText), "Discovered: %d / %d",
                      Bestiary::getDiscoveredCount(), Bestiary::getTotalCount());
        SDL_Surface* cs = TTF_RenderText_Blended(font, countText, SDL_Color{120, 110, 150, 200});
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_Rect cr = {640 - cs->w / 2, 65, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }

    // Entry list (left side)
    int listX = 30;
    int listY = 95;
    int rowH = 40;
    int listW = 350;
    int visibleRows = 8;
    int regularCount = static_cast<int>(EnemyType::COUNT);

    for (int vi = 0; vi < visibleRows && (vi + m_scrollOffset) < m_totalEntries; vi++) {
        int idx = vi + m_scrollOffset;
        bool isBoss = (idx >= regularCount);
        int typeIdx = isBoss ? (idx - regularCount) : idx;

        const BestiaryEntry& entry = isBoss ?
            Bestiary::getBossEntry(typeIdx) :
            Bestiary::getEntry(static_cast<EnemyType>(typeIdx));

        bool selected = (idx == m_selected);
        int ey = listY + vi * (rowH + 4);

        // Row background
        Uint8 bgA = selected ? 80 : 25;
        SDL_SetRenderDrawColor(renderer, 25, 20, 40, bgA);
        SDL_Rect rowBg = {listX, ey, listW, rowH};
        SDL_RenderFillRect(renderer, &rowBg);

        if (selected) {
            float pulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
            Uint8 borderA = static_cast<Uint8>(150 * pulse);
            SDL_Color bc = isBoss ? SDL_Color{255, 140, 40, borderA} : SDL_Color{180, 100, 100, borderA};
            SDL_SetRenderDrawColor(renderer, bc.r, bc.g, bc.b, borderA);
            SDL_RenderDrawRect(renderer, &rowBg);
        }

        const char* displayName = entry.discovered ? entry.name : "???";
        SDL_Color nameColor = entry.discovered ?
            (isBoss ? SDL_Color{255, 180, 80, 255} : SDL_Color{200, 180, 200, 255}) :
            SDL_Color{80, 75, 90, 150};
        if (selected) {
            nameColor.r = static_cast<Uint8>(std::min(255, nameColor.r + 30));
            nameColor.a = 255;
        }

        SDL_Surface* ns = TTF_RenderText_Blended(font, displayName, nameColor);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {listX + 10, ey + 3, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Kill count
        if (entry.discovered) {
            char killText[32];
            std::snprintf(killText, sizeof(killText), "Kills: %d", entry.killCount);
            SDL_Surface* ks = TTF_RenderText_Blended(font, killText, SDL_Color{100, 95, 120, 150});
            if (ks) {
                SDL_Texture* kt = SDL_CreateTextureFromSurface(renderer, ks);
                if (kt) {
                    SDL_Rect kr = {listX + 10, ey + 21, ks->w, ks->h};
                    SDL_RenderCopy(renderer, kt, nullptr, &kr);
                    SDL_DestroyTexture(kt);
                }
                SDL_FreeSurface(ks);
            }
        }
    }

    // Detail panel (right side)
    int detailX = 420;
    int detailY = 95;
    int detailW = 830;
    int detailH = 550;

    SDL_SetRenderDrawColor(renderer, 18, 14, 30, 80);
    SDL_Rect detailBg = {detailX, detailY, detailW, detailH};
    SDL_RenderFillRect(renderer, &detailBg);
    SDL_SetRenderDrawColor(renderer, 80, 70, 100, 60);
    SDL_RenderDrawRect(renderer, &detailBg);

    bool isBoss = (m_selected >= regularCount);
    int typeIdx = isBoss ? (m_selected - regularCount) : m_selected;
    const BestiaryEntry& entry = isBoss ?
        Bestiary::getBossEntry(typeIdx) : Bestiary::getEntry(static_cast<EnemyType>(typeIdx));

    if (entry.discovered) {
        // Preview
        renderEnemyPreview(renderer, detailX + detailW / 2, detailY + 100, typeIdx, isBoss);

        // Name
        SDL_Color nc = isBoss ? SDL_Color{255, 180, 80, 255} : SDL_Color{220, 200, 220, 255};
        SDL_Surface* ns = TTF_RenderText_Blended(font, entry.name, nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                int nw = static_cast<int>(ns->w * 1.3f);
                int nh = static_cast<int>(ns->h * 1.3f);
                SDL_Rect nr = {detailX + detailW / 2 - nw / 2, detailY + 180, nw, nh};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        // Stats
        char stats[128];
        std::snprintf(stats, sizeof(stats), "HP: %.0f  |  DMG: %.0f  |  Weakness: %s",
                      entry.baseHP, entry.baseDMG, entry.weakness);
        SDL_Surface* ss = TTF_RenderText_Blended(font, stats, SDL_Color{160, 155, 180, 200});
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_Rect sr = {detailX + 30, detailY + 220, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &sr);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }

        // Lore text
        SDL_Surface* ls = TTF_RenderText_Blended(font, entry.lore, SDL_Color{140, 135, 160, 180});
        if (ls) {
            SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
            if (lt) {
                int maxW = detailW - 60;
                float scale = (ls->w > maxW) ? static_cast<float>(maxW) / ls->w : 1.0f;
                SDL_Rect lr = {detailX + 30, detailY + 260,
                               static_cast<int>(ls->w * scale), static_cast<int>(ls->h * scale)};
                SDL_RenderCopy(renderer, lt, nullptr, &lr);
                SDL_DestroyTexture(lt);
            }
            SDL_FreeSurface(ls);
        }

        // Kill count
        char killText[32];
        std::snprintf(killText, sizeof(killText), "Total Kills: %d", entry.killCount);
        SDL_Surface* ks = TTF_RenderText_Blended(font, killText, SDL_Color{180, 140, 255, 200});
        if (ks) {
            SDL_Texture* kt = SDL_CreateTextureFromSurface(renderer, ks);
            if (kt) {
                SDL_Rect kr = {detailX + 30, detailY + 310, ks->w, ks->h};
                SDL_RenderCopy(renderer, kt, nullptr, &kr);
                SDL_DestroyTexture(kt);
            }
            SDL_FreeSurface(ks);
        }
    } else {
        // Undiscovered
        SDL_Surface* us = TTF_RenderText_Blended(font, "? ? ?", SDL_Color{80, 75, 90, 180});
        if (us) {
            SDL_Texture* ut = SDL_CreateTextureFromSurface(renderer, us);
            if (ut) {
                int uw = static_cast<int>(us->w * 2);
                int uh = static_cast<int>(us->h * 2);
                SDL_Rect ur = {detailX + detailW / 2 - uw / 2, detailY + detailH / 2 - uh / 2, uw, uh};
                SDL_RenderCopy(renderer, ut, nullptr, &ur);
                SDL_DestroyTexture(ut);
            }
            SDL_FreeSurface(us);
        }

        SDL_Surface* ds = TTF_RenderText_Blended(font, "Defeat this enemy to learn more...",
            SDL_Color{80, 75, 90, 120});
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_Rect dr = {detailX + detailW / 2 - ds->w / 2, detailY + detailH / 2 + 30, ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }
    }

    // Navigation hint
    {
        SDL_Color nc = {60, 55, 85, 140};
        SDL_Surface* ns = TTF_RenderText_Blended(font, "W/S Navigate  |  ESC Back", nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {640 - ns->w / 2, 680, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }
}

void BestiaryState::renderEnemyPreview(SDL_Renderer* renderer, int cx, int cy, int type, bool isBoss) {
    // Simple procedural preview of each enemy
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    int size = isBoss ? 60 : 40;

    // Color based on type
    SDL_Color color = {200, 60, 60, 220};
    if (isBoss) {
        switch (type) {
            case 0: color = {200, 40, 180, 255}; break;  // Rift Guardian
            case 1: color = {40, 180, 120, 255}; break;   // Void Wyrm
            case 2: color = {120, 80, 200, 255}; break;   // Dimensional Architect
            case 3: color = {180, 160, 80, 255}; break;   // Temporal Weaver
        }
    } else {
        switch (type) {
            case 0: color = {200, 60, 60, 220}; break;    // Walker
            case 1: color = {60, 160, 200, 220}; break;   // Flyer
            case 2: color = {160, 160, 60, 220}; break;   // Turret
            case 3: color = {200, 120, 60, 220}; break;   // Charger
            case 4: color = {120, 60, 200, 220}; break;   // Phaser
            case 5: color = {200, 80, 20, 220}; break;    // Exploder
            case 6: color = {60, 120, 200, 220}; break;   // Shielder
            case 7: color = {100, 180, 60, 220}; break;   // Crawler
            case 8: color = {180, 60, 180, 220}; break;   // Summoner
            case 9: color = {200, 200, 60, 220}; break;   // Sniper
        }
    }

    // Body
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect body = {cx - size / 2, cy - size / 2, size, size};
    SDL_RenderFillRect(renderer, &body);

    // Eyes
    int eyeSize = size / 8;
    SDL_SetRenderDrawColor(renderer, 255, 40, 40, 255);
    SDL_Rect eyeL = {cx - size / 4, cy - size / 6, eyeSize, eyeSize};
    SDL_Rect eyeR = {cx + size / 4 - eyeSize, cy - size / 6, eyeSize, eyeSize};
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);

    // Boss crown/marker
    if (isBoss) {
        float pulse = 0.5f + 0.5f * std::sin(m_time * 3.0f);
        Uint8 a = static_cast<Uint8>(150 * pulse);
        SDL_SetRenderDrawColor(renderer, 255, 200, 60, a);
        for (int ring = 0; ring < 2; ring++) {
            int expand = 5 + ring * 6;
            SDL_Rect aura = {cx - size / 2 - expand, cy - size / 2 - expand,
                             size + expand * 2, size + expand * 2};
            SDL_RenderDrawRect(renderer, &aura);
        }
    }
}
