#include "game.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "physics/collision.hpp"
#include "world/player.hpp"

namespace manifold {

namespace {

/// The player's settings, moved to `position`.
PlayerParams with_position(PlayerParams params, const glm::vec3& position) {
    params.position = position;
    return params;
}

}  // namespace

Game::Game(const GameConfig& config, std::uint64_t seed, std::optional<LayoutKind> layout)
    : turn_speed_(config.turn_speed),
      look_speed_(config.look_speed),
      move_speed_(config.move_speed),
      seed_(seed),
      level_(generate_level(config.level, layout, config.player.radius, seed)),
      player_(with_position(config.player, level_.player_start)) {}

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
    glm::vec3 displacement = ((direction.x * forward) + (direction.y * right)) * move_speed_ * dt;
    if (noclip_) {
        displacement.z = input.rise * move_speed_ * dt;
        player_.set_position(player_.position() + displacement);
        // When noclip is turned off in the air, the fall starts from rest.
        player_.set_vertical_speed(0.0f);
        return;
    }

    // Speed changes before the position (semi-implicit Euler), which keeps falls stable.
    const float vertical_speed = player_.vertical_speed() - (level_.gravity * dt);
    displacement.z = vertical_speed * dt;
    const SlideResult result = move_and_slide(level_.world, player_.body(), displacement);
    player_.set_position(result.position);
    // Landing on something, or bumping into something above, stops the vertical movement.
    const bool blocked =
        (vertical_speed < 0.0f && result.correction.z > 0.0f) || (vertical_speed > 0.0f && result.correction.z < 0.0f);
    player_.set_vertical_speed(blocked ? 0.0f : vertical_speed);
}

}  // namespace manifold
