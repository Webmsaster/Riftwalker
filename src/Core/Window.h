#pragma once
#include <SDL2/SDL.h>
#include <string>

class Window {
public:
    Window(const std::string& title, int width, int height, bool fullscreen = false);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* getSDLWindow() const { return m_window; }
    SDL_Renderer* getSDLRenderer() const { return m_renderer; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool isFullscreen() const { return m_fullscreen; }

    void toggleFullscreen();
    void setTitle(const std::string& title);
    void onResize(int w, int h);

private:
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    int m_width;
    int m_height;
    bool m_fullscreen;
};
