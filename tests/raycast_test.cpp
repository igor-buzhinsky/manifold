#include "physics/raycast.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <random>
#include <span>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "door_level.hpp"
#include "level/level.hpp"
#include "world/door.hpp"
#include "world/obstacle.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

// A 2 x 2 x 2 m block with its bottom at z = 1, centered on (5, 0).
constexpr Obstacle kBlock{.base_center = {5.0f, 0.0f, 1.0f}, .half_size = {1.0f, 1.0f}, .height = 2.0f};

TEST(RaycastTest, HitsTheNearFaceOfABlock) {
    const std::optional<float> hit =
        cast_ray({.origin = {0.0f, 0.0f, 2.0f}, .direction = {1.0f, 0.0f, 0.0f}}, kBlock, 10.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_FLOAT_EQ(*hit, 4.0f);
}

TEST(RaycastTest, MissesBesideAboveAndBeyondReach) {
    const glm::vec3 ahead{1.0f, 0.0f, 0.0f};
    EXPECT_FALSE(cast_ray({.origin = {0.0f, 1.5f, 2.0f}, .direction = ahead}, kBlock, 10.0f));
    EXPECT_FALSE(cast_ray({.origin = {0.0f, 0.0f, 3.5f}, .direction = ahead}, kBlock, 10.0f));
    EXPECT_FALSE(cast_ray({.origin = {0.0f, 0.0f, 2.0f}, .direction = -ahead}, kBlock, 10.0f));
    EXPECT_FALSE(cast_ray({.origin = {0.0f, 0.0f, 2.0f}, .direction = ahead}, kBlock, 3.9f));
}

TEST(RaycastTest, StartsInsideAtDistanceZero) {
    const std::optional<float> hit =
        cast_ray({.origin = {5.0f, 0.5f, 2.0f}, .direction = {0.0f, 1.0f, 0.0f}}, kBlock, 1.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_FLOAT_EQ(*hit, 0.0f);
}

TEST(RaycastTest, HitsTopFromAboveAndRotatedBlocks) {
    const std::optional<float> top =
        cast_ray({.origin = {5.0f, 0.0f, 10.0f}, .direction = {0.0f, 0.0f, -1.0f}}, kBlock, 10.0f);
    ASSERT_TRUE(top.has_value());
    EXPECT_FLOAT_EQ(*top, 7.0f);

    // Turned by 45 degrees, the corner of the block points at the origin, sqrt(2) closer than its center.
    Obstacle turned = kBlock;
    turned.yaw = std::numbers::pi_v<float> / 4.0f;
    const std::optional<float> corner =
        cast_ray({.origin = {0.0f, 0.0f, 2.0f}, .direction = {1.0f, 0.0f, 0.0f}}, turned, 10.0f);
    ASSERT_TRUE(corner.has_value());
    EXPECT_NEAR(*corner, 5.0f - std::sqrt(2.0f), 1e-5f);
}

/// A ray from `origin` at eye height towards `target`, in the level of make_door_level().
Ray ray_towards(const glm::vec2& origin, const glm::vec2& target) {
    return {.origin = {origin, 1.5f}, .direction = glm::vec3(glm::normalize(target - origin), 0.0f)};
}

TEST(RaycastTest, FindsClosedDoorsAndFrames) {
    const Level level = make_door_level();
    const std::array<DoorState, 1> closed{};
    const std::optional<RayHit> door = cast_ray(level.world, closed, ray_towards({0.0f, -3.0f}, {0.0f, 0.0f}), 5.0f);
    ASSERT_TRUE(door.has_value());
    EXPECT_EQ(door->target, RayTarget::kDoor);
    EXPECT_EQ(door->door, 0U);
    EXPECT_FLOAT_EQ(door->distance, 3.0f - (Door::kThickness / 2.0f));

    // Frames are obstacles of the world too, but count as frames.
    const std::optional<RayHit> frame = cast_ray(level.world, closed, ray_towards({0.9f, -3.0f}, {0.9f, 0.0f}), 5.0f);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->target, RayTarget::kDoorFrame);
    EXPECT_FLOAT_EQ(frame->distance, 3.0f - (Door::kFrameDepth / 2.0f));

    const std::optional<RayHit> wall = cast_ray(level.world, closed, ray_towards({2.0f, -3.0f}, {2.0f, 0.0f}), 5.0f);
    ASSERT_TRUE(wall.has_value());
    EXPECT_EQ(wall->target, RayTarget::kWorld);
}

TEST(RaycastTest, PassableDoorsLetRaysThroughAndMissingStatesCountAsClosed) {
    const Level level = make_door_level();
    const std::array<DoorState, 1> open{DoorState{.openness = 1.0f, .target = 1.0f}};
    const Ray through = ray_towards({0.0f, -3.0f}, {0.0f, 0.0f});
    EXPECT_FALSE(cast_ray(level.world, open, through, 5.0f).has_value());
    const std::optional<RayHit> hit = cast_ray(level.world, std::span<const DoorState>(), through, 5.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->target, RayTarget::kDoor);
}

TEST(RaycastTest, NearerObstaclesHideDoors) {
    Level level = make_door_level();
    level.world.add_obstacle({.base_center = {0.0f, -1.5f, 0.0f}, .half_size = {0.2f, 0.2f}, .height = 3.0f});
    const std::array<DoorState, 1> closed{};
    const std::optional<RayHit> hit = cast_ray(level.world, closed, ray_towards({0.0f, -3.0f}, {0.0f, 0.0f}), 5.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->target, RayTarget::kWorld);
    EXPECT_FLOAT_EQ(hit->distance, 1.3f);
}

/// The distance to the nearest of all obstacles, door blockers and door frames of a world, tested one by one.
std::optional<float> nearest_of_all(const World& world, const Ray& ray, float max_distance) {
    std::optional<float> nearest;
    const auto keep = [&](const Obstacle& obstacle) {
        if (const std::optional<float> distance = cast_ray(ray, obstacle, max_distance)) {
            nearest = std::min(nearest.value_or(*distance), *distance);
        }
    };
    std::ranges::for_each(world.obstacles(), keep);
    for (const Door& door : world.doors()) {
        keep(door.blocker());
        keep(door.frame(DoorSide::kNegative));
        keep(door.frame(DoorSide::kPositive));
    }
    return nearest;
}

TEST(RaycastTest, FindsTheSameHitsAsTestingEverything) {
    // Random rotated blocks and doors, partly outside the bounds, and random rays, some along x or y, some unlimited.
    World world({.x_min = -20.0f, .x_max = 20.0f, .y_min = -20.0f, .y_max = 20.0f, .height = 3.0f});
    std::mt19937 random(11);
    std::uniform_real_distribution<float> position(-24.0f, 24.0f);
    std::uniform_real_distribution<float> size(0.05f, 1.0f);
    std::uniform_real_distribution<float> angle(0.0f, 2.0f * std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> height(0.0f, 3.0f);
    for (int i = 0; i < 150; ++i) {
        world.add_obstacle({.base_center = {position(random), position(random), 0.0f},
                            .half_size = {size(random), size(random)},
                            .height = height(random),
                            .yaw = angle(random)});
        world.add_door({.base_center = {position(random), position(random), 0.0f},
                        .width = 2.0f * size(random),
                        .height = 2.5f,
                        .yaw = angle(random)});
    }

    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    int hits = 0;
    for (int i = 0; i < 3000; ++i) {
        const float yaw = (i % 3 == 0) ? static_cast<float>(i % 4) * std::numbers::pi_v<float> / 2.0f : angle(random);
        const float pitch = (i % 5 == 0) ? 0.0f : (height(random) - 1.5f) / 3.0f;
        const Ray ray{.origin = {position(random), position(random), height(random)},
                      .direction = {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)}};
        const float max_distance = (i % 7 == 0) ? kInfinity : 2.0f * height(random);
        const std::optional<RayHit> hit = cast_ray(world, std::span<const DoorState>(), ray, max_distance);
        const std::optional<float> expected = nearest_of_all(world, ray, max_distance);
        ASSERT_EQ(hit.has_value(), expected.has_value()) << "ray " << i;
        if (hit) {
            ++hits;
            EXPECT_NEAR(hit->distance, *expected, 1e-3f) << "ray " << i;
        }
    }
    EXPECT_GT(hits, 500);
}

}  // namespace
}  // namespace manifold
