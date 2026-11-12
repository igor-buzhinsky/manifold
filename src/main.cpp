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
#include <system_error>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#include <glm/vec3.hpp>

#include "game.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/renderer.hpp"
#include "render/screenshot.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"
#include "util/log.hpp"

namespace {

/// Width of the window in screen units. The height follows from the 16:9 aspect ratio.
constexpr int kWindowWidth = 1600;

/// Command-line options.
struct Options {
    std::optional<std::uint64_t> seed;           ///< Level seed. Random if not given.
    std::optional<std::string> screenshot_path;  ///< If given, save the first frame there and quit.
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

/// Parses the command line: `manifold [SEED] [--layout debug|rooms] [--screenshot FILE.png]`.
///
/// \param args All arguments, including the program name.
/// \throws std::invalid_argument if an argument is invalid.
Options parse_options(std::span<char*> args) {
    Options options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string arg = args[i];
        if (arg == "--screenshot" || arg == "--layout") {
            if (i + 1 == args.size()) {
                throw std::invalid_argument(arg + " needs a value");
            }
            const std::string value = args[++i];
            if (arg == "--screenshot") {
                options.screenshot_path = value;
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

/// Logs the seed, the level and the player.
void log_game(const manifold::Game& game) {
    const auto& bounds = game.world().bounds();
    const auto& player = game.player();
    manifold::log_info("Manifold, level seed {}, layout {}", game.seed(), manifold::layout_name(game.level().layout));
    manifold::log_info(
        "World: x [{}, {}], y [{}, {}], height {} m, gravity {} m/s^2, {} obstacles, {} ramps, {} torches",
        bounds.x_min, bounds.x_max, bounds.y_min, bounds.y_max, bounds.height, game.level().gravity,
        game.world().obstacles().size(), game.world().ramps().size(), game.world().torches().size());
    manifold::log_info("Player: position ({:.2f}, {:.2f}, {:.2f}), radius {} m, height {} m", player.position().x,
                       player.position().y, player.position().z, player.radius(), player.height());
    manifold::log_info("Level plan:\n{}", game.level().plan);
}

/// Handles pending window and key events, such as quitting and cheats.
///
/// \return False when the game should quit.
bool handle_events(manifold::Game& game) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
            continue;
        }
        if (event.key.key == SDLK_ESCAPE) {
            return false;
        }
        if (event.key.key == SDLK_Z && (event.key.mod & SDL_KMOD_CTRL) != 0) {
            game.toggle_noclip();
            manifold::log_info("Noclip {}", game.noclip() ? "on" : "off");
        }
    }
    return true;
}

/// Frames longer than this, e.g. after the window was dragged, count as this long to avoid jumps.
constexpr float kMaxFrameTime = 0.1f;

/// Reads the keys that are held down.
///
/// These are the key bindings. Keep GAME_MANUAL.md up to date when changing them.
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
    };
}

/// Runs the game until the player quits, or until the screenshot is saved.
void run(const Options& options) {
    manifold::Game game(manifold::GameConfig{}, options.seed.value_or(random_seed()), options.layout);
    log_game(game);

    const auto window_height = static_cast<int>(std::lround(kWindowWidth / manifold::kAspectRatio));
    manifold::Window window("Manifold", kWindowWidth, window_height);
    const manifold::Renderer renderer(manifold::build_world_mesh(game.world()), pick_level_texture(game.seed()),
                                      game.world().torches());

    std::uint64_t previous_ticks = SDL_GetTicksNS();
    do {
        const std::uint64_t ticks = SDL_GetTicksNS();
        const float dt = std::min(static_cast<float>(ticks - previous_ticks) * 1e-9f, kMaxFrameTime);
        previous_ticks = ticks;
        game.update(read_input(), dt);

        // The light spell: a warm light at the player's eyes, plus a little ambient light. The torches
        // nearest to the player fill up the remaining lights.
        const manifold::Camera camera = manifold::make_player_camera(game.player());
        manifold::Lighting lighting{
            .ambient = glm::vec3(0.01f),
            .lights = {{.position = camera.position, .color = {1.0f, 0.85f, 0.6f}, .intensity = 5.0f, .range = 15.0f}},
        };
        const std::vector<manifold::PointLight> torch_lights =
            manifold::select_lights(manifold::torch_lights(game.world().torches(), game.animation_time()),
                                    camera.position, manifold::kMaxLights - 1);
        lighting.lights.insert(lighting.lights.end(), torch_lights.begin(), torch_lights.end());

        const manifold::PixelSize framebuffer = window.framebuffer_size();
        renderer.draw(camera, lighting, manifold::fit_viewport(framebuffer, manifold::kAspectRatio),
                      game.animation_time());
        if (options.screenshot_path) {
            manifold::save_screenshot(*options.screenshot_path, framebuffer.width, framebuffer.height);
            manifold::log_info("Saved {}", *options.screenshot_path);
            return;
        }
        window.swap_buffers();
    } while (handle_events(game));
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
