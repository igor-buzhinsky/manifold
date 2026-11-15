#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <glm/vec3.hpp>

#include "world/door.hpp"
#include "world/obstacle.hpp"
#include "world/world.hpp"

namespace manifold {

/// A half-line from a point in a direction, e.g. along the view of the player.
struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{1.0f, 0.0f, 0.0f};  ///< Unit vector.
};

/// Finds where a ray enters an obstacle.
///
/// \param ray          The ray.
/// \param obstacle     The obstacle.
/// \param max_distance How far along the ray to look, in metres.
/// \return The distance from the origin to where the ray enters the obstacle, 0 if the origin is inside it or on its
///         surface, or nothing if the ray misses it within `max_distance`.
std::optional<float> cast_ray(const Ray& ray, const Obstacle& obstacle, float max_distance);

/// What kind of thing a ray hit.
enum class RayTarget : std::uint8_t {
    kWorld,      ///< An obstacle of the world that is not a door frame, e.g. a wall or a slab.
    kDoor,       ///< A door that is not passable, see Door::blocker().
    kDoorFrame,  ///< A frame of a door, see Door::frame().
};

/// The first thing that a ray hit.
struct RayHit {
    float distance = 0.0f;  ///< From the origin of the ray to the hit, in metres.
    RayTarget target = RayTarget::kWorld;
    std::size_t door = 0;  ///< Index of the door in World::doors(), for kDoor and kDoorFrame.
};

/// Finds the first thing a ray hits in a level: an obstacle, a door that is not passable, or a door frame.
///
/// Ramps and the level boundaries are not tested. A door counts as its blocker, see Door::blocker(), while it is not
/// passable, see DoorState::passable(). Frames are also obstacles of the world, so a hit on a frame is reported as
/// kDoorFrame, not as kWorld.
///
/// Tests only the obstacles and doors in the grid cells under the box around the first `max_distance` metres of the
/// ray, see World::find_obstacles_near(). That box is small for short rays and for rays along x or y, but long
/// diagonal rays test a large part of the level, so they are meant for single events, such as a key press, not for
/// every frame.
///
/// \param world        The level.
/// \param doors        The state of each door of `world.doors()`, in the same order. Doors without a state count as
///                     closed.
/// \param ray          The ray.
/// \param max_distance How far along the ray to look, in metres.
/// \return The hit, or nothing if the ray hits nothing within `max_distance`.
std::optional<RayHit> cast_ray(const World& world, std::span<const DoorState> doors, const Ray& ray,
                               float max_distance);

}  // namespace manifold
