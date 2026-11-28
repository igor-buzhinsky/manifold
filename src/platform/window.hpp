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
    /// Opens a window that can't be resized, or one that fills the screen, makes its OpenGL context current with
    /// vsync on, and loads the OpenGL functions.
    ///
    /// \param title      Text in the title bar.
    /// \param width      Width in screen units, which may differ from pixels on high-DPI screens.
    /// \param height     Height in screen units.
    /// \param fullscreen Whether the window fills the screen instead, at the resolution of the desktop and in its
    ///                   pixels also on high-DPI screens. The mode of the screen isn't changed. `width` and
    ///                   `height` are then ignored.
    /// \throws std::runtime_error if SDL, the window or the OpenGL context can't be created, or OpenGL
    ///         functions can't be loaded.
    Window(const std::string& title, int width, int height, bool fullscreen);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /// Size of the window in window units, as mouse positions are given, which can differ from pixels on high-DPI
    /// screens.
    PixelSize size() const;

    /// Size of the drawable area in pixels, which can differ from the window size on high-DPI screens.
    PixelSize framebuffer_size() const;

    /// Turns text input on or off.
    ///
    /// While it is on, typed text arrives as SDL_EVENT_TEXT_INPUT events, with the keyboard layout and input
    /// methods applied, e.g. for the console. Some platforms show an on-screen keyboard then.
    void set_text_input(bool on);

    /// Turns relative mouse mode on or off, for mouse look.
    ///
    /// While it is on, the mouse pointer is hidden and kept in the window, and mouse motion still arrives, also at the
    /// edges of the screen. SDL lets the pointer go while the window doesn't have the focus.
    void set_relative_mouse_mode(bool on);

    /// Shows the frame drawn since the last call. Waits for vsync, if it is on.
    void swap_buffers();

    /// Turns vsync on, as it is at first, or off, e.g. to measure how fast frames can be drawn. Does nothing if it is
    /// on or off already.
    void set_vsync(bool on);

    /// The SDL window, e.g. for the debug overlay. It lives as long as this object.
    SDL_Window* sdl_window() const { return window_; }
    /// The OpenGL context. It lives as long as this object.
    SDL_GLContext gl_context() const { return context_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    bool vsync_ = true;
};

}  // namespace manifold
