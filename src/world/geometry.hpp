#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace manifold {

/// A circle in the horizontal (x, y) plane, e.g. the footprint of a standing character.
struct Circle {
    glm::vec2 center{0.0f};
    float radius = 0.0f;
};

/// A square in the horizontal (x, y) plane, rotated around its center.
struct RotatedSquare {
    glm::vec2 center{0.0f};
    float half_size = 0.0f;  ///< Half of the side length.
    float yaw = 0.0f;        ///< Radians, counter-clockwise around +z.
};

/// An upright cylinder, e.g. the collision body of a character.
struct VerticalCylinder {
    glm::vec3 base{0.0f};  ///< Center of the bottom face.
    float radius = 0.0f;
    float height = 0.0f;
};

/// Checks whether a square and a circle overlap or touch.
bool intersects(const RotatedSquare& square, const Circle& circle);

}  // namespace manifold
