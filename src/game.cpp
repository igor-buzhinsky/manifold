#include "game.hpp"

#include <cstdint>

#include "level/level_generator.hpp"

namespace manifold {

Game::Game(const GameConfig& config, std::uint64_t seed)
    : seed_(seed), player_(config.player), world_(generate_level(config.level, player_.footprint(), seed)) {}

}  // namespace manifold
