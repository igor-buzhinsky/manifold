#pragma once

#include <cstdint>
#include <numbers>
#include <optional>

#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "world/player.hpp"
#include "world/world.hpp"

namespace manifold {

/// Settings for a new game.
struct GameConfig {
    LevelConfig level;    ///< Settings for level generation.
    PlayerParams player;  ///< The player at the start. The position is replaced by the level's start.
    float turn_speed = std::numbers::pi_v<float> / 2.0f;  ///< Radians per second: 90 degrees.
    float look_speed = std::numbers::pi_v<float> / 3.0f;  ///< Radians per second: 60 degrees.
    float move_speed = 3.0f;                              ///< Metres per second, also when rising.
    /// Steepest slope, as height per horizontal distance, that the player sticks to when walking down.
    float max_snap_slope = 1.0f;
    /// How high a jump lifts the player's feet under Earth gravity, in metres. The take-off speed is the same
    /// in every level, so the jump is higher where gravity is weaker: jump_height * kEarthGravity / gravity.
    float jump_height = 0.5f;
};

/// What the player wants to do during a frame, independent of the input device.
///
/// Rates are in [-1, 1], where 1 means full speed.
struct PlayerInput {
    float turn = 0.0f;     ///< Positive turns left.
    float look = 0.0f;     ///< Positive looks up.
    float forward = 0.0f;  ///< Positive walks forward, negative backward.
    float strafe = 0.0f;   ///< Positive steps right, negative left.
    /// Positive jumps, or flies up with noclip. Negative flies down with noclip. A jump needs a new press:
    /// rise must have been <= 0 in the previous frame.
    float rise = 0.0f;
};

/// Owns the game state.
///
/// The world holds static level geometry, while the player (and later other dynamic entities) live next
/// to it, so systems such as physics or AI can work on both.
class Game {
public:
    /// Generates the level and places the player at its start.
    ///
    /// \param config Game settings.
    /// \param seed   Seed for level generation. The same seed gives the same level.
    /// \param layout The level layout. If not given, one is chosen at random, see LevelConfig.
    /// \throws std::runtime_error if the level can't be generated.
    Game(const GameConfig& config, std::uint64_t seed, std::optional<LayoutKind> layout = std::nullopt);

    /// Starts a game in a level that is already generated, e.g. one built by hand in a test.
    ///
    /// \param config Game settings. The level settings are not used.
    /// \param level  The level. The player starts at its start.
    /// \param seed   The seed reported by seed().
    Game(const GameConfig& config, Level level, std::uint64_t seed = 0);

    /// Advances the game by one frame.
    ///
    /// Turns the player first, then walks in the new direction. Without noclip, the player falls by the
    /// level's gravity and is stopped by obstacles and level walls. Walking into a ramp goes up it, and a
    /// player who walks down a ramp sticks to it instead of falling in small hops. When `input.rise` becomes
    /// positive, a player who stands on something jumps. Holding it doesn't jump again. With noclip, the player flies:
    /// moves up and down by `input.rise`, doesn't fall, and goes through everything.
    ///
    /// \param input What the player wants to do.
    /// \param dt    Duration of the frame in seconds.
    void update(const PlayerInput& input, float dt);

    /// Whether the noclip cheat is on: the player flies through obstacles and walls.
    bool noclip() const { return noclip_; }
    /// Turns the noclip cheat on or off.
    void toggle_noclip() { noclip_ = !noclip_; }

    const Level& level() const { return level_; }
    const World& world() const { return level_.world; }
    const Player& player() const { return player_; }
    /// The seed the level was generated from.
    std::uint64_t seed() const { return seed_; }

private:
    float turn_speed_;
    float look_speed_;
    float move_speed_;
    float max_snap_slope_;
    float jump_speed_;  ///< Take-off speed of a jump, in metres per second.
    bool noclip_ = false;
    bool rise_held_ = false;  ///< Whether input.rise was positive in the previous frame.
    std::uint64_t seed_;
    Level level_;
    Player player_;
};

}  // namespace manifold
