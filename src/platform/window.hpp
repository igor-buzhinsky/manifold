#pragma once

#include <string>

#include <SDL3/SDL_video.h>

#include "render/viewport.hpp"

namespace manifold {

// An SDL window with an OpenGL 3.3 core context. Initializes SDL video on creation and shuts it down
// on destruction, so there must be only one window at a time.
class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    // Size of the drawable area in pixels, which can differ from the window size on high-DPI screens.
    PixelSize framebuffer_size() const;

    // Shows the frame drawn since the last call.
    void swap_buffers();

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
};

}  // namespace manifold
