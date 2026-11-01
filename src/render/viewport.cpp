#include "render/viewport.hpp"

#include <cmath>

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

}  // namespace manifold
