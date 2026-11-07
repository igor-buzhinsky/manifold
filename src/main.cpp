#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#include <glm/vec3.hpp>

#include "game.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"
#include "render/renderer.hpp"
#include "render/screenshot.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"

namespace {

/// Width of the window in screen units. The height follows from the 16:9 aspect ratio.
constexpr int kWindowWidth = 1600;

/// Command-line options.
struct Options {
    std::optional<std::uint64_t> seed;           ///< Level seed. Random if not given.
    std::optional<std::string> screenshot_path;  ///< If given, save the first frame there and quit.
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

/// Parses the command line: `manifold [SEED] [--screenshot FILE.png]`.
///
/// \param args All arguments, including the program name.
/// \throws std::invalid_argument if an argument is invalid.
Options parse_options(std::span<char*> args) {
    Options options;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string arg = args[i];
        if (arg == "--screenshot") {
            if (i + 1 == args.size()) {
                throw std::invalid_argument("--screenshot needs a file name");
            }
            options.screenshot_path = args[++i];
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

/// Prints the seed, the level and the player to stdout.
void print_game(const manifold::Game& game) {
    const auto& bounds = game.world().bounds();
    const auto& player = game.player();
    std::cout << std::format("Manifold, level seed {}\n", game.seed());
    std::cout << std::format("World: x [{}, {}], y [{}, {}], height {} m\n", bounds.x_min, bounds.x_max, bounds.y_min,
                             bounds.y_max, bounds.height);
    std::cout << std::format("Player: position ({:.2f}, {:.2f}, {:.2f}), radius {} m, height {} m\n",
                             player.position().x, player.position().y, player.position().z, player.radius(),
                             player.height());
    for (const auto& obstacle : game.world().obstacles()) {
        std::cout << std::format("Obstacle: center ({:6.2f}, {:6.2f}), size {} m, yaw {:5.1f} deg\n",
                                 obstacle.base_center.x, obstacle.base_center.y, 2.0f * obstacle.half_size,
                                 obstacle.yaw * 180.0f / std::numbers::pi_v<float>);
    }
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
            std::cout << "Noclip " << (game.noclip() ? "on" : "off") << '\n';
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
    };
}

/// Runs the game until the player quits, or until the screenshot is saved.
void run(const Options& options) {
    manifold::Game game(manifold::GameConfig{}, options.seed.value_or(random_seed()));
    print_game(game);

    const auto window_height = static_cast<int>(std::lround(kWindowWidth / manifold::kAspectRatio));
    manifold::Window window("Manifold", kWindowWidth, window_height);
    const manifold::Renderer renderer(manifold::build_world_mesh(game.world()));

    std::uint64_t previous_ticks = SDL_GetTicksNS();
    do {
        const std::uint64_t ticks = SDL_GetTicksNS();
        const float dt = std::min(static_cast<float>(ticks - previous_ticks) * 1e-9f, kMaxFrameTime);
        previous_ticks = ticks;
        game.update(read_input(), dt);

        // The light spell: a warm light at the player's eyes, plus a little ambient light.
        const manifold::Camera camera = manifold::make_player_camera(game.player());
        const manifold::Lighting lighting{
            .ambient = glm::vec3(0.01f),
            .light = {.position = camera.position, .color = {1.0f, 0.85f, 0.6f}, .intensity = 5.0f},
        };

        const manifold::PixelSize framebuffer = window.framebuffer_size();
        renderer.draw(camera, lighting, manifold::fit_viewport(framebuffer, manifold::kAspectRatio));
        if (options.screenshot_path) {
            manifold::save_screenshot(*options.screenshot_path, framebuffer.width, framebuffer.height);
            std::cout << "Saved " << *options.screenshot_path << '\n';
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
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
