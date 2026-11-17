#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "door_level.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "level/texture_kind.hpp"
#include "world/door.hpp"
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

TEST(GameTest, TimeIsSumOfFrameDurations) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_DOUBLE_EQ(game.time(), 0.0);
    game.update({}, 0.25f);
    game.update({}, 0.5f);
    EXPECT_DOUBLE_EQ(game.time(), 0.75);
    EXPECT_FLOAT_EQ(game.animation_time(), 0.75f);
}

TEST(GameTest, AnimationTimeWrapsAround) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    // With noclip and no input, a long frame changes nothing but the time.
    game.set_noclip(true);
    game.update({}, static_cast<float>(Game::kAnimationPeriod) + 0.5f);
    EXPECT_NEAR(game.animation_time(), 0.5f, 1e-3f);
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
    game.set_noclip(true);
    EXPECT_TRUE(game.noclip());
    game.update({.forward = 1.0f}, 100.0f);
    EXPECT_NEAR(game.player().position().x, 200.0f, 1e-3f);
    game.set_noclip(false);
    EXPECT_FALSE(game.noclip());
}

TEST(GameTest, PlayerStartsAtLevelStart) {
    GameConfig config = make_config();
    config.player.position = {100.0f, 100.0f, 0.0f};  // Replaced by the level's start.
    config.player.yaw = 0.1f;                         // Replaced by the level's start too.
    const Game game(config, 3, LayoutKind::kRooms);
    EXPECT_EQ(game.level().layout, LayoutKind::kRooms);
    EXPECT_EQ(game.player().position(), game.level().player_start);
    EXPECT_FLOAT_EQ(game.player().yaw(), game.level().player_yaw);
}

TEST(GameTest, TeleportMovesThePlayerWhoThenFallsFromRest) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({}, 0.1f);
    game.teleport({3.0f, 4.0f, 2.0f}, kPi / 2.0f);
    EXPECT_EQ(game.player().position(), glm::vec3(3.0f, 4.0f, 2.0f));
    EXPECT_FLOAT_EQ(game.player().yaw(), kPi / 2.0f);
    EXPECT_FLOAT_EQ(game.player().vertical_speed(), 0.0f);
    EXPECT_FALSE(game.player().on_ground());
    game.teleport({1.0f, 1.0f, 0.0f});
    EXPECT_FLOAT_EQ(game.player().yaw(), kPi / 2.0f);  // Kept.
    game.update({}, 0.01f);
    EXPECT_TRUE(game.player().on_ground());
}

TEST(GameTest, LevelHasEarthGravityByDefault) {
    const Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_FLOAT_EQ(game.level().gravity, kEarthGravity);
}

TEST(GameTest, FlyingUpAndDownNeedsNoclip) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.rise = -1.0f}, 0.5f);
    EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);
    game.set_noclip(true);
    game.update({.rise = 1.0f}, 0.5f);
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-6f);
    game.update({.rise = -1.0f}, 0.25f);
    EXPECT_NEAR(game.player().position().z, 0.5f, 1e-6f);
}

TEST(GameTest, NoclipDoesNotFall) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.set_noclip(true);
    game.update({.rise = 1.0f}, 0.5f);
    game.update({}, 1.0f);
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-6f);
    EXPECT_FLOAT_EQ(game.player().vertical_speed(), 0.0f);
}

TEST(GameTest, FallsByGravityAndLands) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.set_noclip(true);
    game.update({.rise = 1.0f}, 1.0f);
    game.set_noclip(false);

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

/// Advances `game` by `seconds` in frames of 10 ms with the same input.
void run(Game& game, const PlayerInput& input, float seconds) {
    for (int frame = 0; frame < static_cast<int>(std::lround(seconds / 0.01f)); ++frame) {
        game.update(input, 0.01f);
    }
}

TEST(GameTest, WalkingMakesAFootstepPerStepLengthTheFirstSoon) {
    Game game(make_config(), 1, LayoutKind::kDebug);  // Walks at 2 m/s.
    run(game, {}, 0.1f);                              // Find the ground: no footstep.
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {.forward = 1.0f}, 0.1f);  // 0.2 m.
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {.forward = 1.0f}, 0.05f);  // 0.3 m: the first step at Game::kFirstStepLength.
    EXPECT_EQ(game.take_footsteps(), 1);
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {.forward = 1.0f}, 1.0f);  // 2.3 m: steps at 1.25 and 2.25 m.
    EXPECT_EQ(game.take_footsteps(), 2);
    run(game, {}, 1.0f);  // Standing makes no steps.
    EXPECT_EQ(game.take_footsteps(), 0);
}

TEST(GameTest, NoclipAndFallingMakeNoFootstepsButLandingFromAJumpDoes) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    run(game, {}, 0.1f);
    game.set_noclip(true);
    run(game, {.forward = 1.0f}, 2.0f);
    EXPECT_EQ(game.take_footsteps(), 0);
    game.set_noclip(false);
    run(game, {}, 0.5f);  // Fell from where noclip left the player: lands on the floor.
    game.take_footsteps();
    game.update({.rise = 1.0f}, 0.01f);  // A jump of 0.5 m, in the air for about 0.64 s.
    run(game, {}, 0.3f);
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {}, 0.7f);
    EXPECT_EQ(game.take_footsteps(), 1);
}

TEST(GameTest, StoppingMakesTheNextStepComeSoonAndTeleportStartsAgain) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    run(game, {}, 0.1f);
    run(game, {.forward = 1.0f}, 0.25f);  // 0.5 m: the first step, then 0.25 m towards the next.
    EXPECT_EQ(game.take_footsteps(), 1);
    run(game, {}, 0.1f);                  // Stop.
    run(game, {.forward = 1.0f}, 0.15f);  // 0.3 m: a step at 0.25 m again, not at 0.75 m.
    EXPECT_EQ(game.take_footsteps(), 1);

    run(game, {.forward = 1.0f}, 0.4f);  // 0.8 m, with a step due at 1.05 m.
    game.teleport({0.0f, 0.0f, 0.0f});
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {}, 0.1f);
    run(game, {.forward = 1.0f}, 0.1f);  // 0.2 m: not yet a step after teleport().
    EXPECT_EQ(game.take_footsteps(), 0);
}

TEST(GameTest, FootstepsAndTextureKindStartAgainInTheNextLevel) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_EQ(game.level().texture_kind, TextureKind::kStone);
    game.set_texture_kind(TextureKind::kGrass);
    EXPECT_EQ(game.level().texture_kind, TextureKind::kGrass);
    run(game, {.forward = 1.0f}, 0.5f);
    game.next_level();
    EXPECT_EQ(game.take_footsteps(), 0);
    EXPECT_EQ(game.level().texture_kind, TextureKind::kStone);
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
    world.add_obstacle({.base_center = {4.0f, 0.0f, 0.0f}, .half_size = glm::vec2(2.0f), .height = 2.5f, .yaw = 0.0f});
    return {.layout = LayoutKind::kDebug, .world = std::move(world), .player_start = {-4.0f, 0.0f, 0.0f}, .info = ""};
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
    const GameConfig config = make_config();
    Level level = make_ramp_level();
    level.player_start = {4.0f, 0.0f, 2.5f};
    level.player_yaw = kPi;
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
    const GameConfig config = make_config();
    Level level = make_ramp_level();
    level.player_start = {4.0f, 0.0f, 2.5f};
    level.player_yaw = kPi / 2.0f;  // Facing +y, off the side of the platform.
    Game game(config, std::move(level));
    game.update({}, 1.0f / 60.0f);
    for (int frame = 0; frame < 120 && game.player().position().z > 2.0f; ++frame) {
        game.update({.forward = 1.0f}, 1.0f / 60.0f);
    }
    EXPECT_LT(game.player().vertical_speed(), 0.0f);
    EXPECT_FALSE(game.player().on_ground());
}

/// Jumps from the floor of an empty level and returns the highest position of the feet.
float jump_peak(float dt, float gravity = kEarthGravity) {
    const GameConfig config = make_config();
    Level level = generate_level(config.level, LayoutKind::kDebug, config.player.radius, 1);
    level.gravity = gravity;
    Game game(config, std::move(level));
    game.update({}, dt);  // Find the ground.
    EXPECT_TRUE(game.player().on_ground());
    game.update({.rise = 1.0f}, dt);
    float peak = game.player().position().z;
    for (int frame = 0; frame < static_cast<int>(5.0f / dt); ++frame) {
        game.update({}, dt);
        peak = std::max(peak, game.player().position().z);
    }
    EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);  // Landed again.
    EXPECT_TRUE(game.player().on_ground());
    return peak;
}

TEST(GameTest, JumpReachesJumpHeightAtAnyFrameRate) {
    EXPECT_NEAR(jump_peak(1.0f / 60.0f), 0.5f, 0.01f);
    EXPECT_NEAR(jump_peak(1.0f / 144.0f), 0.5f, 0.01f);
    EXPECT_NEAR(jump_peak(1.0f / 20.0f), 0.5f, 0.02f);
}

TEST(GameTest, JumpIsHigherUnderWeakerGravity) {
    EXPECT_NEAR(jump_peak(1.0f / 60.0f, kEarthGravity / 2.0f), 1.0f, 0.01f);
    EXPECT_NEAR(jump_peak(1.0f / 60.0f, kEarthGravity / 4.0f), 2.0f, 0.01f);
    EXPECT_NEAR(jump_peak(1.0f / 60.0f, kEarthGravity * 2.0f), 0.25f, 0.01f);
}

TEST(GameTest, CannotJumpInTheAir) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({}, 0.01f);
    game.update({.rise = 1.0f}, 0.01f);
    const float speed = game.player().vertical_speed();
    EXPECT_GT(speed, 0.0f);
    game.update({.rise = 1.0f}, 0.01f);
    EXPECT_LT(game.player().vertical_speed(), speed);  // Only gravity acts now.
}

TEST(GameTest, HoldingRiseJumpsOnlyOnce) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({}, 0.01f);
    // Hold for 2 seconds: one jump, then the player stays on the ground.
    for (int frame = 0; frame < 200; ++frame) {
        game.update({.rise = 1.0f}, 0.01f);
    }
    EXPECT_TRUE(game.player().on_ground());
    EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);
    // Release and press again: a second jump.
    game.update({}, 0.01f);
    game.update({.rise = 1.0f}, 0.01f);
    EXPECT_GT(game.player().vertical_speed(), 0.0f);
}

/// Walks towards a 0.4 m high block for 2 seconds, jumping at `jump_frame` if given, and returns the end.
glm::vec3 walk_to_low_block(int jump_frame) {
    Level level = make_ramp_level();
    level.world.add_obstacle(
        {.base_center = {-4.0f, 4.0f, 0.0f}, .half_size = glm::vec2(1.0f), .height = 0.4f, .yaw = 0.0f});
    const GameConfig config = make_config();
    level.player_yaw = kPi / 2.0f;  // Facing the block at +y.
    Game game(config, std::move(level));
    game.update({}, 1.0f / 60.0f);
    for (int frame = 0; frame < 120; ++frame) {
        game.update({.forward = 1.0f, .rise = frame == jump_frame ? 1.0f : 0.0f}, 1.0f / 60.0f);
    }
    return game.player().position();
}

TEST(GameTest, JumpingOntoLowBlock) {
    // Walking alone stops at the block, which starts at y = 3.
    const glm::vec3 walked = walk_to_low_block(-1);
    EXPECT_FLOAT_EQ(walked.z, 0.0f);
    EXPECT_NEAR(walked.y, 3.0f - PlayerParams{}.radius, 1e-4f);
    // A jump just before the block lands on it.
    const glm::vec3 jumped = walk_to_low_block(60);
    EXPECT_NEAR(jumped.z, 0.4f, 1e-4f);
    EXPECT_GT(jumped.y, 3.0f);
}

// An empty level with the exit 3 m ahead of the start, where the player faces +x.
Level make_exit_level() {
    World world({.x_min = -10.0f, .x_max = 10.0f, .y_min = -10.0f, .y_max = 10.0f, .height = 6.0f});
    world.set_exit({.floor_point = {3.0f, 0.0f, 0.0f}});
    return {.layout = LayoutKind::kDebug, .world = std::move(world), .info = ""};
}

TEST(GameTest, TouchingExitStartsNextLevel) {
    Game game(make_config(), make_exit_level(), 5);
    EXPECT_EQ(game.level_number(), 0);
    // The player stops 0.6 m, the radius, before the exit, and touches it after walking 2.15 m at 2 m/s.
    for (int frame = 0; frame < 10; ++frame) {
        game.update({.forward = 1.0f}, 0.1f);
    }
    EXPECT_EQ(game.level_number(), 0);
    game.update({.forward = 1.0f}, 0.1f);
    EXPECT_EQ(game.level_number(), 1);
    EXPECT_EQ(game.seed(), next_level_seed(5));
    // The level settings choose the layout of the next level: rooms by default.
    EXPECT_EQ(game.level().layout, LayoutKind::kRooms);
    EXPECT_TRUE(game.world().exit().has_value());
    EXPECT_EQ(game.player().position(), game.level().player_start);
    EXPECT_FLOAT_EQ(game.player().yaw(), game.level().player_yaw);
    EXPECT_FLOAT_EQ(game.player().vertical_speed(), 0.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(game.time()), 1.1f);
}

TEST(GameTest, NoclipTouchesExitToo) {
    Game game(make_config(), make_exit_level());
    game.set_noclip(true);
    game.teleport({3.0f, 0.0f, 0.5f});
    game.update({}, 0.01f);
    EXPECT_EQ(game.level_number(), 1);
    EXPECT_TRUE(game.noclip());
}

TEST(GameTest, FlyingOverExitDoesNotTouchIt) {
    Game game(make_config(), make_exit_level());
    game.set_noclip(true);
    game.teleport({3.0f, 0.0f, LevelExit::kHoverHeight + LevelExit::kSize + 0.01f});
    game.update({}, 0.01f);
    EXPECT_EQ(game.level_number(), 0);
}

TEST(GameTest, NextLevelChoosesLayoutAtRandomAndFollowsSeeds) {
    Game game(make_config(), 7, LayoutKind::kDebug);
    game.next_level();
    game.next_level();
    EXPECT_EQ(game.level_number(), 2);
    EXPECT_EQ(game.seed(), next_level_seed(next_level_seed(7)));
    // The given layout is only for the first level. The level settings choose rooms by default.
    EXPECT_EQ(game.level().layout, LayoutKind::kRooms);
    // The same seed gives the same level as a new game with it.
    const Game fresh(make_config(), game.seed());
    EXPECT_EQ(game.level().info, fresh.level().info);
    // Every door of the new level starts closed.
    EXPECT_EQ(game.doors().size(), game.world().doors().size());
    EXPECT_FALSE(game.world().doors().empty());
    for (const DoorState& door : game.doors()) {
        EXPECT_FLOAT_EQ(door.openness, 0.0f);
        EXPECT_FALSE(door.moving());
    }
}

/// Walks forward for `seconds`, in frames of 0.1 s.
void walk(Game& game, float seconds) {
    for (int frame = 0; frame < static_cast<int>(std::round(seconds * 10.0f)); ++frame) {
        game.update({.forward = 1.0f}, 0.1f);
    }
}

TEST(GameTest, ClosedDoorBlocksUntilFullyOpen) {
    Game game(make_config(), make_door_level());
    ASSERT_EQ(game.doors().size(), 1U);
    walk(game, 3.0f);
    // The player stops a radius before the closed halves.
    const float stop = -(Door::kThickness / 2.0f) - game.player().radius();
    EXPECT_NEAR(game.player().position().y, stop, 1e-4f);

    ASSERT_TRUE(game.set_door_open(0, true));
    walk(game, 0.5f);
    EXPECT_NEAR(game.player().position().y, stop, 1e-4f);
    walk(game, 1.0f);
    EXPECT_GT(game.player().position().y, 0.5f);
}

TEST(GameTest, ActivatingClosedDoorOpensItInOneSecondAndIsIgnoredWhileItMoves) {
    Game game(make_config(), make_door_level());
    EXPECT_EQ(game.activate(), Activation::kOpened);
    game.update({}, Door::kMoveTime / 2.0f);
    EXPECT_FLOAT_EQ(game.doors()[0].openness, 0.5f);
    EXPECT_EQ(game.activate(), Activation::kRefused);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 1.0f);
    game.update({}, Door::kMoveTime / 2.0f);
    EXPECT_TRUE(game.doors()[0].passable());
    // The halves are hidden in the walls, so the view goes through the open door.
    EXPECT_EQ(game.activate(), Activation::kNothing);
}

TEST(GameTest, ActivatingFrameOpensAndClosesTheDoor) {
    Game game(make_config(), make_door_level());
    game.teleport({0.9f, -2.0f, 0.0f});  // Facing a frame along +y.
    EXPECT_EQ(game.activate(), Activation::kOpened);
    game.update({}, Door::kMoveTime);
    EXPECT_EQ(game.activate(), Activation::kClosed);
    EXPECT_FALSE(game.doors()[0].passable());
    game.update({}, Door::kMoveTime);
    EXPECT_FLOAT_EQ(game.doors()[0].openness, 0.0f);
    EXPECT_EQ(game.activate(), Activation::kOpened);
}

TEST(GameTest, DoorsThatStartToMoveAreReportedOnce) {
    Game game(make_config(), make_door_level());
    EXPECT_TRUE(game.take_door_moves().empty());
    EXPECT_EQ(game.activate(), Activation::kOpened);
    EXPECT_EQ(game.activate(), Activation::kRefused);  // Moving: no new move.
    std::vector<DoorMove> moves = game.take_door_moves();
    ASSERT_EQ(moves.size(), 1U);
    EXPECT_EQ(moves[0].door, 0U);
    EXPECT_TRUE(moves[0].open);
    EXPECT_TRUE(game.take_door_moves().empty());

    game.update({}, Door::kMoveTime);
    ASSERT_TRUE(game.set_door_open(0, false));
    moves = game.take_door_moves();
    ASSERT_EQ(moves.size(), 1U);
    EXPECT_FALSE(moves[0].open);
}

TEST(GameTest, DoorMovesOfALevelAreDroppedWhenTheNextStarts) {
    Game game(make_config(), make_door_level());
    ASSERT_TRUE(game.set_door_open(0, true));
    game.next_level();
    EXPECT_TRUE(game.take_door_moves().empty());
}

TEST(GameTest, ActivationNeedsANewPressOfTheKey) {
    Game game(make_config(), make_door_level());
    game.teleport({0.9f, -2.0f, 0.0f});
    game.update({.activate = true}, 0.1f);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 1.0f);
    for (int frame = 0; frame < 20; ++frame) {
        game.update({.activate = true}, 0.1f);
    }
    EXPECT_TRUE(game.doors()[0].passable());  // Holding the key doesn't close the door again.
    game.update({}, 0.1f);
    game.update({.activate = true}, 0.1f);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 0.0f);
}

TEST(GameTest, ActivationIsLimitedByReachAndWalls) {
    Level level = make_door_level();
    // A low block between the player and the door, below the eyes when standing on the floor.
    level.world.add_obstacle({.base_center = {0.0f, -1.0f, 0.0f}, .half_size = {0.2f, 0.2f}, .height = 1.0f});
    Game game(make_config(), std::move(level));
    game.teleport({0.0f, -2.1f, 0.0f});
    EXPECT_EQ(game.activate(), Activation::kNothing);  // The door is 2.05 m from the eyes.
    // Standing lower, the block is in the way.
    game.teleport({0.0f, -1.5f, -1.0f});
    EXPECT_EQ(game.activate(), Activation::kNothing);
    game.teleport({0.0f, -1.5f, 0.0f});
    EXPECT_EQ(game.activate(), Activation::kOpened);
}

TEST(GameTest, DoorDoesNotCloseOnThePlayer) {
    Game game(make_config(), make_door_level());
    ASSERT_TRUE(game.set_door_open(0, true));
    EXPECT_FALSE(game.set_door_open(0, true));  // Already opening.
    game.update({}, Door::kMoveTime);
    EXPECT_FALSE(game.set_door_open(0, true));  // Already open.
    game.teleport({0.0f, 0.5f, 0.0f});
    EXPECT_FALSE(game.set_door_open(0, false));
    game.teleport({0.0f, 0.7f, 0.0f});
    EXPECT_TRUE(game.set_door_open(0, false));
    EXPECT_THROW(game.set_door_open(1, true), std::out_of_range);
}

}  // namespace
}  // namespace manifold
