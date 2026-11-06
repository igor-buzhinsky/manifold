#include "world/world.hpp"

#include <numbers>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "world/geometry.hpp"
#include "world/player.hpp"

namespace manifold {
namespace {

constexpr WorldBounds kBounds{.x_min = -5.0f, .x_max = 5.0f, .y_min = 0.0f, .y_max = 10.0f, .height = 3.0f};

TEST(WorldBoundsTest, ContainsPointsInsideAndOnBoundary) {
    EXPECT_TRUE(kBounds.contains({0.0f, 5.0f, 1.5f}));
    EXPECT_TRUE(kBounds.contains({-5.0f, 0.0f, 0.0f}));
    EXPECT_TRUE(kBounds.contains({5.0f, 10.0f, 3.0f}));
}

TEST(WorldBoundsTest, RejectsPointsOutside) {
    EXPECT_FALSE(kBounds.contains({5.1f, 5.0f, 1.0f}));
    EXPECT_FALSE(kBounds.contains({0.0f, -0.1f, 1.0f}));
    EXPECT_FALSE(kBounds.contains({0.0f, 5.0f, -0.1f}));
    EXPECT_FALSE(kBounds.contains({0.0f, 5.0f, 3.1f}));
}

TEST(WorldTest, StoresObstacles) {
    World world(kBounds);
    EXPECT_TRUE(world.obstacles().empty());
    world.add_obstacle({.base_center = {1.0f, 2.0f, 0.0f}, .half_size = 0.5f, .yaw = 0.25f});
    ASSERT_EQ(world.obstacles().size(), 1U);
    EXPECT_EQ(world.obstacles()[0].base_center, glm::vec3(1.0f, 2.0f, 0.0f));
    EXPECT_FLOAT_EQ(world.obstacles()[0].footprint().yaw, 0.25f);
}

TEST(PlayerTest, FootprintIsCircleUnderPlayer) {
    const Player player({.position = {1.0f, -2.0f, 0.0f}, .yaw = 0.0f, .radius = 0.3f, .height = 1.8f});
    const Circle footprint = player.footprint();
    EXPECT_FLOAT_EQ(footprint.center.x, 1.0f);
    EXPECT_FLOAT_EQ(footprint.center.y, -2.0f);
    EXPECT_FLOAT_EQ(footprint.radius, 0.3f);
}

TEST(PlayerTest, EyesAreSlightlyBelowTopOfHead) {
    const Player player({.position = {1.0f, -2.0f, 0.5f}, .yaw = 0.0f, .radius = 0.3f, .height = 1.8f});
    const glm::vec3 eye = player.eye_position();
    EXPECT_FLOAT_EQ(eye.x, 1.0f);
    EXPECT_FLOAT_EQ(eye.y, -2.0f);
    EXPECT_FLOAT_EQ(eye.z, 0.5f + 1.8f - Player::kEyeDepth);
    EXPECT_GT(Player::kEyeDepth, 0.0f);
}

TEST(PlayerTest, RotateTurnsAndLooks) {
    Player player({.yaw = 1.0f});
    player.rotate(0.5f, 0.2f);
    EXPECT_FLOAT_EQ(player.yaw(), 1.5f);
    EXPECT_FLOAT_EQ(player.pitch(), 0.2f);
    player.rotate(-0.5f, -0.3f);
    EXPECT_FLOAT_EQ(player.yaw(), 1.0f);
    EXPECT_FLOAT_EQ(player.pitch(), -0.1f);
}

TEST(PlayerTest, PitchIsClampedTo30Degrees) {
    Player player({});
    EXPECT_NEAR(Player::kMaxPitch, 30.0f * std::numbers::pi_v<float> / 180.0f, 1e-6f);
    player.rotate(0.0f, 2.0f);
    EXPECT_FLOAT_EQ(player.pitch(), Player::kMaxPitch);
    player.rotate(0.0f, -5.0f);
    EXPECT_FLOAT_EQ(player.pitch(), -Player::kMaxPitch);
}

TEST(PlayerTest, YawWrapsAround) {
    constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
    Player player({.yaw = 0.5f});
    player.rotate(-1.0f, 0.0f);
    EXPECT_NEAR(player.yaw(), kFullTurn - 0.5f, 1e-5f);
    player.rotate(1.0f + (3.0f * kFullTurn), 0.0f);
    EXPECT_NEAR(player.yaw(), 0.5f, 1e-4f);
}

}  // namespace
}  // namespace manifold
