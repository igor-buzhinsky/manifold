#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>

#include "game.hpp"

namespace {

// Uses the seed from the first command-line argument if there is one, otherwise a random seed.
std::uint64_t choose_seed(std::span<char*> args) {
    if (args.size() > 1) {
        const std::string text = args[1];
        std::uint64_t seed = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
        if (error != std::errc{} || end != text.data() + text.size()) {
            throw std::invalid_argument("invalid seed: " + text);
        }
        return seed;
    }
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32U) | device();
}

void print_game(const manifold::Game& game) {
    const auto& bounds = game.world().bounds();
    const auto& player = game.player();
    std::cout << std::format("Manifold, level seed {}\n", game.seed());
    std::cout << std::format("World: x [{}, {}], y [{}, {}], height {} m\n", bounds.x_min, bounds.x_max, bounds.y_min,
                             bounds.y_max, bounds.height);
    std::cout << std::format("Player: position ({:.2f}, {:.2f}, {:.2f}), radius {} m\n", player.position().x,
                             player.position().y, player.position().z, player.radius());
    for (const auto& obstacle : game.world().obstacles()) {
        std::cout << std::format("Obstacle: center ({:6.2f}, {:6.2f}), size {} m, yaw {:5.1f} deg\n",
                                 obstacle.base_center.x, obstacle.base_center.y, 2.0f * obstacle.half_size,
                                 obstacle.yaw * 180.0f / std::numbers::pi_v<float>);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const manifold::Game game(manifold::GameConfig{}, choose_seed(std::span(argv, argc)));
        print_game(game);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
