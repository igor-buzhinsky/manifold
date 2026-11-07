#pragma once

#include <string>

namespace manifold {

/// Saves the current OpenGL read framebuffer as a PNG file.
///
/// \param path   File to write.
/// \param width  Width of the framebuffer in pixels.
/// \param height Height of the framebuffer in pixels.
/// \throws std::runtime_error if the file can't be written.
void save_screenshot(const std::string& path, int width, int height);

}  // namespace manifold
