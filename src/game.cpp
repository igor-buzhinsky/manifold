#include "game.hpp"

#include <cstdint>

#include "level/level_generator.hpp"

namespace manifold {

Game::Game(const GameConfig& config, std::uint64_t seed)
    : turn_speed_(config.turn_speed),
      look_speed_(config.look_speed),
      seed_(seed),
      player_(config.player),
      world_(generate_level(config.level, player_.footprint(), seed)) {}

void Game::update(const PlayerInput& input, float dt) {
    player_.rotate(input.turn * turn_speed_ * dt, input.look * look_speed_ * dt);
}

}  // namespace manifold
