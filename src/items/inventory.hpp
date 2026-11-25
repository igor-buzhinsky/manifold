#pragma once

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "items/item_catalog.hpp"

namespace manifold {

/// The items that the party or a container holds, in order: a list without gaps, in which each item is one place.
/// Items of the same kind are not stacked, so each takes a place of its own.
class Inventory {
public:
    Inventory() = default;
    explicit Inventory(std::vector<ItemId> items) : items_(std::move(items)) {}

    /// The items, first to last.
    std::span<const ItemId> items() const { return items_; }
    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    /// Puts an item at the end.
    void add(ItemId item) { items_.push_back(item); }

    /// Takes the item at `index` out; the items after it move up by one.
    ///
    /// \throws std::out_of_range if there is no such item.
    ItemId take(std::size_t index);

    /// The sum of the weights of the items, in grams, see ItemType::weight.
    ///
    /// \throws std::out_of_range if an item isn't in `catalog`.
    long long weight(const ItemCatalog& catalog) const;

    /// The sum of the values of the items, see ItemType::value.
    ///
    /// \throws std::out_of_range if an item isn't in `catalog`.
    long long value(const ItemCatalog& catalog) const;

    bool operator==(const Inventory&) const = default;

private:
    std::vector<ItemId> items_;
};

/// Moves the item at `index` of `from` to the end of `to`. The items after it in `from` move up by one. `from` and `to`
/// may be the same, which moves the item to its end.
///
/// \return The new index of the item in `to`.
/// \throws std::out_of_range if `from` has no such item.
std::size_t move_item(Inventory& from, std::size_t index, Inventory& to);

}  // namespace manifold
