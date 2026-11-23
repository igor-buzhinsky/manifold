#pragma once

#include <optional>
#include <span>

#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/obstacle.hpp"
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

/// Finds the overlap of a body with a ramp.
///
/// Where the body stands on the slope, it is pushed straight up, so that the flat bottom of the body rests
/// on the highest point of the slope below it. Bodies walking into the slope are therefore lifted onto it,
/// as long as the slope is less steep than 45 degrees. Against the high end and the sides, the body is
/// pushed out horizontally, like from an obstacle.
///
/// \return The contact, or nothing if they don't overlap.
std::optional<Contact> find_contact(const VerticalCylinder& body, const Ramp& ramp);

/// Finds the overlap of a body with the level boundaries: walls, floor and ceiling.
///
/// \return The deepest contact, or nothing if the body is fully inside the level.
std::optional<Contact> find_contact(const VerticalCylinder& body, const WorldBounds& bounds);

/// Where a body ended up after move_and_slide().
struct SlideResult {
    glm::vec3 position{0.0f};  ///< The new base position of the body.
    /// The sum of all pushes out of the world geometry, and of the horizontal movement taken back where the body
    /// couldn't get free, so that `position` is the start plus the displacement plus this. Its z is positive when the
    /// body landed on something, and negative when it bumped into something above.
    glm::vec3 correction{0.0f};
};

/// Moves a body through the world without letting it enter obstacles, ramps or blockers, or leave the level.
///
/// Whenever the body runs into the world geometry, it is pushed back out along the contact normal. This
/// removes the part of the movement that goes into the surface, so the body slides along walls instead of
/// stopping. Long movements are split into short steps, so the body can't skip through thin walls. If the pushes of a
/// step don't get a body that was free before it free again, as in a gap narrower than the body, the step moves only
/// vertically, so the body stops at the gap instead of getting stuck in it. A body that overlaps something already,
/// e.g. after a teleport into such a gap, is pushed as usual, so that it can move out.
///
/// \param world        The geometry to collide with.
/// \param body         The body at its start position.
/// \param displacement The wanted movement in metres.
/// \param blockers     More obstacles to collide with, which are not part of the world, e.g. closed doors.
SlideResult move_and_slide(const World& world, const VerticalCylinder& body, const glm::vec3& displacement,
                           std::span<const Obstacle> blockers = {});

}  // namespace manifold
