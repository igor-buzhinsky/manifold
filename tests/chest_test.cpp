#include "world/chest.hpp"

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "world/geometry.hpp"
#include "world/obstacle.hpp"

namespace manifold {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

TEST(ChestTest, StandsAtTheWallWithAGap) {
    // A wall at y = 3, facing -y.
    const Chest chest = Chest::at_wall({1.0f, 3.0f, 0.5f}, -kPi / 2.0f, ChestLook::kWood);
    EXPECT_FLOAT_EQ(chest.base_center.x, 1.0f);
    EXPECT_FLOAT_EQ(chest.base_center.z, 0.5f);
    EXPECT_FLOAT_EQ(chest.yaw, -kPi / 2.0f);
    EXPECT_EQ(chest.look, ChestLook::kWood);
    // The back of the lid, the part nearest to the wall, is kWallGap away from it.
    const RotatedRectangle footprint = chest.footprint();
    const glm::vec2 extent = bounding_half_size(footprint);
    EXPECT_NEAR(3.0f - (footprint.center.y + extent.y), Chest::kWallGap, 1e-5f);
    EXPECT_NEAR(2.0f * extent.y, Chest::kDepth + (2.0f * Chest::kLidOverhang), 1e-5f);
    EXPECT_NEAR(2.0f * extent.x, Chest::kWidth + (2.0f * Chest::kLidOverhang), 1e-5f);
}

TEST(ChestTest, IsWideAlongTheWall) {
    // A wall at x = -2, facing +x: the chest is wide along y.
    const Chest chest = Chest::at_wall({-2.0f, 0.0f, 0.0f}, 0.0f, ChestLook::kGold);
    const glm::vec2 extent = bounding_half_size(chest.body().footprint());
    EXPECT_NEAR(2.0f * extent.y, Chest::kWidth, 1e-5f);
    EXPECT_NEAR(2.0f * extent.x, Chest::kDepth, 1e-5f);
    EXPECT_GT(chest.base_center.x, -2.0f + Chest::kWallGap);
}

TEST(ChestTest, LidIsOnTopOfTheBodyAndALittleLarger) {
    const Chest chest = Chest::at_wall({0.0f, 0.0f, 1.0f}, 0.7f, ChestLook::kWood);
    const Obstacle body = chest.body();
    const Obstacle lid = chest.lid();
    EXPECT_FLOAT_EQ(body.base_center.z, 1.0f);
    EXPECT_NEAR(body.height, Chest::kHeight - Chest::kLidHeight, 1e-6f);
    EXPECT_NEAR(lid.base_center.z, body.base_center.z + body.height, 1e-6f);
    EXPECT_NEAR(lid.base_center.z + lid.height, 1.0f + Chest::kHeight, 1e-6f);
    EXPECT_FLOAT_EQ(lid.yaw, body.yaw);
    EXPECT_NEAR(glm::distance(glm::vec2(lid.base_center), glm::vec2(body.base_center)), 0.0f, 1e-6f);
    EXPECT_NEAR(lid.half_size.x - body.half_size.x, Chest::kLidOverhang, 1e-6f);
    EXPECT_NEAR(lid.half_size.y - body.half_size.y, Chest::kLidOverhang, 1e-6f);
}

TEST(ChestTest, LooksHaveNames) {
    EXPECT_EQ(chest_look_name(ChestLook::kWood), "wooden");
    EXPECT_EQ(chest_look_name(ChestLook::kGold), "golden");
}

}  // namespace
}  // namespace manifold
