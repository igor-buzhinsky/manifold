#include "platform/window.hpp"

#include <format>
#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <epoxy/gl.h>

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

Window::Window(const std::string& title, int width, int height) {
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

    window_ = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_OPENGL);
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
    SDL_GL_SetSwapInterval(1);  // Vsync.
    log_info("OpenGL {}, {}", gl_string(GL_VERSION), gl_string(GL_RENDERER));
}

Window::~Window() {
    SDL_GL_DestroyContext(context_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

PixelSize Window::framebuffer_size() const {
    PixelSize size;
    SDL_GetWindowSizeInPixels(window_, &size.width, &size.height);
    return size;
}

void Window::swap_buffers() { SDL_GL_SwapWindow(window_); }

}  // namespace manifold
