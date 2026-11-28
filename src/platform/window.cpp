#include "platform/window.hpp"

#include <format>
#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <glad/gl.h>

#include "render/viewport.hpp"
#include "util/log.hpp"

namespace manifold {

namespace {

/// Combines `what` with SDL's last error message.
std::string sdl_error(const std::string& what) { return what + ": " + SDL_GetError(); }

/// A string from glGetString, or "unknown" if OpenGL doesn't give one.
std::string gl_string(GLenum name) {
    const GLubyte* text = glGetString(name);
    if (text == nullptr) {
        return "unknown";
    }
    std::string result;
    for (; *text != 0; ++text) {
        result += static_cast<char>(*text);
    }
    return result;
}

}  // namespace

Window::Window(const std::string& title, int width, int height, bool fullscreen) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(sdl_error("SDL_Init failed"));
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kGlMajorVersion);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kGlMinorVersion);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // macOS only gives a core context newer than 3.2 if it is forward compatible. Elsewhere this changes
    // nothing, because the core profile has no deprecated functions anyway.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Without a fullscreen mode set, SDL fills the screen at the desktop's resolution. A full-screen picture is drawn
    // in the screen's pixels, also where the desktop is scaled; the black bars come from fit_viewport().
    const SDL_WindowFlags flags =
        SDL_WINDOW_OPENGL | (fullscreen ? SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY : SDL_WindowFlags{0});
    window_ = SDL_CreateWindow(title.c_str(), width, height, flags);
    if (window_ == nullptr) {
        const std::string error = sdl_error("SDL_CreateWindow failed");
        SDL_Quit();
        throw std::runtime_error(error);
    }
    context_ = SDL_GL_CreateContext(window_);
    if (context_ == nullptr) {
        const std::string error = sdl_error(
            std::format("SDL_GL_CreateContext failed (OpenGL {}.{} core is needed)", kGlMajorVersion, kGlMinorVersion));
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(error);
    }
    // OpenGL functions can only be loaded once a context exists. Until then, all of them are null.
    // gladLoadGL returns the context's version, or 0 if it can't be read. A context older than requested
    // would load only part of the functions, so it is rejected too.
    const int version = gladLoadGL([](const char* name) -> GLADapiproc { return SDL_GL_GetProcAddress(name); });
    const int major = GLAD_VERSION_MAJOR(version);
    const int minor = GLAD_VERSION_MINOR(version);
    if (version == 0 || major < kGlMajorVersion || (major == kGlMajorVersion && minor < kGlMinorVersion)) {
        SDL_GL_DestroyContext(context_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(
            std::format("Loading OpenGL functions failed: got OpenGL {}.{}, but {}.{} core is needed", major, minor,
                        kGlMajorVersion, kGlMinorVersion));
    }
    vsync_ = SDL_GL_SetSwapInterval(1);
    if (!vsync_) {
        log_warning("Turning vsync on failed: {}", SDL_GetError());
    }
    log_info("OpenGL {}, {}", gl_string(GL_VERSION), gl_string(GL_RENDERER));
}

Window::~Window() {
    SDL_GL_DestroyContext(context_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

void Window::set_text_input(bool on) {
    if (on) {
        SDL_StartTextInput(window_);
    } else {
        SDL_StopTextInput(window_);
    }
}

void Window::set_relative_mouse_mode(bool on) {
    if (!SDL_SetWindowRelativeMouseMode(window_, on)) {
        log_warning("Can't turn relative mouse mode {}: {}", on ? "on" : "off", SDL_GetError());
    }
}

PixelSize Window::size() const {
    PixelSize size;
    SDL_GetWindowSize(window_, &size.width, &size.height);
    return size;
}

PixelSize Window::framebuffer_size() const {
    PixelSize size;
    SDL_GetWindowSizeInPixels(window_, &size.width, &size.height);
    return size;
}

void Window::swap_buffers() { SDL_GL_SwapWindow(window_); }

void Window::set_vsync(bool on) {
    if (on == vsync_) {
        return;
    }
    if (!SDL_GL_SetSwapInterval(on ? 1 : 0)) {
        log_warning("Setting vsync {} failed: {}", on ? "on" : "off", SDL_GetError());
        return;
    }
    vsync_ = on;
}

}  // namespace manifold
