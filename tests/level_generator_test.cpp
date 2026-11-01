#include "level/level_generator.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <numbers>
#include <stdexcept>

#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

LevelConfig make_config() {
    return {.bounds = {.x_min = -20.0f, .x_max = 20.0f, .y_min = -10.0f, .y_max = 10.0f, .height = 4.0f},
            .obstacle_count = 10,
            .obstacle_size = 2.0f};
}

constexpr Circle kPlayer{.center = {0.0f, 0.0f}, .radius = 0.3f};

TEST(LevelGeneratorTest, CreatesConfiguredNumberOfObstacles) {
    LevelConfig config = make_config();
    EXPECT_EQ(generate_level(config, kPlayer, 1).obstacles().size(), 10U);
    config.obstacle_count = 25;
    EXPECT_EQ(generate_level(config, kPlayer, 1).obstacles().size(), 25U);
    config.obstacle_count = 0;
    EXPECT_TRUE(generate_level(config, kPlayer, 1).obstacles().empty());
}

TEST(LevelGeneratorTest, ObstaclesHaveConfiguredSize) {
    const World world = generate_level(make_config(), kPlayer, 2);
    for (const Obstacle& obstacle : world.obstacles()) {
        EXPECT_FLOAT_EQ(obstacle.half_size, 1.0f);
        EXPECT_FLOAT_EQ(obstacle.base_center.z, 0.0f);
    }
}

TEST(LevelGeneratorTest, ObstaclesNeverOverlapPlayer) {
    // A large keep-clear circle and many obstacles make overlaps likely if the check were broken.
    LevelConfig config = make_config();
    config.obstacle_count = 50;
    const Circle player{.center = {3.0f, -2.0f}, .radius = 4.0f};
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const World world = generate_level(config, player, seed);
        for (const Obstacle& obstacle : world.obstacles()) {
            EXPECT_FALSE(intersects(obstacle.footprint(), player)) << "seed " << seed;
        }
    }
}

TEST(LevelGeneratorTest, ObstaclesStayInsideBounds) {
    const LevelConfig config = make_config();
    for (std::uint64_t seed = 0; seed < 50; ++seed) {
        const World world = generate_level(config, kPlayer, seed);
        for (const Obstacle& obstacle : world.obstacles()) {
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

TEST(LevelGeneratorTest, SameSeedGivesSameLevel) {
    const World first = generate_level(make_config(), kPlayer, 42);
    const World second = generate_level(make_config(), kPlayer, 42);
    ASSERT_EQ(first.obstacles().size(), second.obstacles().size());
    for (std::size_t i = 0; i < first.obstacles().size(); ++i) {
        EXPECT_EQ(first.obstacles()[i].base_center, second.obstacles()[i].base_center);
        EXPECT_EQ(first.obstacles()[i].yaw, second.obstacles()[i].yaw);
    }
}

TEST(LevelGeneratorTest, DifferentSeedsGiveDifferentLevels) {
    const World first = generate_level(make_config(), kPlayer, 1);
    const World second = generate_level(make_config(), kPlayer, 2);
    EXPECT_NE(first.obstacles()[0].base_center, second.obstacles()[0].base_center);
}

TEST(LevelGeneratorTest, ThrowsWhenThereIsNoRoom) {
    // The keep-clear circle covers the whole level.
    const Circle huge{.center = {0.0f, 0.0f}, .radius = 100.0f};
    EXPECT_THROW(generate_level(make_config(), huge, 1), std::runtime_error);
}

}  // namespace
}  // namespace manifold
