#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "physics/collision.hpp"
#include "util/random.hpp"
#include "world/door.hpp"
#include "world/geometry.hpp"
#include "world/obstacle.hpp"
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
    world.add_obstacle({.base_center = {1.0f, 2.0f, 0.0f}, .half_size = glm::vec2(0.5f), .height = 3.0f, .yaw = 0.25f});
    ASSERT_EQ(world.obstacles().size(), 1U);
    EXPECT_EQ(world.obstacles()[0].base_center, glm::vec3(1.0f, 2.0f, 0.0f));
    EXPECT_FLOAT_EQ(world.obstacles()[0].footprint().yaw, 0.25f);
}

TEST(WorldTest, AddingADoorAddsItsFramesAsObstacles) {
    World world({.x_min = -5.0f, .x_max = 5.0f, .y_min = -5.0f, .y_max = 5.0f, .height = 3.0f});
    const Door door{.base_center = {1.0f, 0.0f, 0.0f}, .width = 2.0f, .height = 3.0f, .yaw = 0.5f, .sounds = {}};
    world.add_door(door);
    ASSERT_EQ(world.doors().size(), 1U);
    EXPECT_EQ(world.doors()[0].base_center, door.base_center);
    ASSERT_EQ(world.obstacles().size(), 2U);
    EXPECT_EQ(world.obstacles()[0].base_center, door.frame(DoorSide::kNegative).base_center);
    EXPECT_EQ(world.obstacles()[1].base_center, door.frame(DoorSide::kPositive).base_center);
    EXPECT_EQ(world.obstacles()[0].surface, Surface::kDark);
}

TEST(WorldTest, FindsAllObstaclesAndRampsThatABodyTouches) {
    // Random rotated blocks and ramps, partly outside the bounds, and random bodies, partly outside too.
    World world({.x_min = -10.0f, .x_max = 10.0f, .y_min = -10.0f, .y_max = 10.0f, .height = 5.0f});
    Rng random(7);
    const auto position = [&random] { return random_float(random, -13.0f, 13.0f); };
    const auto size = [&random] { return random_float(random, 0.05f, 3.0f); };
    const auto angle = [&random] { return random_float(random, 0.0f, 2.0f * std::numbers::pi_v<float>); };
    for (int i = 0; i < 200; ++i) {
        world.add_obstacle({.base_center = {position(), position(), 0.0f},
                            .half_size = {size(), size()},
                            .height = 2.0f,
                            .yaw = angle()});
        world.add_ramp({.base_center = {position(), position(), 0.0f},
                        .half_length = size(),
                        .half_width = size(),
                        .rise = 1.0f,
                        .yaw = angle()});
    }

    std::vector<std::size_t> obstacles;
    std::vector<std::size_t> ramps;
    int touching = 0;
    for (int i = 0; i < 5000; ++i) {
        const VerticalCylinder body{.base = {position(), position(), 0.0f}, .radius = 0.3f, .height = 1.8f};
        const Box2 box{.min = glm::vec2(body.base) - body.radius, .max = glm::vec2(body.base) + body.radius};
        world.find_obstacles_near(box, obstacles);
        world.find_ramps_near(box, ramps);
        ASSERT_TRUE(std::ranges::is_sorted(obstacles));
        for (std::size_t index = 0; index < world.obstacles().size(); ++index) {
            if (find_contact(body, world.obstacles()[index])) {
                ++touching;
                EXPECT_TRUE(std::ranges::find(obstacles, index) != obstacles.end())
                    << "obstacle " << index << " body " << i;
            }
        }
        for (std::size_t index = 0; index < world.ramps().size(); ++index) {
            if (find_contact(body, world.ramps()[index])) {
                ++touching;
                EXPECT_TRUE(std::ranges::find(ramps, index) != ramps.end()) << "ramp " << index << " body " << i;
            }
        }
        // Far fewer than all are found.
        EXPECT_LT(obstacles.size(), world.obstacles().size() / 4);
    }
    EXPECT_GT(touching, 1000);
}

TEST(PlayerTest, ViewDirectionFollowsYawAndPitch) {
    Player player({});
    player.rotate(std::numbers::pi_v<float> / 2.0f, Player::kMaxPitch);
    const glm::vec3 direction = player.view_direction();
    EXPECT_NEAR(direction.x, 0.0f, 1e-6f);
    EXPECT_NEAR(direction.y, std::cos(Player::kMaxPitch), 1e-6f);
    EXPECT_NEAR(direction.z, std::sin(Player::kMaxPitch), 1e-6f);
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

TEST(PlayerTest, PitchIsClampedTo40Degrees) {
    Player player({});
    EXPECT_NEAR(Player::kMaxPitch, 40.0f * std::numbers::pi_v<float> / 180.0f, 1e-6f);
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

TEST(PlayerTest, YawStaysBelowAFullTurnAfterATinyTurnRightFromZero) {
    constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
    // The yaw becomes a tiny negative number, and adding a full turn to it rounds to a full turn.
    Player player({.yaw = 0.0f});
    player.rotate(-1e-8f, 0.0f);
    EXPECT_GE(player.yaw(), 0.0f);
    EXPECT_LT(player.yaw(), kFullTurn);
    player.set_view(-1e-8f, 0.0f);
    EXPECT_LT(player.yaw(), kFullTurn);
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
