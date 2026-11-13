#pragma once

#include <cstdint>
#include <random>
#include <string>

#include <glm/vec3.hpp>

#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {

/// Random number generator for level generation.
///
/// Standard library distributions differ between implementations, so the same seed gives the same level
/// only with the same standard library.
using LevelRng = std::mt19937_64;

/// Gravity on Earth, in metres per second squared.
constexpr float kEarthGravity = 9.81f;

/// Chooses the flame of all torches of a level: orange with a chance of 50%, blue and green with 25% each.
TorchFlame choose_torch_flame(LevelRng& rng);

/// The kinds of level layouts.
enum class LayoutKind : std::uint8_t {
    kDebug,  ///< Randomly placed and rotated blocks in one open area, see DebugLayout.
    kRooms,  ///< Rooms on a grid of square tiles, connected by openings, see RoomLayout.
};

/// A generated level, ready to play.
struct Level {
    LayoutKind layout;              ///< The layout that generated the level.
    World world;                    ///< Geometry of the level.
    glm::vec3 player_start{0.0f};   ///< Where the player starts: the point between the feet.
    float player_yaw = 0.0f;        ///< Direction the player faces at the start: radians counter-clockwise from +x.
    std::string plan;               ///< Human-readable description of the level plan, for debugging.
    float gravity = kEarthGravity;  ///< Downward acceleration, in metres per second squared.
};

/// A way of generating levels.
///
/// Each layout has its own settings and generation logic. Use make_layout() to create one.
class LevelLayout {
public:
    LevelLayout() = default;
    virtual ~LevelLayout() = default;
    LevelLayout(const LevelLayout&) = delete;
    LevelLayout& operator=(const LevelLayout&) = delete;
    LevelLayout(LevelLayout&&) = delete;
    LevelLayout& operator=(LevelLayout&&) = delete;

    /// Generates a level.
    ///
    /// \param clearance Radius around the player start that must stay free of obstacles, in metres.
    /// \param rng       Random number generator. The same generator state gives the same level.
    /// \throws std::runtime_error if the level can't be generated.
    virtual Level generate(float clearance, LevelRng& rng) const = 0;
};

}  // namespace manifold
