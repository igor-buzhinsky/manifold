#include "world/torch.hpp"

#include <cmath>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

namespace manifold {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

TEST(TorchTest, HandleLeansAwayFromWall) {
    const Torch torch{.mount = {1.0f, 2.0f, 1.5f}, .yaw = kPi / 2.0f};
    const glm::vec3 outward = torch.outward();
    EXPECT_NEAR(outward.x, 0.0f, 1e-6f);
    EXPECT_NEAR(outward.y, 1.0f, 1e-6f);
    EXPECT_FLOAT_EQ(outward.z, 0.0f);

    const glm::vec3 axis = torch.axis();
    EXPECT_NEAR(glm::length(axis), 1.0f, 1e-6f);
    EXPECT_NEAR(std::acos(axis.z), kTorchTilt, 1e-5f);
    EXPECT_GT(glm::dot(axis, outward), 0.0f);
    EXPECT_NEAR(glm::distance(torch.mount, torch.tip()), kTorchLength, 1e-5f);
}

TEST(TorchTest, FlameCoversTipAndLightIsInsideFlame) {
    const Torch torch{.mount = {0.0f, 0.0f, 1.6f}, .yaw = 0.3f};
    const glm::vec3 tip = torch.tip();
    const glm::vec3 bottom = torch.flame_bottom();
    EXPECT_FLOAT_EQ(bottom.x, tip.x);
    EXPECT_FLOAT_EQ(bottom.y, tip.y);
    EXPECT_LT(bottom.z, tip.z);

    const glm::vec3 light = torch.light_position();
    EXPECT_FLOAT_EQ(light.x, tip.x);
    EXPECT_FLOAT_EQ(light.y, tip.y);
    EXPECT_GT(light.z, tip.z);
    EXPECT_LT(light.z, bottom.z + kFlameHeight);
}

}  // namespace
}  // namespace manifold
