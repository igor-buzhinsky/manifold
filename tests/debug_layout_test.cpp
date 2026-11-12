#include "level/debug_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "level/level.hpp"
#include "world/geometry.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

constexpr float kClearance = 0.6f;
constexpr int kSeeds = 50;

DebugLayoutConfig make_config() { return {.y_min = -15.0f, .y_max = 15.0f}; }

Level generate(const DebugLayoutConfig& config, std::uint64_t seed, float clearance = kClearance) {
    LevelRng rng(seed);
    return DebugLayout(config).generate(clearance, rng);
}

/// Splits the blocks of a level into obstacles on the floor and ceiling blocks above them.
struct Blocks {
    std::vector<Obstacle> floor;
    std::vector<Obstacle> ceiling;
};

Blocks split_blocks(const Level& level) {
    Blocks blocks;
    for (const Obstacle& obstacle : level.world.obstacles()) {
        (obstacle.base_center.z > 0.0f ? blocks.ceiling : blocks.floor).push_back(obstacle);
    }
    return blocks;
}

TEST(DebugLayoutTest, CreatesConfiguredNumberOfObstaclesCeilingBlocksAndRamps) {
    DebugLayoutConfig config = make_config();
    Blocks blocks = split_blocks(generate(config, 1));
    EXPECT_EQ(blocks.floor.size(), 10U);
    EXPECT_EQ(blocks.ceiling.size(), 10U);
    EXPECT_EQ(generate(config, 1).world.ramps().size(), 10U);

    config.obstacle_count = 25;
    config.ramp_count = 3;
    const Level level = generate(config, 1);
    blocks = split_blocks(level);
    EXPECT_EQ(blocks.floor.size(), 25U);
    EXPECT_EQ(blocks.ceiling.size(), 25U);
    EXPECT_EQ(level.world.ramps().size(), 3U);

    config.obstacle_count = 0;
    config.ramp_count = 0;
    EXPECT_TRUE(generate(config, 1).world.obstacles().empty());
    EXPECT_TRUE(generate(config, 1).world.ramps().empty());
}

TEST(DebugLayoutTest, BlocksAndRampsHaveRoomLayoutSizes) {
    const Level level = generate(make_config(), 2);
    const Blocks blocks = split_blocks(level);
    for (const Obstacle& obstacle : blocks.floor) {
        EXPECT_FLOAT_EQ(obstacle.half_size, 1.0f);
        EXPECT_FLOAT_EQ(obstacle.height, 3.0f);
        EXPECT_FLOAT_EQ(obstacle.base_center.z, 0.0f);
    }
    for (const Obstacle& obstacle : blocks.ceiling) {
        EXPECT_FLOAT_EQ(obstacle.half_size, 1.0f);
        EXPECT_FLOAT_EQ(obstacle.height, 0.3f);
        EXPECT_FLOAT_EQ(obstacle.base_center.z, 3.0f);
    }
    for (const Ramp& ramp : level.world.ramps()) {
        EXPECT_FLOAT_EQ(ramp.half_length, 2.0f);
        EXPECT_FLOAT_EQ(ramp.half_width, 1.0f);
        EXPECT_FLOAT_EQ(ramp.rise, 3.3f);
        EXPECT_FLOAT_EQ(ramp.base_center.z, 0.0f);
    }
}

TEST(DebugLayoutTest, LevelIsFiveObstaclesAndFourCeilingBlocksHigh) {
    EXPECT_FLOAT_EQ(generate(make_config(), 1).world.bounds().height, (5.0f * 3.0f) + (4.0f * 0.3f));
}

TEST(DebugLayoutTest, CeilingBlocksArePlacedIndependentlyOfObstacles) {
    const Blocks blocks = split_blocks(generate(make_config(), 3));
    ASSERT_EQ(blocks.floor.size(), blocks.ceiling.size());
    int same = 0;
    for (std::size_t i = 0; i < blocks.floor.size(); ++i) {
        same += glm::vec2(blocks.floor[i].base_center) == glm::vec2(blocks.ceiling[i].base_center) ? 1 : 0;
    }
    EXPECT_EQ(same, 0);
}

TEST(DebugLayoutTest, PlayerStartsInCenter) {
    DebugLayoutConfig config = make_config();
    config.x_min = 0.0f;
    const Level level = generate(config, 3);
    EXPECT_EQ(level.layout, LayoutKind::kDebug);
    EXPECT_EQ(level.player_start, glm::vec3(10.0f, 0.0f, 0.0f));
}

TEST(DebugLayoutTest, NothingOverlapsStartArea) {
    // A large clearance and many obstacles make overlaps likely if the check were broken.
    DebugLayoutConfig config = make_config();
    config.obstacle_count = 50;
    constexpr float kLargeClearance = 4.0f;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(config, seed, kLargeClearance);
        const Circle start{.center = {level.player_start.x, level.player_start.y}, .radius = kLargeClearance};
        for (const Obstacle& obstacle : level.world.obstacles()) {
            EXPECT_FALSE(intersects(obstacle.footprint(), start)) << "seed " << seed;
        }
        for (const Ramp& ramp : level.world.ramps()) {
            EXPECT_FALSE(intersects(ramp.footprint(), start)) << "seed " << seed;
        }
    }
}

TEST(DebugLayoutTest, RampsOverlapNothingElse) {
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(make_config(), seed);
        const auto ramps = level.world.ramps();
        for (std::size_t i = 0; i < ramps.size(); ++i) {
            for (const Obstacle& obstacle : level.world.obstacles()) {
                EXPECT_FALSE(intersects(ramps[i].footprint(), obstacle.footprint())) << "seed " << seed;
            }
            for (std::size_t j = i + 1; j < ramps.size(); ++j) {
                EXPECT_FALSE(intersects(ramps[i].footprint(), ramps[j].footprint())) << "seed " << seed;
            }
        }
    }
}

TEST(DebugLayoutTest, RampsGoUpInAllDirections) {
    // Ramps aren't symmetric, so their yaw covers the full circle.
    float min_yaw = 10.0f;
    float max_yaw = -10.0f;
    for (std::uint64_t seed = 0; seed < 5; ++seed) {
        const Level level = generate(make_config(), seed);
        for (const Ramp& ramp : level.world.ramps()) {
            min_yaw = std::min(min_yaw, ramp.yaw);
            max_yaw = std::max(max_yaw, ramp.yaw);
        }
    }
    EXPECT_LT(min_yaw, 0.5f);
    EXPECT_GT(max_yaw, 2.0f * std::numbers::pi_v<float> - 0.5f);
}

/// Checks whether the box around a footprint lies inside the level.
bool inside(const WorldBounds& bounds, const RotatedRectangle& footprint) {
    const glm::vec2 extent = bounding_half_size(footprint);
    return footprint.center.x - extent.x >= bounds.x_min && footprint.center.x + extent.x <= bounds.x_max &&
           footprint.center.y - extent.y >= bounds.y_min && footprint.center.y + extent.y <= bounds.y_max;
}

TEST(DebugLayoutTest, EverythingStaysInsideBounds) {
    const DebugLayoutConfig config = make_config();
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(config, seed);
        for (const Obstacle& obstacle : level.world.obstacles()) {
            EXPECT_TRUE(inside(config.bounds(), obstacle.footprint())) << "seed " << seed;
            EXPECT_LE(obstacle.base_center.z + obstacle.height, config.bounds().height);
        }
        for (const Ramp& ramp : level.world.ramps()) {
            EXPECT_TRUE(inside(config.bounds(), ramp.footprint())) << "seed " << seed;
        }
    }
}

TEST(DebugLayoutTest, TorchesHangOnLevelWallsFacingInwards) {
    const DebugLayoutConfig config = make_config();
    const WorldBounds bounds = config.bounds();
    // With nothing in the way, the 40 m by 30 m level gets 5 torches on its long walls and 3 on its short ones.
    DebugLayoutConfig empty = config;
    empty.obstacle_count = 0;
    empty.ramp_count = 0;
    EXPECT_EQ(generate(empty, 1).world.torches().size(), 16U);
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(config, seed);
        EXPECT_FALSE(level.world.torches().empty()) << "seed " << seed;
        for (const Torch& torch : level.world.torches()) {
            EXPECT_EQ(torch.flame, level.world.torches()[0].flame) << "seed " << seed;
            const glm::vec3 mount = torch.mount;
            const bool on_wall = mount.x == bounds.x_min || mount.x == bounds.x_max || mount.y == bounds.y_min ||
                                 mount.y == bounds.y_max;
            EXPECT_TRUE(on_wall) << "seed " << seed;
            EXPECT_FLOAT_EQ(mount.z, config.torch_height);
            EXPECT_TRUE(bounds.contains(torch.tip())) << "seed " << seed;
            // Nothing stands where the handle and the flame are.
            const Circle around{.center = glm::vec2(torch.tip()), .radius = 0.1f};
            for (const Obstacle& obstacle : level.world.obstacles()) {
                if (obstacle.base_center.z < torch.tip().z) {
                    EXPECT_FALSE(intersects(obstacle.footprint(), around)) << "seed " << seed;
                }
            }
            for (const Ramp& ramp : level.world.ramps()) {
                EXPECT_FALSE(intersects(ramp.footprint(), around)) << "seed " << seed;
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

TEST(DebugLayoutTest, PlanListsObstaclesCeilingBlocksAndRamps) {
    const Level level = generate(make_config(), 1);
    std::size_t lines = 0;
    for (const char c : level.plan) {
        lines += c == '\n' ? 1 : 0;
    }
    EXPECT_EQ(lines, level.world.obstacles().size() + level.world.ramps().size());
    EXPECT_NE(level.plan.find("Ceiling block"), std::string::npos);
    EXPECT_NE(level.plan.find("Ramp"), std::string::npos);
}

TEST(DebugLayoutTest, ThrowsWhenThereIsNoRoom) {
    // The start area covers the whole level.
    EXPECT_THROW(generate(make_config(), 1, 100.0f), std::runtime_error);
    // The ramps can't all fit between the other blocks.
    DebugLayoutConfig config = make_config();
    config.ramp_count = 200;
    EXPECT_THROW(generate(config, 1), std::runtime_error);
}

}  // namespace
}  // namespace manifold
