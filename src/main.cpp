#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#include <glm/vec3.hpp>

#include "console/console.hpp"
#include "console/console_window.hpp"
#include "console/game_commands.hpp"
#include "game.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "platform/terminal_input.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/marker.hpp"
#include "render/overlay_renderer.hpp"
#include "render/renderer.hpp"
#include "render/screenshot.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "ui/crosshair.hpp"
#include "ui/debug_overlay.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/text_field.hpp"
#include "util/frame_times.hpp"
#include "util/log.hpp"
#include "world/torch.hpp"

namespace {

/// Width of the window in screen units. The height follows from the 16:9 aspect ratio.
constexpr int kWindowWidth = 1600;

/// Command-line options.
struct Options {
    std::optional<std::uint64_t> seed;           ///< Level seed. Random if not given.
    std::string commands;                        ///< Console commands to run at the start. Errors stop the game.
    bool quit = false;                           ///< Whether to add `quit` after the commands.
    std::optional<manifold::LayoutKind> layout;  ///< Level layout. Random if not given.
};

/// Parses a level seed: a non-negative decimal integer.
///
/// \throws std::invalid_argument if `text` is not a valid seed.
std::uint64_t parse_seed(const std::string& text) {
    std::uint64_t seed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("invalid seed: " + text);
    }
    return seed;
}

/// Parses the command line: `manifold [SEED] [--layout debug|rooms] [--exec COMMANDS]... [--screenshot FILE.png]`.
///
/// `--screenshot FILE.png` is short for `--exec "screenshot FILE.png"`, with `quit` after all commands.
/// Commands run in the order of the options.
///
/// \param args All arguments, including the program name.
/// \throws std::invalid_argument if an argument is invalid.
Options parse_options(std::span<char*> args) {
    Options options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string arg = args[i];
        if (arg == "--screenshot" || arg == "--exec" || arg == "--layout") {
            if (i + 1 == args.size()) {
                throw std::invalid_argument(arg + " needs a value");
            }
            const std::string value = args[++i];
            if (arg == "--screenshot") {
                options.commands += "\nscreenshot " + value;
                options.quit = true;
            } else if (arg == "--exec") {
                options.commands += "\n" + value;
            } else {
                options.layout = manifold::parse_layout_name(value);
                if (!options.layout) {
                    throw std::invalid_argument("unknown layout: " + value);
                }
            }
        } else {
            options.seed = parse_seed(arg);
        }
    }
    if (options.quit) {
        options.commands += "\nquit";
    }
    return options;
}

/// Creates an unpredictable level seed.
std::uint64_t random_seed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32U) | device();
}

/// Directory searched for level textures, set by CMake to `assets/textures` in the source tree.
constexpr const char* kTexturesDir = MANIFOLD_TEXTURES_DIR;

/// Picks the texture of the level's surfaces at random, see manifold::pick_texture().
///
/// The choice depends only on the level seed, so a seed always gives the same look. If no texture can be
/// used, a plain grey one is used instead.
manifold::Image pick_level_texture(std::uint64_t level_seed) {
    // Mixing the seed keeps the choice independent of the random numbers used by level generation.
    constexpr std::uint64_t kTextureSeedSalt = 0x9e3779b97f4a7c15ULL;
    const std::vector<std::filesystem::path> candidates = manifold::find_textures(kTexturesDir);
    std::optional<manifold::PickedTexture> picked = manifold::pick_texture(candidates, level_seed ^ kTextureSeedSalt);
    if (!picked) {
        manifold::log_warning("No usable texture found among {} PNG files in {}, using plain grey", candidates.size(),
                              kTexturesDir);
        return manifold::fallback_texture();
    }
    return std::move(picked->image);
}

/// Makes the renderer of the game's current level, and sets the level's ambient light in `view`.
void start_level(std::optional<manifold::Renderer>& renderer, const manifold::Game& game,
                 manifold::ViewSettings& view) {
    const manifold::Image texture = pick_level_texture(game.seed());
    view.level_ambient = manifold::level_ambient(game.seed(), manifold::mean_linear_color(texture));
    view.lights.ambient = view.level_ambient;
    renderer.emplace(game.world(), texture);
}

/// Logs the seed, the level and the player.
void log_game(const manifold::Game& game) {
    const auto& bounds = game.world().bounds();
    const auto& player = game.player();
    manifold::log_info("Manifold, level {}, seed {}, layout {}", game.level_number(), game.seed(),
                       manifold::layout_name(game.level().layout));
    manifold::log_info(
        "World: x [{}, {}], y [{}, {}], height {} m, gravity {} m/s^2, {} obstacles, {} ramps, {} torches",
        bounds.x_min, bounds.x_max, bounds.y_min, bounds.y_max, bounds.height, game.level().gravity,
        game.world().obstacles().size(), game.world().ramps().size(), game.world().torches().size());
    if (!game.world().torches().empty()) {
        manifold::log_info("Torches: {} flames", manifold::torch_flame_name(game.world().torches()[0].flame));
    }
    manifold::log_info("Player: position ({:.2f}, {:.2f}, {:.2f}), radius {} m, height {} m", player.position().x,
                       player.position().y, player.position().z, player.radius(), player.height());
    manifold::log_info("Level info:\n{}", game.level().info);
}

/// The key that opens and closes the console: the one left of 1, labelled `/~ on US keyboards.
constexpr SDL_Scancode kConsoleKey = SDL_SCANCODE_GRAVE;
/// The key that shows and hides the debug overlay.
constexpr SDL_Keycode kDebugOverlayKey = SDLK_F3;

/// The key of the console window that `key` is, if any.
std::optional<manifold::ConsoleKey> console_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return manifold::ConsoleKey::kSubmit;
        case SDLK_UP:
            return manifold::ConsoleKey::kHistoryBack;
        case SDLK_DOWN:
            return manifold::ConsoleKey::kHistoryForward;
        case SDLK_PAGEUP:
            return manifold::ConsoleKey::kScrollUp;
        case SDLK_PAGEDOWN:
            return manifold::ConsoleKey::kScrollDown;
        default:
            return std::nullopt;
    }
}

/// The key that edits a text field that `key` is, if any.
std::optional<manifold::EditKey> edit_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_BACKSPACE:
            return manifold::EditKey::kBackspace;
        case SDLK_DELETE:
            return manifold::EditKey::kDelete;
        case SDLK_LEFT:
            return manifold::EditKey::kLeft;
        case SDLK_RIGHT:
            return manifold::EditKey::kRight;
        case SDLK_HOME:
            return manifold::EditKey::kHome;
        case SDLK_END:
            return manifold::EditKey::kEnd;
        default:
            return std::nullopt;
    }
}

/// Opens or closes the console window, with text input only while it is open.
void set_console_open(manifold::ConsoleWindow& console_window, manifold::Window& window, bool open) {
    console_window.set_open(open);
    window.set_text_input(open);
}

/// Handles a key press while the console window is open: editing, history, scrolling and running commands.
void handle_console_key(const SDL_KeyboardEvent& key, manifold::ConsoleWindow& console_window,
                        manifold::CommandQueue& commands) {
    if (const std::optional<manifold::EditKey> edit = edit_key(key.key)) {
        console_window.input().apply(*edit);
    } else if (const std::optional<manifold::ConsoleKey> console = console_key(key.key)) {
        if (const std::optional<std::string> command = console_window.handle(*console)) {
            commands.add(*command, false);
        }
    }
}

/// Handles a key press: the console key, Escape, and the keys of the console window while it is open.
///
/// \return False when the game should quit.
bool handle_key_down(const SDL_KeyboardEvent& key, manifold::Window& window, manifold::ConsoleWindow& console_window,
                     manifold::CommandQueue& commands) {
    if (key.scancode == kConsoleKey) {
        if (!key.repeat) {
            set_console_open(console_window, window, !console_window.is_open());
        }
    } else if (key.key == SDLK_ESCAPE) {
        if (!key.repeat) {
            if (!console_window.is_open()) {
                return false;
            }
            set_console_open(console_window, window, false);
        }
    } else if (console_window.is_open()) {
        handle_console_key(key, console_window, commands);
    }
    return true;
}

/// Handles pending window and key events: quitting, the console window and the debug overlay.
///
/// These are key bindings, like those of read_input(). Keep GAME_MANUAL.md up to date when changing them.
///
/// \param window             The window, whose text input is on while the console window is open.
/// \param console_window     The console window.
/// \param commands           Where commands run in the console window go.
/// \param overlay            The debug overlay, which gets the events first and may keep keys from the game.
/// \param state              Where the debug overlay key shows or hides the overlay.
/// \param console_key_held   Whether the console key is held down. Kept between calls.
/// \return False when the game should quit.
bool handle_events(manifold::Window& window, manifold::ConsoleWindow& console_window, manifold::CommandQueue& commands,
                   manifold::DebugOverlay& overlay, manifold::ConsoleState& state, bool& console_key_held) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        // The debug overlay key works even while ImGui has the keyboard, so the overlay can always be hidden.
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == kDebugOverlayKey) {
            if (!event.key.repeat) {
                state.debug_overlay = !state.debug_overlay;
            }
            continue;
        }
        if (overlay.handle_event(event)) {
            continue;
        }
        if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.scancode == kConsoleKey) {
            console_key_held = event.type == SDL_EVENT_KEY_DOWN;
        }
        if (event.type == SDL_EVENT_TEXT_INPUT && console_window.is_open()) {
            // The console key types a character, which depends on the keyboard layout. It would land in the
            // input line, so text is ignored while the key is held.
            if (!console_key_held) {
                console_window.input().insert(event.text.text);
            }
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !handle_key_down(event.key, window, console_window, commands)) {
            return false;
        }
    }
    return true;
}

/// Frames longer than this, e.g. after the window was dragged, count as this long to avoid jumps.
constexpr float kMaxFrameTime = 0.1f;

/// Reads the keys that are held down.
///
/// These are the key bindings, with those of handle_events(). Keep GAME_MANUAL.md up to date when changing
/// them.
manifold::PlayerInput read_input() {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const auto axis = [keys](SDL_Scancode positive, SDL_Scancode negative) {
        return (keys[positive] ? 1.0f : 0.0f) - (keys[negative] ? 1.0f : 0.0f);
    };
    return {
        .turn = axis(SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT),
        .look = axis(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN),
        .forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S),
        .strafe = axis(SDL_SCANCODE_D, SDL_SCANCODE_A),
        .rise = axis(SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN),
        .activate = keys[SDL_SCANCODE_E],
    };
}

/// Draws the game into the bound framebuffer, as `view` shows it, with the crosshair if the view has it.
///
/// \param renderer         Draws the world.
/// \param overlay_renderer Draws the crosshair.
/// \param game             What to draw.
/// \param view             How to show it.
/// \param framebuffer      Size of the bound framebuffer. The picture is centered in it, see fit_viewport().
void draw_game(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
               const manifold::Game& game, const manifold::ViewSettings& view, const manifold::PixelSize& framebuffer) {
    // The light spell at the player's eyes, the level's ambient light if any, and the lights of the torches and the
    // exit nearest to the camera.
    const manifold::Camera camera = manifold::view_camera(view, game.player());
    manifold::Lighting lighting =
        manifold::scene_lighting(view.lights, game.player().eye_position(), game.world().torches(), game.world().exit(),
                                 game.animation_time(), camera.position);
    if (!view.shadows) {
        for (manifold::PointLight& light : lighting.lights) {
            light.shadow_id.reset();
        }
    }

    const std::vector<glm::vec3> arrow =
        view.arrow ? manifold::build_arrow(game.player().position(), game.player().yaw()) : std::vector<glm::vec3>();
    const manifold::Viewport viewport = manifold::fit_viewport(framebuffer, manifold::kAspectRatio);
    renderer.draw(
        camera, lighting, viewport, game.animation_time(), game.doors(),
        {.lighting = view.lighting, .marker = arrow, .cut_height = view.cutaway, .shadow_bias = view.shadow_bias});
    // The crosshair shows where the player looks, which a camera put in place doesn't.
    if (view.crosshair && !view.camera) {
        manifold::OverlayMesh crosshair;
        manifold::add_crosshair(crosshair, {.width = viewport.width, .height = viewport.height});
        overlay_renderer.draw(crosshair, viewport);
    }
}

/// Takes the screenshots that console commands asked for, each drawn as it asked.
///
/// A screenshot that fails is logged, and stops the game only if its request says so.
///
/// \param window_size The size of screenshots that don't give their own.
/// \throws std::runtime_error if a screenshot that stops on error fails.
void take_screenshots(manifold::Renderer& renderer, const manifold::OverlayRenderer& overlay_renderer,
                      const manifold::Game& game, std::span<const manifold::ScreenshotRequest> requests,
                      const manifold::PixelSize& window_size) {
    for (const manifold::ScreenshotRequest& request : requests) {
        const manifold::PixelSize size = request.size.value_or(window_size);
        try {
            const manifold::OffscreenFramebuffer framebuffer(size);
            draw_game(renderer, overlay_renderer, game, request.view, size);
            manifold::save_screenshot(request.path, size.width, size.height);
            manifold::log_info("Saved {} ({} x {})", request.path, size.width, size.height);
        } catch (const std::runtime_error& error) {
            if (request.stop_on_error) {
                throw;
            }
            manifold::log_error("screenshot {}: {}", request.path, error.what());
        }
    }
}

/// Removes the log listener when it goes out of scope, before what the listener uses is gone.
struct LogListenerReset {
    LogListenerReset() = default;
    ~LogListenerReset() { manifold::set_log_listener({}); }
    LogListenerReset(const LogListenerReset&) = delete;
    LogListenerReset& operator=(const LogListenerReset&) = delete;
    LogListenerReset(LogListenerReset&&) = delete;
    LogListenerReset& operator=(LogListenerReset&&) = delete;
};

/// Runs the game until the player quits or a console command does.
///
/// \throws manifold::CommandError if one of the commands of `options` fails.
/// \throws std::runtime_error if a level can't be generated, also the next one when the player reaches the exit.
void run(const Options& options) {
    // The console window shows the log, from the start.
    manifold::ConsoleWindow console_window;
    manifold::set_log_listener([&console_window](manifold::LogLevel level, std::string_view message) {
        console_window.add_output(level, message);
    });
    const LogListenerReset reset_log_listener;

    manifold::Game game(manifold::GameConfig{}, options.seed.value_or(random_seed()), options.layout);
    log_game(game);

    const auto window_height = static_cast<int>(std::lround(kWindowWidth / manifold::kAspectRatio));
    manifold::Window window("Manifold", kWindowWidth, window_height);
    // The renderer has the meshes of one level, so each level gets a new one.
    manifold::ConsoleState state;
    std::optional<manifold::Renderer> renderer;
    start_level(renderer, game, state.view);
    int rendered_level = game.level_number();
    const manifold::OverlayRenderer overlay_renderer;

    manifold::Console console;
    manifold::CommandQueue commands;
    manifold::add_game_commands(console, commands, game, state);
    commands.add(options.commands, true);
    manifold::TerminalInput terminal;
    manifold::DebugOverlay overlay(window);
    manifold::FrameTimes frame_times;

    bool console_key_held = false;
    std::uint64_t previous_ticks = SDL_GetTicksNS();
    do {
        for (const std::string& line : terminal.read_lines()) {
            commands.add(line, false);
        }
        commands.run_frame(console);

        const std::uint64_t ticks = SDL_GetTicksNS();
        const float frame_time = static_cast<float>(ticks - previous_ticks) * 1e-9f;
        previous_ticks = ticks;
        frame_times.add(frame_time);
        // While the console window is open, or the debug overlay has the keyboard, the keys type into them
        // instead of moving the player.
        const bool keys_taken = console_window.is_open() || overlay.wants_keyboard();
        game.update(keys_taken ? manifold::PlayerInput{} : read_input(), std::min(frame_time, kMaxFrameTime));
        // The player reached the exit, or a console command started the next level.
        if (game.level_number() != rendered_level) {
            log_game(game);
            start_level(renderer, game, state.view);
            rendered_level = game.level_number();
        }

        const manifold::PixelSize window_size = window.framebuffer_size();
        draw_game(*renderer, overlay_renderer, game, state.view, window_size);
        // The stats of this draw, before screenshots draw again.
        const manifold::RenderStats render_stats = renderer->stats();
        if (state.debug_overlay) {
            overlay.draw(game, state.view, render_stats, frame_times);
        }
        if (console_window.is_open()) {
            const manifold::Viewport viewport = manifold::fit_viewport(window_size, manifold::kAspectRatio);
            overlay_renderer.draw(console_window.build_mesh({.width = viewport.width, .height = viewport.height}),
                                  viewport);
        }
        take_screenshots(*renderer, overlay_renderer, game, state.screenshots, window_size);
        state.screenshots.clear();
        if (state.quit) {
            return;
        }
        window.swap_buffers();
        // The console window has the keys while it is open, so the overlay gets no events then.
        overlay.set_input(state.debug_overlay && !console_window.is_open());
    } while (handle_events(window, console_window, commands, overlay, state, console_key_held));
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        run(parse_options(std::span(argv, argc)));
    } catch (const std::exception& error) {
        manifold::log_error("{}", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
