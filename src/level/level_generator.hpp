#pragma once

#include <cstdint>

#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

/// Settings for level generation.
struct LevelConfig {
    WorldBounds bounds;                    ///< The box that encloses the level.
    int obstacle_count = 10;               ///< How many obstacles to place.
    float obstacle_size = 2.0f;            ///< Side of the square obstacles, in metres.
    int max_attempts_per_obstacle = 1000;  ///< How often to try placing an obstacle before giving up.
};

/// Creates a world with randomly placed and rotated square obstacles.
///
/// Obstacles reach from the floor to the ceiling and lie fully inside the bounds. They may overlap each
/// other, but never overlap `keep_clear`. The result depends only on the arguments.
///
/// \param config     Level settings.
/// \param keep_clear An area that stays free of obstacles, e.g. the player's start footprint.
/// \param seed       Seed for the random number generator.
/// \return The generated world with `config.obstacle_count` obstacles.
/// \throws std::runtime_error if an obstacle can't be placed.
World generate_level(const LevelConfig& config, const Circle& keep_clear, std::uint64_t seed);

}  // namespace manifold
