#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "level/texture_kind.hpp"
#include "util/asset_mapping.hpp"
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

/// The random choices of objects that levels of every layout make the same way, read from the constants file, see
/// load_game_data().
struct LevelObjectChances {
    /// How often the torches of a level burn orange, blue and green, relative to each other, in the order of
    /// TorchFlame, see choose_torch_flame(). Each is 0 or more, and not all are 0.
    std::array<double, 3> torch_flames{0.5, 0.25, 0.25};
    /// Chance that a chest is golden rather than wooden, in [0, 1], see choose_chest_look().
    double golden_chest = 0.2;

    bool operator==(const LevelObjectChances&) const = default;
};

/// Chooses the flame of all torches of a level, with the chances of `chances.torch_flames`: by default orange with a
/// chance of 50%, blue and green with 25% each.
///
/// \throws std::invalid_argument if the chances are negative, not finite or all 0, see random_weighted_index().
TorchFlame choose_torch_flame(LevelRng& rng, const LevelObjectChances& chances = {});

/// Chooses what a chest is made of: gold with the chance `chances.golden_chest`, wood otherwise.
ChestLook choose_chest_look(LevelRng& rng, const LevelObjectChances& chances = {});

/// Whether the level generated from `level_seed` has ambient light, which it has for half of the seeds, see
/// level_ambient(). The choice depends only on the seed, and is independent of the other random choices made from it.
bool level_has_ambient_light(std::uint64_t level_seed);

/// The texture of the surfaces of a level: one of the files of the key of its kind in the asset mapping, see
/// level_textures_key().
struct LevelTexture {
    /// What the surfaces are made of, e.g. to choose the sound of footsteps. Stone if there are no textures.
    TextureKind kind = TextureKind::kStone;
    AssetIndex index = 0;  ///< Which file of the key of `kind`.

    bool operator==(const LevelTexture&) const = default;
};

/// Chooses the texture of the level generated from `level_seed`, among the files of all texture kinds, each with the
/// same chance, so a kind with more files is chosen more often. The choice depends only on the seed and the counts,
/// and is independent of the other random choices made from the seed.
///
/// \return The texture, or stone with index 0 if no kind has files.
LevelTexture choose_level_texture(const AssetCounts& counts, std::uint64_t level_seed);

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
    /// The texture of its surfaces, which also tells what they are made of, see choose_level_texture().
    // Without the initializer, GCC warns about levels that leave it out, such as those of the layouts.
    LevelTexture texture = {};  // NOLINT(readability-redundant-member-init)
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
