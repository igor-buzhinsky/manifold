#pragma once

#include <optional>

#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

/// How a body overlaps with something. Moving the body by `normal * depth` separates them.
struct Contact {
    glm::vec3 normal{0.0f};  ///< Unit vector pointing away from the obstacle.
    float depth = 0.0f;      ///< How far the body is inside, in metres. Always positive.
};

/// Finds the overlap of a body with an obstacle.
///
/// The normal can be horizontal (sides) or vertical (top and bottom faces), whichever separates them with
/// the shorter push. Bodies that only touch the obstacle don't overlap.
///
/// \return The contact, or nothing if they don't overlap.
std::optional<Contact> find_contact(const VerticalCylinder& body, const Obstacle& obstacle);

/// Finds the overlap of a body with the level boundaries: walls, floor and ceiling.
///
/// \return The deepest contact, or nothing if the body is fully inside the level.
std::optional<Contact> find_contact(const VerticalCylinder& body, const WorldBounds& bounds);

/// Where a body ended up after move_and_slide().
struct SlideResult {
    glm::vec3 position{0.0f};  ///< The new base position of the body.
    /// The sum of all pushes out of the world geometry. Its z is positive when the body landed on
    /// something, and negative when it bumped into something above.
    glm::vec3 correction{0.0f};
};

/// Moves a body through the world without letting it enter obstacles or leave the level.
///
/// Whenever the body runs into the world geometry, it is pushed back out along the contact normal. This
/// removes the part of the movement that goes into the surface, so the body slides along walls instead of
/// stopping. Long movements are split into short steps, so the body can't skip through thin walls.
///
/// \param world        The geometry to collide with.
/// \param body         The body at its start position.
/// \param displacement The wanted movement in metres.
SlideResult move_and_slide(const World& world, const VerticalCylinder& body, const glm::vec3& displacement);

}  // namespace manifold
