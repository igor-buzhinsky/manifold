#pragma once

#include <cstdint>

#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

struct LevelConfig {
    WorldBounds bounds;
    int obstacle_count = 10;
    float obstacle_size = 2.0f;  // Side of the square, in metres.
    int max_attempts_per_obstacle = 1000;
};

// Creates a world with `config.obstacle_count` randomly placed and rotated square obstacles.
// Obstacles lie fully inside the bounds, may overlap each other, but never overlap `keep_clear`
// (for example, the player's start footprint). The result depends only on the arguments.
// Throws std::runtime_error if an obstacle can't be placed.
World generate_level(const LevelConfig& config, const Circle& keep_clear, std::uint64_t seed);

}  // namespace manifold
