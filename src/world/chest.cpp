#include "world/chest.hpp"

#include <cmath>
#include <numbers>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/obstacle.hpp"

namespace manifold {

std::string_view chest_look_name(ChestLook look) {
    switch (look) {
        case ChestLook::kWood:
            return "wooden";
        case ChestLook::kGold:
            return "golden";
    }
    return "unknown";
}

std::string_view chest_title(ChestLook look) {
    switch (look) {
        case ChestLook::kWood:
            return "Wooden chest";
        case ChestLook::kGold:
            return "Golden chest";
    }
    return "Chest";
}

Chest Chest::at_wall(const glm::vec3& wall_point, float yaw, ChestLook look) {
    // The lid reaches out over the back of the body too, so the gap is measured from the lid.
    const float from_wall = kWallGap + kLidOverhang + (kDepth / 2.0f);
    const glm::vec3 outward{std::cos(yaw), std::sin(yaw), 0.0f};
    return {.base_center = wall_point + (from_wall * outward), .yaw = yaw, .look = look};
}

Obstacle Chest::body() const {
    // The chest's x runs along its width, while yaw points its y (the front) away from the wall, so the obstacle,
    // whose yaw turns its own x, is turned a quarter less.
    return {.base_center = base_center,
            .half_size = glm::vec2(kWidth, kDepth) / 2.0f,
            .height = kHeight - kLidHeight,
            .yaw = yaw - (std::numbers::pi_v<float> / 2.0f)};
}

Obstacle Chest::lid() const {
    Obstacle lid = body();
    lid.base_center.z += lid.height;
    lid.half_size += kLidOverhang;
    lid.height = kLidHeight;
    return lid;
}

RotatedRectangle Chest::footprint() const { return lid().footprint(); }

}  // namespace manifold
