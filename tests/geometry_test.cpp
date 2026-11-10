#include "world/geometry.hpp"

#include <numbers>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

namespace manifold {
namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 4.0f;

TEST(GeometryTest, DistantCircleDoesNotIntersect) {
    const RotatedRectangle square{.center = {0.0f, 0.0f}, .half_size = {1.0f, 1.0f}, .yaw = 0.0f};
    EXPECT_FALSE(intersects(square, {.center = {5.0f, 0.0f}, .radius = 1.0f}));
}

TEST(GeometryTest, OverlappingCircleIntersects) {
    const RotatedRectangle square{.center = {0.0f, 0.0f}, .half_size = {1.0f, 1.0f}, .yaw = 0.0f};
    EXPECT_TRUE(intersects(square, {.center = {1.5f, 0.0f}, .radius = 1.0f}));
}

TEST(GeometryTest, CircleInsideSquareIntersects) {
    const RotatedRectangle square{.center = {3.0f, 3.0f}, .half_size = {2.0f, 2.0f}, .yaw = 0.3f};
    EXPECT_TRUE(intersects(square, {.center = {3.0f, 3.0f}, .radius = 0.1f}));
}

TEST(GeometryTest, SquareInsideCircleIntersects) {
    const RotatedRectangle square{.center = {0.0f, 0.0f}, .half_size = {0.5f, 0.5f}, .yaw = 0.3f};
    EXPECT_TRUE(intersects(square, {.center = {0.0f, 0.0f}, .radius = 10.0f}));
}

TEST(GeometryTest, CircleNearCornerOfAxisAlignedSquare) {
    // The corner (1, 1) is about 0.71 m from (1.5, 1.5), the sides are 0.5 m away.
    const RotatedRectangle square{.center = {0.0f, 0.0f}, .half_size = {1.0f, 1.0f}, .yaw = 0.0f};
    EXPECT_FALSE(intersects(square, {.center = {1.5f, 1.5f}, .radius = 0.6f}));
    EXPECT_TRUE(intersects(square, {.center = {1.5f, 1.5f}, .radius = 0.8f}));
}

TEST(GeometryTest, RotationMovesCorners) {
    // Rotated by 45 degrees, a corner points along +x and reaches x = sqrt(2) ~ 1.41.
    const RotatedRectangle square{.center = {0.0f, 0.0f}, .half_size = {1.0f, 1.0f}, .yaw = kQuarterTurn};
    const Circle circle{.center = {1.6f, 0.0f}, .radius = 0.3f};
    EXPECT_TRUE(intersects(square, circle));
    EXPECT_FALSE(intersects({.center = {0.0f, 0.0f}, .half_size = {1.0f, 1.0f}, .yaw = 0.0f},
                            {.center = {1.6f, 1.6f}, .radius = 0.3f}));
}

TEST(GeometryTest, RotationIsAroundSquareCenter) {
    const RotatedRectangle square{.center = {10.0f, -5.0f}, .half_size = {1.0f, 1.0f}, .yaw = kQuarterTurn};
    EXPECT_TRUE(intersects(square, {.center = {11.6f, -5.0f}, .radius = 0.3f}));
    EXPECT_FALSE(intersects(square, {.center = {11.6f, -3.4f}, .radius = 0.3f}));
}

TEST(GeometryTest, RectangleUsesBothHalfSizes) {
    const RotatedRectangle rectangle{.center = {0.0f, 0.0f}, .half_size = {2.0f, 0.5f}, .yaw = 0.0f};
    EXPECT_TRUE(intersects(rectangle, {.center = {2.5f, 0.0f}, .radius = 0.6f}));
    EXPECT_FALSE(intersects(rectangle, {.center = {0.0f, 1.2f}, .radius = 0.6f}));
}

TEST(GeometryTest, RectanglesOverlap) {
    const RotatedRectangle first{.center = {0.0f, 0.0f}, .half_size = {2.0f, 1.0f}, .yaw = 0.0f};
    EXPECT_TRUE(intersects(first, {.center = {3.5f, 0.0f}, .half_size = {2.0f, 1.0f}, .yaw = 0.0f}));
    EXPECT_FALSE(intersects(first, {.center = {4.5f, 0.0f}, .half_size = {2.0f, 1.0f}, .yaw = 0.0f}));
    // Turned by 90 degrees, the second rectangle reaches 1 m less far along x.
    EXPECT_FALSE(intersects(first, {.center = {3.5f, 0.0f}, .half_size = {2.0f, 1.0f}, .yaw = 2.0f * kQuarterTurn}));
}

TEST(GeometryTest, RectanglesSeparatedOnlyAlongRotatedAxis) {
    // The boxes around these two would overlap, but the rectangles don't: a diamond next to a square corner.
    const RotatedRectangle square{.center = {0.0f, 0.0f}, .half_size = {1.0f, 1.0f}, .yaw = 0.0f};
    const RotatedRectangle diamond{.center = {2.3f, 2.3f}, .half_size = {1.0f, 1.0f}, .yaw = kQuarterTurn};
    EXPECT_FALSE(intersects(square, diamond));
    EXPECT_FALSE(intersects(diamond, square));
    EXPECT_TRUE(intersects(square, {.center = {1.6f, 1.6f}, .half_size = {1.0f, 1.0f}, .yaw = kQuarterTurn}));
}

TEST(GeometryTest, BoundingBoxOfRotatedRectangle) {
    const RotatedRectangle rectangle{.center = {5.0f, 5.0f}, .half_size = {2.0f, 1.0f}, .yaw = 2.0f * kQuarterTurn};
    const glm::vec2 half = bounding_half_size(rectangle);
    EXPECT_NEAR(half.x, 1.0f, 1e-6f);
    EXPECT_NEAR(half.y, 2.0f, 1e-6f);
}

}  // namespace
}  // namespace manifold
