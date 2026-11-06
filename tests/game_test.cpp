#include "game.hpp"

#include <numbers>

#include <gtest/gtest.h>

namespace manifold {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

GameConfig make_config() {
    GameConfig config;
    config.turn_speed = kPi / 2.0f;
    config.look_speed = kPi / 3.0f;
    return config;
}

TEST(GameTest, TurningRotatesByTurnSpeedTimesTime) {
    Game game(make_config(), 1);
    game.update({.turn = 1.0f}, 0.5f);
    EXPECT_NEAR(game.player().yaw(), kPi / 4.0f, 1e-6f);
    game.update({.turn = -0.5f}, 0.5f);
    EXPECT_NEAR(game.player().yaw(), kPi / 8.0f, 1e-6f);
}

TEST(GameTest, LookingChangesPitchUpToLimit) {
    Game game(make_config(), 1);
    game.update({.look = 1.0f}, 0.25f);
    EXPECT_NEAR(game.player().pitch(), kPi / 12.0f, 1e-6f);
    game.update({.look = 1.0f}, 10.0f);
    EXPECT_FLOAT_EQ(game.player().pitch(), Player::kMaxPitch);
    game.update({.look = -1.0f}, 10.0f);
    EXPECT_FLOAT_EQ(game.player().pitch(), -Player::kMaxPitch);
}

TEST(GameTest, NoInputChangesNothing) {
    Game game(make_config(), 1);
    game.update({}, 1.0f);
    EXPECT_FLOAT_EQ(game.player().yaw(), 0.0f);
    EXPECT_FLOAT_EQ(game.player().pitch(), 0.0f);
}

}  // namespace
}  // namespace manifold
