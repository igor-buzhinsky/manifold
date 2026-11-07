#include "level/layout_factory.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <set>

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

TEST(LayoutFactoryTest, RandomChoiceUsesAllLayouts) {
    std::set<LayoutKind> chosen;
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        LevelRng rng(seed);
        chosen.insert(choose_layout(rng));
    }
    EXPECT_EQ(chosen.size(), 2U);
}

TEST(LayoutFactoryTest, GenerateLevelUsesGivenLayoutOrRandomOne) {
    const LevelConfig config;
    EXPECT_EQ(generate_level(config, LayoutKind::kRooms, 0.3f, 1).layout, LayoutKind::kRooms);
    EXPECT_EQ(generate_level(config, LayoutKind::kDebug, 0.3f, 1).layout, LayoutKind::kDebug);
    // Without a layout, the choice depends only on the seed.
    EXPECT_EQ(generate_level(config, std::nullopt, 0.3f, 5).plan, generate_level(config, std::nullopt, 0.3f, 5).plan);
}

}  // namespace
}  // namespace manifold
