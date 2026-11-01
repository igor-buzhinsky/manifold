#include "world/world.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/geometry.hpp"

namespace manifold {

bool WorldBounds::contains(const glm::vec3& point) const {
    return point.x >= x_min && point.x <= x_max && point.y >= y_min && point.y <= y_max && point.z >= 0.0f &&
           point.z <= height;
}

RotatedSquare Obstacle::footprint() const {
    return {.center = glm::vec2(base_center), .half_size = half_size, .yaw = yaw};
}

World::World(const WorldBounds& bounds) : bounds_(bounds) {}

void World::add_obstacle(const Obstacle& obstacle) { obstacles_.push_back(obstacle); }

}  // namespace manifold
