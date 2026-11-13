#include "level/layout_factory.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>

#include <gtest/gtest.h>

#include "level/level.hpp"

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

}  // namespace
}  // namespace manifold
