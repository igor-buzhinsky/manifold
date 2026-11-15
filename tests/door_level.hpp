#pragma once

#include <numbers>
#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "level/level.hpp"
#include "world/door.hpp"
#include "world/world.hpp"

namespace manifold {

/// A level built by hand for tests of doors: a wall along x at y = 0 with door 0 in the middle, and the player at
/// (0, -1.9) facing the door along +y, within reach to activate it.
///
/// The level is 20 x 20 m and 3 m high, with its floor at z = 0. The door's opening is 2 m wide, from x = -1 to 1, as
/// high as the level, so the frames are at x from -1 to -0.8 and from 0.8 to 1. The wall is made of two blocks next to
/// the opening, each 2 m long, and the rest of the line y = 0 is open.
inline Level make_door_level() {
    World world({.x_min = -10.0f, .x_max = 10.0f, .y_min = -10.0f, .y_max = 10.0f, .height = 3.0f});
    for (const float x : {-2.0f, 2.0f}) {
        world.add_obstacle({.base_center = {x, 0.0f, 0.0f}, .half_size = glm::vec2(1.0f), .height = 3.0f});
    }
    world.add_door({.base_center = {0.0f, 0.0f, 0.0f}, .width = 2.0f, .height = 3.0f, .yaw = 0.0f});
    return {.layout = LayoutKind::kDebug,
            .world = std::move(world),
            .player_start = {0.0f, -1.9f, 0.0f},
            .player_yaw = std::numbers::pi_v<float> / 2.0f,
            .info = "A wall with a door"};
}

}  // namespace manifold
