#pragma once

#include <glm/vec2.hpp>

namespace manifold {

// A circle in the horizontal (x, y) plane, e.g. the footprint of a standing character.
struct Circle {
    glm::vec2 center{0.0f};
    float radius = 0.0f;
};

// A square in the horizontal plane, rotated by `yaw` radians counter-clockwise around +z.
struct RotatedSquare {
    glm::vec2 center{0.0f};
    float half_size = 0.0f;
    float yaw = 0.0f;
};

// True if the shapes overlap or touch.
bool intersects(const RotatedSquare& square, const Circle& circle);

}  // namespace manifold
