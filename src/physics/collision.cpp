#include "physics/collision.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// A single step may move the body at most this fraction of its radius, so it can't skip through thin walls.
constexpr float kMaxStepFraction = 0.5f;
/// How many times per step overlaps are resolved. Several are needed in corners between surfaces.
constexpr int kMaxResolveIterations = 4;

/// Replaces `deepest` with `contact` if `contact` is deeper.
void keep_deepest(std::optional<Contact>& deepest, const std::optional<Contact>& contact) {
    if (contact && (!deepest || contact->depth > deepest->depth)) {
        deepest = contact;
    }
}

/// Finds the deepest overlap of a body with anything in the world, if any.
std::optional<Contact> find_deepest_contact(const World& world, const VerticalCylinder& body) {
    std::optional<Contact> deepest = find_contact(body, world.bounds());
    for (const Obstacle& obstacle : world.obstacles()) {
        keep_deepest(deepest, find_contact(body, obstacle));
    }
    return deepest;
}

}  // namespace

std::optional<Contact> find_contact(const VerticalCylinder& body, const Obstacle& obstacle) {
    const float body_top = body.base.z + body.height;
    const float obstacle_top = obstacle.base_center.z + obstacle.height;
    if (body.base.z >= obstacle_top || body_top <= obstacle.base_center.z) {
        return std::nullopt;
    }

    // Horizontal overlap, computed in the obstacle's frame, where its footprint is axis-aligned.
    const float cos_yaw = std::cos(obstacle.yaw);
    const float sin_yaw = std::sin(obstacle.yaw);
    const glm::vec2 offset = glm::vec2(body.base) - glm::vec2(obstacle.base_center);
    const glm::vec2 local{(cos_yaw * offset.x) + (sin_yaw * offset.y), (-sin_yaw * offset.x) + (cos_yaw * offset.y)};
    const glm::vec2 closest = glm::clamp(local, glm::vec2(-obstacle.half_size), glm::vec2(obstacle.half_size));
    const glm::vec2 outside = local - closest;
    const float distance = glm::length(outside);

    glm::vec2 local_normal{0.0f};
    float horizontal_depth = 0.0f;
    if (distance > 0.0f) {
        if (distance >= body.radius) {
            return std::nullopt;
        }
        local_normal = outside / distance;
        horizontal_depth = body.radius - distance;
    } else {
        // The axis of the body is inside the footprint: leave through the nearest side.
        const float to_x_side = obstacle.half_size - std::abs(local.x);
        const float to_y_side = obstacle.half_size - std::abs(local.y);
        if (to_x_side < to_y_side) {
            local_normal = {local.x >= 0.0f ? 1.0f : -1.0f, 0.0f};
            horizontal_depth = to_x_side + body.radius;
        } else {
            local_normal = {0.0f, local.y >= 0.0f ? 1.0f : -1.0f};
            horizontal_depth = to_y_side + body.radius;
        }
    }

    Contact contact{
        .normal = {(cos_yaw * local_normal.x) - (sin_yaw * local_normal.y),
                   (sin_yaw * local_normal.x) + (cos_yaw * local_normal.y), 0.0f},
        .depth = horizontal_depth,
    };
    // Pushing up onto the obstacle or down below it may be shorter, e.g. for a low block.
    const float up_depth = obstacle_top - body.base.z;
    const float down_depth = body_top - obstacle.base_center.z;
    if (up_depth < contact.depth) {
        contact = {.normal = {0.0f, 0.0f, 1.0f}, .depth = up_depth};
    }
    if (down_depth < contact.depth) {
        contact = {.normal = {0.0f, 0.0f, -1.0f}, .depth = down_depth};
    }
    return contact;
}

std::optional<Contact> find_contact(const VerticalCylinder& body, const WorldBounds& bounds) {
    std::optional<Contact> deepest;
    const auto check = [&deepest](float depth, const glm::vec3& normal) {
        if (depth > 0.0f) {
            keep_deepest(deepest, Contact{.normal = normal, .depth = depth});
        }
    };
    check(bounds.x_min - (body.base.x - body.radius), {1.0f, 0.0f, 0.0f});
    check((body.base.x + body.radius) - bounds.x_max, {-1.0f, 0.0f, 0.0f});
    check(bounds.y_min - (body.base.y - body.radius), {0.0f, 1.0f, 0.0f});
    check((body.base.y + body.radius) - bounds.y_max, {0.0f, -1.0f, 0.0f});
    check(-body.base.z, {0.0f, 0.0f, 1.0f});
    check((body.base.z + body.height) - bounds.height, {0.0f, 0.0f, -1.0f});
    return deepest;
}

SlideResult move_and_slide(const World& world, const VerticalCylinder& body, const glm::vec3& displacement) {
    const float max_step = kMaxStepFraction * body.radius;
    const int steps = std::max(1, static_cast<int>(std::ceil(glm::length(displacement) / max_step)));
    const glm::vec3 step = displacement / static_cast<float>(steps);

    VerticalCylinder moved = body;
    glm::vec3 correction{0.0f};
    for (int i = 0; i < steps; ++i) {
        moved.base += step;
        for (int iteration = 0; iteration < kMaxResolveIterations; ++iteration) {
            const std::optional<Contact> contact = find_deepest_contact(world, moved);
            if (!contact) {
                break;
            }
            moved.base += contact->normal * contact->depth;
            correction += contact->normal * contact->depth;
        }
    }
    return {.position = moved.base, .correction = correction};
}

}  // namespace manifold
