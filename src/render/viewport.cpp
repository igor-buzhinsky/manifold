#include "render/viewport.hpp"

#include <cmath>

#include <glm/common.hpp>
#include <glm/vec2.hpp>

namespace manifold {

Viewport fit_viewport(const PixelSize& framebuffer, float aspect_ratio) {
    const auto width = static_cast<float>(framebuffer.width);
    const auto height = static_cast<float>(framebuffer.height);
    Viewport viewport{.width = framebuffer.width, .height = framebuffer.height};
    if (width > height * aspect_ratio) {
        viewport.width = static_cast<int>(std::lround(height * aspect_ratio));
        viewport.x = (framebuffer.width - viewport.width) / 2;
    } else {
        viewport.height = static_cast<int>(std::lround(width / aspect_ratio));
        viewport.y = (framebuffer.height - viewport.height) / 2;
    }
    return viewport;
}

glm::vec2 pixel_center(const glm::vec2& point) { return glm::floor(point) + glm::vec2(0.5f); }

glm::vec2 window_to_viewport(const glm::vec2& point, const PixelSize& window, const PixelSize& framebuffer,
                             const Viewport& viewport) {
    glm::vec2 pixels = point;
    if (window.width > 0 && window.height > 0) {
        pixels *= glm::vec2(static_cast<float>(framebuffer.width) / static_cast<float>(window.width),
                            static_cast<float>(framebuffer.height) / static_cast<float>(window.height));
    }
    // The viewport's origin is its bottom-left corner, counted from the bottom of the framebuffer.
    const int top = framebuffer.height - (viewport.y + viewport.height);
    return pixels - glm::vec2(static_cast<float>(viewport.x), static_cast<float>(top));
}

}  // namespace manifold
