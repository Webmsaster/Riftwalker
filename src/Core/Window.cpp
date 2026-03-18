#include "Window.h"
#include <stdexcept>

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
    // Update title bar to show current window resolution
    SDL_SetWindowTitle(m_window,
        ("Riftwalker [" + std::to_string(w) + "x" + std::to_string(h) + "]").c_str());
}
