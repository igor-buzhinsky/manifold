#include "util/random.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

// The exact results for a seed are pinned, so that a change to these functions, or a platform where they differ, fails
// a test instead of changing the levels of all seeds unnoticed.

TEST(RandomTest, EngineOutputIsTheOneTheStandardFixes) {
    // The standard requires the 10000th number of a default-constructed Rng (mt19937_64) to be this.
    Rng rng;
    rng.discard(9999);
    EXPECT_EQ(rng(), 9981545732273789042ULL);
}

TEST(RandomTest, ResultsForASeedArePinned) {
    Rng rng(42);
    std::array<std::size_t, 4> indices{};
    for (std::size_t& index : indices) {
        index = random_index(rng, 10);
    }
    EXPECT_EQ(indices, (std::array<std::size_t, 4>{6, 4, 0, 2}));
    EXPECT_EQ(random_int(rng, -5, 5), 0);
    EXPECT_EQ(random_float(rng, 1.0f, 2.0f), 1.09406829f);
    EXPECT_EQ(random_chance(rng, 0.5), false);
    constexpr std::array<double, 3> kWeights{1.0, 2.0, 3.0};
    EXPECT_EQ(random_weighted_index(rng, kWeights), 1U);
    std::array<int, 5> items{0, 1, 2, 3, 4};
    random_shuffle(rng, items);
    EXPECT_EQ(items, (std::array<int, 5>{2, 4, 3, 1, 0}));
}

TEST(RandomTest, IndicesAndIntegersStayInRangeAndReachEveryValue) {
    Rng rng(1);
    std::set<std::size_t> indices;
    std::set<int> integers;
    for (int i = 0; i < 1000; ++i) {
        const std::size_t index = random_index(rng, 7);
        EXPECT_LT(index, 7U);
        indices.insert(index);
        const int integer = random_int(rng, -3, 3);
        EXPECT_GE(integer, -3);
        EXPECT_LE(integer, 3);
        integers.insert(integer);
    }
    EXPECT_EQ(indices.size(), 7U);
    EXPECT_EQ(integers.size(), 7U);
    EXPECT_EQ(random_index(rng, 1), 0U);
    EXPECT_EQ(random_int(rng, 4, 4), 4);
    // The full range of int doesn't overflow, and reaches far to both sides.
    bool far_below = false;
    bool far_above = false;
    for (int i = 0; i < 100; ++i) {
        const int any = random_int(rng, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        far_below = far_below || any < std::numeric_limits<int>::min() / 2;
        far_above = far_above || any > std::numeric_limits<int>::max() / 2;
    }
    EXPECT_TRUE(far_below);
    EXPECT_TRUE(far_above);
}

TEST(RandomTest, FloatsAndUnitsStayInRange) {
    Rng rng(2);
    for (int i = 0; i < 1000; ++i) {
        const double unit = random_unit(rng);
        EXPECT_GE(unit, 0.0);
        EXPECT_LT(unit, 1.0);
        const float number = random_float(rng, -2.5f, 0.5f);
        EXPECT_GE(number, -2.5f);
        EXPECT_LE(number, 0.5f);
    }
    EXPECT_EQ(random_float(rng, 3.0f, 3.0f), 3.0f);
}

TEST(RandomTest, ChancesOfZeroAndOneAreCertain) {
    Rng rng(3);
    int heads = 0;
    for (int i = 0; i < 1000; ++i) {
        EXPECT_FALSE(random_chance(rng, 0.0));
        EXPECT_TRUE(random_chance(rng, 1.0));
        heads += random_chance(rng, 0.5) ? 1 : 0;
    }
    EXPECT_GT(heads, 400);
    EXPECT_LT(heads, 600);
}

TEST(RandomTest, WeightedIndicesFollowTheWeightsAndSkipZeros) {
    Rng rng(4);
    constexpr std::array<double, 4> kWeights{0.0, 1.0, 0.0, 3.0};
    std::array<int, 4> counts{};
    for (int i = 0; i < 4000; ++i) {
        ++counts.at(random_weighted_index(rng, kWeights));
    }
    EXPECT_EQ(counts[0], 0);
    EXPECT_EQ(counts[2], 0);
    EXPECT_GT(counts[1], 800);
    EXPECT_LT(counts[1], 1200);
}

TEST(RandomTest, ShuffleGivesEveryOrderOfAPermutation) {
    Rng rng(5);
    std::set<std::vector<int>> orders;
    for (int i = 0; i < 300; ++i) {
        std::vector<int> items{0, 1, 2};
        random_shuffle(rng, items);
        std::vector<int> sorted = items;
        std::ranges::sort(sorted);
        EXPECT_EQ(sorted, (std::vector<int>{0, 1, 2}));
        orders.insert(items);
    }
    EXPECT_EQ(orders.size(), 6U);
    std::vector<int> empty;
    random_shuffle(rng, empty);
    EXPECT_TRUE(empty.empty());
}

TEST(RandomTest, InvalidArgumentsThrow) {
    Rng rng(6);
    EXPECT_THROW(random_index(rng, 0), std::invalid_argument);
    EXPECT_THROW(random_int(rng, 2, 1), std::invalid_argument);
    EXPECT_THROW(random_float(rng, 2.0f, 1.0f), std::invalid_argument);
    EXPECT_THROW(random_float(rng, 0.0f, std::numeric_limits<float>::infinity()), std::invalid_argument);
    EXPECT_THROW(random_weighted_index(rng, std::array<double, 2>{0.0, 0.0}), std::invalid_argument);
    EXPECT_THROW(random_weighted_index(rng, std::array<double, 2>{1.0, -1.0}), std::invalid_argument);
    EXPECT_THROW(random_weighted_index(rng, std::span<const double>()), std::invalid_argument);
}

}  // namespace
}  // namespace manifold
