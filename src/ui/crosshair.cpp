#include "ui/crosshair.hpp"

#include <algorithm>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"

namespace manifold {

namespace {

/// Picture height at which the crosshair's pixels become 2 pixels large, see crosshair_scale().
constexpr int kScaleStepHeight = 1080;
/// Length of each arm of the plus from the edge of the central pixel, at scale 1, in pixels.
constexpr float kArmLength = 6.0f;
constexpr glm::vec4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};
// Opaque, because the outlines of the two bars overlap around the center.
constexpr glm::vec4 kBlack{0.0f, 0.0f, 0.0f, 1.0f};

}  // namespace

int crosshair_scale(int picture_height) { return std::max(1, (picture_height / kScaleStepHeight) + 1); }

void add_crosshair(OverlayMesh& mesh, const PixelSize& picture) {
    const auto scale = static_cast<float>(crosshair_scale(picture.height));
    // The center is rounded down to whole pixels, so the lines stay sharp.
    const int center_x = picture.width / 2;
    const int center_y = picture.height / 2;
    const glm::vec2 center{static_cast<float>(center_x), static_cast<float>(center_y)};
    const float arm = kArmLength * scale;
    // The central square of the plus, `scale` pixels wide, and the two bars through it.
    const glm::vec2 horizontal_corner = center - glm::vec2(arm, 0.0f);
    const glm::vec2 horizontal_size{(2.0f * arm) + scale, scale};
    const glm::vec2 vertical_corner = center - glm::vec2(0.0f, arm);
    const glm::vec2 vertical_size{scale, (2.0f * arm) + scale};
    const glm::vec2 outline(scale);
    mesh.add_rectangle(horizontal_corner - outline, horizontal_size + (2.0f * outline), kBlack);
    mesh.add_rectangle(vertical_corner - outline, vertical_size + (2.0f * outline), kBlack);
    mesh.add_rectangle(horizontal_corner, horizontal_size, kWhite);
    mesh.add_rectangle(vertical_corner, vertical_size, kWhite);
}

}  // namespace manifold
