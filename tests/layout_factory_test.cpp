#include "level/layout_factory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "items/inventory.hpp"
#include "items/loot.hpp"
#include "level/level.hpp"
#include "level/texture_kind.hpp"
#include "test_items.hpp"
#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"
#include "world/chest.hpp"
#include "world/door.hpp"
#include "world/torch.hpp"

namespace manifold {
namespace {

TEST(LayoutFactoryTest, MakesLayoutsOfRequestedKind) {
    const LevelConfig config;
    for (const LayoutKind kind : {LayoutKind::kDebug, LayoutKind::kRooms}) {
        const std::unique_ptr<LevelLayout> layout = make_layout(kind, config);
        ASSERT_NE(layout, nullptr);
        LevelRng rng(1);
        EXPECT_EQ(layout->generate(0.3f, rng).layout, kind);
    }
}

TEST(LayoutFactoryTest, NextLevelSeedsDiffer) {
    std::set<std::uint64_t> seeds;
    std::uint64_t seed = 0;
    for (int level = 0; level < 100; ++level) {
        seeds.insert(seed);
        seed = next_level_seed(seed);
    }
    EXPECT_EQ(seeds.size(), 100U);
    EXPECT_EQ(next_level_seed(42), next_level_seed(42));
    EXPECT_NE(next_level_seed(1), next_level_seed(2));
}

TEST(LayoutFactoryTest, NamesRoundTrip) {
    for (const LayoutKind kind : {LayoutKind::kDebug, LayoutKind::kRooms}) {
        EXPECT_EQ(parse_layout_name(layout_name(kind)), kind);
    }
    EXPECT_EQ(layout_name(LayoutKind::kDebug), "debug");
    EXPECT_EQ(layout_name(LayoutKind::kRooms), "rooms");
    EXPECT_FALSE(parse_layout_name("caves").has_value());
}

/// Which layouts are chosen at random for seeds 0 to 99.
std::set<LayoutKind> choose_many(const LevelConfig& config) {
    std::set<LayoutKind> chosen;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        LevelRng rng(seed);
        chosen.insert(choose_layout(config, rng));
    }
    return chosen;
}

TEST(LayoutFactoryTest, RandomChoiceIsRoomsByDefault) {
    EXPECT_EQ(choose_many(LevelConfig{}), std::set<LayoutKind>{LayoutKind::kRooms});
}

TEST(LayoutFactoryTest, RandomChoiceFollowsProbabilities) {
    LevelConfig config;
    config.debug_probability = 1.0f;
    config.rooms_probability = 0.0f;
    EXPECT_EQ(choose_many(config), std::set<LayoutKind>{LayoutKind::kDebug});
    config.debug_probability = 0.5f;
    config.rooms_probability = 0.5f;
    EXPECT_EQ(choose_many(config).size(), 2U);
}

TEST(LayoutFactoryTest, RandomChoiceScalesProbabilities) {
    LevelConfig config;
    config.debug_probability = 3.0f;
    config.rooms_probability = 0.0f;
    EXPECT_EQ(choose_many(config), std::set<LayoutKind>{LayoutKind::kDebug});
}

TEST(LayoutFactoryTest, RandomChoiceRejectsInvalidProbabilities) {
    LevelRng rng(1);
    LevelConfig config;
    config.rooms_probability = 0.0f;
    EXPECT_THROW(choose_layout(config, rng), std::invalid_argument);
    config.debug_probability = -1.0f;
    config.rooms_probability = 2.0f;
    EXPECT_THROW(choose_layout(config, rng), std::invalid_argument);
}

TEST(LayoutFactoryTest, GenerateLevelUsesGivenLayoutOrRandomOne) {
    const LevelConfig config;
    EXPECT_EQ(generate_level(config, LayoutKind::kRooms, 0.3f, 1).layout, LayoutKind::kRooms);
    EXPECT_EQ(generate_level(config, LayoutKind::kDebug, 0.3f, 1).layout, LayoutKind::kDebug);
    // Without a layout, the choice depends only on the seed.
    EXPECT_EQ(generate_level(config, std::nullopt, 0.3f, 5).info, generate_level(config, std::nullopt, 0.3f, 5).info);
}

TEST(LayoutFactoryTest, StartTorchIsAddedOnTheNearestFreeWallOnlyIfAsked) {
    const LevelConfig config;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        const Level plain = generate_level(config, LayoutKind::kRooms, 0.3f, seed);
        ASSERT_FALSE(plain.free_start_torches.empty()) << "seed " << seed;
        const Level lit = generate_level(config, LayoutKind::kRooms, 0.3f, seed, true);
        // One more torch, also if the start room has some, the last one.
        ASSERT_EQ(lit.world.torches().size(), plain.world.torches().size() + 1) << "seed " << seed;
        const Torch& added = lit.world.torches().back();
        EXPECT_EQ(lit.free_start_torches.size(), plain.free_start_torches.size() - 1) << "seed " << seed;
        // No free place is nearer to the start.
        const float distance = glm::distance(added.mount, lit.player_start);
        for (const Torch& torch : plain.free_start_torches) {
            EXPECT_GE(glm::distance(torch.mount, plain.player_start), distance - 1e-3f) << "seed " << seed;
        }
        EXPECT_TRUE(std::ranges::any_of(
            plain.free_start_torches,
            [&](const Torch& torch) { return torch.mount == added.mount && torch.yaw == added.yaw; }))
            << "seed " << seed;
        // The rest of the level stays the same, and the choice depends only on the seed.
        EXPECT_EQ(lit.info, plain.info);
        EXPECT_EQ(lit.world.doors().size(), plain.world.doors().size());
        EXPECT_EQ(generate_level(config, LayoutKind::kRooms, 0.3f, seed, true).world.torches().back().mount,
                  added.mount);
    }

    // Without a free place, nothing is added, and the debug layout has no rooms.
    LevelConfig all = config;
    all.rooms.min_torch_chance = 1.0f;
    all.rooms.max_torch_chance = 1.0f;
    EXPECT_EQ(generate_level(all, LayoutKind::kRooms, 0.3f, 4, true).world.torches().size(),
              generate_level(all, LayoutKind::kRooms, 0.3f, 4).world.torches().size());
    EXPECT_EQ(generate_level(config, LayoutKind::kDebug, 0.3f, 4, true).world.torches().size(),
              generate_level(config, LayoutKind::kDebug, 0.3f, 4).world.torches().size());
}

/// Asset counts with a few files for each key, and no metal textures.
AssetCounts make_asset_counts() {
    AssetCounts counts;
    for (const AssetKey key : kAssetKeys) {
        counts.set(key, 3);
    }
    counts.set(AssetKey::kDoorOpenSounds, 4);
    counts.set(AssetKey::kMetalLevelTextures, 0);
    return counts;
}

TEST(LayoutFactoryTest, AssetsOfObjectsAreAmongTheirFilesAndDependOnlyOnTheSeed) {
    LevelConfig config;
    config.assets = make_asset_counts();
    std::set<AssetIndex> open_sounds;
    std::set<AssetIndex> chest_sounds;
    std::size_t doors = 0;
    std::size_t chests = 0;
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        const Level level = generate_level(config, LayoutKind::kRooms, 0.3f, seed);
        const Level again = generate_level(config, LayoutKind::kRooms, 0.3f, seed);
        ASSERT_EQ(level.world.doors().size(), again.world.doors().size());
        for (std::size_t i = 0; i < level.world.doors().size(); ++i) {
            const DoorSounds& sounds = level.world.doors()[i].sounds;
            EXPECT_EQ(sounds.open, again.world.doors()[i].sounds.open);
            EXPECT_EQ(sounds.close, again.world.doors()[i].sounds.close);
            EXPECT_LT(sounds.open, 4U);
            EXPECT_LT(sounds.close, 3U);
            open_sounds.insert(sounds.open);
        }
        ASSERT_EQ(level.world.chests().size(), again.world.chests().size());
        for (std::size_t i = 0; i < level.world.chests().size(); ++i) {
            EXPECT_EQ(level.world.chests()[i].open_sound, again.world.chests()[i].open_sound);
            EXPECT_LT(level.world.chests()[i].open_sound, 3U);
            chest_sounds.insert(level.world.chests()[i].open_sound);
        }
        ASSERT_TRUE(level.world.exit().has_value());
        EXPECT_EQ(level.world.exit()->sound, again.world.exit()->sound);
        EXPECT_LT(level.world.exit()->sound, 3U);
        EXPECT_EQ(level.texture, choose_level_texture(config.assets, seed));
        doors += level.world.doors().size();
        chests += level.world.chests().size();
    }
    ASSERT_GT(doors, 4U);
    ASSERT_GT(chests, 2U);
    EXPECT_GT(open_sounds.size(), 1U);
    EXPECT_GT(chest_sounds.size(), 1U);
}

TEST(LayoutFactoryTest, AssetsDontChangeTheRestOfTheLevel) {
    LevelConfig with_assets;
    with_assets.assets = make_asset_counts();
    for (const LayoutKind layout : {LayoutKind::kRooms, LayoutKind::kDebug}) {
        for (std::uint64_t seed = 1; seed <= 4; ++seed) {
            const Level level = generate_level(with_assets, layout, 0.3f, seed);
            const Level without = generate_level(LevelConfig{}, layout, 0.3f, seed);
            EXPECT_EQ(level.info, without.info);
            EXPECT_EQ(level.player_start, without.player_start);
            EXPECT_EQ(level.world.obstacles().size(), without.world.obstacles().size());
            EXPECT_EQ(level.world.chests().size(), without.world.chests().size());
            // Without files, every asset is the first.
            for (const Door& door : without.world.doors()) {
                EXPECT_EQ(door.sounds.open, 0U);
                EXPECT_EQ(door.sounds.close, 0U);
            }
            for (const Chest& chest : without.world.chests()) {
                EXPECT_EQ(chest.open_sound, 0U);
            }
            EXPECT_EQ(without.texture, LevelTexture{});
        }
    }
}

TEST(LayoutFactoryTest, ChestsHoldItemsWithinTheirValuesThatDependOnlyOnTheSeed) {
    LevelConfig config;
    config.items = make_test_items();
    std::size_t chests = 0;
    std::size_t items = 0;
    for (const LayoutKind layout : {LayoutKind::kRooms, LayoutKind::kDebug}) {
        for (std::uint64_t seed = 1; seed <= 6; ++seed) {
            const Level level = generate_level(config, layout, 0.3f, seed);
            const Level again = generate_level(config, layout, 0.3f, seed);
            const Level without = generate_level(LevelConfig{}, layout, 0.3f, seed);
            // The items don't change the rest of the level.
            EXPECT_EQ(level.info, without.info);
            ASSERT_EQ(level.world.chests().size(), without.world.chests().size());
            for (std::size_t i = 0; i < level.world.chests().size(); ++i) {
                const Chest& chest = level.world.chests()[i];
                EXPECT_EQ(chest.items, again.world.chests()[i].items);
                EXPECT_EQ(chest.look, without.world.chests()[i].look);
                EXPECT_EQ(chest.open_sound, without.world.chests()[i].open_sound);
                EXPECT_TRUE(without.world.chests()[i].items.empty());
                EXPECT_LE(chest.items.size(), static_cast<std::size_t>(config.chest_loot.of(chest.look).max_draws));
                EXPECT_LE(Inventory(chest.items).value(config.items), config.chest_loot.of(chest.look).max_total_value);
                items += chest.items.size();
            }
            chests += level.world.chests().size();
        }
    }
    ASSERT_GT(chests, 2U);
    // Each chest gets at least one item: the first is never worth more than a chest's total.
    EXPECT_GE(items, chests);
}

TEST(LayoutFactoryTest, LevelTextureIsAmongTheFilesOfAllKindsAndReachesEach) {
    const AssetCounts counts = make_asset_counts();
    std::set<std::pair<TextureKind, AssetIndex>> chosen;
    for (std::uint64_t seed = 0; seed < 300; ++seed) {
        const LevelTexture texture = choose_level_texture(counts, seed);
        EXPECT_EQ(texture, choose_level_texture(counts, seed));
        EXPECT_LT(texture.index, counts.of(level_textures_key(texture.kind)));
        chosen.insert({texture.kind, texture.index});
    }
    // Three files for each kind but metal, which has none.
    EXPECT_EQ(chosen.size(), 9U);
    EXPECT_EQ(choose_level_texture(AssetCounts{}, 7), LevelTexture{});
}

}  // namespace
}  // namespace manifold
