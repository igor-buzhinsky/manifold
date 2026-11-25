#include "physics/raycast.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "door_level.hpp"
#include "level/level.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"
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

TEST(RaycastTest, FindsChestsByTheirBodiesAndLidsAndTheNearestHidesTheOthers) {
    Level level = make_door_level();
    // A chest in front of the door, its front towards -y.
    level.world.add_chest({.base_center = {0.0f, -1.0f, 0.0f}, .yaw = -std::numbers::pi_v<float> / 2.0f});
    const std::array<DoorState, 1> closed{};
    const glm::vec3 along_y{0.0f, 1.0f, 0.0f};
    const std::optional<RayHit> body =
        cast_ray(level.world, closed, {.origin = {0.0f, -3.0f, 0.1f}, .direction = along_y}, 5.0f);
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(body->target, RayTarget::kChest);
    EXPECT_EQ(body->chest, 0U);
    EXPECT_FLOAT_EQ(body->distance, 2.0f - (Chest::kDepth / 2.0f));
    // The lid reaches out over the body.
    const std::optional<RayHit> lid =
        cast_ray(level.world, closed, {.origin = {0.0f, -3.0f, Chest::kHeight - 0.01f}, .direction = along_y}, 5.0f);
    ASSERT_TRUE(lid.has_value());
    EXPECT_EQ(lid->target, RayTarget::kChest);
    EXPECT_FLOAT_EQ(lid->distance, 2.0f - (Chest::kDepth / 2.0f) - Chest::kLidOverhang);
    // Above the chest, the door is hit.
    const std::optional<RayHit> above =
        cast_ray(level.world, closed, {.origin = {0.0f, -3.0f, 1.0f}, .direction = along_y}, 5.0f);
    ASSERT_TRUE(above.has_value());
    EXPECT_EQ(above->target, RayTarget::kDoor);
    // A block in front of the chest hides it.
    level.world.add_obstacle({.base_center = {0.0f, -2.0f, 0.0f}, .half_size = {0.1f, 0.1f}, .height = 0.2f});
    const std::optional<RayHit> hidden =
        cast_ray(level.world, closed, {.origin = {0.0f, -3.0f, 0.1f}, .direction = along_y}, 5.0f);
    ASSERT_TRUE(hidden.has_value());
    EXPECT_EQ(hidden->target, RayTarget::kWorld);
    // Beyond reach, it isn't hit.
    EXPECT_FALSE(cast_ray(level.world, closed, {.origin = {0.0f, -3.0f, 0.3f}, .direction = along_y}, 1.0f));
}

/// The first hit of a ray, found by testing all obstacles, doors and chests of a world: cast_ray() as it was before the
/// grid.
std::optional<RayHit> cast_ray_testing_all(const World& world, std::span<const DoorState> doors, const Ray& ray,
                                           float max_distance) {
    constexpr float kTieTolerance = 1e-4f;  // As in raycast.cpp.
    std::optional<RayHit> nearest;
    const auto keep_nearest = [&nearest](const RayHit& hit) {
        if (!nearest || hit.distance < nearest->distance) {
            nearest = hit;
        }
    };
    for (std::size_t i = 0; i < world.doors().size(); ++i) {
        const Door& door = world.doors()[i];
        if (i >= doors.size() || !doors[i].passable()) {
            if (const std::optional<float> distance = cast_ray(ray, door.blocker(), max_distance)) {
                keep_nearest({.distance = *distance, .target = RayTarget::kDoor, .door = i});
            }
        }
        for (const DoorSide side : {DoorSide::kNegative, DoorSide::kPositive}) {
            if (const std::optional<float> distance = cast_ray(ray, door.frame(side), max_distance)) {
                keep_nearest({.distance = *distance, .target = RayTarget::kDoorFrame, .door = i});
            }
        }
    }
    const auto keep_chest = [&](std::size_t chest, const Obstacle& part) {
        if (const std::optional<float> distance = cast_ray(ray, part, max_distance)) {
            keep_nearest({.distance = *distance, .target = RayTarget::kChest, .chest = chest});
        }
    };
    for (std::size_t i = 0; i < world.chests().size(); ++i) {
        keep_chest(i, world.chests()[i].body());
        keep_chest(i, world.chests()[i].lid());
    }
    for (const Obstacle& obstacle : world.obstacles()) {
        const float limit = nearest ? nearest->distance - kTieTolerance : max_distance;
        if (const std::optional<float> distance = cast_ray(ray, obstacle, limit)) {
            nearest = RayHit{.distance = *distance, .target = RayTarget::kWorld};
        }
    }
    return nearest;
}

/// A random unit direction. Some are exactly along x, y or z, some horizontal, the others anywhere.
glm::vec3 random_direction(Rng& random) {
    static constexpr std::array<glm::vec3, 6> kAxes{glm::vec3(1, 0, 0),  glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
                                                    glm::vec3(0, -1, 0), glm::vec3(0, 0, 1),  glm::vec3(0, 0, -1)};
    const auto coordinate = [&random] { return random_float(random, -1.0f, 1.0f); };
    const int chosen = random_int(random, 0, 9);
    if (chosen < 3) {
        return kAxes.at(random_index(random, kAxes.size()));
    }
    const glm::vec3 direction{coordinate(), coordinate(), chosen < 5 ? 0.0f : coordinate() / 2.0f};
    return glm::normalize(direction);
}

TEST(RaycastTest, FindsTheSameHitsAsTestingEverything) {
    // Random rotated blocks and doors, partly outside the bounds, some doors open, and random rays, some exactly along
    // an axis, some unlimited.
    World world({.x_min = -20.0f, .x_max = 20.0f, .y_min = -20.0f, .y_max = 20.0f, .height = 3.0f});
    Rng random(11);
    const auto position = [&random] { return random_float(random, -24.0f, 24.0f); };
    const auto size = [&random] { return random_float(random, 0.05f, 1.0f); };
    const auto angle = [&random] { return random_float(random, 0.0f, 2.0f * std::numbers::pi_v<float>); };
    const auto height = [&random] { return random_float(random, 0.0f, 3.0f); };
    std::vector<DoorState> doors;
    for (int i = 0; i < 150; ++i) {
        world.add_obstacle({.base_center = {position(), position(), 0.0f},
                            .half_size = {size(), size()},
                            .height = height(),
                            .yaw = angle()});
        world.add_door({.base_center = {position(), position(), 0.0f},
                        .width = 2.0f * size(),
                        .height = 2.5f,
                        .yaw = angle(),
                        .sounds = {}});
        // The last doors have no state, so they count as closed.
        if (i < 140) {
            doors.push_back(i % 3 == 0 ? DoorState{.openness = 1.0f, .target = 1.0f} : DoorState{});
        }
    }
    for (int i = 0; i < 100; ++i) {
        world.add_chest({.base_center = {position(), position(), 0.0f}, .yaw = angle()});
    }

    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    int hits = 0;
    int door_hits = 0;
    int chest_hits = 0;
    for (int i = 0; i < 3000; ++i) {
        const Ray ray{.origin = {position(), position(), height()}, .direction = random_direction(random)};
        const float max_distance = (i % 7 == 0) ? kInfinity : 2.0f * height();
        const std::optional<RayHit> hit = cast_ray(world, doors, ray, max_distance);
        const std::optional<RayHit> expected = cast_ray_testing_all(world, doors, ray, max_distance);
        ASSERT_EQ(hit.has_value(), expected.has_value()) << "ray " << i;
        if (hit) {
            ++hits;
            door_hits += hit->target == RayTarget::kDoor || hit->target == RayTarget::kDoorFrame ? 1 : 0;
            chest_hits += hit->target == RayTarget::kChest ? 1 : 0;
            EXPECT_EQ(hit->distance, expected->distance) << "ray " << i;
            EXPECT_EQ(hit->target, expected->target) << "ray " << i;
            EXPECT_EQ(hit->door, expected->door) << "ray " << i;
            EXPECT_EQ(hit->chest, expected->chest) << "ray " << i;
        }
    }
    EXPECT_GT(hits, 500);
    EXPECT_GT(chest_hits, 10);
    EXPECT_GT(door_hits, 100);
}

}  // namespace
}  // namespace manifold
