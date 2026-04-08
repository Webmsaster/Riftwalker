#include "Button.h"
#include "Core/ResourceManager.h"

Button::Button(int x, int y, int w, int h, const std::string& text)
    : m_rect{x, y, w, h}, m_text(text) {
    baseX = x; // Initialize baseX to match constructor position (prevents off-screen render)
    entranceProgress = 1.0f; // Default: no entrance animation (states must opt-in)
}

void Button::update(float dt) {
    float target = (m_selected && enabled) ? 1.0f : 0.0f;
    float speed = 8.0f; // fast but smooth
    if (m_hoverBlend < target) {
        m_hoverBlend += speed * dt;
        if (m_hoverBlend > target) m_hoverBlend = target;
    } else if (m_hoverBlend > target) {
        m_hoverBlend -= speed * dt;
        if (m_hoverBlend < target) m_hoverBlend = target;
    }
}

void Button::render(SDL_Renderer* renderer, TTF_Font* font) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Use render X for visual position (entrance animation), hitbox stays at baseX
    int rx = getRenderX();

    // Texture-based button background
    SDL_Rect btnDst = {rx, m_rect.y, m_rect.w, m_rect.h};
    const char* texPath = "assets/textures/ui/button_normal.png";
    if (!enabled) {
        texPath = "assets/textures/ui/button_normal.png"; // dimmed via alpha
    } else if (m_hoverBlend > 0.5f) {
        texPath = "assets/textures/ui/button_hover.png";
    }

    SDL_Texture* btnTex = ResourceManager::instance().getTexture(texPath);
    if (btnTex) {
        SDL_SetTextureBlendMode(btnTex, SDL_BLENDMODE_BLEND);
        if (!enabled) {
            SDL_SetTextureAlphaMod(btnTex, 140);
            SDL_SetTextureColorMod(btnTex, 120, 120, 130);
        } else {
            SDL_SetTextureAlphaMod(btnTex, 255);
            SDL_SetTextureColorMod(btnTex, 255, 255, 255);
        }
        SDL_RenderCopy(renderer, btnTex, nullptr, &btnDst);
        if (!enabled) {
            SDL_SetTextureAlphaMod(btnTex, 255);
            SDL_SetTextureColorMod(btnTex, 255, 255, 255);
        }
    } else {
        // Fallback: simple fill
        SDL_SetRenderDrawColor(renderer, 35, 30, 55, 220);
        SDL_RenderFillRect(renderer, &btnDst);
    }

    // Glow border (smooth blend) on hover
    if (m_hoverBlend > 0.01f && enabled) {
        for (int i = 3; i >= 0; i--) {
            Uint8 ga = static_cast<Uint8>((50 - i * 12) * m_hoverBlend);
            SDL_SetRenderDrawColor(renderer, 150, 100, 255, ga);
            SDL_Rect border = {rx - i, m_rect.y - i,
                               m_rect.w + i * 2, m_rect.h + i * 2};
            SDL_RenderDrawRect(renderer, &border);
        }

        // Selection indicator diamond on left
        int midY = m_rect.y + m_rect.h / 2;
        SDL_SetRenderDrawColor(renderer, 200, 160, 255, static_cast<Uint8>(220 * m_hoverBlend));
        SDL_RenderDrawLine(renderer, rx - 12, midY, rx - 7, midY - 5);
        SDL_RenderDrawLine(renderer, rx - 7, midY - 5, rx - 2, midY);
        SDL_RenderDrawLine(renderer, rx - 2, midY, rx - 7, midY + 5);
        SDL_RenderDrawLine(renderer, rx - 7, midY + 5, rx - 12, midY);
    }

    // Text with shadow
    if (font && !m_text.empty()) {
        SDL_Color tc = enabled ?
            (m_selected ? SDL_Color{240, 230, 255, 255} : SDL_Color{190, 185, 210, 255}) :
            SDL_Color{80, 75, 95, 255};

        // Shadow
        if (enabled) {
            SDL_Color shadow = {0, 0, 0, 100};
            SDL_Surface* ss = TTF_RenderUTF8_Blended(font, m_text.c_str(), shadow);
            if (ss) {
                SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
                if (st) {
                    SDL_Rect sr = {
                        rx + (m_rect.w - ss->w) / 2 + 1,
                        m_rect.y + (m_rect.h - ss->h) / 2 + 1,
                        ss->w, ss->h
                    };
                    SDL_RenderCopy(renderer, st, nullptr, &sr);
                    SDL_DestroyTexture(st);
                }
                SDL_FreeSurface(ss);
            }
        }

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, m_text.c_str(), tc);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect textRect = {
                    rx + (m_rect.w - surface->w) / 2,
                    m_rect.y + (m_rect.h - surface->h) / 2,
                    surface->w, surface->h
                };
                SDL_RenderCopy(renderer, texture, nullptr, &textRect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

bool Button::isHovered(int mouseX, int mouseY) const {
    return mouseX >= m_rect.x && mouseX < m_rect.x + m_rect.w &&
           mouseY >= m_rect.y && mouseY < m_rect.y + m_rect.h;
}

bool Button::isClicked(int mouseX, int mouseY, bool mouseDown) const {
    return enabled && mouseDown && isHovered(mouseX, mouseY);
}

void Button::updateEntrance(float dt) {
    if (entranceDelay > 0) {
        entranceDelay -= dt;
        // Keep button at final position for hitbox, use renderOffsetX for visual only
        m_rect.x = baseX;
        return;
    }
    if (entranceProgress < 1.0f) {
        entranceProgress += dt * 5.0f; // fast slide-in
        if (entranceProgress > 1.0f) entranceProgress = 1.0f;
    }
    // Always keep m_rect.x at final position for hitbox accuracy
    m_rect.x = baseX;
}

int Button::getRenderX() const {
    if (entranceProgress >= 1.0f) return m_rect.x;
    float eased = 1.0f - (1.0f - entranceProgress) * (1.0f - entranceProgress);
    return baseX - static_cast<int>((1.0f - eased) * 400);
}
