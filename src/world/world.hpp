#pragma once

#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "world/geometry.hpp"

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

/// A block with a square footprint, rotated around the vertical axis.
struct Obstacle {
    glm::vec3 base_center{0.0f};  ///< Center of the bottom face.
    float half_size = 0.0f;       ///< Half of the side length of the footprint.
    float height = 0.0f;
    float yaw = 0.0f;  ///< Radians, counter-clockwise around +z.

    /// The area the obstacle occupies in the horizontal plane.
    RotatedRectangle footprint() const;
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

/// The static part of a level: its boundaries and the geometry inside them.
///
/// Dynamic entities, such as the player, are kept outside of the world model.
class World {
public:
    /// Creates an empty level.
    explicit World(const WorldBounds& bounds);

    const WorldBounds& bounds() const { return bounds_; }
    std::span<const Obstacle> obstacles() const { return obstacles_; }
    std::span<const Ramp> ramps() const { return ramps_; }

    void add_obstacle(const Obstacle& obstacle);
    void add_ramp(const Ramp& ramp);

private:
    WorldBounds bounds_;
    std::vector<Obstacle> obstacles_;
    std::vector<Ramp> ramps_;
};

}  // namespace manifold
