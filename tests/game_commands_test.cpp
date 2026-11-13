#include "console/game_commands.hpp"

#include <numbers>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "console/console.hpp"
#include "game.hpp"
#include "level/level.hpp"
#include "render/camera.hpp"

namespace manifold {
namespace {

/// A game in an empty debug level with the game commands.
struct Fixture {
    Game game{[] {
                  GameConfig config;
                  config.level.debug.obstacle_count = 0;
                  config.level.debug.ramp_count = 0;
                  return config;
              }(),
              1, LayoutKind::kDebug};
    Console console;
    CommandQueue queue;
    ConsoleState state;

    Fixture() { add_game_commands(console, queue, game, state); }

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

TEST(GameCommandsTest, SwitchesChangeTheView) {
    Fixture fixture;
    EXPECT_FALSE(fixture.state.view.arrow);
    EXPECT_TRUE(fixture.state.view.lighting);
    EXPECT_TRUE(fixture.state.view.shadows);
    fixture.run("arrow on; lighting off; shadows off");
    EXPECT_TRUE(fixture.state.view.arrow);
    EXPECT_FALSE(fixture.state.view.lighting);
    EXPECT_FALSE(fixture.state.view.shadows);
    fixture.queue.add("arrow", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
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
    fixture.queue.add("wait 1; quit", true);
    fixture.queue.run_frame(fixture.console);
    fixture.queue.run_frame(fixture.console);
    EXPECT_FALSE(fixture.state.quit);
    fixture.queue.run_frame(fixture.console);
    EXPECT_TRUE(fixture.state.quit);
    fixture.queue.add("wait -1", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
}

TEST(GameCommandsTest, LevelInfoTakesNoArguments) {
    Fixture fixture;
    EXPECT_NO_THROW(fixture.run("level_info"));
    fixture.queue.add("level_info now", true);
    EXPECT_THROW(fixture.queue.run_frame(fixture.console), CommandError);
}

}  // namespace
}  // namespace manifold
