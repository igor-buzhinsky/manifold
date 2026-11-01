#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "world/world.hpp"

namespace manifold {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;  // Surface albedo in linear RGB.
};

// Triangles for the floor, the ceiling, the boundary walls and the obstacles of a world.
// Every three vertices form a triangle, counter-clockwise when seen from the side the normal points to.
// Normals of the boundary walls point into the level, normals of obstacles point out of them.
std::vector<Vertex> build_world_mesh(const World& world);

}  // namespace manifold
