#pragma once

#include <string>

#include <glad/gl.h>

#include "render/viewport.hpp"

namespace manifold {

/// Saves the current OpenGL read framebuffer as a PNG file.
///
/// \param path   File to write.
/// \param width  Width of the framebuffer in pixels.
/// \param height Height of the framebuffer in pixels.
/// \throws std::runtime_error if the file can't be written.
void save_screenshot(const std::string& path, int width, int height);

/// A framebuffer of a given size that is not shown, e.g. to draw a screenshot of any size into.
///
/// It is bound while it exists, and the default framebuffer is bound again when it is destroyed. Needs a
/// current OpenGL context.
class OffscreenFramebuffer {
public:
    /// Creates the framebuffer, with 8-bit RGB color and a depth buffer, and binds it.
    ///
    /// \throws std::runtime_error if the framebuffer can't be created, e.g. because it is too large.
    explicit OffscreenFramebuffer(const PixelSize& size);
    ~OffscreenFramebuffer();
    OffscreenFramebuffer(const OffscreenFramebuffer&) = delete;
    OffscreenFramebuffer& operator=(const OffscreenFramebuffer&) = delete;
    OffscreenFramebuffer(OffscreenFramebuffer&&) = delete;
    OffscreenFramebuffer& operator=(OffscreenFramebuffer&&) = delete;

private:
    GLuint framebuffer_ = 0;
    GLuint color_ = 0;  ///< Renderbuffer.
    GLuint depth_ = 0;  ///< Renderbuffer.
};

}  // namespace manifold
