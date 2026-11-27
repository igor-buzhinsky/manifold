#include "items/item_catalog.hpp"

#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "util/asset_mapping.hpp"

namespace manifold {
namespace {

/// Parses `json` as an item file, and returns what was logged in `log`.
ItemCatalog parse(std::string_view json, std::string& log) {
    testing::internal::CaptureStderr();
    ItemCatalog catalog = ItemCatalog::parse(json, "test.json");
    log = testing::internal::GetCapturedStderr();
    return catalog;
}

TEST(ItemCatalogTest, ParsesItemsInTheOrderOfTheFile) {
    constexpr const char* kJson = R"({
        "zebra": {"name": "Zebra", "value": 3, "weight": 400000, "chance": 0.5},
        "ant": {"name": "Ant", "value": 1, "weight": 0, "chance": 0}
    })";
    std::string log;
    const ItemCatalog catalog = parse(kJson, log);
    EXPECT_EQ(log.find("[warning]"), std::string::npos) << log;
    ASSERT_EQ(catalog.items().size(), 2U);
    EXPECT_EQ(catalog.at(0), (ItemType{.key = "zebra", .name = "Zebra", .value = 3, .weight = 400000, .chance = 0.5}));
    EXPECT_EQ(catalog.at(1), (ItemType{.key = "ant", .name = "Ant", .value = 1, .weight = 0, .chance = 0.0}));
    EXPECT_EQ(catalog.find("ant"), ItemId{1});
    EXPECT_EQ(catalog.find("zebra"), ItemId{0});
    EXPECT_FALSE(catalog.find("Zebra").has_value());
    EXPECT_THROW(static_cast<void>(catalog.at(2)), std::out_of_range);
}

TEST(ItemCatalogTest, WrongItemsAreLeftOutWithAWarning) {
    constexpr const char* kJson = R"({
        "good": {"name": "Good", "value": 1, "weight": 1, "chance": 1},
        "not_an_object": 5,
        "no_name": {"value": 1, "weight": 1, "chance": 1},
        "empty_name": {"name": "", "value": 1, "weight": 1, "chance": 1},
        "free": {"name": "Free", "value": 0, "weight": 1, "chance": 1},
        "fraction": {"name": "Fraction", "value": 1.5, "weight": 1, "chance": 1},
        "negative_weight": {"name": "Negative", "value": 1, "weight": -1, "chance": 1},
        "negative_chance": {"name": "Negative", "value": 1, "weight": 1, "chance": -0.5},
        "text_chance": {"name": "Text", "value": 1, "weight": 1, "chance": "often"}
    })";
    std::string log;
    const ItemCatalog catalog = parse(kJson, log);
    ASSERT_EQ(catalog.items().size(), 1U);
    EXPECT_EQ(catalog.at(0).key, "good");
    for (const char* message :
         {"not_an_object is 5, not an object", "no_name has no name, not a name that isn't empty",
          R"(empty_name has a name "", not)", "free has a value 0, not a positive integer value",
          "fraction has a value 1.5", "negative_weight has a weight -1, not a weight of 0",
          "negative_chance has a chance -0.5, not a chance of 0 or more", R"(text_chance has a chance "often")"}) {
        EXPECT_NE(log.find(message), std::string::npos) << message << "\n" << log;
    }
}

TEST(ItemCatalogTest, TextThatIsNotAnObjectGivesNoItems) {
    for (const std::string_view json : {"", "not json", R"(["small_ruby"])"}) {
        std::string log;
        EXPECT_TRUE(parse(json, log).items().empty()) << json;
        EXPECT_NE(log.find("[warning] Items test.json are not"), std::string::npos) << log;
    }
}

TEST(ItemCatalogTest, MissingFileGivesNoItems) {
    testing::internal::CaptureStderr();
    const ItemCatalog catalog =
        ItemCatalog::load(std::filesystem::temp_directory_path() / "manifold_ItemCatalogTest_missing");
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(catalog.items().empty());
    EXPECT_NE(log.find("[warning] Can't read the items, so there are none"), std::string::npos) << log;
}

TEST(ItemCatalogTest, KeysMustBeUnique) {
    const ItemType item{.key = "A", .name = "A", .value = 1, .weight = 1, .chance = 1.0};
    EXPECT_THROW(ItemCatalog({item, item}), std::invalid_argument);
}

TEST(ItemCatalogTest, WeightsAreShownInKilogramsWithThreeDecimals) {
    EXPECT_EQ(format_weight(0), "0.000");
    EXPECT_EQ(format_weight(4), "0.004");
    EXPECT_EQ(format_weight(1250), "1.250");
    EXPECT_EQ(format_weight(123456), "123.456");
    EXPECT_EQ(format_weight(-5), "-0.005");
}

/// The items of the game, from its data.
ItemCatalog load_game_items(std::string& log) {
    testing::internal::CaptureStderr();
    ItemCatalog catalog = ItemCatalog::load(MANIFOLD_DATA_DIR);
    log = testing::internal::GetCapturedStderr();
    return catalog;
}

TEST(GameItemsTest, ItemsOfTheGameLoadAndEachHasAnIcon) {
    std::string log;
    const ItemCatalog catalog = load_game_items(log);
    EXPECT_EQ(log.find("[warning]"), std::string::npos) << log;
    EXPECT_FALSE(catalog.items().empty());
    testing::internal::CaptureStderr();
    const AssetMapping mapping = AssetMapping::load(MANIFOLD_ASSETS_DIR);
    testing::internal::GetCapturedStderr();
    std::set<std::string> keys;
    for (const ItemType& item : catalog.items()) {
        EXPECT_TRUE(mapping.item_icon(item.key).has_value()) << item.key;
        keys.insert(item.key);
    }
    for (const auto& [key, path] : mapping.item_icons()) {
        EXPECT_TRUE(keys.contains(key)) << key << " has an icon but is no item";
    }
}

TEST(GameItemsTest, LargeGemsAreWorthAndWeighFourTimesTheSmallOnes) {
    std::string log;
    const ItemCatalog catalog = load_game_items(log);
    for (const char* gem : {"diamond", "ruby", "emerald", "sapphire", "amethyst"}) {
        const std::optional<ItemId> small = catalog.find(std::string("small_") + gem);
        const std::optional<ItemId> large = catalog.find(std::string("large_") + gem);
        ASSERT_TRUE(small && large) << gem;
        EXPECT_EQ(catalog.at(*large).value, 4 * catalog.at(*small).value) << gem;
        EXPECT_EQ(catalog.at(*large).weight, 4 * catalog.at(*small).weight) << gem;
    }
    EXPECT_EQ(catalog.at(*catalog.find("large_diamond")).value, 5000);
}

}  // namespace
}  // namespace manifold
