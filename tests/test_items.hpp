#pragma once

#include "items/item_catalog.hpp"

namespace manifold {

/// Items for tests: a cheap one, a dear one, and one that is never put into containers.
inline ItemCatalog make_test_items() {
    return ItemCatalog({
        {.key = "pebble", .name = "Pebble", .value = 100, .weight = 250, .chance = 1.0},
        {.key = "gold_bar", .name = "Gold bar", .value = 1500, .weight = 1000, .chance = 2.0},
        {.key = "crown", .name = "Crown", .value = 200, .weight = 1500, .chance = 0.0},
    });
}

/// The ItemId of each item of make_test_items().
inline constexpr ItemId kPebble = 0;
inline constexpr ItemId kGoldBar = 1;
inline constexpr ItemId kCrown = 2;

}  // namespace manifold
