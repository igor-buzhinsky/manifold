#include "console/game_commands.hpp"

#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "console/console.hpp"
#include "door_level.hpp"
#include "game.hpp"
#include "level/level.hpp"
#include "render/camera.hpp"
#include "world/door.hpp"

namespace manifold {
namespace {

/// Settings for a game in an empty debug level.
GameConfig make_config() {
    GameConfig config;
    config.level.debug.obstacle_count = 0;
    config.level.debug.ramp_count = 0;
    return config;
}

/// A game with the game commands, by default in an empty debug level.
struct Fixture {
    Game game;
    Console console;
    CommandQueue queue;
    ConsoleState state;

    Fixture() : Fixture(Game(make_config(), 1, LayoutKind::kDebug)) {}
    explicit Fixture(Game started) : game(std::move(started)) { add_game_commands(console, queue, game, state); }

    /// Runs a command that must fail.
    void expect_error(const std::string& text) {
        queue.add(text, true);
        EXPECT_THROW(queue.run_frame(console), CommandError) << text;
    }

    /// Runs commands that must succeed, all in one frame.
    void run(const std::string& text) {
        queue.add(text, true);
        queue.run_frame(console);
    }
};

TEST(GameCommandsTest, CameraIsPlacedInDegreesAndReturnsToPlayer) {
    Fixture fixture;
    EXPECT_EQ(view_camera(fixture.state.view, fixture.game.player()).position,
              make_player_camera(fixture.game.player()).position);
    fixture.run("camera 1 2 30 90 -90");
    ASSERT_TRUE(fixture.state.view.camera.has_value());
    EXPECT_EQ(fixture.state.view.camera->position, glm::vec3(1.0f, 2.0f, 30.0f));
    EXPECT_FLOAT_EQ(fixture.state.view.camera->yaw, std::numbers::pi_v<float> / 2.0f);
    EXPECT_FLOAT_EQ(fixture.state.view.camera->pitch, -std::numbers::pi_v<float> / 2.0f);
    EXPECT_EQ(view_camera(fixture.state.view, fixture.game.player()).position, glm::vec3(1.0f, 2.0f, 30.0f));
    EXPECT_NO_THROW(fixture.run("camera"));  // Only shows where the camera is.
    fixture.run("camera player");
    EXPECT_FALSE(fixture.state.view.camera.has_value());
}

TEST(GameCommandsTest, CameraRejectsWrongArguments) {
    Fixture fixture;
    for (const char* command : {"camera 1 2 3", "camera 1 2 3 0 91", "camera 1 2 3 x 0", "camera me"}) {
        fixture.queue.add(command, true);
        EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError) << command;
    }
    EXPECT_FALSE(fixture.state.view.camera.has_value());
}

TEST(GameCommandsTest, TeleportMovesThePlayerInDegrees) {
    Fixture fixture;
    fixture.run("teleport 1 +2 3.5 180");
    EXPECT_EQ(fixture.game.player().position(), glm::vec3(1.0f, 2.0f, 3.5f));
    EXPECT_FLOAT_EQ(fixture.game.player().yaw(), std::numbers::pi_v<float>);
    fixture.run("teleport 0 0 0");
    EXPECT_FLOAT_EQ(fixture.game.player().yaw(), std::numbers::pi_v<float>);
    for (const char* command : {"teleport 1 2", "teleport 1 2 3 4 5", "teleport a 2 3"}) {
        fixture.queue.add(command, true);
        EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError) << command;
    }
}

TEST(GameCommandsTest, NextLevelStartsTheNextLevel) {
    Fixture fixture;
    fixture.run("next_level");
    EXPECT_EQ(fixture.game.level_number(), 1);
    EXPECT_EQ(fixture.game.player().position(), fixture.game.level().player_start);
    fixture.queue.add("next_level 2", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
}

TEST(GameCommandsTest, CommandsAfterNextLevelWaitAFrame) {
    Fixture fixture;
    fixture.run("next_level; teleport 1 2 3");
    EXPECT_EQ(fixture.game.player().position(), fixture.game.level().player_start);
    fixture.queue.run_frame(fixture.console);
    EXPECT_EQ(fixture.game.player().position(), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(GameCommandsTest, NoclipTogglesOrIsSet) {
    Fixture fixture;
    EXPECT_FALSE(fixture.game.noclip());
    fixture.run("noclip");
    EXPECT_TRUE(fixture.game.noclip());
    fixture.run("noclip on");
    EXPECT_TRUE(fixture.game.noclip());
    fixture.run("noclip");
    EXPECT_FALSE(fixture.game.noclip());
    fixture.queue.add("noclip maybe", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
}

TEST(GameCommandsTest, WalkSetsTheFramesToWalk) {
    Fixture fixture;
    fixture.run("walk 30");
    EXPECT_EQ(fixture.state.walk_frames, 30);
    fixture.run("walk 0");
    EXPECT_EQ(fixture.state.walk_frames, 0);
    fixture.expect_error("walk -1");
    fixture.expect_error("walk");
}

TEST(GameCommandsTest, CutawaySetsAndClearsTheHeight) {
    Fixture fixture;
    EXPECT_FALSE(fixture.state.view.cutaway.has_value());
    fixture.run("cutaway 6.2");
    ASSERT_TRUE(fixture.state.view.cutaway.has_value());
    EXPECT_FLOAT_EQ(*fixture.state.view.cutaway, 6.2f);
    fixture.run("cutaway off");
    EXPECT_FALSE(fixture.state.view.cutaway.has_value());
    fixture.queue.add("cutaway", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
}

TEST(GameCommandsTest, SwitchesChangeTheView) {
    Fixture fixture;
    EXPECT_FALSE(fixture.state.view.arrow);
    EXPECT_TRUE(fixture.state.view.lighting);
    EXPECT_TRUE(fixture.state.view.shadows);
    EXPECT_TRUE(fixture.state.view.crosshair);
    fixture.run("arrow on; lighting off; shadows off; crosshair off");
    EXPECT_TRUE(fixture.state.view.arrow);
    EXPECT_FALSE(fixture.state.view.lighting);
    EXPECT_FALSE(fixture.state.view.shadows);
    EXPECT_FALSE(fixture.state.view.crosshair);
    for (const char* command : {"arrow maybe", "arrow on off"}) {
        fixture.queue.add(command, true);
        EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError) << command;
    }
}

TEST(GameCommandsTest, SwitchesToggleWithoutArgument) {
    Fixture fixture;
    fixture.run("arrow; lighting; shadows; imgui");
    EXPECT_TRUE(fixture.state.view.arrow);
    EXPECT_FALSE(fixture.state.view.lighting);
    EXPECT_FALSE(fixture.state.view.shadows);
    EXPECT_TRUE(fixture.state.debug_overlay);
    fixture.run("arrow; lighting; shadows; imgui");
    EXPECT_FALSE(fixture.state.view.arrow);
    EXPECT_TRUE(fixture.state.view.lighting);
    EXPECT_TRUE(fixture.state.view.shadows);
    EXPECT_FALSE(fixture.state.debug_overlay);
    fixture.run("imgui on; imgui on");
    EXPECT_TRUE(fixture.state.debug_overlay);
    fixture.run("imgui off");
    EXPECT_FALSE(fixture.state.debug_overlay);
}

TEST(GameCommandsTest, ScreenshotsKeepTheViewOfTheirTime) {
    Fixture fixture;
    fixture.run("camera 0 0 20 0 -90; screenshot top.png 800 450; camera player; arrow on; screenshot eyes.png");
    ASSERT_EQ(fixture.state.screenshots.size(), 2U);
    const ScreenshotRequest& top = fixture.state.screenshots[0];
    EXPECT_EQ(top.path, "top.png");
    ASSERT_TRUE(top.size.has_value());
    EXPECT_EQ(top.size->width, 800);
    EXPECT_EQ(top.size->height, 450);
    ASSERT_TRUE(top.view.camera.has_value());
    EXPECT_FALSE(top.view.arrow);
    const ScreenshotRequest& eyes = fixture.state.screenshots[1];
    EXPECT_FALSE(eyes.size.has_value());
    EXPECT_FALSE(eyes.view.camera.has_value());
    EXPECT_TRUE(eyes.view.arrow);
    EXPECT_TRUE(top.stop_on_error);  // Added by run(), like --exec.

    fixture.queue.add("screenshot typed.png", false);
    fixture.queue.run_frame(fixture.console);
    EXPECT_FALSE(fixture.state.screenshots.back().stop_on_error);
}

TEST(GameCommandsTest, ScreenshotRejectsWrongSizes) {
    Fixture fixture;
    for (const char* command :
         {"screenshot", "screenshot a.png 800", "screenshot a.png 0 450", "screenshot a.png 800 100000"}) {
        fixture.queue.add(command, true);
        EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError) << command;
    }
    EXPECT_TRUE(fixture.state.screenshots.empty());
}

TEST(GameCommandsTest, WaitDelaysTheNextCommandsAndQuitAsksToQuit) {
    Fixture fixture;
    fixture.queue.add("wait 1; quit; arrow on", true);
    fixture.queue.run_frame(fixture.console);
    EXPECT_FALSE(fixture.state.quit);
    fixture.queue.run_frame(fixture.console);
    EXPECT_TRUE(fixture.state.quit);
    EXPECT_FALSE(fixture.state.view.arrow);  // Commands after quit don't run.
    EXPECT_EQ(fixture.queue.size(), 0U);
    fixture.queue.add("wait -1", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
}

TEST(GameCommandsTest, LevelInfoTakesNoArguments) {
    Fixture fixture;
    EXPECT_NO_THROW(fixture.run("level_info"));
    fixture.queue.add("level_info now", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
}

TEST(GameCommandsTest, DoorOpensAndClosesDoorsByNumber) {
    Fixture fixture(Game(make_config(), make_door_level()));
    EXPECT_NO_THROW(fixture.run("doors"));
    fixture.run("door 0 open");
    EXPECT_FLOAT_EQ(fixture.game.doors()[0].target, 1.0f);
    fixture.expect_error("door 0 close");  // Still opening.
    fixture.game.update({}, Door::kMoveTime);
    fixture.game.teleport({0.0f, 0.0f, 0.0f});  // In the doorway.
    fixture.queue.add("door 0 close", true);
    try {
        fixture.queue.run_frame(fixture.console);
        ADD_FAILURE() << "door 0 close should fail while the player stands in the door";
    } catch (const CommandError& error) {
        EXPECT_TRUE(std::string(error.what()).ends_with("the player stands in it")) << error.what();
    }
    fixture.game.teleport({0.0f, -1.9f, 0.0f});
    fixture.run("door 0 close");
    EXPECT_FLOAT_EQ(fixture.game.doors()[0].target, 0.0f);
    for (const char* command : {"door 1 open", "door -1 open", "door 0 shut", "door 0", "door x open", "doors 0"}) {
        fixture.expect_error(command);
    }
}

TEST(GameCommandsTest, ActivateActsLikeTheKey) {
    Fixture fixture(Game(make_config(), make_door_level()));
    fixture.run("activate");
    EXPECT_FLOAT_EQ(fixture.game.doors()[0].target, 1.0f);
    EXPECT_NO_THROW(fixture.run("activate"));  // Ignored while the door moves.
    EXPECT_FLOAT_EQ(fixture.game.doors()[0].target, 1.0f);
    fixture.expect_error("activate now");
}

}  // namespace
}  // namespace manifold
