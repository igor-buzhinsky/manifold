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
#include <SDL3/SDL_keycode.h>
#include <glm/vec3.hpp>

#include "game.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"
#include "render/renderer.hpp"
#include "render/screenshot.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"

namespace {

constexpr int kWindowWidth = 1600;

struct Options {
    std::optional<std::uint64_t> seed;
    std::optional<std::string> screenshot_path;
};

std::uint64_t parse_seed(const std::string& text) {
    std::uint64_t seed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("invalid seed: " + text);
    }
    return seed;
}

// Usage: manifold [SEED] [--screenshot FILE.png]
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

std::uint64_t random_seed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32U) | device();
}

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

bool quit_requested() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
            return true;
        }
    }
    return false;
}

void run(const Options& options) {
    const manifold::Game game(manifold::GameConfig{}, options.seed.value_or(random_seed()));
    print_game(game);

    const auto window_height = static_cast<int>(std::lround(kWindowWidth / manifold::kAspectRatio));
    manifold::Window window("Manifold", kWindowWidth, window_height);
    const manifold::Renderer renderer(manifold::build_world_mesh(game.world()));

    // The light spell: a warm light at the player's eyes, plus a little ambient light.
    const manifold::Camera camera = manifold::make_player_camera(game.player());
    const manifold::Lighting lighting{
        .ambient = glm::vec3(0.01f),
        .light = {.position = camera.position, .color = {1.0f, 0.85f, 0.6f}, .intensity = 5.0f},
    };

    do {
        const manifold::PixelSize framebuffer = window.framebuffer_size();
        renderer.draw(camera, lighting, manifold::fit_viewport(framebuffer, manifold::kAspectRatio));
        if (options.screenshot_path) {
            manifold::save_screenshot(*options.screenshot_path, framebuffer.width, framebuffer.height);
            std::cout << "Saved " << *options.screenshot_path << '\n';
            return;
        }
        window.swap_buffers();
    } while (!quit_requested());
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
