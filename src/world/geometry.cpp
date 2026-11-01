#include "world/geometry.hpp"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace manifold {

bool intersects(const RotatedSquare& square, const Circle& circle) {
    // Move the circle center into the square's local frame, where the square is axis-aligned.
    const glm::vec2 offset = circle.center - square.center;
    const float cos_yaw = std::cos(square.yaw);
    const float sin_yaw = std::sin(square.yaw);
    const glm::vec2 local{(cos_yaw * offset.x) + (sin_yaw * offset.y), (-sin_yaw * offset.x) + (cos_yaw * offset.y)};

    // The closest point of the square to the circle center decides the intersection.
    const glm::vec2 closest = glm::clamp(local, glm::vec2(-square.half_size), glm::vec2(square.half_size));
    const glm::vec2 delta = local - closest;
    return glm::dot(delta, delta) <= circle.radius * circle.radius;
}

}  // namespace manifold
