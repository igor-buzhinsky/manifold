#pragma once

#include <optional>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "world/door.hpp"
#include "world/geometry.hpp"
#include "world/obstacle.hpp"
#include "world/torch.hpp"

namespace manifold {

// All coordinates are in metres, z points up, and the floor is at z = 0.

/// The box that encloses a level: x_min <= x <= x_max, y_min <= y <= y_max, 0 <= z <= height.
struct WorldBounds {
    float x_min = 0.0f;
    float x_max = 0.0f;
    float y_min = 0.0f;
    float y_max = 0.0f;
    float height = 0.0f;  ///< Height of the ceiling.

    /// Checks whether a point is inside the box or on its boundary.
    bool contains(const glm::vec3& point) const;
};

/// A wedge: a block with a rectangular footprint whose top slopes up from one end to the other.
///
/// In its own frame, u runs along the slope and v across it. The low end at u = -half_length has zero
/// height, the high end at u = +half_length is `rise` high.
struct Ramp {
    glm::vec3 base_center{0.0f};  ///< Center of the bottom face.
    float half_length = 0.0f;     ///< Half of the length along the slope, in metres.
    float half_width = 0.0f;      ///< Half of the width across the slope, in metres.
    float rise = 0.0f;            ///< Height of the high end above the base, in metres.
    float yaw = 0.0f;             ///< Direction in which the ramp goes up: radians counter-clockwise from +x.

    /// Height of the slope above the base at position `u` along it, in [0, rise] for u in the footprint.
    float height_at(float u) const { return rise * (u + half_length) / (2.0f * half_length); }

    /// The area the ramp occupies in the horizontal plane. The rectangle's own x axis runs up the slope.
    RotatedRectangle footprint() const;
};

/// The way out of a level: for now a cube that floats above the floor. Touching it leads to the next level.
///
/// The cube doesn't collide: the player walks into it. The renderer spins it around the vertical, but whether a
/// body touches it is checked for the cube without rotation.
struct LevelExit {
    static constexpr float kSize = 0.5f;         ///< Side of the cube, in metres.
    static constexpr float kHoverHeight = 0.6f;  ///< Height of the cube's bottom above the floor, in metres.

    glm::vec3 floor_point{0.0f};  ///< The point on the floor right below the center of the cube.

    /// Center of the cube's bottom face.
    glm::vec3 base_center() const { return floor_point + glm::vec3(0.0f, 0.0f, kHoverHeight); }
    /// Center of the cube, where its light is and around which it spins.
    glm::vec3 center() const { return base_center() + glm::vec3(0.0f, 0.0f, kSize / 2.0f); }

    /// The area the cube occupies in the horizontal plane.
    RotatedRectangle footprint() const;

    /// Checks whether a body overlaps or touches the cube.
    bool touches(const VerticalCylinder& body) const;
};

/// The static part of a level: its boundaries, the geometry inside them, the torches on the walls, the doors and the
/// exit.
///
/// Only obstacles, ramps and the boundaries collide. The frames of doors are obstacles too, but the halves are not:
/// how far a door is open changes, so it is kept outside of the world model, with the other dynamic state, such as
/// the player. See DoorState.
class World {
public:
    /// Creates an empty level.
    explicit World(const WorldBounds& bounds);

    const WorldBounds& bounds() const { return bounds_; }
    std::span<const Obstacle> obstacles() const { return obstacles_; }
    std::span<const Ramp> ramps() const { return ramps_; }
    std::span<const Torch> torches() const { return torches_; }
    std::span<const Door> doors() const { return doors_; }
    /// The exit, or none, e.g. in a level built by hand for a test.
    const std::optional<LevelExit>& exit() const { return exit_; }

    void add_obstacle(const Obstacle& obstacle);
    void add_ramp(const Ramp& ramp);
    void add_torch(const Torch& torch);
    /// Adds a door, and its two frames as obstacles, see Door::frame().
    void add_door(const Door& door);
    void set_exit(const LevelExit& exit) { exit_ = exit; }

private:
    WorldBounds bounds_;
    std::vector<Obstacle> obstacles_;
    std::vector<Ramp> ramps_;
    std::vector<Torch> torches_;
    std::vector<Door> doors_;
    std::optional<LevelExit> exit_;
};

}  // namespace manifold
