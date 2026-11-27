#include "level/layout_factory.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include "items/loot.hpp"
#include "level/debug_layout.hpp"
#include "level/level.hpp"
#include "level/room_layout.hpp"
#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"
#include "util/random.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

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
            return std::make_unique<DebugLayout>(config.debug, config.object_chances);
        case LayoutKind::kRooms:
            return std::make_unique<RoomLayout>(config.rooms, config.object_chances);
    }
    return nullptr;
}

LayoutKind choose_layout(const LevelConfig& config, LevelRng& rng) {
    std::array<double, kLayoutNames.size()> weights{};
    for (std::size_t i = 0; i < weights.size(); ++i) {
        weights.at(i) = layout_probability(kLayoutNames.at(i).first, config);
    }
    // random_weighted_index() refuses negative, infinite and NaN weights, and weights that are all zero.
    return kLayoutNames.at(random_weighted_index(rng, weights)).first;
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

namespace {

/// Adds the torch of Level::free_start_torches nearest to the player start, see generate_level(), and takes it out of
/// the list. Among torches equally near, within 1 mm, one is chosen from random numbers of `seed`.
void add_nearest_start_torch(Level& level, std::uint64_t seed) {
    std::vector<Torch>& free = level.free_start_torches;
    if (free.empty()) {
        return;
    }
    const auto distance = [&level](const Torch& torch) { return glm::distance(torch.mount, level.player_start); };
    const float nearest = std::ranges::min(free | std::views::transform(distance));
    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < free.size(); ++i) {
        if (distance(free[i]) < nearest + 1e-3f) {
            candidates.push_back(i);
        }
    }
    constexpr std::uint64_t kStartTorchSeedSalt = 0x3c6ef372fe94f82bULL;
    LevelRng rng(seed ^ kStartTorchSeedSalt);
    const std::size_t chosen = candidates[random_index(rng, candidates.size())];
    level.world.add_torch(free[chosen]);
    free.erase(free.begin() + static_cast<std::ptrdiff_t>(chosen));
}

}  // namespace

Level generate_level(const LevelConfig& config, std::optional<LayoutKind> layout, float clearance, std::uint64_t seed,
                     bool start_torch) {
    LevelRng rng(seed);
    const LayoutKind kind = layout ? *layout : choose_layout(config, rng);
    Level level = make_layout(kind, config)->generate(clearance, rng);
    const AssetCounts& assets = config.assets;
    constexpr std::uint64_t kDoorSoundSeedSalt = 0x8cb92ba72f3d8dd7ULL;
    LevelRng door_rng(seed ^ kDoorSoundSeedSalt);
    for (std::size_t i = 0; i < level.world.doors().size(); ++i) {
        const AssetIndex open = choose_asset(door_rng, assets, AssetKey::kDoorOpenSounds);
        const AssetIndex close = choose_asset(door_rng, assets, AssetKey::kDoorCloseSounds);
        level.world.set_door_sounds(i, {.open = open, .close = close});
    }
    constexpr std::uint64_t kChestSoundSeedSalt = 0x5be0cd19137e2179ULL;
    LevelRng chest_rng(seed ^ kChestSoundSeedSalt);
    for (std::size_t i = 0; i < level.world.chests().size(); ++i) {
        level.world.set_chest_sound(i, choose_asset(chest_rng, assets, AssetKey::kChestOpenSounds));
    }
    constexpr std::uint64_t kChestItemsSeedSalt = 0x1f83d9abfb41bd6bULL;
    LevelRng items_rng(seed ^ kChestItemsSeedSalt);
    for (std::size_t i = 0; i < level.world.chests().size(); ++i) {
        level.world.set_chest_items(
            i, fill_chest(config.items, config.chest_loot, level.world.chests()[i].look, items_rng));
    }
    if (std::optional<LevelExit> exit = level.world.exit()) {
        constexpr std::uint64_t kExitSoundSeedSalt = 0xa54ff53a5f1d36f1ULL;
        LevelRng exit_rng(seed ^ kExitSoundSeedSalt);
        exit->sound = choose_asset(exit_rng, assets, AssetKey::kExitSounds);
        level.world.set_exit(*exit);
    }
    level.texture = choose_level_texture(assets, seed);
    if (start_torch) {
        add_nearest_start_torch(level, seed);
    }
    return level;
}

}  // namespace manifold
