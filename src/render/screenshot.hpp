#pragma once

#include <string>

namespace manifold {

// Saves the current OpenGL read framebuffer (width x height pixels) as a PNG file.
void save_screenshot(const std::string& path, int width, int height);

}  // namespace manifold
