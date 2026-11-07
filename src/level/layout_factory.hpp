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
struct LevelConfig {
    DebugLayoutConfig debug;
    RoomLayoutConfig rooms;
};

/// Creates a layout of the given kind with its settings from `config`.
std::unique_ptr<LevelLayout> make_layout(LayoutKind kind, const LevelConfig& config);

/// Picks one of the layout kinds, all with the same probability.
LayoutKind choose_layout(LevelRng& rng);

/// Short name of a layout kind, as used on the command line: "debug" or "rooms".
std::string_view layout_name(LayoutKind kind);

/// Finds the layout kind with the given name.
///
/// \return The kind, or nothing if no layout has this name.
std::optional<LayoutKind> parse_layout_name(std::string_view name);

/// Generates a level.
///
/// \param config    Settings for all layouts.
/// \param layout    The layout to use. If not given, a random one is chosen.
/// \param clearance Radius around the player start that must stay free of obstacles, in metres.
/// \param seed      Seed for all random choices. The same seed gives the same level.
/// \throws std::runtime_error if the level can't be generated.
Level generate_level(const LevelConfig& config, std::optional<LayoutKind> layout, float clearance, std::uint64_t seed);

}  // namespace manifold
