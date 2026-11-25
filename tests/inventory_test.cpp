#include "items/inventory.hpp"

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "items/item_catalog.hpp"
#include "test_items.hpp"

namespace manifold {
namespace {

TEST(InventoryTest, ItemsAreAddedAtTheEndAndTakenFromAnywhere) {
    Inventory inventory;
    EXPECT_TRUE(inventory.empty());
    inventory.add(kPebble);
    inventory.add(kGoldBar);
    inventory.add(kCrown);
    EXPECT_EQ(inventory, Inventory({kPebble, kGoldBar, kCrown}));
    EXPECT_EQ(inventory.take(1), kGoldBar);
    EXPECT_EQ(inventory, Inventory({kPebble, kCrown}));
    EXPECT_THROW(inventory.take(2), std::out_of_range);
    EXPECT_EQ(inventory.size(), 2U);
}

TEST(InventoryTest, WeightAndValueAreSums) {
    const ItemCatalog items = make_test_items();
    EXPECT_EQ(Inventory().weight(items), 0);
    const Inventory inventory({kPebble, kPebble, kGoldBar});
    EXPECT_EQ(inventory.weight(items), 1500);
    EXPECT_EQ(inventory.value(items), 1700);
    EXPECT_THROW(static_cast<void>(Inventory({7}).weight(items)), std::out_of_range);
}

TEST(InventoryTest, MovingAnItemPutsItAtTheEnd) {
    Inventory from({kPebble, kGoldBar, kCrown});
    Inventory to({kCrown});
    EXPECT_EQ(move_item(from, 0, to), 1U);
    EXPECT_EQ(from, Inventory({kGoldBar, kCrown}));
    EXPECT_EQ(to, Inventory({kCrown, kPebble}));
    // Within one inventory, the items after it move up.
    EXPECT_EQ(move_item(from, 0, from), 1U);
    EXPECT_EQ(from, Inventory({kCrown, kGoldBar}));
    EXPECT_THROW(move_item(from, 2, to), std::out_of_range);
    EXPECT_EQ(to.size(), 2U);
}

}  // namespace
}  // namespace manifold
