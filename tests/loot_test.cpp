#include "items/loot.hpp"

#include <array>
#include <cstddef>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "test_items.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"

namespace manifold {
namespace {

TEST(LootTest, ChestValuesAreTheirMeanLessOrMoreHalf) {
    EXPECT_EQ(chest_value_range(ChestLook::kWood).min, 500);
    EXPECT_EQ(chest_value_range(ChestLook::kWood).max, 1500);
    EXPECT_EQ(chest_value_range(ChestLook::kGold).min, 2000);
    EXPECT_EQ(chest_value_range(ChestLook::kGold).max, 6000);
}

TEST(LootTest, ItemsNeverExceedTheValueAndStopWhenNoneFits) {
    const ItemCatalog items = make_test_items();
    for (int seed = 0; seed < 200; ++seed) {
        Rng rng(static_cast<Rng::result_type>(seed));
        const int value = 50 + (seed * 37);
        const Inventory chosen(fill_container(items, value, 1000, rng));
        EXPECT_LE(chosen.value(items), value) << seed;
        // The pebble always fits while 100 or more are left, so less than that is left.
        EXPECT_LT(value - chosen.value(items), items.at(kPebble).value) << seed;
        for (const ItemId item : chosen.items()) {
            EXPECT_NE(item, kCrown) << "an item without a chance was chosen";
        }
    }
}

TEST(LootTest, AtMostMaxItemsAreChosen) {
    const ItemCatalog items = make_test_items();
    Rng rng(3);
    EXPECT_EQ(fill_container(items, 100000, 8, rng).size(), 8U);
    EXPECT_TRUE(fill_container(items, 100000, 0, rng).empty());
}

TEST(LootTest, NothingIsChosenWithoutItemsThatFitAndHaveAChance) {
    Rng rng(4);
    Rng unused(4);
    EXPECT_TRUE(fill_container(ItemCatalog(), 1000, 8, rng).empty());
    EXPECT_TRUE(fill_container(make_test_items(), 99, 8, rng).empty());
    const ItemCatalog crowns({{.key = "CROWN", .name = "Crown", .value = 1, .weight = 1, .chance = 0.0}});
    EXPECT_TRUE(fill_container(crowns, 1000, 8, rng).empty());
    EXPECT_EQ(rng(), unused());
}

TEST(LootTest, ItemsAreChosenInProportionToTheirChances) {
    // The gold bar has twice the chance of the pebble, and both always fit.
    const ItemCatalog items = make_test_items();
    std::array<int, 3> counts{};
    Rng rng(5);
    for (int i = 0; i < 3000; ++i) {
        for (const ItemId item : fill_container(items, 1500, 1, rng)) {
            ++counts.at(item);
        }
    }
    EXPECT_EQ(counts[kPebble] + counts[kGoldBar], 3000);
    EXPECT_EQ(counts[kCrown], 0);
    EXPECT_NEAR(static_cast<double>(counts[kGoldBar]) / counts[kPebble], 2.0, 0.25);
}

TEST(LootTest, ChestsAreFilledWithinTheirValuesAndTheSameStateGivesTheSameItems) {
    const ItemCatalog items = make_test_items();
    std::set<std::size_t> sizes;
    for (const ChestLook look : {ChestLook::kWood, ChestLook::kGold}) {
        for (int seed = 0; seed < 100; ++seed) {
            Rng rng(static_cast<Rng::result_type>(seed));
            Rng again(static_cast<Rng::result_type>(seed));
            const std::vector<ItemId> chosen = fill_chest(items, look, rng);
            EXPECT_EQ(fill_chest(items, look, again), chosen);
            EXPECT_LE(chosen.size(), kMaxChestItems);
            EXPECT_LE(Inventory(chosen).value(items), chest_value_range(look).max);
            sizes.insert(chosen.size());
        }
    }
    EXPECT_TRUE(sizes.contains(kMaxChestItems));
    EXPECT_GT(sizes.size(), 3U);
}

}  // namespace
}  // namespace manifold
