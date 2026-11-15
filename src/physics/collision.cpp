#include "physics/collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/obstacle.hpp"
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

/// Replaces `shallowest` with `contact` if `contact` is shallower.
void keep_shallowest(std::optional<Contact>& shallowest, const Contact& contact) {
    if (!shallowest || contact.depth < shallowest->depth) {
        shallowest = contact;
    }
}

/// Indices of the obstacles and ramps near a body, kept between searches so that they don't allocate memory each time.
struct NearbyIndices {
    std::vector<std::size_t> obstacles;
    std::vector<std::size_t> ramps;
};

/// Finds the deepest overlap of a body with anything in the world or with a blocker, if any.
///
/// \param world        The geometry to collide with. Only its obstacles and ramps near the body are tested.
/// \param blockers     More obstacles to collide with. All of them are tested.
/// \param body         The body.
/// \param[out] nearby  Space for the search of nearby obstacles and ramps. Its contents are replaced.
std::optional<Contact> find_deepest_contact(const World& world, std::span<const Obstacle> blockers,
                                            const VerticalCylinder& body, NearbyIndices& nearby) {
    // Only what overlaps the box around the body's footprint can touch it.
    const Box2 box{.min = glm::vec2(body.base) - body.radius, .max = glm::vec2(body.base) + body.radius};
    world.find_obstacles_near(box, nearby.obstacles);
    world.find_ramps_near(box, nearby.ramps);

    // The indices are in increasing order, so ties are resolved as when testing everything in order.
    std::optional<Contact> deepest = find_contact(body, world.bounds());
    for (const std::size_t index : nearby.obstacles) {
        keep_deepest(deepest, find_contact(body, world.obstacles()[index]));
    }
    for (const Obstacle& blocker : blockers) {
        keep_deepest(deepest, find_contact(body, blocker));
    }
    for (const std::size_t index : nearby.ramps) {
        keep_deepest(deepest, find_contact(body, world.ramps()[index]));
    }
    return deepest;
}

/// The horizontal frame of a block rotated around the vertical axis, where its footprint is axis-aligned.
class LocalFrame {
public:
    /// \param origin Center of the block. Only x and y are used.
    /// \param yaw    Rotation of the block, in radians counter-clockwise.
    LocalFrame(const glm::vec3& origin, float yaw) : origin_(origin), cos_(std::cos(yaw)), sin_(std::sin(yaw)) {}

    /// Converts a world point to the frame, ignoring z.
    glm::vec2 to_local(const glm::vec3& point) const {
        const glm::vec2 offset = glm::vec2(point) - origin_;
        return {(cos_ * offset.x) + (sin_ * offset.y), (-sin_ * offset.x) + (cos_ * offset.y)};
    }

    /// Converts a horizontal direction from the frame to the world.
    glm::vec3 to_world_direction(const glm::vec2& direction) const {
        return {(cos_ * direction.x) - (sin_ * direction.y), (sin_ * direction.x) + (cos_ * direction.y), 0.0f};
    }

private:
    glm::vec2 origin_;
    float cos_;
    float sin_;
};

/// Finds the shortest horizontal push of a vertical cylinder out of a block's sides.
///
/// \param frame     The block's frame.
/// \param local     The cylinder's axis in that frame.
/// \param half_size Half of the footprint's size along the frame's axes.
/// \param radius    The cylinder's radius.
/// \return The contact, or nothing if the footprints don't overlap.
std::optional<Contact> side_contact(const LocalFrame& frame, const glm::vec2& local, const glm::vec2& half_size,
                                    float radius) {
    const glm::vec2 closest = glm::clamp(local, -half_size, half_size);
    const glm::vec2 outside = local - closest;
    const float distance = glm::length(outside);
    if (distance > 0.0f) {
        if (distance >= radius) {
            return std::nullopt;
        }
        return Contact{.normal = frame.to_world_direction(outside / distance), .depth = radius - distance};
    }
    // The axis of the cylinder is inside the footprint: leave through the nearest side.
    const float to_x_side = half_size.x - std::abs(local.x);
    const float to_y_side = half_size.y - std::abs(local.y);
    if (to_x_side < to_y_side) {
        return Contact{.normal = frame.to_world_direction({local.x >= 0.0f ? 1.0f : -1.0f, 0.0f}),
                       .depth = to_x_side + radius};
    }
    return Contact{.normal = frame.to_world_direction({0.0f, local.y >= 0.0f ? 1.0f : -1.0f}),
                   .depth = to_y_side + radius};
}

}  // namespace

std::optional<Contact> find_contact(const VerticalCylinder& body, const Obstacle& obstacle) {
    const float body_top = body.base.z + body.height;
    const float obstacle_top = obstacle.base_center.z + obstacle.height;
    if (body.base.z >= obstacle_top || body_top <= obstacle.base_center.z) {
        return std::nullopt;
    }
    const LocalFrame frame(obstacle.base_center, obstacle.yaw);
    const glm::vec2 local = frame.to_local(body.base);
    std::optional<Contact> contact = side_contact(frame, local, obstacle.half_size, body.radius);
    if (!contact) {
        return std::nullopt;
    }
    // Pushing up onto the obstacle or down below it may be shorter, e.g. for a low block.
    keep_shallowest(contact, {.normal = {0.0f, 0.0f, 1.0f}, .depth = obstacle_top - body.base.z});
    keep_shallowest(contact, {.normal = {0.0f, 0.0f, -1.0f}, .depth = body_top - obstacle.base_center.z});
    return contact;
}

std::optional<Contact> find_contact(const VerticalCylinder& body, const Ramp& ramp) {
    const float body_top = body.base.z + body.height;
    if (body.base.z >= ramp.base_center.z + ramp.rise || body_top <= ramp.base_center.z) {
        return std::nullopt;
    }
    const LocalFrame frame(ramp.base_center, ramp.yaw);
    const glm::vec2 local = frame.to_local(body.base);
    const glm::vec2 half_size{ramp.half_length, ramp.half_width};
    std::optional<Contact> contact = side_contact(frame, local, half_size, body.radius);
    if (!contact) {
        return std::nullopt;
    }

    // The flat bottom of the body rests on the highest point of the slope below it. That is the point of the
    // footprint inside the body's circle that is furthest up the slope: across the slope it is as close to
    // the body's axis as the footprint allows, and along the slope as far as the circle reaches.
    const float across = glm::clamp(local.y, -half_size.y, half_size.y) - local.y;
    const float reach = std::sqrt(std::max(0.0f, (body.radius * body.radius) - (across * across)));
    const float highest_u = std::min(half_size.x, local.x + reach);
    const float up_depth = ramp.base_center.z + ramp.height_at(highest_u) - body.base.z;
    if (up_depth <= 0.0f) {
        return std::nullopt;  // The body is above the slope.
    }
    // Pushing straight up, not along the slope's normal, lets the body walk up at full speed, and keeps it
    // from sliding down when it stands still.
    keep_shallowest(contact, {.normal = {0.0f, 0.0f, 1.0f}, .depth = up_depth});
    keep_shallowest(contact, {.normal = {0.0f, 0.0f, -1.0f}, .depth = body_top - ramp.base_center.z});
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

SlideResult move_and_slide(const World& world, const VerticalCylinder& body, const glm::vec3& displacement,
                           std::span<const Obstacle> blockers) {
    const float max_step = kMaxStepFraction * body.radius;
    const int steps = std::max(1, static_cast<int>(std::ceil(glm::length(displacement) / max_step)));
    const glm::vec3 step = displacement / static_cast<float>(steps);

    VerticalCylinder moved = body;
    glm::vec3 correction{0.0f};
    NearbyIndices nearby;
    for (int i = 0; i < steps; ++i) {
        moved.base += step;
        for (int iteration = 0; iteration < kMaxResolveIterations; ++iteration) {
            const std::optional<Contact> contact = find_deepest_contact(world, blockers, moved, nearby);
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
