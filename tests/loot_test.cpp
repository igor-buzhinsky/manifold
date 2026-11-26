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

TEST(LootTest, ChestsAreFilledAsTheirLookSays) {
    EXPECT_EQ(
        chest_loot(ChestLook::kWood),
        (ContainerLoot{
            .min_items = 1, .max_items = 3, .min_item_value = 100, .max_item_value = 2000, .max_total_value = 2000}));
    EXPECT_EQ(
        chest_loot(ChestLook::kGold),
        (ContainerLoot{
            .min_items = 3, .max_items = 5, .min_item_value = 100, .max_item_value = 5000, .max_total_value = 6000}));
}

TEST(LootTest, TheNumberOfItemsIsChosenFromTheRangeWhenTheTotalAllowsIt) {
    const ItemCatalog items = make_test_items();
    const ContainerLoot loot{
        .min_items = 2, .max_items = 4, .min_item_value = 0, .max_item_value = 10000, .max_total_value = 100000};
    std::set<std::size_t> sizes;
    for (int seed = 0; seed < 100; ++seed) {
        Rng rng(static_cast<Rng::result_type>(seed));
        sizes.insert(fill_container(items, loot, rng).size());
    }
    EXPECT_EQ(sizes, (std::set<std::size_t>{2, 3, 4}));
}

TEST(LootTest, OnlyItemsInTheRangeOfValuesWithAChanceAreChosen) {
    const ItemCatalog items = make_test_items();
    // The pebble is too cheap, the crown has no chance, and the gold bar is in the range.
    const ContainerLoot loot{
        .min_items = 5, .max_items = 5, .min_item_value = 150, .max_item_value = 2000, .max_total_value = 100000};
    Rng rng(1);
    EXPECT_EQ(fill_container(items, loot, rng), std::vector<ItemId>(5, kGoldBar));
    const ContainerLoot nothing_fits{
        .min_items = 1, .max_items = 3, .min_item_value = 300, .max_item_value = 1000, .max_total_value = 100000};
    EXPECT_TRUE(fill_container(items, nothing_fits, rng).empty());
    EXPECT_TRUE(fill_container(ItemCatalog(), loot, rng).empty());
}

TEST(LootTest, AnItemAboveTheTotalIsLeftOutAndStopsTheFilling) {
    const ItemCatalog items = make_test_items();
    // Each gold bar is worth 1500, so the third would make 4500.
    const ContainerLoot loot{
        .min_items = 5, .max_items = 5, .min_item_value = 1000, .max_item_value = 2000, .max_total_value = 4000};
    for (int seed = 0; seed < 20; ++seed) {
        Rng rng(static_cast<Rng::result_type>(seed));
        EXPECT_EQ(fill_container(items, loot, rng), std::vector<ItemId>(2, kGoldBar)) << seed;
    }
    // Also before the fewest items.
    const ContainerLoot too_dear{
        .min_items = 2, .max_items = 2, .min_item_value = 1000, .max_item_value = 2000, .max_total_value = 1000};
    Rng rng(3);
    EXPECT_TRUE(fill_container(items, too_dear, rng).empty());
}

TEST(LootTest, ItemsAreChosenInProportionToTheirChances) {
    // The gold bar has twice the chance of the pebble.
    const ItemCatalog items = make_test_items();
    const ContainerLoot loot{
        .min_items = 1, .max_items = 1, .min_item_value = 0, .max_item_value = 10000, .max_total_value = 10000};
    std::array<int, 3> counts{};
    Rng rng(5);
    for (int i = 0; i < 3000; ++i) {
        for (const ItemId item : fill_container(items, loot, rng)) {
            ++counts.at(item);
        }
    }
    EXPECT_EQ(counts[kPebble] + counts[kGoldBar], 3000);
    EXPECT_EQ(counts[kCrown], 0);
    EXPECT_NEAR(static_cast<double>(counts[kGoldBar]) / counts[kPebble], 2.0, 0.25);
}

TEST(LootTest, ChestsStayWithinTheirLimitsAndTheSameStateGivesTheSameItems) {
    const ItemCatalog items = make_test_items();
    for (const ChestLook look : {ChestLook::kWood, ChestLook::kGold}) {
        const ContainerLoot& loot = chest_loot(look);
        for (int seed = 0; seed < 100; ++seed) {
            Rng rng(static_cast<Rng::result_type>(seed));
            Rng again(static_cast<Rng::result_type>(seed));
            const std::vector<ItemId> chosen = fill_chest(items, look, rng);
            EXPECT_EQ(fill_chest(items, look, again), chosen);
            EXPECT_LE(chosen.size(), static_cast<std::size_t>(loot.max_items));
            EXPECT_LE(Inventory(chosen).value(items), loot.max_total_value);
        }
    }
}

}  // namespace
}  // namespace manifold
