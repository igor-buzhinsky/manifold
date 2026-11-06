#include "render/world_mesh.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "world/world.hpp"

namespace manifold {

namespace {

constexpr glm::vec3 kFloorColor{0.30f, 0.28f, 0.25f};
constexpr glm::vec3 kCeilingColor{0.20f, 0.20f, 0.22f};
constexpr glm::vec3 kWallColor{0.45f, 0.40f, 0.33f};
constexpr glm::vec3 kObstacleColor{0.35f, 0.38f, 0.45f};

// Adds a quad given by its corners in counter-clockwise order, as seen from the front side.
void add_quad(std::vector<Vertex>& vertices, const std::array<glm::vec3, 4>& corners, const glm::vec3& color) {
    const glm::vec3 normal = glm::normalize(glm::cross(corners[1] - corners[0], corners[2] - corners[0]));
    for (const int index : {0, 1, 2, 0, 2, 3}) {
        vertices.push_back({.position = corners.at(index), .normal = normal, .color = color});
    }
}

// Adds a vertical wall from `from` to `to` (on the floor), facing left of the from -> to direction.
void add_wall(std::vector<Vertex>& vertices, const glm::vec3& from, const glm::vec3& to, float height,
              const glm::vec3& color) {
    const glm::vec3 up{0.0f, 0.0f, height};
    add_quad(vertices, {to, from, from + up, to + up}, color);
}

}  // namespace

std::vector<Vertex> build_world_mesh(const World& world) {
    std::vector<Vertex> vertices;
    const WorldBounds& bounds = world.bounds();
    const float h = bounds.height;

    // Corners of the floor, counter-clockwise when seen from above.
    const glm::vec3 a{bounds.x_min, bounds.y_min, 0.0f};
    const glm::vec3 b{bounds.x_max, bounds.y_min, 0.0f};
    const glm::vec3 c{bounds.x_max, bounds.y_max, 0.0f};
    const glm::vec3 d{bounds.x_min, bounds.y_max, 0.0f};
    const glm::vec3 up{0.0f, 0.0f, h};

    add_quad(vertices, {a, b, c, d}, kFloorColor);
    add_quad(vertices, {d + up, c + up, b + up, a + up}, kCeilingColor);
    // Going counter-clockwise around the level, the inside is on the left, so the walls face inwards.
    add_wall(vertices, a, b, h, kWallColor);
    add_wall(vertices, b, c, h, kWallColor);
    add_wall(vertices, c, d, h, kWallColor);
    add_wall(vertices, d, a, h, kWallColor);

    for (const Obstacle& obstacle : world.obstacles()) {
        // Footprint corners, counter-clockwise. Walls go through them clockwise to face outwards.
        std::array<glm::vec3, 4> corners{};
        const float cos_yaw = std::cos(obstacle.yaw);
        const float sin_yaw = std::sin(obstacle.yaw);
        const std::array<float, 4> local_x{-1.0f, 1.0f, 1.0f, -1.0f};
        const std::array<float, 4> local_y{-1.0f, -1.0f, 1.0f, 1.0f};
        for (std::size_t i = 0; i < corners.size(); ++i) {
            const float x = local_x.at(i) * obstacle.half_size;
            const float y = local_y.at(i) * obstacle.half_size;
            corners.at(i) =
                obstacle.base_center + glm::vec3((cos_yaw * x) - (sin_yaw * y), (sin_yaw * x) + (cos_yaw * y), 0.0f);
        }
        for (std::size_t i = 0; i < corners.size(); ++i) {
            add_wall(vertices, corners.at((i + 1) % corners.size()), corners.at(i), obstacle.height, kObstacleColor);
        }
        // The top is hidden when the obstacle reaches the ceiling. The bottom always stands on the floor.
        if (obstacle.base_center.z + obstacle.height < h) {
            const glm::vec3 top{0.0f, 0.0f, obstacle.height};
            add_quad(vertices, {corners[0] + top, corners[1] + top, corners[2] + top, corners[3] + top},
                     kObstacleColor);
        }
    }
    return vertices;
}

}  // namespace manifold
