#include "platform/window.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include "render/viewport.hpp"

namespace manifold {

namespace {

/// Combines `what` with SDL's last error message.
std::string sdl_error(const std::string& what) { return what + ": " + SDL_GetError(); }

}  // namespace

Window::Window(const std::string& title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(sdl_error("SDL_Init failed"));
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
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
        const std::string error = sdl_error("SDL_GL_CreateContext failed");
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(error);
    }
    SDL_GL_SetSwapInterval(1);  // Vsync.
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
