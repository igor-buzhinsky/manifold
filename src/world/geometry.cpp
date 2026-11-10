#include "world/geometry.hpp"

#include <array>
#include <cmath>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace manifold {

namespace {

/// The rectangle's own x and y axes, as unit vectors in the world.
std::array<glm::vec2, 2> axes_of(const RotatedRectangle& rectangle) {
    const float cos_yaw = std::cos(rectangle.yaw);
    const float sin_yaw = std::sin(rectangle.yaw);
    return {glm::vec2(cos_yaw, sin_yaw), glm::vec2(-sin_yaw, cos_yaw)};
}

/// Half of the length of the rectangle's shadow on a line along the unit vector `axis`.
float half_extent_along(const RotatedRectangle& rectangle, const glm::vec2& axis) {
    const auto [x_axis, y_axis] = axes_of(rectangle);
    return (rectangle.half_size.x * std::abs(glm::dot(x_axis, axis))) +
           (rectangle.half_size.y * std::abs(glm::dot(y_axis, axis)));
}

}  // namespace

bool intersects(const RotatedRectangle& rectangle, const Circle& circle) {
    // Move the circle center into the rectangle's local frame, where the rectangle is axis-aligned.
    const auto [x_axis, y_axis] = axes_of(rectangle);
    const glm::vec2 offset = circle.center - rectangle.center;
    const glm::vec2 local{glm::dot(offset, x_axis), glm::dot(offset, y_axis)};

    // The closest point of the rectangle to the circle center decides the intersection.
    const glm::vec2 closest = glm::clamp(local, -rectangle.half_size, rectangle.half_size);
    const glm::vec2 delta = local - closest;
    return glm::dot(delta, delta) <= circle.radius * circle.radius;
}

bool intersects(const RotatedRectangle& first, const RotatedRectangle& second) {
    // Separating axis test: two rectangles are apart exactly if their shadows don't overlap on one of the
    // four axes along their sides.
    const glm::vec2 offset = second.center - first.center;
    for (const RotatedRectangle* rectangle : {&first, &second}) {
        for (const glm::vec2& axis : axes_of(*rectangle)) {
            if (std::abs(glm::dot(offset, axis)) > half_extent_along(first, axis) + half_extent_along(second, axis)) {
                return false;
            }
        }
    }
    return true;
}

glm::vec2 bounding_half_size(const RotatedRectangle& rectangle) {
    return {half_extent_along(rectangle, {1.0f, 0.0f}), half_extent_along(rectangle, {0.0f, 1.0f})};
}

}  // namespace manifold
