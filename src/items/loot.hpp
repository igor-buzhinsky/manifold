#pragma once

#include <vector>

#include "items/item_catalog.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"

namespace manifold {

// What containers, such as chests, hold when a level starts: a random number of items, chosen at random within limits
// of their values.

/// How a container is filled, see fill_container().
struct ContainerLoot {
    int min_items = 0;  ///< The fewest items it gets, unless max_total_value stops the filling first.
    int max_items = 0;  ///< The most items it gets.
    /// Only items worth from min_item_value to max_item_value, both included, are put into it, see ItemType::value.
    int min_item_value = 0;
    int max_item_value = 0;
    /// The most that its items may be worth together.
    int max_total_value = 0;

    bool operator==(const ContainerLoot&) const = default;
};

/// How a wooden chest is filled: 1 to 3 items, each worth 100 to 2000, worth at most 2000 together.
inline constexpr ContainerLoot kWoodenChestLoot{
    .min_items = 1, .max_items = 3, .min_item_value = 100, .max_item_value = 2000, .max_total_value = 2000};
/// How a golden chest is filled: 3 to 5 items, each worth 100 to 5000, worth at most 6000 together.
inline constexpr ContainerLoot kGoldenChestLoot{
    .min_items = 3, .max_items = 5, .min_item_value = 100, .max_item_value = 5000, .max_total_value = 6000};

/// How a chest of `look` is filled: kWoodenChestLoot or kGoldenChestLoot.
const ContainerLoot& chest_loot(ChestLook look);

/// Chooses items for a container at random.
///
/// First the number of items is chosen, from `loot.min_items` to `loot.max_items`, each number with the same chance.
/// Then the items are chosen one by one, each with a chance proportional to ItemType::chance among the items worth
/// from `loot.min_item_value` to `loot.max_item_value`. An item that would make the items worth more than
/// `loot.max_total_value` together is left out, and the filling stops there, also if there are fewer than
/// `loot.min_items` items. Without items in that range that have a chance, nothing is chosen.
///
/// \param rng Random number generator. The same state gives the same items.
/// \return The chosen items, in the order they were chosen.
std::vector<ItemId> fill_container(const ItemCatalog& catalog, const ContainerLoot& loot, Rng& rng);

/// Chooses what a chest of `look` holds: fill_container() with chest_loot().
std::vector<ItemId> fill_chest(const ItemCatalog& catalog, ChestLook look, Rng& rng);

}  // namespace manifold
