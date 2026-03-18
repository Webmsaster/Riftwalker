#include "ClassSelectState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include <cmath>
#include <cstdio>

void ClassSelectState::enter() {
    m_selected = static_cast<int>(g_selectedClass);
    m_time = 0;
    m_previewBob = 0;
}

void ClassSelectState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:
                m_selected = (m_selected - 1 + ClassSystem::CLASS_COUNT) % ClassSystem::CLASS_COUNT;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT:
                m_selected = (m_selected + 1) % ClassSystem::CLASS_COUNT;
                AudioManager::instance().play(SFX::MenuSelect);
                break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                if (ClassSystem::isUnlocked(static_cast<PlayerClass>(m_selected))) {
                    g_selectedClass = static_cast<PlayerClass>(m_selected);
                    AudioManager::instance().play(SFX::MenuConfirm);
                    game->changeState(StateID::DifficultySelect);
                } else {
                    AudioManager::instance().play(SFX::RiftFail);
                }
                break;
            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::Menu);
                break;
            default: break;
        }
    }
}

void ClassSelectState::update(float dt) {
    m_time += dt;
    m_previewBob = std::sin(m_time * 2.0f) * 4.0f;
}

void ClassSelectState::render(SDL_Renderer* renderer) {
    // Background
    SDL_SetRenderDrawColor(renderer, 10, 8, 20, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Grid
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 20);
    for (int x = 0; x < SCREEN_WIDTH; x += 80) SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 80) SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Title
    {
        SDL_Color c = {140, 100, 220, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, "C H O O S E  Y O U R  C L A S S", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int tw = static_cast<int>(s->w * 1.5f);
                int th = static_cast<int>(s->h * 1.5f);
                SDL_Rect r = {SCREEN_WIDTH / 2 - tw / 2, 50, tw, th};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Class cards side by side (auto-size for any count)
    int cardW = (ClassSystem::CLASS_COUNT <= 3) ? 340 : 280;
    int cardH = 440;
    int gap = (ClassSystem::CLASS_COUNT <= 3) ? 30 : 20;
    int totalW = cardW * ClassSystem::CLASS_COUNT + gap * (ClassSystem::CLASS_COUNT - 1);
    int startX = SCREEN_WIDTH / 2 - totalW / 2;
    int cardY = 110;

    for (int i = 0; i < ClassSystem::CLASS_COUNT; i++) {
        const auto& data = ClassSystem::getData(i);
        int cx = startX + i * (cardW + gap);
        bool selected = (i == m_selected);
        renderClassCard(renderer, font, data, cx, cardY, cardW, cardH, selected);
    }

    // Navigation hint
    {
        SDL_Color nc = {60, 55, 85, 140};
        SDL_Surface* ns = TTF_RenderText_Blended(font, "A/D Navigate  |  ENTER Select  |  ESC Back", nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {SCREEN_WIDTH / 2 - ns->w / 2, 680, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }
}

void ClassSelectState::renderClassCard(SDL_Renderer* renderer, TTF_Font* font,
                                        const ClassData& data, int x, int y,
                                        int w, int h, bool selected) {
    bool locked = !ClassSystem::isUnlocked(data.id);

    // Card background
    Uint8 bgA = locked ? 20 : (selected ? 80 : 30);
    SDL_SetRenderDrawColor(renderer, 20, 16, 35, bgA);
    SDL_Rect card = {x, y, w, h};
    SDL_RenderFillRect(renderer, &card);

    // Top accent bar in class color
    Uint8 accentA = locked ? 40 : (selected ? 220 : 80);
    SDL_SetRenderDrawColor(renderer, data.color.r, data.color.g, data.color.b, accentA);
    SDL_Rect accent = {x, y, w, 4};
    SDL_RenderFillRect(renderer, &accent);

    // Selection border with pulse
    if (selected) {
        float pulse = 0.6f + 0.4f * std::sin(this->m_time * 4.0f);
        Uint8 borderA = static_cast<Uint8>(200 * pulse);
        SDL_SetRenderDrawColor(renderer, data.color.r, data.color.g, data.color.b, borderA);
        SDL_RenderDrawRect(renderer, &card);
        // Inner glow
        SDL_Rect inner = {x + 1, y + 1, w - 2, h - 2};
        SDL_SetRenderDrawColor(renderer, data.color.r, data.color.g, data.color.b,
                               static_cast<Uint8>(borderA / 3));
        SDL_RenderDrawRect(renderer, &inner);
    }

    int cardCx = x + w / 2;

    // Procedural character preview
    int previewY = y + 60 + static_cast<int>(selected ? this->m_previewBob : 0.0f);
    this->renderClassPreview(renderer, data, cardCx, previewY);

    // Class name
    SDL_Color nameColor = selected ? data.color :
        SDL_Color{static_cast<Uint8>(data.color.r / 2),
                  static_cast<Uint8>(data.color.g / 2),
                  static_cast<Uint8>(data.color.b / 2), 255};
    SDL_Surface* ns = TTF_RenderText_Blended(font, data.name, nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            int nw = static_cast<int>(ns->w * 1.4f);
            int nh = static_cast<int>(ns->h * 1.4f);
            SDL_Rect nr = {cardCx - nw / 2, y + 155, nw, nh};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    // Description
    Uint8 descA = selected ? 200 : 120;
    SDL_Color descColor = {160, 155, 180, descA};
    SDL_Surface* ds = TTF_RenderText_Blended(font, data.description, descColor);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            // Scale down if too wide
            int maxW = w - 20;
            float scale = (ds->w > maxW) ? static_cast<float>(maxW) / ds->w : 1.0f;
            SDL_Rect dr = {cardCx - static_cast<int>(ds->w * scale) / 2, y + 190,
                           static_cast<int>(ds->w * scale), static_cast<int>(ds->h * scale)};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }

    // Passive name (highlighted)
    SDL_Color passColor = selected ?
        SDL_Color{255, 220, 100, 255} :
        SDL_Color{140, 120, 80, 180};
    SDL_Surface* ps = TTF_RenderText_Blended(font, data.passiveName, passColor);
    if (ps) {
        SDL_Texture* pt = SDL_CreateTextureFromSurface(renderer, ps);
        if (pt) {
            SDL_Rect pr = {cardCx - ps->w / 2, y + 230, ps->w, ps->h};
            SDL_RenderCopy(renderer, pt, nullptr, &pr);
            SDL_DestroyTexture(pt);
        }
        SDL_FreeSurface(ps);
    }

    // Passive description
    SDL_Color pdColor = {140, 135, 160, static_cast<Uint8>(selected ? 200 : 100)};
    SDL_Surface* pds = TTF_RenderText_Blended(font, data.passiveDesc, pdColor);
    if (pds) {
        SDL_Texture* pdt = SDL_CreateTextureFromSurface(renderer, pds);
        if (pdt) {
            int maxW = w - 20;
            float scale = (pds->w > maxW) ? static_cast<float>(maxW) / pds->w : 1.0f;
            SDL_Rect pdr = {cardCx - static_cast<int>(pds->w * scale) / 2, y + 255,
                            static_cast<int>(pds->w * scale), static_cast<int>(pds->h * scale)};
            SDL_RenderCopy(renderer, pdt, nullptr, &pdr);
            SDL_DestroyTexture(pdt);
        }
        SDL_FreeSurface(pds);
    }

    // Ability modification
    SDL_Color amColor = {120, 200, 180, static_cast<Uint8>(selected ? 200 : 100)};
    SDL_Surface* ams = TTF_RenderText_Blended(font, data.abilityMod, amColor);
    if (ams) {
        SDL_Texture* amt = SDL_CreateTextureFromSurface(renderer, ams);
        if (amt) {
            int maxW = w - 20;
            float scale = (ams->w > maxW) ? static_cast<float>(maxW) / ams->w : 1.0f;
            SDL_Rect amr = {cardCx - static_cast<int>(ams->w * scale) / 2, y + 285,
                            static_cast<int>(ams->w * scale), static_cast<int>(ams->h * scale)};
            SDL_RenderCopy(renderer, amt, nullptr, &amr);
            SDL_DestroyTexture(amt);
        }
        SDL_FreeSurface(ams);
    }

    // Locked overlay: show requirement instead of stats
    if (locked) {
        // Dark overlay
        SDL_SetRenderDrawColor(renderer, 5, 3, 15, 180);
        SDL_Rect overlay = {x + 2, y + 180, w - 4, h - 185};
        SDL_RenderFillRect(renderer, &overlay);

        // Lock icon (simple padlock shape)
        int lockCx = x + w / 2;
        int lockCy = y + 280;
        SDL_SetRenderDrawColor(renderer, 120, 100, 80, 200);
        SDL_Rect lockBody = {lockCx - 15, lockCy, 30, 24};
        SDL_RenderFillRect(renderer, &lockBody);
        // Lock shackle (arc with lines)
        SDL_RenderDrawLine(renderer, lockCx - 10, lockCy, lockCx - 10, lockCy - 12);
        SDL_RenderDrawLine(renderer, lockCx + 10, lockCy, lockCx + 10, lockCy - 12);
        SDL_RenderDrawLine(renderer, lockCx - 10, lockCy - 12, lockCx + 10, lockCy - 12);
        // Keyhole
        SDL_SetRenderDrawColor(renderer, 30, 25, 40, 255);
        SDL_Rect keyhole = {lockCx - 3, lockCy + 8, 6, 8};
        SDL_RenderFillRect(renderer, &keyhole);

        // "LOCKED" text
        SDL_Color lockColor = {180, 150, 80, 220};
        SDL_Surface* ls = TTF_RenderText_Blended(font, "LOCKED", lockColor);
        if (ls) {
            SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
            if (lt) {
                int lw = static_cast<int>(ls->w * 1.2f);
                int lh = static_cast<int>(ls->h * 1.2f);
                SDL_Rect lr = {lockCx - lw / 2, lockCy + 35, lw, lh};
                SDL_RenderCopy(renderer, lt, nullptr, &lr);
                SDL_DestroyTexture(lt);
            }
            SDL_FreeSurface(ls);
        }

        // Unlock requirement
        const char* req = ClassSystem::getUnlockRequirement(data.id);
        SDL_Color reqColor = {140, 120, 90, 180};
        SDL_Surface* rs = TTF_RenderText_Blended(font, req, reqColor);
        if (rs) {
            SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
            if (rt) {
                int maxW = w - 30;
                float scale = (rs->w > maxW) ? static_cast<float>(maxW) / rs->w : 1.0f;
                SDL_Rect rr = {lockCx - static_cast<int>(rs->w * scale) / 2, lockCy + 60,
                               static_cast<int>(rs->w * scale), static_cast<int>(rs->h * scale)};
                SDL_RenderCopy(renderer, rt, nullptr, &rr);
                SDL_DestroyTexture(rt);
            }
            SDL_FreeSurface(rs);
        }
        return; // Skip stats for locked classes
    }

    // Stats: HP and Speed bars
    int statY = y + 330;
    int barW = w - 60;
    int barH = 14;
    int barX = x + 30;

    // HP label + bar
    {
        char hpText[32];
        std::snprintf(hpText, sizeof(hpText), "HP: %.0f", data.baseHP);
        SDL_Surface* hs = TTF_RenderText_Blended(font, hpText,
            SDL_Color{200, 80, 80, static_cast<Uint8>(selected ? 220 : 120)});
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            if (ht) {
                SDL_Rect hr = {barX, statY, hs->w, hs->h};
                SDL_RenderCopy(renderer, ht, nullptr, &hr);
                SDL_DestroyTexture(ht);
            }
            SDL_FreeSurface(hs);
        }
        statY += 20;
        float hpPercent = data.baseHP / 150.0f; // max possible HP for bar scale
        SDL_SetRenderDrawColor(renderer, 40, 30, 50, 120);
        SDL_Rect bgBar = {barX, statY, barW, barH};
        SDL_RenderFillRect(renderer, &bgBar);
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, selected ? static_cast<Uint8>(200) : static_cast<Uint8>(100));
        SDL_Rect fillBar = {barX, statY, static_cast<int>(barW * hpPercent), barH};
        SDL_RenderFillRect(renderer, &fillBar);
    }

    statY += barH + 10;

    // Speed label + bar
    {
        char spdText[32];
        std::snprintf(spdText, sizeof(spdText), "Speed: %.0f", data.baseSpeed);
        SDL_Surface* ss = TTF_RenderText_Blended(font, spdText,
            SDL_Color{80, 180, 200, static_cast<Uint8>(selected ? 220 : 120)});
        if (ss) {
            SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
            if (st) {
                SDL_Rect sr = {barX, statY, ss->w, ss->h};
                SDL_RenderCopy(renderer, st, nullptr, &sr);
                SDL_DestroyTexture(st);
            }
            SDL_FreeSurface(ss);
        }
        statY += 20;
        float spdPercent = data.baseSpeed / 350.0f;
        SDL_SetRenderDrawColor(renderer, 40, 30, 50, 120);
        SDL_Rect bgBar = {barX, statY, barW, barH};
        SDL_RenderFillRect(renderer, &bgBar);
        SDL_SetRenderDrawColor(renderer, 60, 180, 200, selected ? static_cast<Uint8>(200) : static_cast<Uint8>(100));
        SDL_Rect fillBar = {barX, statY, static_cast<int>(barW * spdPercent), barH};
        SDL_RenderFillRect(renderer, &fillBar);
    }
}

void ClassSelectState::renderClassPreview(SDL_Renderer* renderer, const ClassData& data,
                                           int cx, int cy) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Character body (28x48 like player)
    int bw = 42, bh = 72; // scaled up for preview
    SDL_SetRenderDrawColor(renderer, data.color.r, data.color.g, data.color.b, 220);
    SDL_Rect body = {cx - bw / 2, cy - bh / 2, bw, bh};
    SDL_RenderFillRect(renderer, &body);

    // Eyes
    int eyeY = cy - bh / 2 + 14;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    SDL_Rect eyeL = {cx - 8, eyeY, 5, 6};
    SDL_Rect eyeR = {cx + 3, eyeY, 5, 6};
    SDL_RenderFillRect(renderer, &eyeL);
    SDL_RenderFillRect(renderer, &eyeR);

    // Pupils
    SDL_SetRenderDrawColor(renderer, 20, 15, 30, 255);
    SDL_Rect pupilL = {cx - 6, eyeY + 2, 3, 3};
    SDL_Rect pupilR = {cx + 5, eyeY + 2, 3, 3};
    SDL_RenderFillRect(renderer, &pupilL);
    SDL_RenderFillRect(renderer, &pupilR);

    // Class-specific visual detail
    float t = m_time;
    if (data.id == PlayerClass::Voidwalker) {
        // Rift energy aura (pulsing blue glow)
        for (int ring = 0; ring < 3; ring++) {
            float pulse = 0.5f + 0.5f * std::sin(t * 3.0f + ring * 1.2f);
            Uint8 a = static_cast<Uint8>(60 * pulse);
            int expand = 4 + ring * 5;
            SDL_SetRenderDrawColor(renderer, 60, 120, 220, a);
            SDL_Rect aura = {cx - bw / 2 - expand, cy - bh / 2 - expand,
                             bw + expand * 2, bh + expand * 2};
            SDL_RenderDrawRect(renderer, &aura);
        }
    } else if (data.id == PlayerClass::Berserker) {
        // Rage lines emanating from body
        for (int i = 0; i < 6; i++) {
            float angle = t * 2.0f + i * 1.047f;
            float len = 12.0f + 8.0f * std::sin(t * 4.0f + i);
            int x1 = cx + static_cast<int>(std::cos(angle) * (bw / 2 + 2));
            int y1 = cy + static_cast<int>(std::sin(angle) * (bh / 2 + 2) * 0.5f);
            int x2 = cx + static_cast<int>(std::cos(angle) * (bw / 2 + len));
            int y2 = cy + static_cast<int>(std::sin(angle) * (bh / 2 + len) * 0.5f);
            Uint8 a = static_cast<Uint8>(120 + 80 * std::sin(t * 3.0f + i));
            SDL_SetRenderDrawColor(renderer, 255, 80, 40, a);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
        // Shoulder pads
        SDL_SetRenderDrawColor(renderer, 180, 40, 40, 200);
        SDL_Rect padL = {cx - bw / 2 - 6, cy - bh / 2 + 4, 8, 12};
        SDL_Rect padR = {cx + bw / 2 - 2, cy - bh / 2 + 4, 8, 12};
        SDL_RenderFillRect(renderer, &padL);
        SDL_RenderFillRect(renderer, &padR);
    } else if (data.id == PlayerClass::Phantom) {
        // Shadow trail copies
        for (int i = 1; i <= 3; i++) {
            float offset = i * 8.0f;
            float phase = std::sin(t * 2.0f + i * 0.8f) * 3.0f;
            Uint8 a = static_cast<Uint8>(60 - i * 15);
            SDL_SetRenderDrawColor(renderer, data.color.r, data.color.g, data.color.b, a);
            SDL_Rect ghost = {cx - bw / 2 - static_cast<int>(offset),
                              cy - bh / 2 + static_cast<int>(phase),
                              bw, bh};
            SDL_RenderFillRect(renderer, &ghost);
        }
        // Speed lines
        for (int i = 0; i < 4; i++) {
            int ly = cy - bh / 4 + i * (bh / 4);
            int lx = cx + bw / 2 + 5;
            float len = 15.0f + 10.0f * std::sin(t * 5.0f + i);
            Uint8 a = static_cast<Uint8>(100 + 50 * std::sin(t * 3.0f + i));
            SDL_SetRenderDrawColor(renderer, 60, 220, 200, a);
            SDL_RenderDrawLine(renderer, lx, ly, lx + static_cast<int>(len), ly);
        }
    } else if (data.id == PlayerClass::Technomancer) {
        // Mini turret on the right side
        int turretX = cx + bw / 2 + 10;
        int turretY = cy + bh / 4;
        float turretPulse = 0.6f + 0.4f * std::sin(t * 4.0f);
        // Turret base
        SDL_SetRenderDrawColor(renderer, 180, 140, 40, 200);
        SDL_Rect tBase = {turretX - 8, turretY + 4, 16, 6};
        SDL_RenderFillRect(renderer, &tBase);
        // Turret body
        SDL_SetRenderDrawColor(renderer, 200, 160, 50, 220);
        SDL_Rect tBody = {turretX - 6, turretY - 4, 12, 10};
        SDL_RenderFillRect(renderer, &tBody);
        // Turret barrel (oscillates to track)
        float barrelAngle = std::sin(t * 2.5f) * 0.4f;
        int bx2 = turretX + static_cast<int>(std::cos(barrelAngle) * 14);
        int by2 = turretY + static_cast<int>(std::sin(barrelAngle) * 14) - 2;
        SDL_SetRenderDrawColor(renderer, 230, 190, 60, 220);
        SDL_RenderDrawLine(renderer, turretX + 4, turretY - 1, bx2, by2);
        SDL_RenderDrawLine(renderer, turretX + 4, turretY, bx2, by2 + 1);
        // Muzzle flash
        SDL_SetRenderDrawColor(renderer, 255, 220, 80, static_cast<Uint8>(180 * turretPulse));
        SDL_Rect muzzle = {bx2 - 1, by2 - 1, 3, 3};
        SDL_RenderFillRect(renderer, &muzzle);

        // Shock trap on the left side (pulsing diamond)
        int trapX = cx - bw / 2 - 14;
        int trapY = cy + bh / 4 + 4;
        float trapPulse = 0.5f + 0.5f * std::sin(t * 5.0f);
        SDL_SetRenderDrawColor(renderer, 255, static_cast<Uint8>(200 + 30 * trapPulse),
                               static_cast<Uint8>(50 + 50 * trapPulse), 200);
        // Diamond shape
        SDL_RenderDrawLine(renderer, trapX, trapY - 5, trapX + 5, trapY);
        SDL_RenderDrawLine(renderer, trapX + 5, trapY, trapX, trapY + 5);
        SDL_RenderDrawLine(renderer, trapX, trapY + 5, trapX - 5, trapY);
        SDL_RenderDrawLine(renderer, trapX - 5, trapY, trapX, trapY - 5);
        // Inner glow
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, static_cast<Uint8>(150 * trapPulse));
        SDL_Rect trapGlow = {trapX - 2, trapY - 2, 4, 4};
        SDL_RenderFillRect(renderer, &trapGlow);

        // Rotating gear particles around body
        for (int i = 0; i < 6; i++) {
            float angle = t * 1.5f + i * 1.047f;
            float radius = bw / 2 + 6.0f + 3.0f * std::sin(t * 3.0f + i);
            int gx = cx + static_cast<int>(std::cos(angle) * radius);
            int gy = cy + static_cast<int>(std::sin(angle) * radius * 0.6f);
            Uint8 ga = static_cast<Uint8>(80 + 60 * std::sin(t * 2.0f + i));
            SDL_SetRenderDrawColor(renderer, 230, 180, 50, ga);
            SDL_Rect gear = {gx - 1, gy - 1, 3, 3};
            SDL_RenderFillRect(renderer, &gear);
        }
    }
}
