#pragma once

#include <string_view>
#include <vector>

#include "items/item_catalog.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"

namespace manifold {

// What containers, such as chests, hold when a level starts: items drawn at random a random number of times, within
// limits of their values.

/// How a container is filled, see fill_container().
struct ContainerLoot {
    /// How many items are drawn for it: a number from min_draws to max_draws, both included. It gets at most that many
    /// items, and fewer if a draw ends the filling, see max_total_value.
    int min_draws = 0;
    int max_draws = 0;
    /// Only items worth from min_item_value to max_item_value, both included, are put into it, see ItemType::value.
    int min_item_value = 0;
    int max_item_value = 0;
    /// The most that its items may be worth together. A drawn item that would make them worth more ends the filling.
    /// It should be at least max_item_value, see check_container_loot().
    int max_total_value = 0;

    bool operator==(const ContainerLoot&) const = default;
};

/// How a wooden chest is filled: 1 to 3 draws of items worth 100 to 2000 each, worth at most 2000 together.
inline constexpr ContainerLoot kWoodenChestLoot{
    .min_draws = 1, .max_draws = 3, .min_item_value = 100, .max_item_value = 2000, .max_total_value = 2000};
/// How a golden chest is filled: 3 to 5 draws of items worth 100 to 5000 each, worth at most 6000 together.
inline constexpr ContainerLoot kGoldenChestLoot{
    .min_draws = 3, .max_draws = 5, .min_item_value = 100, .max_item_value = 5000, .max_total_value = 6000};

/// How a chest of `look` is filled: kWoodenChestLoot or kGoldenChestLoot.
const ContainerLoot& chest_loot(ChestLook look);

/// Chooses items for a container at random.
///
/// First the number of draws is chosen, from `loot.min_draws` to `loot.max_draws`, each number with the same chance.
/// Then each draw chooses an item, with a chance proportional to ItemType::chance among the items worth from
/// `loot.min_item_value` to `loot.max_item_value`. An item that would make the items worth more than
/// `loot.max_total_value` together is left out, and the filling stops there, so a container often gets fewer items
/// than draws, also fewer than `loot.min_draws`. Without items in that range that have a chance, nothing is chosen.
///
/// \param rng Random number generator. The same state gives the same items.
/// \return The chosen items, in the order they were chosen.
std::vector<ItemId> fill_container(const ItemCatalog& catalog, const ContainerLoot& loot, Rng& rng);

/// Checks that `loot` makes sense, and logs a warning for each problem, with `name` (e.g. "wooden chests"): a
/// max_total_value below max_item_value, with which a dear first draw leaves the container empty, and ranges whose
/// minimum is above their maximum.
///
/// \return Whether there is no problem.
bool check_container_loot(const ContainerLoot& loot, std::string_view name);

/// Chooses what a chest of `look` holds: fill_container() with chest_loot().
std::vector<ItemId> fill_chest(const ItemCatalog& catalog, ChestLook look, Rng& rng);

}  // namespace manifold
