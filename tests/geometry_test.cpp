#include "world/geometry.hpp"

#include <numbers>

#include <gtest/gtest.h>

namespace manifold {
namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 4.0f;

TEST(GeometryTest, DistantCircleDoesNotIntersect) {
    const RotatedSquare square{.center = {0.0f, 0.0f}, .half_size = 1.0f, .yaw = 0.0f};
    EXPECT_FALSE(intersects(square, {.center = {5.0f, 0.0f}, .radius = 1.0f}));
}

TEST(GeometryTest, OverlappingCircleIntersects) {
    const RotatedSquare square{.center = {0.0f, 0.0f}, .half_size = 1.0f, .yaw = 0.0f};
    EXPECT_TRUE(intersects(square, {.center = {1.5f, 0.0f}, .radius = 1.0f}));
}

TEST(GeometryTest, CircleInsideSquareIntersects) {
    const RotatedSquare square{.center = {3.0f, 3.0f}, .half_size = 2.0f, .yaw = 0.3f};
    EXPECT_TRUE(intersects(square, {.center = {3.0f, 3.0f}, .radius = 0.1f}));
}

TEST(GeometryTest, SquareInsideCircleIntersects) {
    const RotatedSquare square{.center = {0.0f, 0.0f}, .half_size = 0.5f, .yaw = 0.3f};
    EXPECT_TRUE(intersects(square, {.center = {0.0f, 0.0f}, .radius = 10.0f}));
}

TEST(GeometryTest, CircleNearCornerOfAxisAlignedSquare) {
    // The corner (1, 1) is about 0.71 m from (1.5, 1.5), the sides are 0.5 m away.
    const RotatedSquare square{.center = {0.0f, 0.0f}, .half_size = 1.0f, .yaw = 0.0f};
    EXPECT_FALSE(intersects(square, {.center = {1.5f, 1.5f}, .radius = 0.6f}));
    EXPECT_TRUE(intersects(square, {.center = {1.5f, 1.5f}, .radius = 0.8f}));
}

TEST(GeometryTest, RotationMovesCorners) {
    // Rotated by 45 degrees, a corner points along +x and reaches x = sqrt(2) ~ 1.41.
    const RotatedSquare square{.center = {0.0f, 0.0f}, .half_size = 1.0f, .yaw = kQuarterTurn};
    const Circle circle{.center = {1.6f, 0.0f}, .radius = 0.3f};
    EXPECT_TRUE(intersects(square, circle));
    EXPECT_FALSE(
        intersects({.center = {0.0f, 0.0f}, .half_size = 1.0f, .yaw = 0.0f}, {.center = {1.6f, 1.6f}, .radius = 0.3f}));
}

TEST(GeometryTest, RotationIsAroundSquareCenter) {
    const RotatedSquare square{.center = {10.0f, -5.0f}, .half_size = 1.0f, .yaw = kQuarterTurn};
    EXPECT_TRUE(intersects(square, {.center = {11.6f, -5.0f}, .radius = 0.3f}));
    EXPECT_FALSE(intersects(square, {.center = {11.6f, -3.4f}, .radius = 0.3f}));
}

}  // namespace
}  // namespace manifold
