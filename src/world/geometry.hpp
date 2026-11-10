#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace manifold {

/// A circle in the horizontal (x, y) plane, e.g. the footprint of a standing character.
struct Circle {
    glm::vec2 center{0.0f};
    float radius = 0.0f;
};

/// A rectangle in the horizontal (x, y) plane, rotated around its center.
struct RotatedRectangle {
    glm::vec2 center{0.0f};
    glm::vec2 half_size{0.0f};  ///< Half of the side lengths, along the rectangle's own x and y axes.
    float yaw = 0.0f;           ///< Radians, counter-clockwise around +z.
};

/// An upright cylinder, e.g. the collision body of a character.
struct VerticalCylinder {
    glm::vec3 base{0.0f};  ///< Center of the bottom face.
    float radius = 0.0f;
    float height = 0.0f;
};

/// Checks whether a rectangle and a circle overlap or touch.
bool intersects(const RotatedRectangle& rectangle, const Circle& circle);

/// Checks whether two rectangles overlap or touch.
bool intersects(const RotatedRectangle& first, const RotatedRectangle& second);

/// Half of the size of the axis-aligned box around a rectangle, along x and y.
glm::vec2 bounding_half_size(const RotatedRectangle& rectangle);

}  // namespace manifold
