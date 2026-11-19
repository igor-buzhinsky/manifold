#include "level/layout_factory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>

#include <gtest/gtest.h>

#include "level/level.hpp"
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

TEST(LayoutFactoryTest, StartTorchIsAddedOnlyIfAskedAndTheStartRoomHasNone) {
    LevelConfig config;
    config.rooms.min_torch_chance = 0.0f;
    config.rooms.max_torch_chance = 0.0f;
    const Level dark = generate_level(config, LayoutKind::kRooms, 0.3f, 4);
    EXPECT_TRUE(dark.world.torches().empty());
    ASSERT_FALSE(dark.spare_start_torches.empty());
    const Level lit = generate_level(config, LayoutKind::kRooms, 0.3f, 4, true);
    ASSERT_EQ(lit.world.torches().size(), 1U);
    EXPECT_TRUE(lit.spare_start_torches.empty());
    EXPECT_TRUE(std::ranges::any_of(dark.spare_start_torches, [&](const Torch& torch) {
        return torch.mount == lit.world.torches()[0].mount && torch.yaw == lit.world.torches()[0].yaw;
    }));
    // The rest of the level stays the same, and the choice depends only on the seed.
    EXPECT_EQ(lit.info, dark.info);
    EXPECT_EQ(lit.world.doors().size(), dark.world.doors().size());
    EXPECT_EQ(generate_level(config, LayoutKind::kRooms, 0.3f, 4, true).world.torches()[0].mount,
              lit.world.torches()[0].mount);

    // A start room that has a torch gets no other, and the debug layout has no rooms.
    config.rooms.min_torch_chance = 1.0f;
    config.rooms.max_torch_chance = 1.0f;
    EXPECT_EQ(generate_level(config, LayoutKind::kRooms, 0.3f, 4, true).world.torches().size(),
              generate_level(config, LayoutKind::kRooms, 0.3f, 4).world.torches().size());
    EXPECT_EQ(generate_level(config, LayoutKind::kDebug, 0.3f, 4, true).world.torches().size(),
              generate_level(config, LayoutKind::kDebug, 0.3f, 4).world.torches().size());
}

TEST(LayoutFactoryTest, DoorSoundsDependOnlyOnTheSeedAndDiffer) {
    const LevelConfig config;
    std::set<std::uint32_t> open_sounds;
    std::size_t doors = 0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        const Level level = generate_level(config, LayoutKind::kRooms, 0.3f, seed);
        const Level again = generate_level(config, LayoutKind::kRooms, 0.3f, seed);
        ASSERT_EQ(level.world.doors().size(), again.world.doors().size());
        for (std::size_t i = 0; i < level.world.doors().size(); ++i) {
            EXPECT_EQ(level.world.doors()[i].sounds.open, again.world.doors()[i].sounds.open);
            EXPECT_EQ(level.world.doors()[i].sounds.close, again.world.doors()[i].sounds.close);
            open_sounds.insert(level.world.doors()[i].sounds.open);
        }
        doors += level.world.doors().size();
    }
    ASSERT_GT(doors, 2U);
    EXPECT_GT(open_sounds.size(), 1U);
}

}  // namespace
}  // namespace manifold
