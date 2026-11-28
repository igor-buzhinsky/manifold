#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "items/item_catalog.hpp"

namespace manifold {

/// Items of one kind that lie together in one place of an Inventory, e.g. 3 small rubies.
struct ItemStack {
    ItemId item = 0;
    int count = 1;  ///< How many, at least 1.

    bool operator==(const ItemStack&) const = default;
};

/// The items that the party or a container holds, in order: a list of stacks without gaps, each in one place, see
/// ItemStack.
///
/// Items of the same kind are always in one stack, so an inventory has at most one stack of each kind. A hash map from
/// the kind to the place of its stack keeps adding an item and finding its stack in constant time, however many
/// stacks there are, and a stack of any size takes the same memory. Items that differ in more than their kind, such as
/// enchanted ones later, will need a key of the kind and those properties instead of ItemId, so that they stack only
/// with items of the same properties.
class Inventory {
public:
    Inventory() = default;
    /// The items in stacks, in the order in which each kind first comes in `items`.
    explicit Inventory(const std::vector<ItemId>& items);

    /// The stacks, first to last.
    std::span<const ItemStack> stacks() const { return stacks_; }
    /// The number of stacks, which is the number of places that the items take.
    std::size_t size() const { return stacks_.size(); }
    bool empty() const { return stacks_.empty(); }
    /// The number of items of all stacks together.
    long long item_count() const;

    /// The place of the stack of `item`, if the inventory has one.
    std::optional<std::size_t> find(ItemId item) const;

    /// Puts `count` items of the kind `item` into the stack of that kind, or into a new stack at the end if there is
    /// none.
    ///
    /// \return The place of the stack.
    /// \throws std::invalid_argument if `count` is less than 1.
    /// \throws std::overflow_error if the stack would get more items than an int can count.
    std::size_t add(ItemId item, int count = 1);

    /// Takes `count` items out of the stack at `index`. A stack that loses all its items is removed, and the stacks
    /// after it move up by one.
    ///
    /// \return What was taken.
    /// \throws std::out_of_range if there is no such stack.
    /// \throws std::invalid_argument if `count` is less than 1 or more than the stack has.
    ItemStack take(std::size_t index, int count);

    /// Takes the whole stack at `index` out; the stacks after it move up by one.
    ///
    /// \throws std::out_of_range if there is no such stack.
    ItemStack take(std::size_t index);

    /// Moves the stack at `index` to the end; the stacks after it move up by one.
    ///
    /// \throws std::out_of_range if there is no such stack.
    void move_to_end(std::size_t index);

    /// The sum of the weights of the items, in grams, see ItemType::weight.
    ///
    /// \throws std::out_of_range if an item isn't in `catalog`.
    long long weight(const ItemCatalog& catalog) const;

    /// The sum of the values of the items, see ItemType::value.
    ///
    /// \throws std::out_of_range if an item isn't in `catalog`.
    long long value(const ItemCatalog& catalog) const;

    /// Equal if the stacks and their order are.
    bool operator==(const Inventory& other) const { return stacks_ == other.stacks_; }

private:
    /// Puts the places of the stacks from `first` on into `places_`, after stacks before them were removed or moved.
    void renumber_from(std::size_t first);

    std::vector<ItemStack> stacks_;
    /// The place in `stacks_` of the stack of each kind of item that the inventory has.
    std::unordered_map<ItemId, std::size_t> places_;
};

/// Moves `count` items of the stack at `index` of `from` to `to`, see Inventory::add(), so they join the stack of
/// their kind there, or start one at its end. `from` and `to` may be the same only if the whole stack is moved, which
/// moves it to the end.
///
/// \return The place of their stack in `to`.
/// \throws std::out_of_range if `from` has no such stack.
/// \throws std::invalid_argument if `count` is less than 1 or more than the stack has, or if `from` is `to` and
///         `count` is less than the whole stack.
std::size_t move_items(Inventory& from, std::size_t index, int count, Inventory& to);

/// Moves the whole stack at `index` of `from` to `to`, see move_items().
std::size_t move_stack(Inventory& from, std::size_t index, Inventory& to);

}  // namespace manifold
