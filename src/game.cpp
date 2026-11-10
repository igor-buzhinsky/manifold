#include "game.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

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
    : Game(config, generate_level(config.level, layout, config.player.radius, seed), seed) {}

Game::Game(const GameConfig& config, Level level, std::uint64_t seed)
    : turn_speed_(config.turn_speed),
      look_speed_(config.look_speed),
      move_speed_(config.move_speed),
      max_snap_slope_(config.max_snap_slope),
      jump_speed_(std::sqrt(2.0f * kEarthGravity * config.jump_height)),
      seed_(seed),
      level_(std::move(level)),
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
        player_.set_on_ground(false);
        return;
    }

    float start_speed = player_.vertical_speed();
    if (input.rise > 0.0f && player_.on_ground()) {
        // The extra half step of gravity makes up for the position lagging behind the speed in the update
        // below, so the peak is the same at any frame rate.
        start_speed = jump_speed_ + (level_.gravity * dt / 2.0f);
    }
    // Speed changes before the position (semi-implicit Euler), which keeps falls stable.
    const float vertical_speed = start_speed - (level_.gravity * dt);
    displacement.z = vertical_speed * dt;
    const SlideResult result = move_and_slide(level_.world, player_.body(), displacement);
    player_.set_position(result.position);
    bool landed = vertical_speed < 0.0f && result.correction.z > 0.0f;

    // Walking down a slope, the ground drops away faster than gravity pulls the player down at first. If
    // there is ground close below, move down onto it, so the player doesn't fall in small hops.
    if (!landed && player_.on_ground() && vertical_speed <= 0.0f) {
        const float snap = glm::length(glm::vec2(displacement)) * max_snap_slope_;
        const SlideResult snapped = move_and_slide(level_.world, player_.body(), {0.0f, 0.0f, -snap});
        if (snapped.correction.z > 0.0f) {
            player_.set_position(snapped.position);
            landed = true;
        }
    }

    // Landing on something, or bumping into something above, stops the vertical movement.
    const bool bumped = vertical_speed > 0.0f && result.correction.z < 0.0f;
    player_.set_vertical_speed(landed || bumped ? 0.0f : vertical_speed);
    player_.set_on_ground(landed);
}

}  // namespace manifold
