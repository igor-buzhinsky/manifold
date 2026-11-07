#pragma once

#include "level/level.hpp"
#include "world/world.hpp"

namespace manifold {

/// Settings for DebugLayout.
struct DebugLayoutConfig {
    /// The box that encloses the level.
    WorldBounds bounds{.x_min = -20.0f, .x_max = 20.0f, .y_min = -20.0f, .y_max = 20.0f, .height = 4.0f};
    int obstacle_count = 10;               ///< How many obstacles to place.
    float obstacle_size = 2.0f;            ///< Side of the square obstacles, in metres.
    int max_attempts_per_obstacle = 1000;  ///< How often to try placing an obstacle before giving up.
};

/// One open area with randomly placed and rotated square obstacles.
///
/// Obstacles reach from the floor to the ceiling and lie fully inside the bounds. They may overlap each
/// other. The player starts in the center of the level, which stays free of obstacles. The plan lists all
/// obstacles.
class DebugLayout final : public LevelLayout {
public:
    explicit DebugLayout(const DebugLayoutConfig& config) : config_(config) {}

    Level generate(float clearance, LevelRng& rng) const override;

private:
    DebugLayoutConfig config_;
};

}  // namespace manifold
