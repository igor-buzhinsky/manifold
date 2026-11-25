#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "console/console.hpp"
#include "game.hpp"
#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/shadow_map.hpp"
#include "render/viewport.hpp"
#include "world/player.hpp"

namespace manifold {

/// How the game is shown, as console commands and the debug overlay set it.
struct ViewSettings {
    std::optional<Camera> camera;  ///< A camera that stays in place, or none to see through the player's eyes.
    bool arrow = false;            ///< Whether an arrow at the player's feet shows where the player is and faces.
    /// Whether a crosshair shows the center of the player's view. It is hidden while `camera` is set.
    bool crosshair = true;
    bool lighting = true;  ///< If false, surfaces have their plain texture colors, without lights or shadows.
    bool shadows = true;   ///< Whether lights cast shadows.
    /// If set, everything above this height is left out, so that a camera above can see into a layer.
    std::optional<float> cutaway;
    /// The lights, which only the debug overlay changes, except that each level sets the ambient light.
    LightSettings lights;
    /// The ambient light of the level, see level_ambient(), which resetting the lights in the debug overlay
    /// puts back.
    glm::vec3 level_ambient{0.0f};
    float shadow_bias = kShadowBias;  ///< See kShadowBias. Only the debug overlay changes it.
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

/// Which menu the `menu` command asks the main loop to show, see ConsoleState::menu.
enum class MenuRequest : std::uint8_t {
    kStart,  ///< The menu shown at the start when there is a save, see make_start_menu().
    kGame,   ///< The game menu that Escape opens, see make_game_menu().
    kClose,  ///< No menu.
};

/// What console commands change, and what they ask the main loop to do.
struct ConsoleState {
    ViewSettings view;                           ///< How the game is shown.
    std::vector<ScreenshotRequest> screenshots;  ///< To take after this frame is drawn, in order.
    bool quit = false;                           ///< Whether to quit after this frame.
    bool debug_overlay = false;                  ///< Whether the debug overlay is shown.
    /// For how many more frames the player walks forward as if W were held, see the `walk` and `run` commands. The
    /// main loop counts it down.
    int walk_frames = 0;
    /// Whether the player runs during walk_frames, as if Shift were held too, see the `run` command.
    bool walk_runs = false;
    /// Whether to load the saved game at the start of the next frame, as the load key (F9) does and the `load` command
    /// asks. The main loop does it and resets this.
    bool load = false;
    /// Whether failing to load should stop the game, as it does for the commands of `--exec`, or only be logged.
    bool load_stops_on_error = false;
    /// The menu to show instead of those that are open, as the `menu` command asks, or nothing to leave them. The main
    /// loop does it at the start of the next frame and resets this.
    std::optional<MenuRequest> menu;
};

/// Largest width and height of a screenshot, in pixels.
inline constexpr int kMaxScreenshotSize = 8192;

/// Adds the commands that inspect and show the game, see DEBUGGING.md for the list.
///
/// Angles in commands are in degrees, which are easier to type.
///
/// \param console Where to add the commands.
/// \param queue   The queue that `wait` delays.
/// \param game    The game the commands inspect and change. It must outlive the console.
/// \param state   What the commands change. It must outlive the console.
void add_game_commands(Console& console, CommandQueue& queue, Game& game, ConsoleState& state);

}  // namespace manifold
