#include "render/screenshot.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <stb_image_write.h>

#include "render/viewport.hpp"

namespace manifold {

void save_screenshot(const std::string& path, int width, int height) {
    constexpr int kChannels = 3;
    const auto row_size = static_cast<std::size_t>(width) * kChannels;
    std::vector<std::uint8_t> pixels(row_size * static_cast<std::size_t>(height));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL rows go bottom to top, PNG rows top to bottom.
    stbi_flip_vertically_on_write(1);
    if (stbi_write_png(path.c_str(), width, height, kChannels, pixels.data(), static_cast<int>(row_size)) == 0) {
        throw std::runtime_error("failed to write screenshot: " + path);
    }
}

OffscreenFramebuffer::OffscreenFramebuffer(const PixelSize& size) {
    GLint max_size = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_size);
    if (size.width < 1 || size.height < 1 || size.width > max_size || size.height > max_size) {
        throw std::runtime_error("off-screen framebuffer size must be from 1 to " + std::to_string(max_size) +
                                 ", not " + std::to_string(size.width) + " x " + std::to_string(size.height));
    }
    glGenRenderbuffers(1, &color_);
    glBindRenderbuffer(GL_RENDERBUFFER, color_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, size.width, size.height);
    glGenRenderbuffers(1, &depth_);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.width, size.height);
    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer_);
        glDeleteRenderbuffers(1, &depth_);
        glDeleteRenderbuffers(1, &color_);
        throw std::runtime_error("off-screen framebuffer is incomplete: status " + std::to_string(status));
    }
}

OffscreenFramebuffer::~OffscreenFramebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer_);
    glDeleteRenderbuffers(1, &depth_);
    glDeleteRenderbuffers(1, &color_);
}

}  // namespace manifold
