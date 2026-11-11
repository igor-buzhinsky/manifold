#pragma once

#include <string>

#include <SDL3/SDL_video.h>

#include "render/viewport.hpp"

namespace manifold {

/// OpenGL version that the window's context provides, and that the renderer needs.
constexpr int kGlMajorVersion = 4;
constexpr int kGlMinorVersion = 1;

/// An SDL window with an OpenGL 4.1 core context (kGlMajorVersion.kGlMinorVersion).
///
/// 4.1 is the newest OpenGL version on macOS, so the game can run there too.
///
/// Initializes SDL video on creation and shuts it down on destruction, so there must be only one window
/// at a time.
class Window {
public:
    /// Opens a window that can't be resized, and makes its OpenGL context current with vsync on.
    ///
    /// \param title  Text in the title bar.
    /// \param width  Width in screen units, which may differ from pixels on high-DPI screens.
    /// \param height Height in screen units.
    /// \throws std::runtime_error if SDL, the window or the OpenGL context can't be created.
    Window(const std::string& title, int width, int height);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /// Size of the drawable area in pixels, which can differ from the window size on high-DPI screens.
    PixelSize framebuffer_size() const;

    /// Shows the frame drawn since the last call. Waits for vsync.
    void swap_buffers();

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
};

}  // namespace manifold
