#pragma once

#include <cstdint>

#include "level/level_generator.hpp"
#include "world/player.hpp"
#include "world/world.hpp"

namespace manifold {

struct GameConfig {
    LevelConfig level{.bounds = {.x_min = -20.0f, .x_max = 20.0f, .y_min = -20.0f, .y_max = 20.0f, .height = 4.0f}};
    PlayerParams player;  // The player's start.
};

// Owns the game state. The world holds static level geometry, while the player (and later other
// dynamic entities) live next to it, so systems such as physics or AI can work on both.
class Game {
public:
    Game(const GameConfig& config, std::uint64_t seed);

    const World& world() const { return world_; }
    const Player& player() const { return player_; }
    std::uint64_t seed() const { return seed_; }

private:
    std::uint64_t seed_;
    Player player_;
    World world_;
};

}  // namespace manifold
