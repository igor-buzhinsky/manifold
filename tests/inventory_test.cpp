#include "items/inventory.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "items/item_catalog.hpp"
#include "test_items.hpp"

namespace manifold {
namespace {

TEST(InventoryTest, ItemsOfAKindJoinTheirStackAndNewKindsGoToTheEnd) {
    Inventory inventory;
    EXPECT_TRUE(inventory.empty());
    EXPECT_EQ(inventory.add(kPebble), 0U);
    EXPECT_EQ(inventory.add(kGoldBar), 1U);
    EXPECT_EQ(inventory.add(kPebble, 4), 0U);
    EXPECT_EQ(inventory.add(kCrown), 2U);
    const std::vector<ItemStack> stacks{{.item = kPebble, .count = 5}, {.item = kGoldBar}, {.item = kCrown}};
    EXPECT_EQ(std::vector<ItemStack>(inventory.stacks().begin(), inventory.stacks().end()), stacks);
    EXPECT_EQ(inventory.size(), 3U);
    EXPECT_EQ(inventory.item_count(), 7);
    EXPECT_EQ(inventory.find(kGoldBar), 1U);
    // The kinds keep the order in which they first come.
    EXPECT_EQ(Inventory({kGoldBar, kPebble, kGoldBar}).stacks()[0], (ItemStack{.item = kGoldBar, .count = 2}));
    EXPECT_THROW(inventory.add(kPebble, 0), std::invalid_argument);
}

TEST(InventoryTest, TakingSomeItemsLeavesTheStackAndTakingAllRemovesIt) {
    Inventory inventory;
    inventory.add(kPebble, 3);
    inventory.add(kGoldBar);
    inventory.add(kCrown, 2);
    EXPECT_EQ(inventory.take(0, 2), (ItemStack{.item = kPebble, .count = 2}));
    EXPECT_EQ(inventory.stacks()[0].count, 1);
    EXPECT_THROW(inventory.take(0, 2), std::invalid_argument);
    EXPECT_THROW(inventory.take(0, 0), std::invalid_argument);
    EXPECT_EQ(inventory.take(1), (ItemStack{.item = kGoldBar, .count = 1}));
    // The stacks after it moved up, and are still found.
    EXPECT_EQ(inventory.find(kCrown), 1U);
    EXPECT_FALSE(inventory.find(kGoldBar).has_value());
    EXPECT_EQ(inventory.add(kCrown), 1U);
    EXPECT_EQ(inventory.add(kGoldBar), 2U);
    EXPECT_THROW(inventory.take(3), std::out_of_range);
    EXPECT_THROW(inventory.take(3, 1), std::out_of_range);
}

TEST(InventoryTest, MovingAStackToTheEndKeepsTheStacksFound) {
    Inventory inventory({kPebble, kGoldBar, kCrown});
    inventory.move_to_end(0);
    EXPECT_EQ(inventory, Inventory({kGoldBar, kCrown, kPebble}));
    EXPECT_EQ(inventory.find(kGoldBar), 0U);
    EXPECT_EQ(inventory.find(kPebble), 2U);
    EXPECT_THROW(inventory.move_to_end(3), std::out_of_range);
}

TEST(InventoryTest, AStackCountsAtMostTheLargestInt) {
    Inventory inventory;
    inventory.add(kPebble, std::numeric_limits<int>::max() - 1);
    inventory.add(kPebble);
    EXPECT_THROW(inventory.add(kPebble), std::overflow_error);
    EXPECT_EQ(inventory.stacks()[0].count, std::numeric_limits<int>::max());
    // A move that the stack can't take changes nothing.
    Inventory from({kPebble});
    EXPECT_THROW(move_stack(from, 0, inventory), std::overflow_error);
    EXPECT_EQ(from, Inventory({kPebble}));
}

TEST(InventoryTest, WeightAndValueAreSums) {
    const ItemCatalog items = make_test_items();
    EXPECT_EQ(Inventory().weight(items), 0);
    const Inventory inventory({kPebble, kPebble, kGoldBar});
    EXPECT_EQ(inventory.weight(items), 1500);
    EXPECT_EQ(inventory.value(items), 1700);
    EXPECT_THROW(static_cast<void>(Inventory({7}).weight(items)), std::out_of_range);
    // Large stacks don't overflow an int.
    Inventory heavy;
    heavy.add(kGoldBar, 10000000);
    EXPECT_EQ(heavy.weight(items), 10000000000LL);
}

TEST(InventoryTest, MovedItemsJoinTheirStackOrStartOneAtTheEnd) {
    Inventory from({kPebble, kGoldBar, kGoldBar, kCrown});
    Inventory to({kCrown});
    EXPECT_EQ(move_stack(from, 0, to), 1U);
    EXPECT_EQ(from, Inventory({kGoldBar, kGoldBar, kCrown}));
    EXPECT_EQ(to, Inventory({kCrown, kPebble}));
    // Some of a stack leave the rest in its place.
    EXPECT_EQ(move_items(from, 0, 1, to), 2U);
    EXPECT_EQ(from, Inventory({kGoldBar, kCrown}));
    EXPECT_EQ(to, Inventory({kCrown, kPebble, kGoldBar}));
    // Into the stack of their kind.
    EXPECT_EQ(move_stack(from, 1, to), 0U);
    EXPECT_EQ(to.stacks()[0], (ItemStack{.item = kCrown, .count = 2}));
    EXPECT_THROW(move_items(from, 0, 2, to), std::invalid_argument);
    EXPECT_THROW(move_stack(from, 1, to), std::out_of_range);
    // Within one inventory, only whole stacks, to the end.
    EXPECT_EQ(move_stack(to, 0, to), 2U);
    EXPECT_EQ(to, Inventory({kPebble, kGoldBar, kCrown, kCrown}));
    EXPECT_THROW(move_items(to, 2, 1, to), std::invalid_argument);
}

}  // namespace
}  // namespace manifold
