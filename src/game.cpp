#include "game.hpp"

#include <cmath>
#include <cstdint>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "level/level_generator.hpp"

namespace manifold {

Game::Game(const GameConfig& config, std::uint64_t seed)
    : turn_speed_(config.turn_speed),
      look_speed_(config.look_speed),
      move_speed_(config.move_speed),
      seed_(seed),
      player_(config.player),
      world_(generate_level(config.level, player_.footprint(), seed)) {}

void Game::update(const PlayerInput& input, float dt) {
    player_.rotate(input.turn * turn_speed_ * dt, input.look * look_speed_ * dt);

    // Walking is horizontal, in the direction the player faces, whatever the pitch.
    // Walking diagonally is not faster than walking straight.
    glm::vec2 direction{input.forward, input.strafe};
    const float length = glm::length(direction);
    if (length > 1.0f) {
        direction /= length;
    }
    const glm::vec3 forward{std::cos(player_.yaw()), std::sin(player_.yaw()), 0.0f};
    const glm::vec3 right{forward.y, -forward.x, 0.0f};
    // No collision checking yet: the player can walk through obstacles and walls.
    player_.set_position(player_.position() + (((direction.x * forward) + (direction.y * right)) * move_speed_ * dt));
}

}  // namespace manifold
