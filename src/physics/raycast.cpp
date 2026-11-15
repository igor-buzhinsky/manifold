#include "physics/raycast.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/door.hpp"
#include "world/geometry.hpp"
#include "world/obstacle.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// How much nearer an obstacle of the world must be than a door part to count as hit first, in metres. Frames are
/// both, so this makes them count as frames.
constexpr float kTieTolerance = 1e-4f;

/// Keeps `hit` if it is nearer than `nearest`, or `nearest` has none yet.
void keep_nearest(std::optional<RayHit>& nearest, const RayHit& hit) {
    if (!nearest || hit.distance < nearest->distance) {
        nearest = hit;
    }
}

/// The box in the horizontal plane around the part of a ray within `max_distance` of its origin.
Box2 ray_box(const Ray& ray, float max_distance) {
    const glm::vec2 start(ray.origin);
    glm::vec2 end = start;
    for (int axis = 0; axis < 2; ++axis) {
        // Skipped along axes the ray doesn't move along, so that an infinite `max_distance` doesn't give NaN.
        if (ray.direction[axis] != 0.0f) {
            end[axis] += ray.direction[axis] * max_distance;
        }
    }
    return {.min = glm::min(start, end), .max = glm::max(start, end)};
}

}  // namespace

std::optional<float> cast_ray(const Ray& ray, const Obstacle& obstacle, float max_distance) {
    // In the obstacle's frame, it is the axis-aligned box from (-half_size, 0) to (half_size, height).
    const float cos_yaw = std::cos(obstacle.yaw);
    const float sin_yaw = std::sin(obstacle.yaw);
    const auto to_local = [&](const glm::vec3& v) {
        return glm::vec3((cos_yaw * v.x) + (sin_yaw * v.y), (-sin_yaw * v.x) + (cos_yaw * v.y), v.z);
    };
    const glm::vec3 origin = to_local(ray.origin - obstacle.base_center);
    const glm::vec3 direction = to_local(ray.direction);
    const glm::vec3 low{-obstacle.half_size, 0.0f};
    const glm::vec3 high{obstacle.half_size, obstacle.height};

    // The ray is inside the box between `enter` and `leave`: inside the slabs of all three axes at once.
    float enter = 0.0f;
    float leave = max_distance;
    for (int axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0.0f) {
            if (origin[axis] < low[axis] || origin[axis] > high[axis]) {
                return std::nullopt;
            }
            continue;
        }
        float near = (low[axis] - origin[axis]) / direction[axis];
        float far = (high[axis] - origin[axis]) / direction[axis];
        if (near > far) {
            std::swap(near, far);
        }
        enter = std::max(enter, near);
        leave = std::min(leave, far);
        if (enter > leave) {
            return std::nullopt;
        }
    }
    return enter;
}

std::optional<RayHit> cast_ray(const World& world, std::span<const DoorState> doors, const Ray& ray,
                               float max_distance) {
    // Only what overlaps the box around the ray can be hit. The indices are in increasing order, so ties are
    // resolved as when testing everything in order.
    const Box2 box = ray_box(ray, max_distance);
    std::vector<std::size_t> nearby;
    std::optional<RayHit> nearest;
    world.find_doors_near(box, nearby);
    for (const std::size_t i : nearby) {
        const Door& door = world.doors()[i];
        if (i >= doors.size() || !doors[i].passable()) {
            if (const std::optional<float> distance = cast_ray(ray, door.blocker(), max_distance)) {
                keep_nearest(nearest, {.distance = *distance, .target = RayTarget::kDoor, .door = i});
            }
        }
        for (const DoorSide side : {DoorSide::kNegative, DoorSide::kPositive}) {
            if (const std::optional<float> distance = cast_ray(ray, door.frame(side), max_distance)) {
                keep_nearest(nearest, {.distance = *distance, .target = RayTarget::kDoorFrame, .door = i});
            }
        }
    }
    world.find_obstacles_near(box, nearby);
    for (const std::size_t index : nearby) {
        const Obstacle& obstacle = world.obstacles()[index];
        const float limit = nearest ? nearest->distance - kTieTolerance : max_distance;
        if (limit < 0.0f) {
            break;
        }
        if (const std::optional<float> distance = cast_ray(ray, obstacle, limit)) {
            nearest = RayHit{.distance = *distance, .target = RayTarget::kWorld};
        }
    }
    return nearest;
}

}  // namespace manifold
