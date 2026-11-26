#include "items/loot.hpp"

#include <cstddef>
#include <vector>

#include "items/item_catalog.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"

namespace manifold {

const ContainerLoot& chest_loot(ChestLook look) {
    return look == ChestLook::kGold ? kGoldenChestLoot : kWoodenChestLoot;
}

std::vector<ItemId> fill_container(const ItemCatalog& catalog, const ContainerLoot& loot, Rng& rng) {
    const int count = random_int(rng, loot.min_items, loot.max_items);
    std::vector<double> chances(catalog.items().size());
    bool any = false;
    for (std::size_t i = 0; i < chances.size(); ++i) {
        const ItemType& item = catalog.items()[i];
        const bool in_range = item.value >= loot.min_item_value && item.value <= loot.max_item_value;
        chances[i] = in_range ? item.chance : 0.0;
        any = any || chances[i] > 0.0;
    }
    std::vector<ItemId> chosen;
    if (!any) {
        return chosen;
    }
    long long total = 0;
    for (int i = 0; i < count; ++i) {
        const auto item = static_cast<ItemId>(random_weighted_index(rng, chances));
        total += catalog.at(item).value;
        if (total > loot.max_total_value) {
            break;
        }
        chosen.push_back(item);
    }
    return chosen;
}

std::vector<ItemId> fill_chest(const ItemCatalog& catalog, ChestLook look, Rng& rng) {
    return fill_container(catalog, chest_loot(look), rng);
}

}  // namespace manifold
