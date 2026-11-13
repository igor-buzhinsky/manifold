#include "console/game_commands.hpp"

#include <cstddef>
#include <format>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "console/console.hpp"
#include "game.hpp"
#include "level/layout_factory.hpp"
#include "render/camera.hpp"
#include "render/viewport.hpp"
#include "util/log.hpp"
#include "world/player.hpp"

namespace manifold {

namespace {

float to_degrees(float radians) { return radians * 180.0f / std::numbers::pi_v<float>; }
float to_radians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0f; }

/// Throws CommandError with the usage unless there are `count` arguments.
void expect_count(std::span<const std::string> arguments, std::size_t count, const std::string& usage) {
    if (arguments.size() != count) {
        throw CommandError("usage: " + usage);
    }
}

/// Adds a command that turns a setting on or off.
void add_switch(Console& console, const std::string& name, const std::string& help, bool& setting) {
    const std::string usage = name + " on|off";
    console.add(name, usage, help, [&setting, usage](std::span<const std::string> arguments) {
        expect_count(arguments, 1, usage);
        setting = parse_switch_argument(arguments[0]);
    });
}

/// Parses a screenshot size in pixels.
int parse_screenshot_size(const std::string& text) {
    const int size = parse_int_argument(text);
    if (size < 1 || size > kMaxScreenshotSize) {
        throw CommandError(std::format("screenshot size must be from 1 to {}: {}", kMaxScreenshotSize, text));
    }
    return size;
}

}  // namespace

Camera view_camera(const ViewSettings& view, const Player& player) {
    return view.camera.value_or(make_player_camera(player));
}

void add_game_commands(Console& console, CommandQueue& queue, const Game& game, ConsoleState& state) {
    console.add("level_info", "level_info", "Describes the level, depending on its layout, and the player.",
                [&game](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "level_info");
                    const Player& player = game.player();
                    log_info("Level seed {}, layout {}:\n{}Player at ({:.2f}, {:.2f}, {:.2f}), yaw {:.1f} deg",
                             game.seed(), layout_name(game.level().layout), game.level().info, player.position().x,
                             player.position().y, player.position().z, to_degrees(player.yaw()));
                });

    const std::string camera_usage = "camera [X Y Z YAW PITCH | player]";
    console.add("camera", camera_usage,
                "Shows where the camera is, puts it in place (angles in degrees), or back at the player's eyes.",
                [&game, &state, camera_usage](std::span<const std::string> arguments) {
                    if (arguments.empty()) {
                        const Camera camera = view_camera(state.view, game.player());
                        log_info("Camera {}at ({:.2f}, {:.2f}, {:.2f}), yaw {:.1f} deg, pitch {:.1f} deg",
                                 state.view.camera ? "" : "of the player ", camera.position.x, camera.position.y,
                                 camera.position.z, to_degrees(camera.yaw), to_degrees(camera.pitch));
                    } else if (arguments.size() == 1 && arguments[0] == "player") {
                        state.view.camera.reset();
                    } else {
                        expect_count(arguments, 5, camera_usage);
                        const float pitch = parse_float_argument(arguments[4]);
                        if (pitch < -90.0f || pitch > 90.0f) {
                            throw CommandError("pitch must be from -90 to 90 degrees: " + arguments[4]);
                        }
                        state.view.camera =
                            Camera{.position = {parse_float_argument(arguments[0]), parse_float_argument(arguments[1]),
                                                parse_float_argument(arguments[2])},
                                   .yaw = to_radians(parse_float_argument(arguments[3])),
                                   .pitch = to_radians(pitch)};
                    }
                });

    add_switch(console, "arrow",
               "Shows an arrow at the player's feet, in the direction the player faces, on top of "
               "everything.",
               state.view.arrow);
    add_switch(console, "lighting", "Turns lights on, or off to show the plain texture colors.", state.view.lighting);
    add_switch(console, "shadows", "Turns shadows on or off.", state.view.shadows);

    const std::string screenshot_usage = "screenshot FILE [WIDTH HEIGHT]";
    console.add("screenshot", screenshot_usage,
                "Saves the view as a PNG file after this frame, by default as large as the window.",
                [&queue, &state, screenshot_usage](std::span<const std::string> arguments) {
                    if (arguments.size() != 1) {
                        expect_count(arguments, 3, screenshot_usage);
                    }
                    ScreenshotRequest request{.path = arguments[0],
                                              .size = std::nullopt,
                                              .view = state.view,
                                              .stop_on_error = queue.running_stops_on_error()};
                    if (arguments.size() == 3) {
                        request.size = PixelSize{.width = parse_screenshot_size(arguments[1]),
                                                 .height = parse_screenshot_size(arguments[2])};
                    }
                    state.screenshots.push_back(std::move(request));
                });

    console.add("wait", "wait FRAMES", "Runs the next commands after this many frames.",
                [&queue](std::span<const std::string> arguments) {
                    expect_count(arguments, 1, "wait FRAMES");
                    const int frames = parse_int_argument(arguments[0]);
                    if (frames < 0) {
                        throw CommandError("frames must not be negative: " + arguments[0]);
                    }
                    queue.wait(frames);
                });

    console.add("quit", "quit", "Quits the game after this frame, without running the commands after it.",
                [&queue, &state](std::span<const std::string> arguments) {
                    expect_count(arguments, 0, "quit");
                    state.quit = true;
                    if (const std::size_t dropped = queue.clear(); dropped > 0) {
                        log_warning("Quitting without running the {} command(s) after quit", dropped);
                    }
                });
}

}  // namespace manifold
