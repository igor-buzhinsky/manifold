#pragma once

#include "level/level.hpp"
#include "world/world.hpp"

namespace manifold {

/// Settings for DebugLayout. The default block and ramp sizes are the same as in RoomLayout.
struct DebugLayoutConfig {
    float x_min = -20.0f;  ///< Horizontal extent of the level, in metres.
    float x_max = 20.0f;
    float y_min = -20.0f;
    float y_max = 20.0f;
    int obstacle_count = 10;            ///< How many obstacles stand on the floor. As many ceiling blocks float.
    float obstacle_size = 2.0f;         ///< Side of the square obstacles and ceiling blocks, in metres.
    float obstacle_height = 3.0f;       ///< Height of the obstacles, in metres.
    float ceiling_thickness = 0.3f;     ///< Height of the ceiling blocks, in metres.
    int ramp_count = 10;                ///< How many ramps to place.
    float ramp_length = 4.0f;           ///< Length of a ramp along its slope, in metres.
    float ramp_width = 2.0f;            ///< Width of a ramp across its slope, in metres.
    int max_attempts_per_block = 1000;  ///< How often to try placing an obstacle or a ramp before giving up.
    float torch_spacing = 8.0f;         ///< Distance between torches along the level walls, in metres.
    float torch_height = 1.6f;          ///< Height of a torch's mount above the floor, in metres.

    /// The box that encloses the level. It is 5 obstacle heights plus 4 ceiling thicknesses high, like 5
    /// layers of a room level, although only the lowest one is used.
    WorldBounds bounds() const;
};

/// One open area with randomly placed and rotated obstacles, ceiling blocks and ramps.
///
/// - Obstacles are square blocks standing on the floor. They may overlap each other.
/// - Ceiling blocks are thin square blocks floating right above the height of the obstacles. They may
///   overlap each other and touch the obstacles below.
/// - Ramps go up from the floor to the top of the ceiling blocks, in a random direction. They don't overlap
///   obstacles, ceiling blocks or other ramps.
///
/// - Torches hang on the level walls, `torch_spacing` apart, except where an obstacle or a ramp is in the way.
///   All of them have the same flame, see choose_torch_flame().
///
/// - The exit floats at a random place, see LevelExit. It doesn't overlap obstacles that reach its height, or
///   ramps.
///
/// Everything lies fully inside the bounds, and the start in the center of the level stays free. The level
/// info lists all obstacles, ceiling blocks, ramps and the exit.
class DebugLayout final : public LevelLayout {
public:
    explicit DebugLayout(const DebugLayoutConfig& config) : config_(config) {}

    Level generate(float clearance, LevelRng& rng) const override;

private:
    DebugLayoutConfig config_;
};

}  // namespace manifold
