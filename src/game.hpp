#pragma once

#include <cstdint>
#include <numbers>

#include "level/level_generator.hpp"
#include "world/player.hpp"
#include "world/world.hpp"

namespace manifold {

/// Settings for a new game.
struct GameConfig {
    /// How the level is generated.
    LevelConfig level{.bounds = {.x_min = -20.0f, .x_max = 20.0f, .y_min = -20.0f, .y_max = 20.0f, .height = 4.0f}};
    PlayerParams player;                                  ///< The player at the start of the level.
    float turn_speed = std::numbers::pi_v<float> / 2.0f;  ///< Radians per second: 90 degrees.
    float look_speed = std::numbers::pi_v<float> / 3.0f;  ///< Radians per second: 60 degrees.
    float move_speed = 3.0f;                              ///< Metres per second.
};

/// What the player wants to do during a frame, independent of the input device.
///
/// Rates are in [-1, 1], where 1 means full speed.
struct PlayerInput {
    float turn = 0.0f;     ///< Positive turns left.
    float look = 0.0f;     ///< Positive looks up.
    float forward = 0.0f;  ///< Positive walks forward, negative backward.
    float strafe = 0.0f;   ///< Positive steps right, negative left.
};

/// Owns the game state.
///
/// The world holds static level geometry, while the player (and later other dynamic entities) live next
/// to it, so systems such as physics or AI can work on both.
class Game {
public:
    /// Generates the level and places the player at the start.
    ///
    /// \param config Game settings.
    /// \param seed   Seed for level generation. The same seed gives the same level.
    /// \throws std::runtime_error if the level can't be generated.
    Game(const GameConfig& config, std::uint64_t seed);

    /// Advances the game by one frame.
    ///
    /// Turns the player first, then walks in the new direction. Walking is stopped by obstacles and
    /// level walls unless noclip is on.
    ///
    /// \param input What the player wants to do.
    /// \param dt    Duration of the frame in seconds.
    void update(const PlayerInput& input, float dt);

    /// Whether the noclip cheat is on: the player walks through obstacles and walls.
    bool noclip() const { return noclip_; }
    /// Turns the noclip cheat on or off.
    void toggle_noclip() { noclip_ = !noclip_; }

    const World& world() const { return world_; }
    const Player& player() const { return player_; }
    /// The seed the level was generated from.
    std::uint64_t seed() const { return seed_; }

private:
    float turn_speed_;
    float look_speed_;
    float move_speed_;
    bool noclip_ = false;
    std::uint64_t seed_;
    Player player_;
    World world_;
};

}  // namespace manifold
