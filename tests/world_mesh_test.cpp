#include "render/world_mesh.hpp"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

constexpr WorldBounds kBounds{.x_min = -5.0f, .x_max = 5.0f, .y_min = -4.0f, .y_max = 4.0f, .height = 3.0f};
constexpr std::size_t kVerticesPerQuad = 6;

World make_world() {
    World world(kBounds);
    world.add_obstacle({.base_center = {2.0f, 1.0f, 0.0f}, .half_size = 0.5f, .height = 3.0f, .yaw = 0.4f});
    world.add_obstacle({.base_center = {-2.0f, -1.0f, 0.0f}, .half_size = 1.0f, .height = 3.0f, .yaw = 0.0f});
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

TEST(WorldMeshTest, LowObstacleGetsTopFace) {
    World world(kBounds);
    world.add_obstacle({.base_center = {0.0f, 0.0f, 0.0f}, .half_size = 1.0f, .height = 1.0f, .yaw = 0.3f});
    const std::vector<Vertex> mesh = build_world_mesh(world);
    ASSERT_EQ(mesh.size(), (6 + 4 + 1) * kVerticesPerQuad);
    for (std::size_t i = mesh.size() - kVerticesPerQuad; i < mesh.size(); ++i) {
        EXPECT_FLOAT_EQ(mesh[i].position.z, 1.0f);
        EXPECT_GT(mesh[i].normal.z, 0.99f);
    }
}

TEST(WorldMeshTest, RaisedObstacleGetsBottomFace) {
    World world(kBounds);
    world.add_obstacle({.base_center = {0.0f, 0.0f, 1.0f}, .half_size = 1.0f, .height = 2.0f, .yaw = 0.3f});
    const std::vector<Vertex> mesh = build_world_mesh(world);
    ASSERT_EQ(mesh.size(), (6 + 4 + 1) * kVerticesPerQuad);
    for (std::size_t i = mesh.size() - kVerticesPerQuad; i < mesh.size(); ++i) {
        EXPECT_FLOAT_EQ(mesh[i].position.z, 1.0f);
        EXPECT_LT(mesh[i].normal.z, -0.99f);
    }
}

TEST(WorldMeshTest, RampHasSlopeHighEndAndSidesFacingOutwards) {
    World world(kBounds);
    const Ramp ramp{
        .base_center = {1.0f, 0.0f, 0.0f}, .half_length = 2.0f, .half_width = 1.0f, .rise = 2.0f, .yaw = 0.5f};
    world.add_ramp(ramp);
    const std::vector<Vertex> mesh = build_world_mesh(world);
    // After the level boundaries: 2 quads (slope and high end) and 2 triangles (sides).
    const std::size_t first = 6 * kVerticesPerQuad;
    ASSERT_EQ(mesh.size(), first + (2 * kVerticesPerQuad) + 6);
    // Every face points away from the middle of the ramp, and the slope points up.
    const glm::vec3 middle = ramp.base_center + glm::vec3(0.0f, 0.0f, ramp.rise / 4.0f);
    for (std::size_t i = first; i < mesh.size(); ++i) {
        EXPECT_GT(glm::dot(mesh[i].position - middle, mesh[i].normal), 0.0f) << "vertex " << i;
        EXPECT_TRUE(kBounds.contains(mesh[i].position));
    }
    EXPECT_GT(mesh[first].normal.z, 0.5f);
}

TEST(WorldMeshTest, RaisedRampGetsBottomFace) {
    World world(kBounds);
    world.add_ramp({.base_center = {0.0f, 0.0f, 0.5f}, .half_length = 2.0f, .half_width = 1.0f, .rise = 2.0f});
    const std::vector<Vertex> mesh = build_world_mesh(world);
    ASSERT_EQ(mesh.size(), ((6 + 3) * kVerticesPerQuad) + 6);
    EXPECT_LT(mesh.back().normal.z, -0.99f);
}

TEST(WorldMeshTest, TextureIsNotStretched) {
    // Along every edge, the distance in texture coordinates times the tile size equals the distance in the
    // world, also on rotated obstacles and ramp slopes.
    World world = make_world();
    world.add_ramp(
        {.base_center = {0.0f, 2.0f, 0.0f}, .half_length = 2.0f, .half_width = 1.0f, .rise = 2.0f, .yaw = 0.7f});
    const std::vector<Vertex> mesh = build_world_mesh(world);
    for (std::size_t i = 0; i < mesh.size(); i += 3) {
        for (std::size_t j = 0; j < 3; ++j) {
            const Vertex& from = mesh[i + j];
            const Vertex& to = mesh[i + ((j + 1) % 3)];
            EXPECT_NEAR(glm::length(to.uv - from.uv) * kTextureTileSize, glm::length(to.position - from.position),
                        1e-4f)
                << "triangle " << i / 3;
        }
    }
}

TEST(WorldMeshTest, TextureIsUprightOnWalls) {
    // Image rows go from the top down, so v decreases going up a wall.
    for (const Vertex& vertex : build_world_mesh(make_world())) {
        if (std::abs(vertex.normal.z) < 0.01f) {
            const float v_at_floor = vertex.uv.y + (vertex.position.z / kTextureTileSize);
            EXPECT_NEAR(v_at_floor, 0.0f, 1e-4f);
        }
    }
}

TEST(WorldMeshTest, LevelSurfacesUseTextureAndTorchesTheirColors) {
    World world = make_world();
    const std::size_t level_vertices = build_world_mesh(world).size();
    world.add_torch({.mount = {0.0f, 4.0f, 1.6f}, .yaw = -std::numbers::pi_v<float> / 2.0f});
    const std::vector<Vertex> mesh = build_world_mesh(world);
    // Two closed six-sided prisms: 6 side quads and 2 caps of 4 triangles each.
    constexpr std::size_t kVerticesPerTriangle = 3;
    constexpr std::size_t kPrismVertices = (6 * kVerticesPerQuad) + (8 * kVerticesPerTriangle);
    ASSERT_EQ(mesh.size(), level_vertices + (2 * kPrismVertices));
    for (std::size_t i = 0; i < mesh.size(); ++i) {
        if (i < level_vertices) {
            EXPECT_EQ(mesh[i].color, glm::vec3(1.0f));
            EXPECT_FLOAT_EQ(mesh[i].texture_weight, 1.0f);
        } else {
            EXPECT_EQ(mesh[i].color, i < level_vertices + kPrismVertices ? kTorchHandleColor : kTorchHeadColor);
            EXPECT_FLOAT_EQ(mesh[i].texture_weight, 0.0f);
        }
    }
}

TEST(WorldMeshTest, TorchIsAClosedPrismAlongItsHandle) {
    World world(kBounds);
    const Torch torch{.mount = {kBounds.x_min, 0.0f, 1.6f}, .yaw = 0.0f};
    world.add_torch(torch);
    const std::vector<Vertex> all = build_world_mesh(world);
    const std::vector<Vertex> mesh(all.begin() + static_cast<std::ptrdiff_t>(6 * kVerticesPerQuad), all.end());
    ASSERT_FALSE(mesh.empty());
    const glm::vec3 axis = torch.axis();
    for (std::size_t i = 0; i < mesh.size(); i += 3) {
        // Normals point away from the handle's axis or along it, at the ends.
        const glm::vec3 center = (mesh[i].position + mesh[i + 1].position + mesh[i + 2].position) / 3.0f;
        const float along = glm::dot(center - torch.mount, axis);
        const glm::vec3 from_axis = center - torch.mount - (along * axis);
        const float outward = glm::dot(from_axis, mesh[i].normal);
        const float end = std::abs(glm::dot(axis, mesh[i].normal));
        EXPECT_TRUE(outward > 0.0f || end > 0.99f) << "triangle " << i / 3;
        EXPECT_GE(along, -1e-5f);
        EXPECT_LE(along, kTorchLength + 1e-5f);
    }
    // The triangles enclose a volume: the sum of their area vectors is zero.
    glm::vec3 total{0.0f};
    for (std::size_t i = 0; i < mesh.size(); i += 3) {
        total += glm::cross(mesh[i + 1].position - mesh[i].position, mesh[i + 2].position - mesh[i].position);
    }
    EXPECT_NEAR(glm::length(total), 0.0f, 1e-5f);
}

TEST(WorldMeshTest, FloorTextureFollowsXAndY) {
    const std::vector<Vertex> mesh = build_world_mesh(make_world());
    for (std::size_t i = 0; i < kVerticesPerQuad; ++i) {
        EXPECT_FLOAT_EQ(mesh[i].uv.x, mesh[i].position.x / kTextureTileSize);
        EXPECT_FLOAT_EQ(mesh[i].uv.y, -mesh[i].position.y / kTextureTileSize);
    }
}

}  // namespace
}  // namespace manifold
