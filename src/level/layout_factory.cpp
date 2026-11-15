#include "level/layout_factory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
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

/// The probability of choosing a layout of the given kind at random.
float layout_probability(LayoutKind kind, const LevelConfig& config) {
    switch (kind) {
        case LayoutKind::kDebug:
            return config.debug_probability;
        case LayoutKind::kRooms:
            return config.rooms_probability;
    }
    return 0.0f;
}

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

LayoutKind choose_layout(const LevelConfig& config, LevelRng& rng) {
    std::array<float, kLayoutNames.size()> probabilities{};
    float total = 0.0f;
    for (std::size_t i = 0; i < kLayoutNames.size(); ++i) {
        probabilities.at(i) = layout_probability(kLayoutNames.at(i).first, config);
        if (probabilities.at(i) < 0.0f) {
            throw std::invalid_argument("layout probabilities can't be negative");
        }
        total += probabilities.at(i);
    }
    if (total <= 0.0f) {
        throw std::invalid_argument("at least one layout probability must be positive");
    }
    std::discrete_distribution<std::size_t> distribution(probabilities.begin(), probabilities.end());
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

std::uint64_t next_level_seed(std::uint64_t seed) {
    // The SplitMix64 generator: a step of a Weyl sequence, then a finalizer that mixes the bits.
    std::uint64_t z = seed + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

Level generate_level(const LevelConfig& config, std::optional<LayoutKind> layout, float clearance, std::uint64_t seed) {
    LevelRng rng(seed);
    const LayoutKind kind = layout ? *layout : choose_layout(config, rng);
    return make_layout(kind, config)->generate(clearance, rng);
}

}  // namespace manifold
