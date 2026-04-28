#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <functional>

class Button {
public:
    Button(int x, int y, int w, int h, const std::string& text);

    void render(SDL_Renderer* renderer, TTF_Font* font);
    bool isHovered(int mouseX, int mouseY) const;
    bool isClicked(int mouseX, int mouseY, bool mouseDown) const;

    void setSelected(bool selected) { m_selected = selected; }
    bool isSelected() const { return m_selected; }
    void update(float dt);  // smooth hover transition

    std::function<void()> onClick;

    SDL_Color normalColor{60, 60, 80, 255};
    SDL_Color hoverColor{80, 80, 120, 255};
    SDL_Color selectedColor{100, 60, 180, 255};
    SDL_Color textColor{220, 220, 240, 255};
    SDL_Color disabledColor{40, 40, 50, 255};
    bool enabled = true;

    void setPosition(int x, int y) { m_rect.x = x; m_rect.y = y; }
    void setText(const std::string& text) { m_text = text; }
    const std::string& getText() const { return m_text; }
    int getX() const { return m_rect.x; }
    int getY() const { return m_rect.y; }
    int getW() const { return m_rect.w; }
    int getH() const { return m_rect.h; }

private:
    SDL_Rect m_rect;
    std::string m_text;
    bool m_selected = false;
    float m_hoverBlend = 0.0f;  // 0.0 = unselected, 1.0 = fully selected

public:
    // Entrance animation: slide from left with staggered delay
    float entranceDelay = 0.0f;   // seconds before this button starts sliding in
    float entranceProgress = 0.0f; // 0.0 = off-screen left, 1.0 = at final position
    int baseX = 0;                 // final resting X position
    void updateEntrance(float dt);
    int getRenderX() const;        // visual X position (may differ from hitbox during entrance)
};
