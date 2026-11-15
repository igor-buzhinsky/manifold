#include "world/world.hpp"

#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/door.hpp"
#include "world/geometry.hpp"
#include "world/obstacle.hpp"
#include "world/spatial_grid.hpp"
#include "world/torch.hpp"

namespace manifold {

namespace {

/// Side of a cell of the grids of obstacles and ramps, in metres: about the size of a room tile. Any size works,
/// only the speed depends on it.
constexpr float kGridCellSize = 2.0f;
/// The boxes of obstacles and ramps in the grids are this much larger on each side, in metres, so that rounding
/// errors of their rotation can't leave out a cell that they touch.
constexpr float kGridMargin = 0.01f;

/// The box of a rectangle in the grids.
Box2 grid_box(const RotatedRectangle& rectangle) {
    const glm::vec2 half_size = bounding_half_size(rectangle) + kGridMargin;
    return {.min = rectangle.center - half_size, .max = rectangle.center + half_size};
}

}  // namespace

bool WorldBounds::contains(const glm::vec3& point) const {
    return point.x >= x_min && point.x <= x_max && point.y >= y_min && point.y <= y_max && point.z >= 0.0f &&
           point.z <= height;
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

World::World(const WorldBounds& bounds)
    : bounds_(bounds),
      obstacle_grid_({.min = {bounds.x_min, bounds.y_min}, .max = {bounds.x_max, bounds.y_max}}, kGridCellSize),
      ramp_grid_({.min = {bounds.x_min, bounds.y_min}, .max = {bounds.x_max, bounds.y_max}}, kGridCellSize) {}

void World::find_obstacles_near(const Box2& box, std::vector<std::size_t>& indices) const {
    obstacle_grid_.find(box, indices);
}

void World::find_ramps_near(const Box2& box, std::vector<std::size_t>& indices) const { ramp_grid_.find(box, indices); }

void World::add_obstacle(const Obstacle& obstacle) {
    obstacle_grid_.insert(obstacles_.size(), grid_box(obstacle.footprint()));
    obstacles_.push_back(obstacle);
}

void World::add_ramp(const Ramp& ramp) {
    ramp_grid_.insert(ramps_.size(), grid_box(ramp.footprint()));
    ramps_.push_back(ramp);
}

void World::add_torch(const Torch& torch) { torches_.push_back(torch); }

void World::add_door(const Door& door) {
    doors_.push_back(door);
    add_obstacle(door.frame(DoorSide::kNegative));
    add_obstacle(door.frame(DoorSide::kPositive));
}

}  // namespace manifold
