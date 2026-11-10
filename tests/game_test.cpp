#include "game.hpp"

#include <numbers>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "level/level.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// Most tests use an empty debug level, where the player starts at the origin.
GameConfig make_config() {
    GameConfig config;
    config.level.debug.obstacle_count = 0;
    config.level.debug.ramp_count = 0;
    config.turn_speed = kPi / 2.0f;
    config.look_speed = kPi / 3.0f;
    config.move_speed = 2.0f;
    return config;
}

TEST(GameTest, TurningRotatesByTurnSpeedTimesTime) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.turn = 1.0f}, 0.5f);
    EXPECT_NEAR(game.player().yaw(), kPi / 4.0f, 1e-6f);
    game.update({.turn = -0.5f}, 0.5f);
    EXPECT_NEAR(game.player().yaw(), kPi / 8.0f, 1e-6f);
}

TEST(GameTest, LookingChangesPitchUpToLimit) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.look = 1.0f}, 0.25f);
    EXPECT_NEAR(game.player().pitch(), kPi / 12.0f, 1e-6f);
    game.update({.look = 1.0f}, 10.0f);
    EXPECT_FLOAT_EQ(game.player().pitch(), Player::kMaxPitch);
    game.update({.look = -1.0f}, 10.0f);
    EXPECT_FLOAT_EQ(game.player().pitch(), -Player::kMaxPitch);
}

TEST(GameTest, NoInputChangesNothing) {
    Game game(make_config(), 1, LayoutKind::kDebug);
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
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.forward = 1.0f}, 0.5f);
    expect_near(game.player().position(), {1.0f, 0.0f, 0.0f});
    game.update({.strafe = 1.0f}, 0.5f);
    expect_near(game.player().position(), {1.0f, -1.0f, 0.0f});
    game.update({.forward = -1.0f}, 0.5f);
    game.update({.strafe = -1.0f}, 0.5f);
    expect_near(game.player().position(), {0.0f, 0.0f, 0.0f});
}

TEST(GameTest, WalkingFollowsYaw) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.turn = 1.0f}, 1.0f);  // Now facing +y, so right is +x.
    game.update({.forward = 1.0f}, 1.0f);
    expect_near(game.player().position(), {0.0f, 2.0f, 0.0f});
    game.update({.strafe = 1.0f}, 1.0f);
    expect_near(game.player().position(), {2.0f, 2.0f, 0.0f});
}

TEST(GameTest, WalkingIgnoresPitch) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.look = 1.0f}, 10.0f);
    game.update({.forward = 1.0f}, 1.0f);
    expect_near(game.player().position(), {2.0f, 0.0f, 0.0f});
}

TEST(GameTest, DiagonalWalkIsNotFaster) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.forward = 1.0f, .strafe = 1.0f}, 1.0f);
    EXPECT_NEAR(glm::length(game.player().position()), 2.0f, 1e-5f);
}

TEST(GameTest, CollisionsStopPlayerAtLevelWall) {
    const GameConfig config = make_config();
    Game game(config, 1, LayoutKind::kDebug);
    game.update({.forward = 1.0f}, 100.0f);
    EXPECT_NEAR(game.player().position().x, config.level.debug.x_max - game.player().radius(), 1e-4f);
}

TEST(GameTest, NoclipWalksThroughWalls) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_FALSE(game.noclip());
    game.toggle_noclip();
    EXPECT_TRUE(game.noclip());
    game.update({.forward = 1.0f}, 100.0f);
    EXPECT_NEAR(game.player().position().x, 200.0f, 1e-3f);
    game.toggle_noclip();
    EXPECT_FALSE(game.noclip());
}

TEST(GameTest, PlayerStartsAtLevelStart) {
    GameConfig config = make_config();
    config.player.position = {100.0f, 100.0f, 0.0f};  // Replaced by the level's start.
    const Game game(config, 3, LayoutKind::kRooms);
    EXPECT_EQ(game.level().layout, LayoutKind::kRooms);
    EXPECT_EQ(game.player().position(), game.level().player_start);
}

TEST(GameTest, LevelHasEarthGravityByDefault) {
    const Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_FLOAT_EQ(game.level().gravity, kEarthGravity);
}

TEST(GameTest, RisingWorksOnlyWithNoclip) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.rise = 1.0f}, 0.5f);
    EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);
    game.toggle_noclip();
    game.update({.rise = 1.0f}, 0.5f);
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-6f);
    game.update({.rise = -1.0f}, 0.25f);
    EXPECT_NEAR(game.player().position().z, 0.5f, 1e-6f);
}

TEST(GameTest, NoclipDoesNotFall) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.toggle_noclip();
    game.update({.rise = 1.0f}, 0.5f);
    game.update({}, 1.0f);
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-6f);
    EXPECT_FLOAT_EQ(game.player().vertical_speed(), 0.0f);
}

TEST(GameTest, FallsByGravityAndLands) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.toggle_noclip();
    game.update({.rise = 1.0f}, 1.0f);
    game.toggle_noclip();

    // Each frame, the speed changes first and then the position.
    const float dt = 0.1f;
    const float g = game.level().gravity;
    game.update({}, dt);
    EXPECT_NEAR(game.player().vertical_speed(), -g * dt, 1e-5f);
    EXPECT_NEAR(game.player().position().z, 2.0f - (g * dt * dt), 1e-5f);
    game.update({}, dt);
    EXPECT_NEAR(game.player().vertical_speed(), -2.0f * g * dt, 1e-5f);
    EXPECT_NEAR(game.player().position().z, 2.0f - (3.0f * g * dt * dt), 1e-5f);

    for (int i = 0; i < 20; ++i) {
        game.update({}, dt);
    }
    EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);
    EXPECT_FLOAT_EQ(game.player().vertical_speed(), 0.0f);
}

TEST(GameTest, StandingOnFloorKeepsVerticalSpeedZero) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    for (int i = 0; i < 10; ++i) {
        game.update({.forward = 1.0f}, 0.1f);
        EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);
        EXPECT_FLOAT_EQ(game.player().vertical_speed(), 0.0f);
    }
}

// A ramp from x = -2 to x = 2, rising by 2.5 m towards +x, onto a platform from x = 2 to x = 6.
// The player starts at x = -4, facing the ramp.
Level make_ramp_level() {
    World world({.x_min = -10.0f, .x_max = 10.0f, .y_min = -10.0f, .y_max = 10.0f, .height = 6.0f});
    world.add_ramp({.base_center = {0.0f, 0.0f, 0.0f}, .half_length = 2.0f, .half_width = 1.0f, .rise = 2.5f});
    world.add_obstacle({.base_center = {4.0f, 0.0f, 0.0f}, .half_size = 2.0f, .height = 2.5f, .yaw = 0.0f});
    return {.layout = LayoutKind::kDebug, .world = std::move(world), .player_start = {-4.0f, 0.0f, 0.0f}, .plan = ""};
}

TEST(GameTest, WalksUpRampOntoPlatform) {
    Game game(make_config(), make_ramp_level());
    for (int frame = 0; frame < 240; ++frame) {
        game.update({.forward = 1.0f}, 1.0f / 60.0f);
    }
    expect_near(game.player().position(), {4.0f, 0.0f, 2.5f});
    EXPECT_TRUE(game.player().on_ground());
}

TEST(GameTest, WalksDownRampWithoutLeavingGround) {
    GameConfig config = make_config();
    Level level = make_ramp_level();
    level.player_start = {4.0f, 0.0f, 2.5f};
    config.player.yaw = kPi;
    Game game(config, std::move(level));
    game.update({}, 1.0f / 60.0f);  // Find the ground.
    for (int frame = 0; frame < 240; ++frame) {
        game.update({.forward = 1.0f}, 1.0f / 60.0f);
        EXPECT_TRUE(game.player().on_ground()) << "frame " << frame;
    }
    expect_near(game.player().position(), {-4.0f, 0.0f, 0.0f});
}

TEST(GameTest, WalkingOffLedgeFalls) {
    // Snapping to the ground only works for small steps down, not for a 2.5 m drop.
    GameConfig config = make_config();
    Level level = make_ramp_level();
    level.player_start = {4.0f, 0.0f, 2.5f};
    config.player.yaw = kPi / 2.0f;  // Facing +y, off the side of the platform.
    Game game(config, std::move(level));
    game.update({}, 1.0f / 60.0f);
    for (int frame = 0; frame < 120 && game.player().position().z > 2.0f; ++frame) {
        game.update({.forward = 1.0f}, 1.0f / 60.0f);
    }
    EXPECT_LT(game.player().vertical_speed(), 0.0f);
    EXPECT_FALSE(game.player().on_ground());
}

}  // namespace
}  // namespace manifold
