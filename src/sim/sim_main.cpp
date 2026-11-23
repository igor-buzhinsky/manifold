// manifold_sim: plays levels with random input and checks invariants of the player's movement, see
// run_movement_sim(). Prints a report to stdout; the logs of level generation go to stderr.

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>

#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "sim/movement_sim.hpp"

namespace {

/// Exit code when the command line is invalid or the run fails. A run that finds violations exits with 1.
constexpr int kError = 2;

constexpr const char* kUsage =
    "Usage: manifold_sim [--seeds N] [--first-seed S] [--seconds T] [--dt DT] [--layout debug|rooms|both]\n"
    "                    [--examples K] | --help\n"
    "Plays N seeds from S of each layout (default 40 from 1, both layouts) for T seconds each (default 30), in\n"
    "frames of DT seconds (default 1/60, at most 0.35), with random walking, running, stepping aside, standing,\n"
    "turning, looking and jumping, and opening and closing doors near the player, and checks the player's movement\n"
    "after every frame. Shows K examples of each kind of violation (default 5).\n"
    "Exits with 0 if all invariants hold, 1 if one is broken, and 2 on a wrong command line or another error.\n";

/// Parses a whole argument as a number of type T.
///
/// \throws std::invalid_argument if `text` is not such a number.
template <typename T>
T parse_number(const std::string& option, const std::string& text) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("invalid value of " + option + ": " + text);
    }
    return value;
}

/// Parses the command line, see kUsage. The values are checked by run_movement_sim().
///
/// \param args All arguments, including the program name.
/// \return The config, or nothing if the usage was asked for with `--help`.
/// \throws std::invalid_argument if an argument is invalid.
std::optional<manifold::MovementSimConfig> parse_options(std::span<char*> args) {
    manifold::MovementSimConfig config;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string option = args[i];
        if (option == "--help" || option == "-h") {
            return std::nullopt;
        }
        if (i + 1 == args.size()) {
            throw std::invalid_argument(option + " needs a value");
        }
        const std::string value = args[++i];
        if (option == "--seeds") {
            config.seeds = parse_number<int>(option, value);
        } else if (option == "--first-seed") {
            config.first_seed = parse_number<std::uint64_t>(option, value);
        } else if (option == "--seconds") {
            config.seconds = parse_number<float>(option, value);
        } else if (option == "--dt") {
            config.dt = parse_number<float>(option, value);
        } else if (option == "--examples") {
            config.examples = parse_number<std::size_t>(option, value);
        } else if (option == "--layout") {
            if (value == "both") {
                config.layouts = {manifold::LayoutKind::kDebug, manifold::LayoutKind::kRooms};
            } else if (const auto layout = manifold::parse_layout_name(value)) {
                config.layouts = {*layout};
            } else {
                throw std::invalid_argument("unknown layout: " + value);
            }
        } else {
            throw std::invalid_argument("unknown option: " + option);
        }
    }
    return config;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const std::optional<manifold::MovementSimConfig> config = parse_options(std::span(argv, argc));
        if (!config) {
            std::cout << kUsage;
            return EXIT_SUCCESS;
        }
        const manifold::MovementSimReport report = manifold::run_movement_sim(*config);
        std::cout << manifold::format_movement_sim_report(report);
        return report.ok() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::invalid_argument& error) {
        std::cerr << error.what() << "\n" << kUsage;
        return kError;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return kError;
    }
}
