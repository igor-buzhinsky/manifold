#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace manifold {

/// Which item of an ItemCatalog: its position in ItemCatalog::items().
using ItemId = std::uint16_t;

/// A kind of item that the party can carry, such as a small ruby, with what the game needs to know about it.
struct ItemType {
    /// The item's key in the item file and in saves, e.g. `small_ruby`. Its icon is found by it too, see
    /// AssetMapping::item_icon().
    std::string key;
    std::string name;  ///< What the player sees, e.g. "Small ruby".
    /// What the item is worth, which chooses what containers hold, see fill_container(). Hidden from the player.
    int value = 0;
    int weight = 0;  ///< In grams, see format_weight().
    /// How often the item is chosen for a container, relative to the other items in the container's range of values,
    /// see fill_container(). The chances of all items needn't add up to 1. An item with no chance is never put into a
    /// container.
    double chance = 0.0;

    bool operator==(const ItemType&) const = default;
};

/// Name of the file of the items, in the directory of the game's data, see kDataDirName.
inline constexpr std::string_view kItemsFileName = "items.json";

/// All kinds of items of the game, read from a JSON file, so that items can be added and changed without building the
/// game again.
///
/// The file is an object whose fields are the keys of the items, each an object with `name` (a string that isn't
/// empty), `value` (a positive integer), `weight` (grams, an integer that isn't negative) and `chance` (a finite number
/// that isn't negative), e.g. `{"small_ruby": {"name": "Small ruby", "value": 1000, "weight": 1, "chance": 1}}`.
/// The items keep the order of the file, which the random choices of containers depend on, see fill_container().
class ItemCatalog {
public:
    /// A catalog without items.
    ItemCatalog() = default;

    /// A catalog of these items, e.g. for tests.
    ///
    /// \throws std::invalid_argument if there are more than can have an ItemId, or two have the same key.
    explicit ItemCatalog(std::vector<ItemType> items);

    /// Reads a catalog from the text of its JSON file.
    ///
    /// Problems don't stop it, but are logged as warnings, with `source` (e.g. the file's path): text that isn't a
    /// JSON object gives a catalog without items, and an item that isn't an object or lacks a field or has a wrong one
    /// is left out.
    static ItemCatalog parse(std::string_view json, std::string_view source);

    /// Reads the items of the game: kItemsFileName in `data_dir`, see parse(). A file that can't be read gives a
    /// catalog without items, after a warning. How many items there are is logged as info.
    static ItemCatalog load(const std::filesystem::path& data_dir);

    /// All items, in the order of the file: the position of each is its ItemId.
    std::span<const ItemType> items() const { return items_; }

    /// The item `id`.
    ///
    /// \throws std::out_of_range if there is no such item.
    const ItemType& at(ItemId id) const { return items_.at(id); }

    /// The item with the key `key`, if there is one.
    std::optional<ItemId> find(std::string_view key) const;

    bool operator==(const ItemCatalog&) const = default;

private:
    std::vector<ItemType> items_;
};

/// A weight of `grams` as the game shows it: in kilograms with three decimals, without a unit, e.g. "1.250" for 1250
/// grams and "0.004" for 4 grams.
std::string format_weight(long long grams);

}  // namespace manifold
