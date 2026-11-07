#include "level/layout_factory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

#include "level/debug_layout.hpp"
#include "level/level.hpp"
#include "level/room_layout.hpp"

namespace manifold {

namespace {

/// All layout kinds with their names.
constexpr std::array<std::pair<LayoutKind, std::string_view>, 2> kLayoutNames{{
    {LayoutKind::kDebug, "debug"},
    {LayoutKind::kRooms, "rooms"},
}};

}  // namespace

std::unique_ptr<LevelLayout> make_layout(LayoutKind kind, const LevelConfig& config) {
    switch (kind) {
        case LayoutKind::kDebug:
            return std::make_unique<DebugLayout>(config.debug);
        case LayoutKind::kRooms:
            return std::make_unique<RoomLayout>(config.rooms);
    }
    return nullptr;
}

LayoutKind choose_layout(LevelRng& rng) {
    std::uniform_int_distribution<std::size_t> distribution(0, kLayoutNames.size() - 1);
    return kLayoutNames.at(distribution(rng)).first;
}

std::string_view layout_name(LayoutKind kind) {
    for (const auto& [known_kind, name] : kLayoutNames) {
        if (known_kind == kind) {
            return name;
        }
    }
    return "unknown";
}

std::optional<LayoutKind> parse_layout_name(std::string_view name) {
    for (const auto& [kind, known_name] : kLayoutNames) {
        if (known_name == name) {
            return kind;
        }
    }
    return std::nullopt;
}

Level generate_level(const LevelConfig& config, std::optional<LayoutKind> layout, float clearance, std::uint64_t seed) {
    LevelRng rng(seed);
    const LayoutKind kind = layout ? *layout : choose_layout(rng);
    return make_layout(kind, config)->generate(clearance, rng);
}

}  // namespace manifold
