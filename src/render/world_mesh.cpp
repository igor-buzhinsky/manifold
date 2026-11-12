#include "render/world_mesh.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// Computes the texture coordinates of a point on a plane.
///
/// The texture's u axis is horizontal and its v axis goes down the plane, so that the image is upright on
/// walls. On horizontal planes, u follows +x.
///
/// \param position A point on the plane, in world coordinates.
/// \param normal   Unit normal of the plane, pointing to its visible side.
glm::vec2 texture_coordinates(const glm::vec3& position, const glm::vec3& normal) {
    // The horizontal direction in the plane. It is undefined only for exactly horizontal planes.
    constexpr float kMinLength = 1e-6f;
    const glm::vec3 horizontal = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), normal);
    const float length = glm::length(horizontal);
    const glm::vec3 tangent = length > kMinLength ? horizontal / length : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 bitangent = glm::cross(normal, tangent);
    // Image rows go from the top down, so v grows downwards to keep the image upright.
    return glm::vec2(glm::dot(position, tangent), -glm::dot(position, bitangent)) / kTextureTileSize;
}

/// How a surface looks, see Vertex.
struct Material {
    glm::vec3 color{1.0f};
    float texture_weight = 1.0f;
};

/// The material of walls, floors, ceilings, obstacles and ramps: the level texture as it is.
constexpr Material kLevelMaterial{};

/// Adds a vertex with texture coordinates from its position.
void add_vertex(std::vector<Vertex>& vertices, const glm::vec3& position, const glm::vec3& normal,
                const Material& material) {
    vertices.push_back({.position = position,
                        .normal = normal,
                        .uv = texture_coordinates(position, normal),
                        .color = material.color,
                        .texture_weight = material.texture_weight});
}

/// Adds a triangle. The normal is computed from the corner order.
///
/// \param corners Corners in counter-clockwise order, as seen from the front side.
void add_triangle(std::vector<Vertex>& vertices, const std::array<glm::vec3, 3>& corners,
                  const Material& material = kLevelMaterial) {
    const glm::vec3 normal = glm::normalize(glm::cross(corners[1] - corners[0], corners[2] - corners[0]));
    for (const glm::vec3& corner : corners) {
        add_vertex(vertices, corner, normal, material);
    }
}

/// Adds a quad as two triangles. The normal is computed from the corner order.
///
/// \param corners Corners in counter-clockwise order, as seen from the front side.
void add_quad(std::vector<Vertex>& vertices, const std::array<glm::vec3, 4>& corners,
              const Material& material = kLevelMaterial) {
    const glm::vec3 normal = glm::normalize(glm::cross(corners[1] - corners[0], corners[2] - corners[0]));
    for (const int index : {0, 1, 2, 0, 2, 3}) {
        add_vertex(vertices, corners.at(index), normal, material);
    }
}

/// Number of sides of the prisms that torch handles are made of.
constexpr int kPrismSides = 6;

/// Adds a prism with a regular polygon as its cross-section, with both ends closed.
///
/// \param[out] vertices The triangles are appended here.
/// \param base     Center of the first end.
/// \param axis     Unit vector from the first end to the second.
/// \param radius   Distance from the axis to the corners of the cross-section, in metres.
/// \param length   Distance between the two ends, in metres.
/// \param material How all faces look.
void add_prism(std::vector<Vertex>& vertices, const glm::vec3& base, const glm::vec3& axis, float radius, float length,
               const Material& material) {
    // Two unit vectors that are perpendicular to the axis and to each other, with u x v = axis.
    const glm::vec3 helper = std::abs(axis.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 u = glm::normalize(glm::cross(helper, axis));
    const glm::vec3 v = glm::cross(axis, u);
    std::array<glm::vec3, kPrismSides> bottom{};
    std::array<glm::vec3, kPrismSides> top{};
    for (std::size_t i = 0; i < bottom.size(); ++i) {
        const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / kPrismSides;
        bottom.at(i) = base + (radius * ((std::cos(angle) * u) + (std::sin(angle) * v)));
        top.at(i) = bottom.at(i) + (length * axis);
    }
    // The corners go counter-clockwise around the axis, as seen from the second end.
    for (std::size_t i = 0; i < bottom.size(); ++i) {
        const std::size_t next = (i + 1) % bottom.size();
        add_quad(vertices, {bottom.at(i), bottom.at(next), top.at(next), top.at(i)}, material);
    }
    for (std::size_t i = 1; i + 1 < bottom.size(); ++i) {
        add_triangle(vertices, {top[0], top.at(i), top.at(i + 1)}, material);
        add_triangle(vertices, {bottom[0], bottom.at(i + 1), bottom.at(i)}, material);
    }
}

/// Adds the handle of a torch and its wider, burning end.
void add_torch(std::vector<Vertex>& vertices, const Torch& torch) {
    constexpr float kHeadLength = 0.1f;
    constexpr float kHeadRadius = 1.5f * kTorchRadius;
    constexpr Material kHandle{.color = kTorchHandleColor, .texture_weight = 0.0f};
    constexpr Material kHead{.color = kTorchHeadColor, .texture_weight = 0.0f};
    const glm::vec3 axis = torch.axis();
    add_prism(vertices, torch.mount, axis, kTorchRadius, kTorchLength - kHeadLength, kHandle);
    add_prism(vertices, torch.tip() - (kHeadLength * axis), axis, kHeadRadius, kHeadLength, kHead);
}

/// Adds the slope, the high end, the two sides and, if the ramp doesn't stand on the floor, the bottom.
void add_ramp(std::vector<Vertex>& vertices, const Ramp& ramp) {
    const float cos_yaw = std::cos(ramp.yaw);
    const float sin_yaw = std::sin(ramp.yaw);
    const auto corner = [&](float u, float v, float z) {
        return ramp.base_center + glm::vec3((cos_yaw * u) - (sin_yaw * v), (sin_yaw * u) + (cos_yaw * v), z);
    };
    // Low end (l), high end at the bottom (h) and at the top (t); 0 is on the right side, 1 on the left.
    const glm::vec3 l0 = corner(-ramp.half_length, -ramp.half_width, 0.0f);
    const glm::vec3 l1 = corner(-ramp.half_length, ramp.half_width, 0.0f);
    const glm::vec3 h0 = corner(ramp.half_length, -ramp.half_width, 0.0f);
    const glm::vec3 h1 = corner(ramp.half_length, ramp.half_width, 0.0f);
    const glm::vec3 t0 = corner(ramp.half_length, -ramp.half_width, ramp.rise);
    const glm::vec3 t1 = corner(ramp.half_length, ramp.half_width, ramp.rise);
    add_quad(vertices, {l0, t0, t1, l1});
    add_quad(vertices, {h0, h1, t1, t0});
    add_triangle(vertices, {l0, h0, t0});
    add_triangle(vertices, {l1, t1, h1});
    if (ramp.base_center.z > 0.0f) {
        add_quad(vertices, {l0, l1, h1, h0});
    }
}

/// Adds a vertical wall standing on the line from `from` to `to`.
///
/// The wall faces to the left of the direction from `from` to `to`, as seen from above.
void add_wall(std::vector<Vertex>& vertices, const glm::vec3& from, const glm::vec3& to, float height) {
    const glm::vec3 up{0.0f, 0.0f, height};
    add_quad(vertices, {to, from, from + up, to + up});
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

    add_quad(vertices, {a, b, c, d});
    add_quad(vertices, {d + up, c + up, b + up, a + up});
    // Going counter-clockwise around the level, the inside is on the left, so the walls face inwards.
    add_wall(vertices, a, b, h);
    add_wall(vertices, b, c, h);
    add_wall(vertices, c, d, h);
    add_wall(vertices, d, a, h);

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
            add_wall(vertices, corners.at((i + 1) % corners.size()), corners.at(i), obstacle.height);
        }
        // The top is hidden when the obstacle reaches the ceiling, and the bottom when it stands on the floor.
        if (obstacle.base_center.z + obstacle.height < h) {
            const glm::vec3 top{0.0f, 0.0f, obstacle.height};
            add_quad(vertices, {corners[0] + top, corners[1] + top, corners[2] + top, corners[3] + top});
        }
        if (obstacle.base_center.z > 0.0f) {
            add_quad(vertices, {corners[3], corners[2], corners[1], corners[0]});
        }
    }
    for (const Ramp& ramp : world.ramps()) {
        add_ramp(vertices, ramp);
    }
    for (const Torch& torch : world.torches()) {
        add_torch(vertices, torch);
    }
    return vertices;
}

}  // namespace manifold
