#include "level/debug_layout.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "level/level.hpp"
#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

constexpr float kClearance = 0.3f;

DebugLayoutConfig make_config() {
    return {.bounds = {.x_min = -20.0f, .x_max = 20.0f, .y_min = -10.0f, .y_max = 10.0f, .height = 4.0f},
            .obstacle_count = 10,
            .obstacle_size = 2.0f};
}

Level generate(const DebugLayoutConfig& config, std::uint64_t seed, float clearance = kClearance) {
    LevelRng rng(seed);
    return DebugLayout(config).generate(clearance, rng);
}

TEST(DebugLayoutTest, CreatesConfiguredNumberOfObstacles) {
    DebugLayoutConfig config = make_config();
    EXPECT_EQ(generate(config, 1).world.obstacles().size(), 10U);
    config.obstacle_count = 25;
    EXPECT_EQ(generate(config, 1).world.obstacles().size(), 25U);
    config.obstacle_count = 0;
    EXPECT_TRUE(generate(config, 1).world.obstacles().empty());
}

TEST(DebugLayoutTest, ObstaclesHaveConfiguredSize) {
    const Level level = generate(make_config(), 2);
    for (const Obstacle& obstacle : level.world.obstacles()) {
        EXPECT_FLOAT_EQ(obstacle.half_size, 1.0f);
        EXPECT_FLOAT_EQ(obstacle.height, 4.0f);
        EXPECT_FLOAT_EQ(obstacle.base_center.z, 0.0f);
    }
}

TEST(DebugLayoutTest, PlayerStartsInCenter) {
    DebugLayoutConfig config = make_config();
    config.bounds.x_min = 0.0f;
    const Level level = generate(config, 3);
    EXPECT_EQ(level.layout, LayoutKind::kDebug);
    EXPECT_EQ(level.player_start, glm::vec3(10.0f, 0.0f, 0.0f));
}

TEST(DebugLayoutTest, ObstaclesNeverOverlapStartArea) {
    // A large clearance and many obstacles make overlaps likely if the check were broken.
    DebugLayoutConfig config = make_config();
    config.obstacle_count = 50;
    constexpr float kLargeClearance = 4.0f;
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const Level level = generate(config, seed, kLargeClearance);
        const Circle start{.center = {level.player_start.x, level.player_start.y}, .radius = kLargeClearance};
        for (const Obstacle& obstacle : level.world.obstacles()) {
            EXPECT_FALSE(intersects(obstacle.footprint(), start)) << "seed " << seed;
        }
    }
}

TEST(DebugLayoutTest, ObstaclesStayInsideBounds) {
    const DebugLayoutConfig config = make_config();
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const Level level = generate(config, seed);
        for (const Obstacle& obstacle : level.world.obstacles()) {
            // Check all four corners of the rotated square.
            for (int corner = 0; corner < 4; ++corner) {
                const float angle = obstacle.yaw + (static_cast<float>(corner) * std::numbers::pi_v<float> / 2.0f) +
                                    (std::numbers::pi_v<float> / 4.0f);
                const float distance = obstacle.half_size * std::numbers::sqrt2_v<float>;
                const glm::vec3 point =
                    obstacle.base_center + glm::vec3(distance * std::cos(angle), distance * std::sin(angle), 0.0f);
                EXPECT_TRUE(config.bounds.contains(point)) << "seed " << seed;
            }
        }
    }
}

TEST(DebugLayoutTest, SameSeedGivesSameLevel) {
    const Level first = generate(make_config(), 42);
    const Level second = generate(make_config(), 42);
    ASSERT_EQ(first.world.obstacles().size(), second.world.obstacles().size());
    for (std::size_t i = 0; i < first.world.obstacles().size(); ++i) {
        EXPECT_EQ(first.world.obstacles()[i].base_center, second.world.obstacles()[i].base_center);
        EXPECT_EQ(first.world.obstacles()[i].yaw, second.world.obstacles()[i].yaw);
    }
    EXPECT_EQ(first.plan, second.plan);
}

TEST(DebugLayoutTest, DifferentSeedsGiveDifferentLevels) {
    const Level first = generate(make_config(), 1);
    const Level second = generate(make_config(), 2);
    EXPECT_NE(first.world.obstacles()[0].base_center, second.world.obstacles()[0].base_center);
}

TEST(DebugLayoutTest, PlanListsObstacles) {
    const Level level = generate(make_config(), 1);
    std::size_t lines = 0;
    for (const char c : level.plan) {
        lines += c == '\n' ? 1 : 0;
    }
    EXPECT_EQ(lines, level.world.obstacles().size());
}

TEST(DebugLayoutTest, ThrowsWhenThereIsNoRoom) {
    // The start area covers the whole level.
    EXPECT_THROW(generate(make_config(), 1, 100.0f), std::runtime_error);
}

}  // namespace
}  // namespace manifold
