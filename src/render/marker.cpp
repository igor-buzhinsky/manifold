#include "render/marker.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace manifold {

std::vector<glm::vec3> build_arrow(const glm::vec3& feet, float yaw) {
    // The arrow in its own coordinates: tip along +x, centered on the origin.
    constexpr float kTail = -kArrowLength / 2.0f;
    constexpr float kTip = kArrowLength / 2.0f;
    constexpr float kNeck = kTip - (0.4f * kArrowLength);  // Where the head starts.
    constexpr float kShaftHalfWidth = 0.08f;
    constexpr float kHeadHalfWidth = 0.3f;
    // High enough above the floor that it doesn't flicker against it when seen from a level's height away.
    constexpr float kLift = 0.02f;
    const std::array<glm::vec2, 7> corners{{
        {kTail, -kShaftHalfWidth},
        {kNeck, -kShaftHalfWidth},
        {kNeck, kShaftHalfWidth},
        {kTail, kShaftHalfWidth},
        {kNeck, -kHeadHalfWidth},
        {kTip, 0.0f},
        {kNeck, kHeadHalfWidth},
    }};
    constexpr std::array<std::array<std::size_t, 3>, 3> kTriangles{{{0, 1, 2}, {0, 2, 3}, {4, 5, 6}}};

    const glm::vec2 along{std::cos(yaw), std::sin(yaw)};
    const glm::vec2 left{-along.y, along.x};
    std::vector<glm::vec3> triangles;
    for (const auto& triangle : kTriangles) {
        for (const std::size_t index : triangle) {
            const glm::vec2 corner = corners.at(index);
            const glm::vec2 offset = (corner.x * along) + (corner.y * left);
            triangles.emplace_back(feet.x + offset.x, feet.y + offset.y, feet.z + kLift);
        }
    }
    return triangles;
}

}  // namespace manifold
