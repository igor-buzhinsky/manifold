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
/// The top of an obstacle is only added if it doesn't reach the ceiling, and the bottom only if it is above
/// the floor.
std::vector<Vertex> build_world_mesh(const World& world);

}  // namespace manifold
