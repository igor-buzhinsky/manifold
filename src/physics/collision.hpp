#pragma once

#include <optional>

#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

// How a body overlaps with something. Moving the body by `normal * depth` separates them.
struct Contact {
    glm::vec3 normal{0.0f};  // Unit vector pointing away from the obstacle.
    float depth = 0.0f;
};

// The overlap of a body with an obstacle, if any. The normal can be horizontal (sides) or vertical
// (top and bottom faces), whichever separates them with the shorter push.
std::optional<Contact> find_contact(const VerticalCylinder& body, const Obstacle& obstacle);

// The overlap of a body with the level boundaries (walls, floor and ceiling), if any.
std::optional<Contact> find_contact(const VerticalCylinder& body, const WorldBounds& bounds);

// Moves the body by `displacement` and returns its new base position. Whenever the body runs into the
// world geometry, it is pushed back out along the contact normal. This removes the part of the
// movement that goes into the surface, so the body slides along walls instead of stopping.
glm::vec3 move_and_slide(const World& world, const VerticalCylinder& body, const glm::vec3& displacement);

}  // namespace manifold
