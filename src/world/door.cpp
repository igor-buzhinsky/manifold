#include "world/door.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/obstacle.hpp"

namespace manifold {

namespace {

/// A block of the door, from the floor to the top of the opening, centered at `u` along the wall.
Obstacle door_block(const Door& door, float u, const glm::vec2& half_size, Surface surface) {
    const glm::vec3 along{std::cos(door.yaw), std::sin(door.yaw), 0.0f};
    return {.base_center = door.base_center + (u * along),
            .half_size = half_size,
            .height = door.height,
            .yaw = door.yaw,
            .surface = surface};
}

/// The sign of u on a side of the door: -1 or 1.
float side_sign(DoorSide side) { return side == DoorSide::kNegative ? -1.0f : 1.0f; }

}  // namespace

const char* door_look_name(DoorLook look) { return look == DoorLook::kMetal ? "metal" : "wood"; }

Obstacle Door::frame(DoorSide side) const {
    return door_block(*this, side_sign(side) * (width - kFrameWidth) / 2.0f,
                      glm::vec2(kFrameWidth / 2.0f, kFrameDepth / 2.0f), Surface::kDark);
}

Obstacle Door::blocker() const {
    return door_block(*this, 0.0f, glm::vec2(gap() / 2.0f, kThickness / 2.0f), Surface::kTextured);
}

std::array<Obstacle, 2> Door::halves(float openness) const {
    // Fully open, the inner edge of a half reaches the outer side of its frame, so the half is inside the wall.
    const float slide = std::clamp(openness, 0.0f, 1.0f) * width / 2.0f;
    const glm::vec2 half_size(gap() / 4.0f, kThickness / 2.0f);
    const float center = (gap() / 4.0f) + slide;
    return {door_block(*this, -center, half_size, Surface::kTextured),
            door_block(*this, center, half_size, Surface::kTextured)};
}

void DoorState::advance(float dt) {
    const float step = dt / Door::kMoveTime;
    openness = target > openness ? std::min(target, openness + step) : std::max(target, openness - step);
}

}  // namespace manifold
