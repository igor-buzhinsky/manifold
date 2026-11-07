#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "world/world.hpp"

namespace manifold {

/// A vertex of the world mesh, as uploaded to the GPU.
struct Vertex {
    glm::vec3 position;  ///< In world coordinates.
    glm::vec3 normal;    ///< Unit vector pointing out of the visible side of the surface.
    glm::vec3 color;     ///< Surface albedo in linear RGB.
};

/// Creates triangles for the floor, the ceiling, the boundary walls and the obstacles of a world.
///
/// Every three vertices form a triangle, counter-clockwise when seen from the side the normal points to.
/// Normals of the boundary walls point into the level, normals of obstacles point out of them.
/// Obstacles are assumed to stand on the floor. Their tops are only added if they don't reach the ceiling.
std::vector<Vertex> build_world_mesh(const World& world);

}  // namespace manifold
