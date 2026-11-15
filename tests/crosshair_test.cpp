#include "ui/crosshair.hpp"

#include <cstddef>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "ui/overlay_mesh.hpp"

namespace manifold {
namespace {

constexpr std::size_t kVerticesPerRectangle = 6;

/// The smallest and largest corner of the rectangle that starts at vertex `first`.
struct Bounds {
    glm::vec2 min;
    glm::vec2 max;
};

Bounds rectangle_bounds(const std::vector<OverlayVertex>& vertices, std::size_t first) {
    Bounds bounds{.min = vertices.at(first).position, .max = vertices.at(first).position};
    for (std::size_t i = first; i < first + kVerticesPerRectangle; ++i) {
        bounds.min = glm::min(bounds.min, vertices.at(i).position);
        bounds.max = glm::max(bounds.max, vertices.at(i).position);
    }
    return bounds;
}

TEST(CrosshairTest, ScaleGrowsWithThePictureHeight) {
    EXPECT_EQ(crosshair_scale(0), 1);
    EXPECT_EQ(crosshair_scale(720), 1);
    EXPECT_EQ(crosshair_scale(1079), 1);
    EXPECT_EQ(crosshair_scale(1080), 2);
    EXPECT_EQ(crosshair_scale(2160), 3);
}

TEST(CrosshairTest, IsAWhitePlusOverABlackOneAtTheCenter) {
    OverlayMesh mesh;
    add_crosshair(mesh, {.width = 800, .height = 450});
    const std::vector<OverlayVertex>& vertices = mesh.vertices();
    ASSERT_EQ(vertices.size(), 4 * kVerticesPerRectangle);
    // The outline comes first, so the white lines are drawn over it.
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const bool outline = i < 2 * kVerticesPerRectangle;
        EXPECT_EQ(vertices[i].color, outline ? glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) : glm::vec4(1.0f)) << i;
    }
    // The white bars cross at pixel (400, 225), 1 pixel thick and 6 long on each side.
    const Bounds horizontal = rectangle_bounds(vertices, 2 * kVerticesPerRectangle);
    EXPECT_EQ(horizontal.min, glm::vec2(394.0f, 225.0f));
    EXPECT_EQ(horizontal.max, glm::vec2(407.0f, 226.0f));
    const Bounds vertical = rectangle_bounds(vertices, 3 * kVerticesPerRectangle);
    EXPECT_EQ(vertical.min, glm::vec2(400.0f, 219.0f));
    EXPECT_EQ(vertical.max, glm::vec2(401.0f, 232.0f));
    // The outline is 1 pixel wider on every side.
    const Bounds outline = rectangle_bounds(vertices, 0);
    EXPECT_EQ(outline.min, glm::vec2(393.0f, 224.0f));
    EXPECT_EQ(outline.max, glm::vec2(408.0f, 227.0f));
}

TEST(CrosshairTest, LargePicturesGetALargerCrosshair) {
    OverlayMesh mesh;
    add_crosshair(mesh, {.width = 1920, .height = 1080});
    const Bounds horizontal = rectangle_bounds(mesh.vertices(), 2 * kVerticesPerRectangle);
    EXPECT_EQ(horizontal.min, glm::vec2(948.0f, 540.0f));
    EXPECT_EQ(horizontal.max, glm::vec2(974.0f, 542.0f));
}

}  // namespace
}  // namespace manifold
