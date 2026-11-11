#include "render/screenshot.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <stb_image_write.h>

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

}  // namespace manifold
