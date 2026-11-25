#pragma once

#include <cstddef>
#include <vector>

#include "items/item_catalog.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"

namespace manifold {

// What containers, such as chests, hold when a level starts: items chosen at random, up to a value that is chosen at
// random too.

/// The mean value of what a wooden chest holds, see chest_value_range().
inline constexpr int kWoodenChestValue = 1000;
/// The mean value of what a golden chest holds, see chest_value_range().
inline constexpr int kGoldenChestValue = 4000;
/// How far the value of what a chest holds may be from its mean, as a part of it: 50%.
inline constexpr double kChestValueSpread = 0.5;
/// The most items a chest holds.
inline constexpr std::size_t kMaxChestItems = 8;

/// The smallest and the largest value of what a chest holds, both possible.
struct ValueRange {
    int min = 0;
    int max = 0;
};

/// The values that what a chest of `look` holds may have: its mean (kWoodenChestValue or kGoldenChestValue) less or
/// more kChestValueSpread of it, from 500 to 1500 for a wooden chest and from 2000 to 6000 for a golden one.
ValueRange chest_value_range(ChestLook look);

/// Chooses items for a container, at random, up to a value.
///
/// Items are added one by one, while there are fewer than `max_items`: each is chosen with a chance proportional to
/// ItemType::chance among the items that keep the value of all chosen items at most `value`, so the value is never
/// exceeded. When no item fits any more, or none that fits has a chance, the filling stops.
///
/// \param catalog   The items to choose among.
/// \param value     The most that the chosen items may be worth together, see ItemType::value.
/// \param max_items The most items to choose.
/// \param rng       Random number generator. The same state gives the same items.
/// \return The chosen items, in the order they were chosen.
std::vector<ItemId> fill_container(const ItemCatalog& catalog, int value, std::size_t max_items, Rng& rng);

/// Chooses what a chest of `look` holds: a value at random from chest_value_range(), each value with the same chance,
/// and then at most kMaxChestItems items up to it, see fill_container().
std::vector<ItemId> fill_chest(const ItemCatalog& catalog, ChestLook look, Rng& rng);

}  // namespace manifold
