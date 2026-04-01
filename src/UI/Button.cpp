#include "Button.h"

Button::Button(int x, int y, int w, int h, const std::string& text)
    : m_rect{x, y, w, h}, m_text(text) {}

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

    // Background gradient (3 bands)
    int thirdH = m_rect.h / 3;
    SDL_Rect top = {m_rect.x, m_rect.y, m_rect.w, thirdH};
    SDL_Rect mid = {m_rect.x, m_rect.y + thirdH, m_rect.w, thirdH};
    SDL_Rect bot = {m_rect.x, m_rect.y + thirdH * 2, m_rect.w, m_rect.h - thirdH * 2};

    if (!enabled) {
        SDL_SetRenderDrawColor(renderer, 30, 28, 38, 220);
        SDL_RenderFillRect(renderer, &m_rect);
    } else {
        // Smooth blend between unselected and selected colors
        float b = m_hoverBlend;
        auto lerp = [](Uint8 a, Uint8 c, float t) -> Uint8 {
            return static_cast<Uint8>(a + (c - a) * t);
        };
        SDL_SetRenderDrawColor(renderer, lerp(48, 90, b), lerp(42, 55, b), lerp(65, 155, b), lerp(220, 240, b));
        SDL_RenderFillRect(renderer, &top);
        SDL_SetRenderDrawColor(renderer, lerp(38, 70, b), lerp(33, 40, b), lerp(55, 130, b), lerp(220, 240, b));
        SDL_RenderFillRect(renderer, &mid);
        SDL_SetRenderDrawColor(renderer, lerp(30, 55, b), lerp(26, 30, b), lerp(45, 110, b), lerp(220, 240, b));
        SDL_RenderFillRect(renderer, &bot);
    }

    // Glow border (smooth blend)
    if (m_hoverBlend > 0.01f && enabled) {
        for (int i = 3; i >= 0; i--) {
            Uint8 ga = static_cast<Uint8>((50 - i * 12) * m_hoverBlend);
            SDL_SetRenderDrawColor(renderer, 150, 100, 255, ga);
            SDL_Rect border = {m_rect.x - i, m_rect.y - i,
                               m_rect.w + i * 2, m_rect.h + i * 2};
            SDL_RenderDrawRect(renderer, &border);
        }

        // Selection indicator diamond on left
        int midY = m_rect.y + m_rect.h / 2;
        SDL_SetRenderDrawColor(renderer, 200, 160, 255, static_cast<Uint8>(220 * m_hoverBlend));
        SDL_RenderDrawLine(renderer, m_rect.x - 12, midY, m_rect.x - 7, midY - 5);
        SDL_RenderDrawLine(renderer, m_rect.x - 7, midY - 5, m_rect.x - 2, midY);
        SDL_RenderDrawLine(renderer, m_rect.x - 2, midY, m_rect.x - 7, midY + 5);
        SDL_RenderDrawLine(renderer, m_rect.x - 7, midY + 5, m_rect.x - 12, midY);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 60, 90, 150);
        SDL_RenderDrawRect(renderer, &m_rect);
    }

    // Top highlight line
    if (enabled) {
        Uint8 ha = m_selected ? static_cast<Uint8>(100) : static_cast<Uint8>(35);
        SDL_SetRenderDrawColor(renderer, 180, 160, 220, ha);
        SDL_RenderDrawLine(renderer, m_rect.x + 1, m_rect.y,
                          m_rect.x + m_rect.w - 1, m_rect.y);
    }

    // Text with shadow
    if (font && !m_text.empty()) {
        SDL_Color tc = enabled ?
            (m_selected ? SDL_Color{240, 230, 255, 255} : SDL_Color{190, 185, 210, 255}) :
            SDL_Color{80, 75, 95, 255};

        // Shadow
        if (enabled) {
            SDL_Color shadow = {0, 0, 0, 100};
            SDL_Surface* ss = TTF_RenderText_Blended(font, m_text.c_str(), shadow);
            if (ss) {
                SDL_Texture* st = SDL_CreateTextureFromSurface(renderer, ss);
                if (st) {
                    SDL_Rect sr = {
                        m_rect.x + (m_rect.w - ss->w) / 2 + 1,
                        m_rect.y + (m_rect.h - ss->h) / 2 + 1,
                        ss->w, ss->h
                    };
                    SDL_RenderCopy(renderer, st, nullptr, &sr);
                    SDL_DestroyTexture(st);
                }
                SDL_FreeSurface(ss);
            }
        }

        SDL_Surface* surface = TTF_RenderText_Blended(font, m_text.c_str(), tc);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect textRect = {
                    m_rect.x + (m_rect.w - surface->w) / 2,
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
