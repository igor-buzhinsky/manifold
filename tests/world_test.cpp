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

TEST(WorldTest, StoresRamps) {
    World world({.x_min = -5.0f, .x_max = 5.0f, .y_min = -5.0f, .y_max = 5.0f, .height = 3.0f});
    EXPECT_TRUE(world.ramps().empty());
    world.add_ramp({.base_center = {1.0f, 2.0f, 0.0f}, .half_length = 2.0f, .half_width = 1.0f, .rise = 3.0f});
    ASSERT_EQ(world.ramps().size(), 1U);
    EXPECT_FLOAT_EQ(world.ramps()[0].half_length, 2.0f);
}

TEST(WorldTest, RampHeightGrowsAlongSlope) {
    const Ramp ramp{.half_length = 2.0f, .half_width = 1.0f, .rise = 3.0f};
    EXPECT_FLOAT_EQ(ramp.height_at(-2.0f), 0.0f);
    EXPECT_FLOAT_EQ(ramp.height_at(0.0f), 1.5f);
    EXPECT_FLOAT_EQ(ramp.height_at(2.0f), 3.0f);
}

TEST(WorldTest, StoresObstacles) {
    World world(kBounds);
    EXPECT_TRUE(world.obstacles().empty());
    world.add_obstacle({.base_center = {1.0f, 2.0f, 0.0f}, .half_size = 0.5f, .height = 3.0f, .yaw = 0.25f});
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

TEST(LevelExitTest, BodyTouchesCubeFromTheSideAndBelow) {
    const LevelExit exit{.floor_point = {1.0f, 2.0f, 3.3f}};
    EXPECT_EQ(exit.base_center(), glm::vec3(1.0f, 2.0f, 3.3f + LevelExit::kHoverHeight));
    EXPECT_EQ(exit.center(), exit.base_center() + glm::vec3(0.0f, 0.0f, LevelExit::kSize / 2.0f));
    const auto body = [](float x, float z) {
        return VerticalCylinder{.base = {x, 2.0f, z}, .radius = 0.6f, .height = 1.8f};
    };
    const float reach = (LevelExit::kSize / 2.0f) + 0.6f;
    EXPECT_TRUE(exit.touches(body(1.0f, 3.3f)));
    EXPECT_TRUE(exit.touches(body(1.0f + reach - 0.01f, 3.3f)));
    EXPECT_FALSE(exit.touches(body(1.0f + reach + 0.01f, 3.3f)));
    // Below the cube: the head must reach its bottom.
    const float bottom = 3.3f + LevelExit::kHoverHeight;
    EXPECT_TRUE(exit.touches(body(1.0f, bottom - 1.79f)));
    EXPECT_FALSE(exit.touches(body(1.0f, bottom - 1.81f)));
    // Above the cube.
    EXPECT_TRUE(exit.touches(body(1.0f, bottom + LevelExit::kSize - 0.01f)));
    EXPECT_FALSE(exit.touches(body(1.0f, bottom + LevelExit::kSize + 0.01f)));
}

TEST(WorldTest, HasNoExitUntilSet) {
    World world(kBounds);
    EXPECT_FALSE(world.exit().has_value());
    world.set_exit({.floor_point = {1.0f, 2.0f, 0.0f}});
    ASSERT_TRUE(world.exit().has_value());
    EXPECT_EQ(world.exit()->floor_point, glm::vec3(1.0f, 2.0f, 0.0f));
}

}  // namespace
}  // namespace manifold
