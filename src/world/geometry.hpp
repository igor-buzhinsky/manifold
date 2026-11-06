#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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

// An upright cylinder standing on `base`, e.g. the collision body of a character.
struct VerticalCylinder {
    glm::vec3 base{0.0f};  // Center of the bottom face.
    float radius = 0.0f;
    float height = 0.0f;
};

// True if the shapes overlap or touch.
bool intersects(const RotatedSquare& square, const Circle& circle);

}  // namespace manifold
