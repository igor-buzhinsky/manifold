#pragma once

#include <cstdint>
#include <numbers>

#include "level/level_generator.hpp"
#include "world/player.hpp"
#include "world/world.hpp"

namespace manifold {

struct GameConfig {
    LevelConfig level{.bounds = {.x_min = -20.0f, .x_max = 20.0f, .y_min = -20.0f, .y_max = 20.0f, .height = 4.0f}};
    PlayerParams player;                                  // The player's start.
    float turn_speed = std::numbers::pi_v<float> / 2.0f;  // Radians per second: 90 degrees.
    float look_speed = std::numbers::pi_v<float> / 3.0f;  // Radians per second: 60 degrees.
};

// What the player wants to do during a frame, independent of the input device.
// Rates are in [-1, 1], where 1 means full speed.
struct PlayerInput {
    float turn = 0.0f;  // Positive turns left.
    float look = 0.0f;  // Positive looks up.
};

// Owns the game state. The world holds static level geometry, while the player (and later other
// dynamic entities) live next to it, so systems such as physics or AI can work on both.
class Game {
public:
    Game(const GameConfig& config, std::uint64_t seed);

    // Advances the game by `dt` seconds.
    void update(const PlayerInput& input, float dt);

    const World& world() const { return world_; }
    const Player& player() const { return player_; }
    std::uint64_t seed() const { return seed_; }

private:
    float turn_speed_;
    float look_speed_;
    std::uint64_t seed_;
    Player player_;
    World world_;
};

}  // namespace manifold
