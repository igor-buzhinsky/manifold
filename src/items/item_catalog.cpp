#include "items/item_catalog.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "util/log.hpp"
#include "util/text_file.hpp"

namespace manifold {

namespace {

/// Whether `value` is an integer from `min` to `max`.
bool is_int_in(const nlohmann::ordered_json& value, std::int64_t min, std::int64_t max) {
    if (!value.is_number_integer()) {
        return false;
    }
    // A large unsigned number doesn't fit into std::int64_t, but is out of range anyway.
    if (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(max)) {
        return false;
    }
    const auto number = value.get<std::int64_t>();
    return number >= min && number <= max;
}

/// Reads the item `key` of an item file.
///
/// \return The item, or nothing (after logging a warning with `source`) if it isn't an object or lacks a field or has
///         a wrong one.
std::optional<ItemType> parse_item(const std::string& key, const nlohmann::ordered_json& object,
                                   std::string_view source) {
    if (!object.is_object()) {
        log_warning("Items {}: {} is {}, not an object, so it is left out", source, key, object.dump());
        return std::nullopt;
    }
    // Each field with what it must be, or nothing if it is right.
    const auto wrong = [&object](const char* name, bool valid) -> std::optional<std::string> {
        if (valid) {
            return std::nullopt;
        }
        const auto found = object.find(name);
        return found == object.end() ? std::string("no ") + name : std::string("a ") + name + " " + found->dump();
    };
    const auto field = [&object](const char* name) -> const nlohmann::ordered_json& {
        static const nlohmann::ordered_json missing;
        const auto found = object.find(name);
        return found == object.end() ? missing : *found;
    };
    const nlohmann::ordered_json& name = field("name");
    const nlohmann::ordered_json& value = field("value");
    const nlohmann::ordered_json& weight = field("weight");
    const nlohmann::ordered_json& chance = field("chance");
    const std::array<std::pair<std::optional<std::string>, const char*>, 4> checks{{
        {wrong("name", name.is_string() && !name.get<std::string>().empty()), "a name that isn't empty"},
        {wrong("value", is_int_in(value, 1, std::numeric_limits<int>::max())), "a positive integer value"},
        {wrong("weight", is_int_in(weight, 0, std::numeric_limits<int>::max())), "a weight of 0 or more grams"},
        {wrong("chance", chance.is_number() && std::isfinite(chance.get<double>()) && chance.get<double>() >= 0.0),
         "a chance of 0 or more"},
    }};
    for (const auto& [problem, expected] : checks) {
        if (problem) {
            log_warning("Items {}: {} has {}, not {}, so it is left out", source, key, *problem, expected);
            return std::nullopt;
        }
    }
    return ItemType{.key = key,
                    .name = name.get<std::string>(),
                    .value = value.get<int>(),
                    .weight = weight.get<int>(),
                    .chance = chance.get<double>()};
}

}  // namespace

ItemCatalog::ItemCatalog(std::vector<ItemType> items) : items_(std::move(items)) {
    if (items_.size() > static_cast<std::size_t>(std::numeric_limits<ItemId>::max()) + 1) {
        throw std::invalid_argument(std::format("too many items: {}", items_.size()));
    }
    std::set<std::string_view> keys;
    for (const ItemType& item : items_) {
        if (!keys.insert(item.key).second) {
            throw std::invalid_argument("two items have the key " + item.key);
        }
    }
}

ItemCatalog ItemCatalog::parse(std::string_view json, std::string_view source) {
    nlohmann::ordered_json object;
    try {
        object = nlohmann::ordered_json::parse(json);
    } catch (const nlohmann::ordered_json::parse_error& error) {
        log_warning("Items {} are not valid JSON, so there are no items: {}", source, error.what());
        return {};
    }
    if (!object.is_object()) {
        log_warning("Items {} are not a JSON object, so there are no items", source);
        return {};
    }
    std::vector<ItemType> items;
    for (const auto& [key, value] : object.items()) {
        if (items.size() > std::numeric_limits<ItemId>::max()) {
            log_warning("Items {} has more than {} items, so the rest are left out", source,
                        static_cast<std::size_t>(std::numeric_limits<ItemId>::max()) + 1);
            break;
        }
        if (std::optional<ItemType> item = parse_item(key, value, source)) {
            items.push_back(std::move(*item));
        }
    }
    // A JSON object has each key once, as nlohmann::json keeps only the last of keys that repeat.
    return ItemCatalog(std::move(items));
}

ItemCatalog ItemCatalog::load(const std::filesystem::path& data_dir) {
    const std::filesystem::path file = data_dir / kItemsFileName;
    std::string json;
    try {
        json = read_text_file(file);
    } catch (const std::runtime_error& error) {
        log_warning("Can't read the items, so there are none: {}", error.what());
        return {};
    }
    ItemCatalog catalog = parse(json, file.string());
    log_info("Items {}: {} items", file.string(), catalog.items().size());
    return catalog;
}

std::optional<ItemId> ItemCatalog::find(std::string_view key) const {
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].key == key) {
            return static_cast<ItemId>(i);
        }
    }
    return std::nullopt;
}

std::string format_weight(long long grams) {
    constexpr long long kGramsPerKilogram = 1000;
    const char* sign = grams < 0 ? "-" : "";
    const unsigned long long magnitude =
        grams < 0 ? 0ULL - static_cast<unsigned long long>(grams) : static_cast<unsigned long long>(grams);
    return std::format("{}{}.{:03}", sign, magnitude / kGramsPerKilogram, magnitude % kGramsPerKilogram);
}

}  // namespace manifold
