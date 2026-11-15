#include "world/world.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/torch.hpp"

namespace manifold {

bool WorldBounds::contains(const glm::vec3& point) const {
    return point.x >= x_min && point.x <= x_max && point.y >= y_min && point.y <= y_max && point.z >= 0.0f &&
           point.z <= height;
}

RotatedRectangle Obstacle::footprint() const {
    return {.center = glm::vec2(base_center), .half_size = glm::vec2(half_size), .yaw = yaw};
}

RotatedRectangle Ramp::footprint() const {
    return {.center = glm::vec2(base_center), .half_size = {half_length, half_width}, .yaw = yaw};
}

RotatedRectangle LevelExit::footprint() const {
    return {.center = glm::vec2(floor_point), .half_size = glm::vec2(kSize / 2.0f), .yaw = 0.0f};
}

bool LevelExit::touches(const VerticalCylinder& body) const {
    const float bottom = base_center().z;
    return body.base.z <= bottom + kSize && body.base.z + body.height >= bottom &&
           intersects(footprint(), Circle{.center = glm::vec2(body.base), .radius = body.radius});
}

World::World(const WorldBounds& bounds) : bounds_(bounds) {}

void World::add_obstacle(const Obstacle& obstacle) { obstacles_.push_back(obstacle); }

void World::add_ramp(const Ramp& ramp) { ramps_.push_back(ramp); }

void World::add_torch(const Torch& torch) { torches_.push_back(torch); }

}  // namespace manifold
