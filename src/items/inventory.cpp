#include "items/inventory.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "items/item_catalog.hpp"

namespace manifold {

namespace {

/// Whether a stack of `count` items can get `more` without counting more than an int can.
bool fits(int count, int more) { return more <= std::numeric_limits<int>::max() - count; }

}  // namespace

Inventory::Inventory(const std::vector<ItemId>& items) {
    for (const ItemId item : items) {
        add(item);
    }
}

long long Inventory::item_count() const {
    long long total = 0;
    for (const ItemStack& stack : stacks_) {
        total += stack.count;
    }
    return total;
}

std::optional<std::size_t> Inventory::find(ItemId item) const {
    if (const auto place = places_.find(item); place != places_.end()) {
        return place->second;
    }
    return std::nullopt;
}

std::size_t Inventory::add(ItemId item, int count) {
    if (count < 1) {
        throw std::invalid_argument("an inventory can only get a positive number of items");
    }
    if (const std::optional<std::size_t> place = find(item)) {
        ItemStack& stack = stacks_[*place];
        if (!fits(stack.count, count)) {
            throw std::overflow_error("a stack can't hold that many items");
        }
        stack.count += count;
        return *place;
    }
    places_.emplace(item, stacks_.size());
    stacks_.push_back({.item = item, .count = count});
    return stacks_.size() - 1;
}

ItemStack Inventory::take(std::size_t index, int count) {
    ItemStack& stack = stacks_.at(index);
    if (count < 1 || count > stack.count) {
        throw std::invalid_argument("a stack can only give from 1 item to all it has");
    }
    if (count < stack.count) {
        stack.count -= count;
        return {.item = stack.item, .count = count};
    }
    const ItemStack taken = stack;
    places_.erase(taken.item);
    stacks_.erase(stacks_.begin() + static_cast<std::ptrdiff_t>(index));
    renumber_from(index);
    return taken;
}

ItemStack Inventory::take(std::size_t index) { return take(index, stacks_.at(index).count); }

void Inventory::move_to_end(std::size_t index) {
    const ItemStack stack = stacks_.at(index);
    stacks_.erase(stacks_.begin() + static_cast<std::ptrdiff_t>(index));
    stacks_.push_back(stack);
    renumber_from(index);
}

void Inventory::renumber_from(std::size_t first) {
    for (std::size_t i = first; i < stacks_.size(); ++i) {
        places_[stacks_[i].item] = i;
    }
}

long long Inventory::weight(const ItemCatalog& catalog) const {
    long long total = 0;
    for (const ItemStack& stack : stacks_) {
        total += static_cast<long long>(catalog.at(stack.item).weight) * stack.count;
    }
    return total;
}

long long Inventory::value(const ItemCatalog& catalog) const {
    long long total = 0;
    for (const ItemStack& stack : stacks_) {
        total += static_cast<long long>(catalog.at(stack.item).value) * stack.count;
    }
    return total;
}

std::size_t move_items(Inventory& from, std::size_t index, int count, Inventory& to) {
    if (index >= from.size()) {
        throw std::out_of_range("no such stack");
    }
    const ItemStack stack = from.stacks()[index];
    if (count < 1 || count > stack.count) {
        throw std::invalid_argument("a stack can only give from 1 item to all it has");
    }
    if (&from == &to) {
        if (count != stack.count) {
            throw std::invalid_argument("only a whole stack can be moved within an inventory");
        }
        from.move_to_end(index);
        return from.size() - 1;
    }
    // Checked before taking, so that a stack that can't take them loses nothing.
    if (const std::optional<std::size_t> place = to.find(stack.item);
        place && !fits(to.stacks()[*place].count, count)) {
        throw std::overflow_error("a stack can't hold that many items");
    }
    from.take(index, count);
    return to.add(stack.item, count);
}

std::size_t move_stack(Inventory& from, std::size_t index, Inventory& to) {
    if (index >= from.size()) {
        throw std::out_of_range("no such stack");
    }
    return move_items(from, index, from.stacks()[index].count, to);
}

}  // namespace manifold
