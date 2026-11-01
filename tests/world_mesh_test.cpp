#include "render/world_mesh.hpp"

#include <cstddef>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "world/world.hpp"

namespace manifold {
namespace {

constexpr WorldBounds kBounds{.x_min = -5.0f, .x_max = 5.0f, .y_min = -4.0f, .y_max = 4.0f, .height = 3.0f};
constexpr std::size_t kVerticesPerQuad = 6;

World make_world() {
    World world(kBounds);
    world.add_obstacle({.base_center = {2.0f, 1.0f, 0.0f}, .half_size = 0.5f, .yaw = 0.4f});
    world.add_obstacle({.base_center = {-2.0f, -1.0f, 0.0f}, .half_size = 1.0f, .yaw = 0.0f});
    return world;
}

TEST(WorldMeshTest, HasFloorCeilingWallsAndObstacleSides) {
    // Floor, ceiling, 4 walls, and 4 sides per obstacle.
    EXPECT_EQ(build_world_mesh(make_world()).size(), (6 + (2 * 4)) * kVerticesPerQuad);
}

TEST(WorldMeshTest, VerticesStayInsideBounds) {
    for (const Vertex& vertex : build_world_mesh(make_world())) {
        EXPECT_TRUE(kBounds.contains(vertex.position));
    }
}

TEST(WorldMeshTest, TrianglesAreCounterClockwiseAroundNormal) {
    const std::vector<Vertex> mesh = build_world_mesh(make_world());
    ASSERT_EQ(mesh.size() % 3, 0U);
    for (std::size_t i = 0; i < mesh.size(); i += 3) {
        const glm::vec3 winding_normal =
            glm::cross(mesh[i + 1].position - mesh[i].position, mesh[i + 2].position - mesh[i].position);
        EXPECT_GT(glm::dot(winding_normal, mesh[i].normal), 0.0f) << "triangle " << i / 3;
    }
}

TEST(WorldMeshTest, LevelSurfacesFaceInwardsAndObstaclesOutwards) {
    // Each triangle's normal points towards the level center for the boundary surfaces, and away from
    // its obstacle's center for obstacle sides.
    const World world = make_world();
    const std::vector<Vertex> mesh = build_world_mesh(world);
    const glm::vec3 level_center{0.0f, 0.0f, kBounds.height / 2.0f};
    const std::size_t boundary_vertices = 6 * kVerticesPerQuad;
    for (std::size_t i = 0; i < mesh.size(); ++i) {
        if (i < boundary_vertices) {
            EXPECT_GT(glm::dot(level_center - mesh[i].position, mesh[i].normal), 0.0f) << "vertex " << i;
        } else {
            const Obstacle& obstacle = world.obstacles()[(i - boundary_vertices) / (4 * kVerticesPerQuad)];
            glm::vec3 outward = mesh[i].position - obstacle.base_center;
            outward.z = 0.0f;
            EXPECT_GT(glm::dot(outward, mesh[i].normal), 0.0f) << "vertex " << i;
        }
    }
}

}  // namespace
}  // namespace manifold
