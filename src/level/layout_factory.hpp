#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "level/debug_layout.hpp"
#include "level/level.hpp"
#include "level/room_layout.hpp"

namespace manifold {

/// Settings for all layouts. Only the settings of the chosen layout are used.
///
/// The probabilities say how often each layout is chosen when no layout is given. They should add up to 1;
/// if they don't, they are scaled so that they do.
struct LevelConfig {
    DebugLayoutConfig debug;
    float debug_probability = 0.0f;  ///< Only for testing, so it is never chosen at random.
    RoomLayoutConfig rooms;
    float rooms_probability = 1.0f;
};

/// Creates a layout of the given kind with its settings from `config`.
std::unique_ptr<LevelLayout> make_layout(LayoutKind kind, const LevelConfig& config);

/// Picks one of the layout kinds at random, using the probabilities from `config`.
///
/// \throws std::invalid_argument if a probability is negative or not finite, or all of them are zero.
LayoutKind choose_layout(const LevelConfig& config, LevelRng& rng);

/// Short name of a layout kind, as used on the command line: "debug" or "rooms".
std::string_view layout_name(LayoutKind kind);

/// Finds the layout kind with the given name.
///
/// \return The kind, or nothing if no layout has this name.
std::optional<LayoutKind> parse_layout_name(std::string_view name);

/// The seed of the level after the one generated from `seed`.
///
/// It is a mix of all bits of `seed`, so a series of levels repeats only with the same first seed.
std::uint64_t next_level_seed(std::uint64_t seed);

/// Generates a level.
///
/// \param config      Settings for all layouts.
/// \param layout      The layout to use. If not given, a random one is chosen.
/// \param clearance   Radius around the player start that must stay free of obstacles, in metres.
/// \param seed        Seed for all random choices. The same seed gives the same level.
/// \param start_torch Whether to add a torch on the wall nearest to the player start: the one of
///                    Level::free_start_torches whose mount is nearest to the start, also if the room has torches
///                    already. Among equally near ones, it is chosen from its own random numbers. Nothing is added if
///                    there are none, e.g. for the debug layout. The rest of the level is the same either way.
///
/// The sounds of the doors are chosen last, from their own random numbers, so the rest of the level doesn't depend on
/// them.
/// \throws std::invalid_argument if `layout` is not given and the layout probabilities are invalid.
/// \throws std::runtime_error if the level can't be generated.
Level generate_level(const LevelConfig& config, std::optional<LayoutKind> layout, float clearance, std::uint64_t seed,
                     bool start_torch = false);

}  // namespace manifold
