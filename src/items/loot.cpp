#include "items/loot.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

#include "items/item_catalog.hpp"
#include "util/log.hpp"
#include "util/random.hpp"
#include "world/chest.hpp"

namespace manifold {

std::vector<ItemId> fill_container(const ItemCatalog& catalog, const ContainerLoot& loot, Rng& rng) {
    const int count = random_int(rng, loot.min_draws, loot.max_draws);
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

bool check_container_loot(const ContainerLoot& loot, std::string_view name) {
    bool fine = true;
    if (loot.max_total_value < loot.max_item_value) {
        log_warning(
            "Loot of {}: the total value {} is below the largest value of an item, {}, so a dear item drawn first "
            "leaves the container empty",
            name, loot.max_total_value, loot.max_item_value);
        fine = false;
    }
    if (loot.min_draws > loot.max_draws || loot.min_draws < 0) {
        log_warning("Loot of {}: the draws from {} to {} are not a range", name, loot.min_draws, loot.max_draws);
        fine = false;
    }
    if (loot.min_item_value > loot.max_item_value) {
        log_warning("Loot of {}: the values of items from {} to {} are not a range", name, loot.min_item_value,
                    loot.max_item_value);
        fine = false;
    }
    return fine;
}

std::vector<ItemId> fill_chest(const ItemCatalog& catalog, const ChestLoot& loot, ChestLook look, Rng& rng) {
    return fill_container(catalog, loot.of(look), rng);
}

}  // namespace manifold
