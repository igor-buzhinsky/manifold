#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "level/texture_kind.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {

/// Random number generator for level generation. Choices are made with the functions of `util/random.hpp`, so the same
/// seed gives the same level everywhere.
using LevelRng = Rng;

/// Gravity on Earth, in metres per second squared.
constexpr float kEarthGravity = 9.81f;

/// Chooses the flame of all torches of a level: orange with a chance of 50%, blue and green with 25% each.
TorchFlame choose_torch_flame(LevelRng& rng);

/// Chance that a chest is golden rather than wooden, see choose_chest_look().
inline constexpr double kGoldenChestChance = 0.2;

/// Chooses what a chest is made of: gold with a chance of kGoldenChestChance, wood otherwise.
ChestLook choose_chest_look(LevelRng& rng);

/// Whether the level generated from `level_seed` has ambient light, which it has for half of the seeds, see
/// level_ambient(). The choice depends only on the seed, and is independent of the other random choices made from it.
bool level_has_ambient_light(std::uint64_t level_seed);

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
    std::string info;               ///< Human-readable description of the level, for debugging. Depends on the layout.
    float gravity = kEarthGravity;  ///< Downward acceleration, in metres per second squared.
    /// What its surfaces are made of: the kind of its texture, which the game sets when it picks the texture, see
    /// Game::set_texture_kind(). Stone until then, and when no texture can be used.
    TextureKind texture_kind = TextureKind::kStone;
    /// Torches that could hang on the walls of the room where the player starts, on the wall sides that got none.
    /// Empty for layouts without rooms. generate_level() may add the one nearest to the start, which takes it out.
    // Without the initializer, GCC warns about levels that leave it out, such as those built by hand in tests.
    std::vector<Torch> free_start_torches = {};  // NOLINT(readability-redundant-member-init)
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
