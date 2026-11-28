#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "door_level.hpp"
#include "items/inventory.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "level/texture_kind.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "save/saved_game.hpp"
#include "test_items.hpp"
#include "util/asset_mapping.hpp"
#include "world/chest.hpp"
#include "world/door.hpp"
#include "world/player.hpp"
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
    config.walk_speed = 2.0f;
    config.run_speed = 4.0f;
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

TEST(GameTest, MouseRotationTurnsTheViewWhateverTheFrameTimeWithinThePitchLimit) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.mouse_rotation = {0.5f, 0.25f}}, 0.0f);
    EXPECT_NEAR(game.player().yaw(), 0.5f, 1e-6f);
    EXPECT_NEAR(game.player().pitch(), 0.25f, 1e-6f);
    game.update({.mouse_rotation = {-1.0f, 2.0f}}, 0.25f);
    EXPECT_NEAR(game.player().yaw(), (2.0f * kPi) - 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(game.player().pitch(), Player::kMaxPitch);
    game.update({.mouse_rotation = {0.0f, -4.0f}}, 0.25f);
    EXPECT_FLOAT_EQ(game.player().pitch(), -Player::kMaxPitch);
}

TEST(GameTest, MouseRotationAddsToTheKeys) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.turn = 1.0f, .look = 1.0f, .mouse_rotation = {0.25f, -0.125f}}, 0.25f);
    EXPECT_NEAR(game.player().yaw(), (kPi / 8.0f) + 0.25f, 1e-6f);
    EXPECT_NEAR(game.player().pitch(), (kPi / 12.0f) - 0.125f, 1e-6f);
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

TEST(GameTest, RunningIsFasterOnlyForward) {
    // Facing +x, forward is +x and right is -y. Walks at 2 m/s and runs at 4 m/s.
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.forward = 1.0f, .run = true}, 0.5f);
    expect_near(game.player().position(), {2.0f, 0.0f, 0.0f});
    game.update({.forward = -1.0f, .run = true}, 0.5f);
    expect_near(game.player().position(), {1.0f, 0.0f, 0.0f});
    game.update({.run = true, .strafe = 1.0f}, 0.5f);
    expect_near(game.player().position(), {1.0f, -1.0f, 0.0f});
}

TEST(GameTest, RunningDiagonallyIsFasterOnlyInTheForwardPart) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.forward = 1.0f, .run = true, .strafe = 1.0f}, 1.0f);
    const float part = std::numbers::sqrt2_v<float> / 2.0f;
    expect_near(game.player().position(), {4.0f * part, -2.0f * part, 0.0f});
}

TEST(GameTest, RunningIgnoresPitch) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.look = 1.0f}, 10.0f);
    game.update({.forward = 1.0f, .run = true}, 1.0f);
    expect_near(game.player().position(), {4.0f, 0.0f, 0.0f});
}

TEST(GameTest, NoclipRunsButLevitationDoesNot) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.set_noclip(true);
    game.update({.forward = 1.0f, .run = true}, 1.0f);
    EXPECT_NEAR(game.player().position().x, 4.0f, 1e-5f);
    game.set_noclip(false);
    game.teleport({0.0f, 0.0f, 0.0f});
    ASSERT_TRUE(game.cast(Spell::kLevitate));
    game.update({.forward = 1.0f, .run = true}, 1.0f);
    EXPECT_NEAR(game.player().position().x, 2.0f, 1e-5f);
}

TEST(GameTest, CollisionsStopPlayerAtLevelWall) {
    const GameConfig config = make_config();
    Game game(config, 1, LayoutKind::kDebug);
    game.update({.forward = 1.0f}, 15.0f);
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

/// Advances `game` in frames of 10 ms with the same input until `done()` holds, but for at most `max_seconds`, so that
/// a test fails instead of hanging if it never does.
///
/// \return Whether `done()` held in time.
template <typename Done>
bool run_until(Game& game, const PlayerInput& input, Done done, float max_seconds = 10.0f) {
    for (int frame = 0; frame < static_cast<int>(std::lround(max_seconds / 0.01f)); ++frame) {
        if (done()) {
            return true;
        }
        game.update(input, 0.01f);
    }
    return done();
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
    run(game, {.forward = 1.0f}, 0.05f);  // 0.3 m: the first step at a quarter of Game::kWalkStepLength.
    EXPECT_EQ(game.take_footsteps(), 1);
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {.forward = 1.0f}, 1.0f);  // 2.3 m: steps at 1.25 and 2.25 m.
    EXPECT_EQ(game.take_footsteps(), 2);
    run(game, {}, 1.0f);  // Standing makes no steps.
    EXPECT_EQ(game.take_footsteps(), 0);
}

TEST(GameTest, RunningMakesAFootstepPerRunStepLength) {
    Game game(make_config(), 1, LayoutKind::kDebug);  // Runs at 4 m/s.
    run(game, {}, 0.1f);
    run(game, {.forward = 1.0f, .run = true}, 0.05f);  // 0.2 m.
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {.forward = 1.0f, .run = true}, 0.05f);  // 0.4 m: the first step at a quarter of 1.5 m.
    EXPECT_EQ(game.take_footsteps(), 1);
    run(game, {.forward = 1.0f, .run = true}, 1.0f);  // 4.4 m: steps at 1.875 and 3.375 m.
    EXPECT_EQ(game.take_footsteps(), 2);
}

TEST(GameTest, WalkingAndRunningAddUpTheirPartsOfAStep) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    run(game, {}, 0.1f);
    run(game, {.forward = 1.0f}, 0.15f);  // 0.3 m: the first footstep, and 0.05 of the next step.
    EXPECT_EQ(game.take_footsteps(), 1);
    run(game, {.forward = 1.0f}, 0.25f);  // Half a walking step: 0.55 of the step.
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {.forward = 1.0f, .run = true}, 0.15f);  // 0.6 m, 0.4 of a running step: 0.95, none yet.
    EXPECT_EQ(game.take_footsteps(), 0);
    run(game, {.forward = 1.0f, .run = true}, 0.05f);  // 0.2 m more: 1.08, a footstep.
    EXPECT_EQ(game.take_footsteps(), 1);
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

TEST(GameTest, FootstepsStartAgainInTheNextLevel) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    run(game, {.forward = 1.0f}, 0.5f);
    game.next_level();
    EXPECT_EQ(game.take_footsteps(), 0);
}

TEST(GameTest, EachLevelHasTheTextureOfItsSeedAmongTheAssets) {
    GameConfig config = make_config();
    config.level.assets.set(AssetKey::kGrassLevelTextures, 3);
    config.level.assets.set(AssetKey::kMetalLevelTextures, 2);
    Game game(config, 1, LayoutKind::kDebug);
    for (int level = 0; level < 5; ++level) {
        const LevelTexture& texture = game.level().texture;
        EXPECT_EQ(texture, choose_level_texture(config.level.assets, game.seed()));
        EXPECT_TRUE(texture.kind == TextureKind::kGrass || texture.kind == TextureKind::kMetal);
        EXPECT_LT(texture.index, config.level.assets.of(level_textures_key(texture.kind)));
        game.next_level();
    }
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

/// An empty level with a wall at y = 1, and the player at x = 0, `gap` metres from touching it.
Level make_wall_level(float yaw, float gap = 0.01f) {
    const World world({.x_min = -10.0f, .x_max = 10.0f, .y_min = -10.0f, .y_max = 1.0f, .height = 3.0f});
    return {.layout = LayoutKind::kDebug,
            .world = world,
            .player_start = {0.0f, 1.0f - PlayerParams{}.radius - gap, 0.0f},
            .player_yaw = yaw,
            .info = ""};
}

TEST(GameTest, RunningIntoAWallMovesAtWalkingSpeed) {
    // Facing 45 degrees into the wall, the player slides along it at the walking speed's part along the wall.
    Game game(make_config(), make_wall_level(kPi / 4.0f));
    run(game, {.forward = 1.0f, .run = true}, 1.0f);
    EXPECT_NEAR(game.player().position().x, 2.0f * std::numbers::sqrt2_v<float> / 2.0f, 0.02f);
}

TEST(GameTest, RunningAlongAWallIsFullSpeed) {
    Game game(make_config(), make_wall_level(0.0f));
    run(game, {.forward = 1.0f, .run = true}, 1.0f);
    EXPECT_NEAR(game.player().position().x, 4.0f, 1e-3f);
}

TEST(GameTest, RunningIntoAWallAtLessThan30DegreesSlidesAtRunningSpeed) {
    // Facing 20 degrees into the wall, the player slides along it at the running speed's part along the wall.
    const float angle = kPi / 9.0f;
    Game game(make_config(), make_wall_level(angle));
    run(game, {.forward = 1.0f, .run = true}, 1.0f);
    EXPECT_NEAR(game.player().position().x, 4.0f * std::cos(angle), 0.02f);
    // At 40 degrees, it slides at the walking speed's part.
    Game steep(make_config(), make_wall_level(2.0f * angle));
    run(steep, {.forward = 1.0f, .run = true}, 1.0f);
    EXPECT_NEAR(steep.player().position().x, 2.0f * std::cos(2.0f * angle), 0.02f);
}

TEST(GameTest, RunningJumpGoesFarther) {
    // A jump of 0.5 m is in the air for about 0.64 s, so the distance follows the horizontal speed.
    const auto jump_distance = [](bool running) {
        Game game(make_config(), 1, LayoutKind::kDebug);
        run(game, {}, 0.1f);  // Find the ground.
        game.update({.forward = 1.0f, .run = running, .rise = 1.0f}, 0.01f);
        EXPECT_TRUE(run_until(game, {.forward = 1.0f, .run = running}, [&game] { return game.player().on_ground(); }));
        return game.player().position().x;
    };
    const float walking = jump_distance(false);
    EXPECT_NEAR(walking, 2.0f * 0.64f, 0.05f);
    EXPECT_NEAR(jump_distance(true), 2.0f * walking, 0.05f);
}

TEST(GameTest, InTheAirTheVelocityOfTheTakeOffIsKeptWhateverTheKeys) {
    // The same running jump, but in the air the player lets go of Shift, walks backward, steps aside or turns.
    const std::vector<PlayerInput> air_inputs = {
        {.forward = 1.0f, .run = true}, {}, {.forward = -1.0f}, {.strafe = 1.0f}, {.turn = 1.0f, .forward = 1.0f}};
    std::vector<glm::vec3> landings;
    for (const PlayerInput& air_input : air_inputs) {
        Game game(make_config(), 1, LayoutKind::kDebug);
        run(game, {}, 0.1f);
        game.update({.forward = 1.0f, .run = true, .rise = 1.0f}, 0.01f);
        EXPECT_TRUE(run_until(game, air_input, [&game] { return game.player().on_ground(); }));
        landings.push_back(game.player().position());
    }
    for (const glm::vec3& landing : landings) {
        expect_near(landing, landings[0]);
    }
    EXPECT_GT(landings[0].x, 2.0f);
}

TEST(GameTest, SteppingOffALedgeKeepsTheWalkingVelocity) {
    // From the platform of the ramp level, which ends at x = 6, 2.5 m high, walking along +x.
    Level level = make_ramp_level();
    level.player_start = {5.0f, 0.0f, 2.5f};
    Game game(make_config(), std::move(level));
    ASSERT_TRUE(run_until(game, {.forward = 1.0f}, [&game] { return game.player().position().z <= 2.4f; }));
    const float x = game.player().position().x;
    run(game, {.forward = -1.0f}, 0.2f);  // Falling: the keys change nothing.
    EXPECT_FALSE(game.player().on_ground());
    EXPECT_NEAR(game.player().position().x, x + 0.4f, 1e-3f);
    EXPECT_NEAR(game.player().horizontal_velocity().x, 2.0f, 1e-3f);
}

TEST(GameTest, AWallInTheAirStopsOnlyThePartOfTheVelocityIntoIt) {
    // A jump at 45 degrees towards the wall at y = 1, 0.3 m away: the player reaches it in the air, at about 0.2 s,
    // and then slides along it.
    Game game(make_config(), make_wall_level(kPi / 4.0f, 0.3f));
    run(game, {}, 0.1f);
    game.update({.forward = 1.0f, .rise = 1.0f}, 0.01f);
    const float part = std::numbers::sqrt2_v<float> / 2.0f;
    EXPECT_NEAR(game.player().horizontal_velocity().y, 2.0f * part, 1e-3f);
    run(game, {}, 0.4f);
    EXPECT_FALSE(game.player().on_ground());
    EXPECT_NEAR(game.player().horizontal_velocity().x, 2.0f * part, 1e-3f);
    EXPECT_NEAR(game.player().horizontal_velocity().y, 0.0f, 1e-3f);
}

TEST(GameTest, ARunningTakeOffIntoAWallKeepsTheWalkingSlide) {
    // Running and jumping at 45 degrees into the wall, from touching it: the take-off frame is moved again at walking
    // speed, so the velocity kept in the air is the walking speed's part along the wall.
    Game game(make_config(), make_wall_level(kPi / 4.0f, 0.0f));
    run(game, {}, 0.1f);
    game.update({.forward = 1.0f, .run = true, .rise = 1.0f}, 0.01f);
    EXPECT_FALSE(game.player().on_ground());
    const float part = std::numbers::sqrt2_v<float> / 2.0f;
    EXPECT_NEAR(game.player().horizontal_velocity().x, 2.0f * part, 1e-3f);
    EXPECT_NEAR(game.player().horizontal_velocity().y, 0.0f, 1e-3f);
}

TEST(GameTest, NoclipTurnedOffInTheAirFallsStraightDown) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.set_noclip(true);
    game.update({.forward = 1.0f, .rise = 1.0f}, 1.0f);
    game.set_noclip(false);
    const float x = game.player().position().x;
    run(game, {.forward = 1.0f}, 0.3f);
    EXPECT_FALSE(game.player().on_ground());
    EXPECT_NEAR(game.player().position().x, x, 1e-5f);
}

TEST(GameTest, TheNextAndTheLoadedLevelStartOnTheGround) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_TRUE(game.player().on_ground());
    const SavedGame save = game.level_start_save();
    game.teleport({0.0f, 0.0f, 2.0f});
    game.next_level();
    EXPECT_TRUE(game.player().on_ground());
    game.teleport({0.0f, 0.0f, 2.0f});
    game.load(save);
    EXPECT_TRUE(game.player().on_ground());
}

TEST(GameTest, TeleportAndTheEndOfLevitationFallFromRest) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    run(game, {.forward = 1.0f}, 0.5f);
    game.teleport({0.0f, 0.0f, 2.0f});
    run(game, {.forward = 1.0f}, 0.3f);
    EXPECT_FALSE(game.player().on_ground());
    EXPECT_NEAR(game.player().position().x, 0.0f, 1e-5f);

    ASSERT_TRUE(game.cast(Spell::kLevitate));
    game.update({.forward = 1.0f}, 0.5f);
    EXPECT_NEAR(game.player().position().x, 1.0f, 1e-5f);
    // The spell ends in the air while walking forward: the fall is straight down.
    run(game, {.forward = 1.0f}, game.magic().spells[0].remaining - 0.005f);
    const float x = game.player().position().x;
    run(game, {.forward = 1.0f}, 0.3f);
    EXPECT_FALSE(game.levitating());
    EXPECT_FALSE(game.player().on_ground());
    EXPECT_NEAR(game.player().position().x, x, 1e-5f);
}

TEST(GameTest, RunsUpRampAtFullSpeed) {
    // Ramps push the player straight up, so they don't slow running down.
    Game game(make_config(), make_ramp_level());
    for (int frame = 0; frame < 90; ++frame) {
        game.update({.forward = 1.0f, .run = true}, 1.0f / 60.0f);
    }
    EXPECT_NEAR(game.player().position().x, 2.0f, 1e-3f);
    EXPECT_GT(game.player().position().z, 2.0f);
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

/// The characters of the game's party.
std::vector<Character> party_of(const Game& game) { return {game.party().begin(), game.party().end()}; }

TEST(GameTest, LevelStartSaveIsTheStartOfTheLevel) {
    Game game(make_config(), 3, LayoutKind::kDebug);
    const std::vector<Character> start_party = party_of(game);
    game.update({.forward = 1.0f}, 0.5f);
    game.set_hp(1, 40);
    EXPECT_EQ(game.level_start_save(), (SavedGame{.seed = 3,
                                                  .level_number = 0,
                                                  .layout = LayoutKind::kDebug,
                                                  .time = 0.0,
                                                  .party = start_party,
                                                  .magic = {}}));
    game.next_level();
    const std::vector<Character> second_party = party_of(game);
    EXPECT_EQ(second_party[1].hp, 40);
    game.update({.forward = 1.0f}, 0.25f);
    game.set_hp(1, 10);
    // Only the first level has the given layout.
    EXPECT_EQ(game.level_start_save(), (SavedGame{.seed = next_level_seed(3),
                                                  .level_number = 1,
                                                  .layout = std::nullopt,
                                                  .time = 0.5,
                                                  .party = second_party,
                                                  .magic = {}}));
}

TEST(GameTest, LoadStartsTheSavedLevelAgain) {
    Game original(make_config(), 3, LayoutKind::kDebug);
    original.update({}, 0.5f);
    original.set_hp(0, 25);
    original.next_level();
    const SavedGame save = original.level_start_save();

    Game game(make_config(), 11, LayoutKind::kDebug);
    game.set_noclip(true);
    game.update({.forward = 1.0f}, 2.0f);
    const int starts = game.level_starts();
    game.load(save);
    EXPECT_EQ(game.level_starts(), starts + 1);
    EXPECT_EQ(game.seed(), original.seed());
    EXPECT_EQ(game.level_number(), 1);
    EXPECT_EQ(game.level().layout, LayoutKind::kRooms);
    EXPECT_EQ(game.level().info, original.level().info);
    EXPECT_EQ(game.player().position(), game.level().player_start);
    EXPECT_FLOAT_EQ(game.player().yaw(), game.level().player_yaw);
    EXPECT_DOUBLE_EQ(game.time(), 0.5);
    EXPECT_TRUE(game.noclip());
    EXPECT_EQ(game.doors().size(), game.world().doors().size());
    EXPECT_EQ(party_of(game), party_of(original));
    EXPECT_EQ(game.party()[0].hp, 25);
    EXPECT_EQ(game.level_start_save(), save);
}

TEST(GameTest, NewGameStartsAgainFromTheFirstLevel) {
    Game game(GameConfig{}, 3, LayoutKind::kDebug);
    game.update({}, 2.0f);
    game.next_level();
    game.set_noclip(true);
    game.set_hp(0, 1);
    const int starts = game.level_starts();
    game.new_game(7, LayoutKind::kDebug);
    const Game fresh(GameConfig{}, 7, LayoutKind::kDebug);
    // The new party is unhurt, and the same as a game started with this seed has.
    EXPECT_EQ(party_of(game), party_of(fresh));
    EXPECT_EQ(game.party()[0].hp, game.party()[0].max_hp);
    EXPECT_EQ(game.level_start_save(), (SavedGame{.seed = 7,
                                                  .level_number = 0,
                                                  .layout = LayoutKind::kDebug,
                                                  .time = 0.0,
                                                  .party = party_of(fresh),
                                                  .magic = {}}));
    EXPECT_EQ(game.time(), 0.0);
    EXPECT_EQ(game.level_starts(), starts + 1);
    EXPECT_EQ(game.player().position(), game.level().player_start);
    EXPECT_TRUE(game.noclip());
    EXPECT_EQ(game.level().player_start, fresh.level().player_start);
}

TEST(GameTest, LoadKeepsTheGivenLayoutOfTheFirstLevel) {
    const Game original(make_config(), 3, LayoutKind::kDebug);
    // Without the layout, the seed would choose rooms.
    Game game(make_config(), 3);
    ASSERT_EQ(game.level().layout, LayoutKind::kRooms);
    game.load(original.level_start_save());
    EXPECT_EQ(game.level().layout, LayoutKind::kDebug);
    EXPECT_EQ(game.level().info, original.level().info);
    EXPECT_EQ(game.level_start_save(), original.level_start_save());
}

/// A game in a rooms level, which has doors and chests, with the test items, after the party changed it: walked and
/// turned, opened a door, which is still opening, lost hit points and mana, cast Levitate, and took an item out of a
/// chest and put one into another.
Game make_changed_game() {
    GameConfig config = make_config();
    config.level.items = make_test_items();
    Game game(config, 3, LayoutKind::kRooms);
    if (game.world().doors().empty() || game.world().chests().size() < 2) {
        throw std::logic_error("the level should have doors and two chests");
    }
    game.update({.mouse_rotation = {0.5f, 0.2f}, .forward = 1.0f}, 0.5f);
    game.set_door_open(0, true);
    game.update({}, 0.25f);
    game.set_hp(0, 5);
    game.set_mana(40.0f);
    game.cast(Spell::kLevitate);
    game.update({.rise = 1.0f}, 0.1f);
    game.chest_inventory(0).add(kCrown);
    move_stack(game.chest_inventory(0), game.chest_inventory(0).size() - 1, game.inventory());
    game.chest_inventory(1).add(kPebble);
    return game;
}

TEST(GameTest, ProgressSaveIsTheLevelStartWithTheGameAsItIs) {
    const Game game = make_changed_game();
    SavedGame save = game.progress_save();
    ASSERT_TRUE(save.progress);
    const LevelProgress& progress = *save.progress;
    EXPECT_DOUBLE_EQ(progress.time, game.time());
    EXPECT_EQ(progress.party, party_of(game));
    EXPECT_EQ(progress.magic, game.magic());
    EXPECT_EQ(progress.inventory, (std::vector<SavedItemStack>{{.key = "crown", .count = 1}}));
    EXPECT_EQ(progress.player.position, game.player().position());
    EXPECT_EQ(progress.player.yaw, game.player().yaw());
    EXPECT_EQ(progress.player.pitch, game.player().pitch());
    EXPECT_EQ(progress.player.vertical_speed, game.player().vertical_speed());
    EXPECT_EQ(progress.player.horizontal_velocity, game.player().horizontal_velocity());
    EXPECT_EQ(progress.player.on_ground, game.player().on_ground());
    EXPECT_EQ(progress.doors, std::vector<DoorState>(game.doors().begin(), game.doors().end()));
    EXPECT_TRUE(progress.doors[0].moving());
    ASSERT_EQ(progress.chests.size(), game.chest_inventories().size());
    // Each stack of the chest, with its number of items.
    const Inventory& chest = game.chest_inventories()[1];
    ASSERT_EQ(progress.chests[1].size(), chest.size());
    const std::size_t pebbles = chest.find(kPebble).value();
    EXPECT_EQ(progress.chests[1][pebbles], (SavedItemStack{.key = "pebble", .count = chest.stacks()[pebbles].count}));
    save.progress.reset();
    EXPECT_EQ(save, game.level_start_save());
}

TEST(GameTest, LoadingAProgressSavePutsTheLevelAsItWasAndItGoesOnTheSame) {
    Game game = make_changed_game();
    const SavedGame save = parse_saved_game(save_to_json(game.progress_save()));

    GameConfig config = make_config();
    config.level.items = make_test_items();
    Game loaded(config, 11, LayoutKind::kDebug);
    loaded.set_noclip(true);
    loaded.load(save);
    // Noclip isn't saved, and stays as it was.
    EXPECT_TRUE(loaded.noclip());
    loaded.set_noclip(false);
    EXPECT_EQ(loaded.seed(), game.seed());
    EXPECT_EQ(loaded.level().info, game.level().info);
    EXPECT_DOUBLE_EQ(loaded.time(), game.time());
    EXPECT_EQ(party_of(loaded), party_of(game));
    EXPECT_EQ(loaded.magic(), game.magic());
    EXPECT_EQ(loaded.inventory(), game.inventory());
    EXPECT_TRUE(std::ranges::equal(loaded.chest_inventories(), game.chest_inventories()));
    EXPECT_TRUE(std::ranges::equal(loaded.doors(), game.doors()));
    EXPECT_EQ(loaded.player().position(), game.player().position());
    EXPECT_EQ(loaded.player().yaw(), game.player().yaw());
    EXPECT_EQ(loaded.player().pitch(), game.player().pitch());
    EXPECT_EQ(loaded.player().vertical_speed(), game.player().vertical_speed());
    EXPECT_EQ(loaded.player().horizontal_velocity(), game.player().horizontal_velocity());
    EXPECT_EQ(loaded.player().on_ground(), game.player().on_ground());
    EXPECT_EQ(loaded.level_start_save(), game.level_start_save());
    EXPECT_EQ(loaded.progress_save(), game.progress_save());

    // Both go on the same way, also the door that was opening and the player who levitated.
    for (int frame = 0; frame < 60; ++frame) {
        const PlayerInput input{.forward = 1.0f, .strafe = 0.5f};
        game.update(input, 1.0f / 30.0f);
        loaded.update(input, 1.0f / 30.0f);
    }
    EXPECT_EQ(loaded.player().position(), game.player().position());
    EXPECT_TRUE(std::ranges::equal(loaded.doors(), game.doors()));
    EXPECT_EQ(loaded.magic(), game.magic());
}

TEST(GameTest, LoadingTheLevelStartAfterAProgressSaveStartsTheLevelAgain) {
    Game game = make_changed_game();
    const SavedGame start = game.level_start_save();
    game.load(game.progress_save());
    game.load(start);
    EXPECT_EQ(game.player().position(), game.level().player_start);
    EXPECT_DOUBLE_EQ(game.time(), 0.0);
    EXPECT_TRUE(game.inventory().empty());
    EXPECT_TRUE(std::ranges::all_of(game.doors(), [](const DoorState& door) { return door == DoorState{}; }));
    for (std::size_t i = 0; i < game.world().chests().size(); ++i) {
        EXPECT_EQ(game.chest_inventories()[i], Inventory(game.world().chests()[i].items));
    }
}

TEST(GameTest, LoadingAProgressOfAnotherLevelFailsAndKeepsTheLevel) {
    Game game = make_changed_game();
    const glm::vec3 position = game.player().position();
    const int starts = game.level_starts();
    for (const bool doors : {true, false}) {
        SavedGame save = game.progress_save();
        if (doors) {
            save.progress->doors.pop_back();
        } else {
            save.progress->chests.emplace_back();
        }
        EXPECT_THROW(game.load(save), std::runtime_error);
        EXPECT_EQ(game.level_starts(), starts);
        EXPECT_EQ(game.player().position(), position);
        EXPECT_EQ(game.inventory(), Inventory({kCrown}));
    }
}

TEST(GameTest, LoadingAProgressLeavesOutUnknownItemsOfChests) {
    Game game = make_changed_game();
    SavedGame save = game.progress_save();
    save.progress->chests[0] = {{.key = "unicorn"}, {.key = "gold_bar"}};
    testing::internal::CaptureStderr();
    game.load(save);
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(game.chest_inventories()[0], Inventory({kGoldBar}));
    EXPECT_NE(log.find("[warning] The saved item unicorn is unknown, so a chest loses it"), std::string::npos) << log;
}

TEST(GameTest, PartyIsMadeFromThePortraitsAndTheSeed) {
    GameConfig config = make_config();
    config.level.assets.set(AssetKey::kMalePortraits, 2);
    config.level.assets.set(AssetKey::kFemalePortraits, 1);
    config.level.assets.set(AssetKey::kAnyPortraits, 1);
    config.party = {.size = 3, .max_hp = 50};
    const Game game(config, 5, LayoutKind::kDebug);
    ASSERT_EQ(game.party().size(), 3U);
    for (const Character& character : game.party()) {
        EXPECT_TRUE(character.portrait.has_value());
        EXPECT_EQ(character.hp, 50);
        EXPECT_EQ(character.max_hp, 50);
    }
    EXPECT_EQ(party_of(game), party_of(Game(config, 5, LayoutKind::kDebug)));
}

TEST(GameTest, SetHpIsClampedAndChecksTheIndex) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    const int max_hp = game.party()[2].max_hp;
    game.set_hp(2, -5);
    EXPECT_EQ(game.party()[2].hp, 0);
    game.set_hp(2, max_hp + 5);
    EXPECT_EQ(game.party()[2].hp, max_hp);
    EXPECT_THROW(game.set_hp(3, 1), std::out_of_range);
}

/// Walks forward for `seconds`, in frames of 0.1 s.
void walk(Game& game, float seconds) {
    for (int frame = 0; frame < static_cast<int>(std::round(seconds * 10.0f)); ++frame) {
        game.update({.forward = 1.0f}, 0.1f);
    }
}

/// A seed whose level has ambient light, or none, see level_has_ambient_light().
std::uint64_t seed_with_ambient(bool ambient) {
    // Bounded, so that a change that gives every level ambient light, or none, fails instead of hanging.
    constexpr std::uint64_t kMaxSeed = 1000;
    for (std::uint64_t seed = 1; seed <= kMaxSeed; ++seed) {
        if (level_has_ambient_light(seed) == ambient) {
            return seed;
        }
    }
    ADD_FAILURE() << "no level with ambient light " << ambient << " up to seed " << kMaxSeed;
    return 1;
}

TEST(GameTest, FirstLevelGetsATorchOnTheNearestWall) {
    GameConfig config;
    config.level.rooms.min_torch_chance = 0.0f;
    config.level.rooms.max_torch_chance = 0.0f;
    // With ambient light and without.
    for (const bool ambient : {true, false}) {
        const Game game(config, seed_with_ambient(ambient), LayoutKind::kRooms);
        EXPECT_EQ(game.world().torches().size(), 1U) << "ambient " << ambient;
    }
    Game game(config, 3, LayoutKind::kRooms);
    EXPECT_EQ(game.world().torches().size(), 1U);
    const SavedGame first = game.level_start_save();
    // The levels after the first get none.
    game.next_level();
    EXPECT_TRUE(game.world().torches().empty());
    // Loading the first level, or a new game, gives it again.
    game.load(first);
    EXPECT_EQ(game.world().torches().size(), 1U);
    game.new_game(8, LayoutKind::kRooms);
    EXPECT_EQ(game.world().torches().size(), 1U);
}

TEST(GameTest, PartyStartsWithFullManaAndNoSpells) {
    const Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_EQ(game.magic(), PartyMagic{});
    EXPECT_FLOAT_EQ(game.magic().mana, 100.0f);
    EXPECT_FALSE(game.levitating());
}

TEST(GameTest, SpellsOfTheInputAreCastAfterTheMagicAdvances) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    game.update({.casts = {Spell::kCandlelight, Spell::kLevitate}}, 0.5f);
    // The mana was full, so it regained nothing before the casts.
    EXPECT_FLOAT_EQ(game.magic().mana, 85.0f);
    ASSERT_EQ(game.magic().spells.size(), 2U);
    EXPECT_FLOAT_EQ(game.magic().spells[0].remaining, 60.0f);
    EXPECT_TRUE(game.levitating());
    game.update({}, 2.0f);
    EXPECT_FLOAT_EQ(game.magic().mana, 87.0f);
    EXPECT_FLOAT_EQ(game.magic().spells[1].remaining, 58.0f);
    game.set_mana(3.0f);
    EXPECT_FALSE(game.cast(Spell::kCandlelight));
    game.set_mana(1000.0f);
    EXPECT_FLOAT_EQ(game.magic().mana, 100.0f);
}

TEST(GameTest, CastsOfTheInputAndOfCastAreTakenInOrderButFailedOnesAreNot) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    EXPECT_TRUE(game.take_casts().empty());
    game.update({.casts = {Spell::kLevitate, Spell::kCandlelight}}, 0.01f);
    ASSERT_TRUE(game.cast(Spell::kLevitate));
    EXPECT_EQ(game.take_casts(), (std::vector<Spell>{Spell::kLevitate, Spell::kCandlelight, Spell::kLevitate}));
    EXPECT_TRUE(game.take_casts().empty());
    game.set_mana(3.0f);
    EXPECT_FALSE(game.cast(Spell::kCandlelight));
    game.update({.casts = {Spell::kLevitate}}, 0.01f);
    EXPECT_TRUE(game.take_casts().empty());
}

TEST(GameTest, LevitationFliesWithCollisionAndFallsWhenItEnds) {
    const GameConfig config = make_config();
    Game game(config, 1, LayoutKind::kDebug);
    ASSERT_TRUE(game.cast(Spell::kLevitate));
    game.update({.rise = 1.0f}, 0.5f);
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-5f);
    // No gravity: the player stays in the air, and makes no footsteps walking there.
    game.update({}, 1.0f);
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-5f);
    EXPECT_FALSE(game.player().on_ground());
    game.update({.forward = 1.0f}, 1.0f);
    EXPECT_EQ(game.take_footsteps(), 0);
    // Unlike noclip, the level's walls stop the player, and so do its floor and ceiling.
    game.update({.forward = 1.0f}, 15.0f);
    EXPECT_NEAR(game.player().position().x, config.level.debug.x_max - game.player().radius(), 1e-4f);
    game.update({.rise = -1.0f}, 1.0f);
    EXPECT_NEAR(game.player().position().z, 0.0f, 1e-4f);
    game.update({.rise = 1.0f}, 0.5f);
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-4f);
    // When the spell ends, the player falls from rest.
    const float left = game.magic().spells[0].remaining;
    game.update({}, left - 0.01f);
    EXPECT_TRUE(game.levitating());
    EXPECT_NEAR(game.player().position().z, 1.0f, 1e-4f);
    // The frame in which it ends has gravity already.
    game.update({}, 0.02f);
    EXPECT_FALSE(game.levitating());
    EXPECT_NEAR(game.player().position().z, 1.0f - (game.level().gravity * 0.02f * 0.02f), 1e-4f);
    for (int frame = 0; frame < 100; ++frame) {
        game.update({}, 0.02f);
    }
    EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);
    EXPECT_TRUE(game.player().on_ground());
}

TEST(GameTest, LevitationMakesNoFootstepsOnTheFloor) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    ASSERT_TRUE(game.cast(Spell::kLevitate));
    walk(game, 3.0f);
    EXPECT_GT(game.player().position().x, 5.0f);
    EXPECT_FLOAT_EQ(game.player().position().z, 0.0f);
    EXPECT_EQ(game.take_footsteps(), 0);
}

TEST(GameTest, ClosedDoorStopsALevitatingPlayer) {
    Game game(make_config(), make_door_level());
    ASSERT_TRUE(game.cast(Spell::kLevitate));
    walk(game, 3.0f);
    EXPECT_NEAR(game.player().position().y, -(Door::kThickness / 2.0f) - game.player().radius(), 1e-4f);
}

TEST(GameTest, LevitationStopsAtTheTopOfTheLevel) {
    Game game(make_config(), 1, LayoutKind::kDebug);
    ASSERT_TRUE(game.cast(Spell::kLevitate));
    game.update({.rise = 1.0f}, 50.0f);
    EXPECT_NEAR(game.player().position().z + game.player().height(), game.world().bounds().height, 1e-3f);
}

TEST(GameTest, MagicIsKeptBetweenLevelsAndSavedAtTheirStart) {
    Game game(make_config(), 3, LayoutKind::kDebug);
    game.cast(Spell::kCandlelight);
    game.update({}, 1.0f);
    game.next_level();
    const PartyMagic start_magic = game.magic();
    EXPECT_TRUE(start_magic.active(Spell::kCandlelight));
    game.cast(Spell::kLevitate);
    EXPECT_EQ(game.level_start_save().magic, start_magic);

    Game loaded(make_config(), 11, LayoutKind::kDebug);
    loaded.load(game.level_start_save());
    EXPECT_EQ(loaded.magic(), start_magic);
    loaded.new_game(5, LayoutKind::kDebug);
    EXPECT_EQ(loaded.magic(), PartyMagic{});
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

/// make_door_level() with a chest on the floor between the player and the door, its front towards the player, and the
/// direction from the player's eyes to its middle.
std::pair<Level, glm::vec3> make_chest_level(const GameConfig& config) {
    Level level = make_door_level();
    level.world.add_chest({.base_center = {0.0f, -1.0f, 0.0f},
                           .yaw = -std::numbers::pi_v<float> / 2.0f,
                           .look = ChestLook::kWood,
                           .open_sound = 1});
    const Player player(config.player);
    const glm::vec3 eyes = level.player_start + glm::vec3(0.0f, 0.0f, player.eye_position().z - player.position().z);
    return {std::move(level), glm::normalize(glm::vec3(0.0f, -1.0f, Chest::kHeight / 2.0f) - eyes)};
}

TEST(GameTest, ActivatingAChestReportsItEachTimeAndLeavesTheDoor) {
    const GameConfig config = make_config();
    auto [level, to_chest] = make_chest_level(config);
    Game game(config, std::move(level));
    EXPECT_TRUE(game.take_chest_activations().empty());
    EXPECT_EQ(game.activate(to_chest), Activation::kChest);
    EXPECT_EQ(game.activate(to_chest), Activation::kChest);
    EXPECT_EQ(game.take_chest_activations(), (std::vector<std::size_t>{0, 0}));
    EXPECT_TRUE(game.take_chest_activations().empty());
    EXPECT_TRUE(game.take_door_moves().empty());
    // The chest is below the view, which reaches the door above it.
    EXPECT_EQ(game.activate(), Activation::kOpened);
    EXPECT_TRUE(game.take_chest_activations().empty());
}

TEST(GameTest, ClickingAChestActivatesItAndTheNextLevelDropsItsActivations) {
    const GameConfig config = make_config();
    auto [level, to_chest] = make_chest_level(config);
    Game game(config, std::move(level));
    game.update({.click = to_chest}, 0.0f);
    EXPECT_EQ(game.take_chest_activations(), (std::vector<std::size_t>{0}));
    game.update({.click = to_chest}, 0.0f);
    game.next_level();
    EXPECT_TRUE(game.take_chest_activations().empty());
}

TEST(GameTest, ChestsHoldTheirItemsUntilTheNextLevel) {
    GameConfig config = make_config();
    config.level.items = make_test_items();
    Level level = make_door_level();
    level.world.add_chest({.base_center = {0.0f, -1.0f, 0.0f}, .look = ChestLook::kGold, .items = {kPebble, kCrown}});
    level.world.add_chest({.base_center = {2.0f, -1.0f, 0.0f}});
    Game game(config, std::move(level));
    ASSERT_EQ(game.chest_inventories().size(), 2U);
    EXPECT_EQ(game.chest_inventories()[0], Inventory({kPebble, kCrown}));
    EXPECT_TRUE(game.chest_inventories()[1].empty());
    EXPECT_EQ(game.items(), make_test_items());
    move_stack(game.chest_inventory(0), 1, game.inventory());
    EXPECT_EQ(game.chest_inventories()[0], Inventory({kPebble}));
    EXPECT_EQ(game.inventory(), Inventory({kCrown}));
    EXPECT_THROW(static_cast<void>(game.chest_inventory(2)), std::out_of_range);
    // The next level has its own chests, and the party keeps its items.
    game.next_level();
    ASSERT_EQ(game.chest_inventories().size(), game.world().chests().size());
    for (std::size_t i = 0; i < game.world().chests().size(); ++i) {
        EXPECT_EQ(game.chest_inventories()[i], Inventory(game.world().chests()[i].items));
    }
    EXPECT_EQ(game.inventory(), Inventory({kCrown}));
}

TEST(GameTest, ItemsAreSavedAtTheStartOfTheLevelAndANewGameHasNone) {
    GameConfig config = make_config();
    config.level.items = make_test_items();
    Game game(config, 3, LayoutKind::kDebug);
    EXPECT_TRUE(game.inventory().empty());
    game.inventory().add(kGoldBar);
    EXPECT_TRUE(game.level_start_save().inventory.empty());
    game.next_level();
    game.inventory().add(kPebble);
    const SavedGame save = game.level_start_save();
    EXPECT_EQ(save.inventory, (std::vector<SavedItemStack>{{.key = "gold_bar", .count = 1}}));

    Game loaded(config, 5, LayoutKind::kDebug);
    loaded.inventory().add(kCrown);
    loaded.load(save);
    EXPECT_EQ(loaded.inventory(), Inventory({kGoldBar}));
    loaded.new_game(7, LayoutKind::kDebug);
    EXPECT_TRUE(loaded.inventory().empty());
}

TEST(GameTest, LoadingLeavesOutItemsThatAreUnknown) {
    GameConfig config = make_config();
    config.level.items = make_test_items();
    Game game(config, 3, LayoutKind::kDebug);
    SavedGame save = game.level_start_save();
    // Stacks of a kind are joined, as in older saves, which have a key for each item.
    save.inventory = {{.key = "pebble"}, {.key = "unicorn"}, {.key = "crown", .count = 2}, {.key = "pebble"}};
    testing::internal::CaptureStderr();
    game.load(save);
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(game.inventory(), Inventory({kPebble, kCrown, kCrown, kPebble}));
    EXPECT_NE(log.find("[warning] The saved item unicorn is unknown"), std::string::npos) << log;
}

TEST(GameTest, LoadingCutsSpellsToTheirDurationAndMagicFollowsTheConfig) {
    GameConfig config = make_config();
    config.magic.max_mana = 50.0f;
    config.magic.mana_regeneration = 2.0f;
    config.magic.spell(Spell::kLevitate).duration = 10.0f;
    Game game(config, 3, LayoutKind::kDebug);
    EXPECT_EQ(game.magic(), (PartyMagic{.mana = 50.0f, .max_mana = 50.0f, .spells = {}}));
    SavedGame save = game.level_start_save();
    // Saved while Levitate lasted longer.
    save.magic = {
        .mana = 20.0f,
        .max_mana = 50.0f,
        .spells = {{.spell = Spell::kLevitate, .remaining = 60.0f}, {.spell = Spell::kCandlelight, .remaining = 5.0f}}};
    game.load(save);
    ASSERT_EQ(game.magic().spells.size(), 2U);
    EXPECT_FLOAT_EQ(game.magic().spells[0].remaining, 10.0f);
    EXPECT_FLOAT_EQ(game.magic().spells[1].remaining, 5.0f);
    game.update({}, 1.0f);
    EXPECT_FLOAT_EQ(game.magic().mana, 22.0f);
    ASSERT_TRUE(game.cast(Spell::kLevitate));
    EXPECT_FLOAT_EQ(game.magic().spells[0].remaining, 10.0f);
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

TEST(GameTest, ClickActivatesInItsDirectionWithTheSameReach) {
    Game game(make_config(), make_door_level());
    // Facing along +x, with the door to the left, 1.45 m from the eyes.
    game.teleport({0.0f, -1.5f, 0.0f}, 0.0f);
    EXPECT_EQ(game.activate(), Activation::kNothing);
    EXPECT_EQ(game.activate(glm::vec3(0.0f, 1.0f, 0.0f)), Activation::kOpened);
    game.update({}, Door::kMoveTime);
    // Each click in an update activates, without a new press.
    game.teleport({0.9f, -1.5f, 0.0f}, 0.0f);  // The frame to the left.
    game.update({.click = glm::vec3(0.0f, 1.0f, 0.0f)}, 0.1f);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 0.0f);
    game.update({}, Door::kMoveTime);
    game.update({.click = glm::vec3(0.0f, 1.0f, 0.0f)}, 0.1f);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 1.0f);
    // The door is 2.05 m from the eyes, out of reach as for the center of the view.
    game.update({}, Door::kMoveTime);
    ASSERT_TRUE(game.set_door_open(0, false));
    game.update({}, Door::kMoveTime);
    game.teleport({0.0f, -2.1f, 0.0f}, 0.0f);
    game.update({}, 0.01f);  // Land on the floor, since a teleported player is in the air until then.
    EXPECT_EQ(game.activate(glm::vec3(0.0f, 1.0f, 0.0f)), Activation::kNothing);
    // Stepping 0.2 m to the left in the same update brings it within reach: the click activates after the move.
    game.update({.strafe = -1.0f, .click = glm::vec3(0.0f, 1.0f, 0.0f)}, 0.1f);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 1.0f);
}

TEST(GameTest, CenterClickActivatesAlongTheViewAfterTheTurnOfTheUpdate) {
    Game game(make_config(), make_door_level());
    // Facing along +x, with the door to the left, 1.45 m from the eyes.
    game.teleport({0.0f, -1.5f, 0.0f}, 0.0f);
    game.update({.center_click = true}, 0.1f);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 0.0f);
    // Mouse look turns the view to the door in the same update as the click.
    game.update({.mouse_rotation = {kPi / 2.0f, 0.0f}, .center_click = true}, 0.1f);
    EXPECT_FLOAT_EQ(game.doors()[0].target, 1.0f);
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
