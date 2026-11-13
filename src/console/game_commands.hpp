#pragma once

#include <optional>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "game.hpp"
#include "render/camera.hpp"
#include "render/viewport.hpp"
#include "world/player.hpp"

namespace manifold {

/// How the game is shown, as console commands set it.
struct ViewSettings {
    std::optional<Camera> camera;  ///< A camera that stays in place, or none to see through the player's eyes.
    bool arrow = false;            ///< Whether an arrow at the player's feet shows where the player is and faces.
    bool lighting = true;          ///< If false, surfaces have their plain texture colors, without lights or shadows.
    bool shadows = true;           ///< Whether lights cast shadows.
};

/// The camera that `view` looks through.
Camera view_camera(const ViewSettings& view, const Player& player);

/// A screenshot that a console command asked for.
struct ScreenshotRequest {
    std::string path;               ///< PNG file to write.
    std::optional<PixelSize> size;  ///< Size of the picture, or none for the size of the window.
    ViewSettings view;              ///< How the game was shown when the screenshot was asked for.
    /// Whether failing to save it should stop the game, as it does for the commands of `--exec`, or only be
    /// logged, see CommandQueue::running_stops_on_error().
    bool stop_on_error = false;
};

/// What console commands change, and what they ask the main loop to do.
struct ConsoleState {
    ViewSettings view;                           ///< How the game is shown.
    std::vector<ScreenshotRequest> screenshots;  ///< To take after this frame is drawn, in order.
    bool quit = false;                           ///< Whether to quit after this frame.
};

/// Largest width and height of a screenshot, in pixels.
inline constexpr int kMaxScreenshotSize = 8192;

/// Adds the commands that inspect and show the game, see GAME_MANUAL.md for the list.
///
/// Angles in commands are in degrees, which are easier to type.
///
/// \param console Where to add the commands.
/// \param queue   The queue that `wait` delays.
/// \param game    The game the commands inspect. It must outlive the console.
/// \param state   What the commands change. It must outlive the console.
void add_game_commands(Console& console, CommandQueue& queue, const Game& game, ConsoleState& state);

}  // namespace manifold
