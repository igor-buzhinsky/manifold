#include "ui/mouse_look.hpp"

#include <numbers>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

namespace manifold {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

TEST(MouseLookRotationTest, TurnsByTheSensitivityInDegrees) {
    const glm::vec2 rotation = mouse_look_rotation({10.0f, 5.0f}, 0.5f);
    EXPECT_NEAR(rotation.x, -5.0f * kPi / 180.0f, 1e-6f);
    EXPECT_NEAR(rotation.y, -2.5f * kPi / 180.0f, 1e-6f);
}

TEST(MouseLookRotationTest, RightTurnsRightAndUpLooksUp) {
    // Positive yaw turns left and positive pitch looks up, see Player::rotate(); the mouse's y is down.
    const glm::vec2 right_and_up = mouse_look_rotation({1.0f, -1.0f}, 0.1f);
    EXPECT_LT(right_and_up.x, 0.0f);
    EXPECT_GT(right_and_up.y, 0.0f);
    EXPECT_EQ(mouse_look_rotation({0.0f, 0.0f}, 0.1f), glm::vec2(0.0f));
}

}  // namespace
}  // namespace manifold
