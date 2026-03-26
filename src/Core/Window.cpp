#include "Window.h"
#include <stdexcept>
#include <cmath>

// Create a 32x32 programmatic window icon (purple rift crystal)
static SDL_Surface* createRiftIcon() {
    SDL_Surface* icon = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!icon) return nullptr;

    SDL_LockSurface(icon);
    auto* pixels = static_cast<Uint32*>(icon->pixels);
    const int pitch = icon->pitch / 4;

    // Clear to transparent
    for (int i = 0; i < 32 * pitch; i++) pixels[i] = 0;

    // Diamond vertices: top(16,2), right(29,16), bottom(16,29), left(2,16)
    auto setPixel = [&](int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
        if (x >= 0 && x < 32 && y >= 0 && y < 32)
            pixels[y * pitch + x] = (r << 24) | (g << 16) | (b << 8) | a;
    };

    // Fill diamond shape with gradient
    const int cx = 16, cy = 16;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            // Diamond test: |x-cx| + |y-cy| <= radius
            int dist = std::abs(x - cx) + std::abs(y - cy);
            if (dist <= 14) {
                float t = static_cast<float>(dist) / 14.0f;
                // Core: bright cyan-white, edges: deep purple
                Uint8 r = static_cast<Uint8>(200 - t * 140);  // 200 -> 60
                Uint8 g = static_cast<Uint8>(180 - t * 130);  // 180 -> 50
                Uint8 b = static_cast<Uint8>(255 - t * 55);   // 255 -> 200
                Uint8 a = 255;
                // Inner glow highlight (off-center for 3D feel)
                int hlDist = std::abs(x - (cx - 3)) + std::abs(y - (cy - 4));
                if (hlDist < 6) {
                    float hl = 1.0f - static_cast<float>(hlDist) / 6.0f;
                    r = static_cast<Uint8>(std::min(255, r + static_cast<int>(hl * 80)));
                    g = static_cast<Uint8>(std::min(255, g + static_cast<int>(hl * 90)));
                    b = 255;
                }
                setPixel(x, y, r, g, b, a);
            }
            // Soft glow outline (1px around diamond)
            else if (dist == 15) {
                setPixel(x, y, 120, 40, 180, 160);
            }
            else if (dist == 16) {
                setPixel(x, y, 80, 20, 140, 80);
            }
        }
    }

    // Rift crack line through center (bright cyan slash)
    for (int i = -10; i <= 10; i++) {
        int x = cx + i;
        int y = cy + i / 3;
        setPixel(x, y, 150, 255, 255, 220);
        setPixel(x, y + 1, 100, 200, 255, 120);
    }

    SDL_UnlockSurface(icon);
    return icon;
}

Window::Window(const std::string& title, int width, int height, bool fullscreen)
    : m_window(nullptr)
    , m_renderer(nullptr)
    , m_width(width)
    , m_height(height)
    , m_fullscreen(fullscreen)
{
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, flags
    );
    if (!m_window) {
        throw std::runtime_error("Failed to create window: " + std::string(SDL_GetError()));
    }

    // Set programmatic window icon (purple rift crystal)
    SDL_Surface* icon = createRiftIcon();
    if (icon) {
        SDL_SetWindowIcon(m_window, icon);
        SDL_FreeSurface(icon);
    }

    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        SDL_DestroyWindow(m_window);
        throw std::runtime_error("Failed to create renderer: " + std::string(SDL_GetError()));
    }

    SDL_RenderSetLogicalSize(m_renderer, width, height);
    SDL_SetWindowMinimumSize(m_window, 640, 360);
}

Window::~Window() {
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
}

void Window::toggleFullscreen() {
    m_fullscreen = !m_fullscreen;
    SDL_SetWindowFullscreen(m_window, m_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_GetWindowSize(m_window, &m_width, &m_height);
}

void Window::setTitle(const std::string& title) {
    SDL_SetWindowTitle(m_window, title.c_str());
}

void Window::onResize(int w, int h) {
    m_width  = w;
    m_height = h;
}
