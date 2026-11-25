#include "items/inventory.hpp"

#include <cstddef>

#include "items/item_catalog.hpp"

namespace manifold {

ItemId Inventory::take(std::size_t index) {
    const ItemId item = items_.at(index);
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
    return item;
}

long long Inventory::weight(const ItemCatalog& catalog) const {
    long long total = 0;
    for (const ItemId item : items_) {
        total += catalog.at(item).weight;
    }
    return total;
}

long long Inventory::value(const ItemCatalog& catalog) const {
    long long total = 0;
    for (const ItemId item : items_) {
        total += catalog.at(item).value;
    }
    return total;
}

std::size_t move_item(Inventory& from, std::size_t index, Inventory& to) {
    to.add(from.take(index));
    return to.size() - 1;
}

}  // namespace manifold
