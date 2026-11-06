#include "game.hpp"

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

namespace manifold {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

GameConfig make_config() {
    GameConfig config;
    config.turn_speed = kPi / 2.0f;
    config.look_speed = kPi / 3.0f;
    config.move_speed = 2.0f;
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
    EXPECT_EQ(game.player().position(), glm::vec3(0.0f));
}

void expect_near(const glm::vec3& actual, const glm::vec3& expected) {
    EXPECT_NEAR(glm::distance(actual, expected), 0.0f, 1e-5f) << actual.x << ", " << actual.y << ", " << actual.z;
}

TEST(GameTest, WalksInFacingDirection) {
    // Facing +x, forward is +x and right is -y.
    Game game(make_config(), 1);
    game.update({.forward = 1.0f}, 0.5f);
    expect_near(game.player().position(), {1.0f, 0.0f, 0.0f});
    game.update({.strafe = 1.0f}, 0.5f);
    expect_near(game.player().position(), {1.0f, -1.0f, 0.0f});
    game.update({.forward = -1.0f}, 0.5f);
    game.update({.strafe = -1.0f}, 0.5f);
    expect_near(game.player().position(), {0.0f, 0.0f, 0.0f});
}

TEST(GameTest, WalkingFollowsYaw) {
    Game game(make_config(), 1);
    game.update({.turn = 1.0f}, 1.0f);  // Now facing +y, so right is +x.
    game.update({.forward = 1.0f}, 1.0f);
    expect_near(game.player().position(), {0.0f, 2.0f, 0.0f});
    game.update({.strafe = 1.0f}, 1.0f);
    expect_near(game.player().position(), {2.0f, 2.0f, 0.0f});
}

TEST(GameTest, WalkingIgnoresPitch) {
    Game game(make_config(), 1);
    game.update({.look = 1.0f}, 10.0f);
    game.update({.forward = 1.0f}, 1.0f);
    expect_near(game.player().position(), {2.0f, 0.0f, 0.0f});
}

TEST(GameTest, DiagonalWalkIsNotFaster) {
    Game game(make_config(), 1);
    game.update({.forward = 1.0f, .strafe = 1.0f}, 1.0f);
    EXPECT_NEAR(glm::length(game.player().position()), 2.0f, 1e-5f);
}

TEST(GameTest, CollisionsStopPlayerAtLevelWall) {
    // No obstacle is ever placed over the player's start, so walking from the start of an empty level
    // along +x ends at the wall. Obstacles are removed by setting their count to zero.
    GameConfig config = make_config();
    config.level.obstacle_count = 0;
    Game game(config, 1);
    game.update({.forward = 1.0f}, 100.0f);
    EXPECT_NEAR(game.player().position().x, config.level.bounds.x_max - game.player().radius(), 1e-4f);
}

TEST(GameTest, NoclipWalksThroughWalls) {
    GameConfig config = make_config();
    config.level.obstacle_count = 0;
    Game game(config, 1);
    EXPECT_FALSE(game.noclip());
    game.toggle_noclip();
    EXPECT_TRUE(game.noclip());
    game.update({.forward = 1.0f}, 100.0f);
    EXPECT_NEAR(game.player().position().x, 200.0f, 1e-3f);
    game.toggle_noclip();
    EXPECT_FALSE(game.noclip());
}

}  // namespace
}  // namespace manifold
