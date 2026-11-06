#pragma once

#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "world/geometry.hpp"

namespace manifold {

// All coordinates are in metres, z points up, and the floor is at z = 0.

// The box that encloses a level: x_min <= x <= x_max, y_min <= y <= y_max, 0 <= z <= height.
struct WorldBounds {
    float x_min = 0.0f;
    float x_max = 0.0f;
    float y_min = 0.0f;
    float y_max = 0.0f;
    float height = 0.0f;

    bool contains(const glm::vec3& point) const;
};

// A block with a square footprint, rotated around the vertical axis.
struct Obstacle {
    glm::vec3 base_center{0.0f};  // Center of the bottom face.
    float half_size = 0.0f;
    float height = 0.0f;
    float yaw = 0.0f;  // Radians, counter-clockwise around +z.

    RotatedSquare footprint() const;
};

// The static part of a level: its boundaries and the geometry inside them.
// Dynamic entities, such as the player, are kept outside of the world model.
class World {
public:
    explicit World(const WorldBounds& bounds);

    const WorldBounds& bounds() const { return bounds_; }
    std::span<const Obstacle> obstacles() const { return obstacles_; }

    void add_obstacle(const Obstacle& obstacle);

private:
    WorldBounds bounds_;
    std::vector<Obstacle> obstacles_;
};

}  // namespace manifold
