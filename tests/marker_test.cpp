#include "render/marker.hpp"

#include <algorithm>
#include <cstddef>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(MarkerTest, ArrowLiesFlatAboveFeetAndPointsAlongYaw) {
    const glm::vec3 feet{2.0f, -3.0f, 1.5f};
    const float yaw = std::numbers::pi_v<float> / 2.0f;  // Towards +y.
    const std::vector<glm::vec3> arrow = build_arrow(feet, yaw);
    ASSERT_FALSE(arrow.empty());
    ASSERT_EQ(arrow.size() % 3, 0U);
    for (const glm::vec3& corner : arrow) {
        EXPECT_GT(corner.z, feet.z);
        EXPECT_LT(corner.z, feet.z + 0.1f);
    }
    // The tip is the farthest point along the yaw, half the length ahead of the feet.
    const auto tip =
        std::max_element(arrow.begin(), arrow.end(), [](const glm::vec3& a, const glm::vec3& b) { return a.y < b.y; });
    EXPECT_NEAR(tip->x, feet.x, 1e-5f);
    EXPECT_NEAR(tip->y, feet.y + (kArrowLength / 2.0f), 1e-5f);
}

TEST(MarkerTest, ArrowTrianglesFaceUp) {
    for (const glm::vec3& corner : {glm::vec3(0.0f), glm::vec3(5.0f, 1.0f, 2.0f)}) {
        const std::vector<glm::vec3> arrow = build_arrow(corner, 2.0f);
        for (std::size_t i = 0; i < arrow.size(); i += 3) {
            const glm::vec3 normal = glm::cross(arrow[i + 1] - arrow[i], arrow[i + 2] - arrow[i]);
            EXPECT_GT(normal.z, 0.0f);
        }
    }
}

}  // namespace
}  // namespace manifold
