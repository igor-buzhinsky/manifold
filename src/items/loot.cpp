#include "items/loot.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

#include "items/item_catalog.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"

namespace manifold {

ValueRange chest_value_range(ChestLook look) {
    const int mean = look == ChestLook::kGold ? kGoldenChestValue : kWoodenChestValue;
    const auto spread = static_cast<int>(std::lround(mean * kChestValueSpread));
    return {.min = mean - spread, .max = mean + spread};
}

std::vector<ItemId> fill_container(const ItemCatalog& catalog, int value, std::size_t max_items, Rng& rng) {
    std::vector<ItemId> chosen;
    int left = value;
    std::vector<double> chances(catalog.items().size());
    while (chosen.size() < max_items) {
        bool any = false;
        for (std::size_t i = 0; i < chances.size(); ++i) {
            const ItemType& item = catalog.items()[i];
            chances[i] = item.value <= left ? item.chance : 0.0;
            any = any || chances[i] > 0.0;
        }
        if (!any) {
            break;
        }
        const auto item = static_cast<ItemId>(random_weighted_index(rng, chances));
        chosen.push_back(item);
        left -= catalog.at(item).value;
    }
    return chosen;
}

std::vector<ItemId> fill_chest(const ItemCatalog& catalog, ChestLook look, Rng& rng) {
    const ValueRange range = chest_value_range(look);
    const int value = random_int(rng, range.min, range.max);
    return fill_container(catalog, value, kMaxChestItems, rng);
}

}  // namespace manifold
