#include "data/game_data.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <string_view>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "game.hpp"
#include "items/loot.hpp"
#include "level/layout_factory.hpp"
#include "level/level.hpp"
#include "party/spells.hpp"
#include "render/lighting.hpp"
#include "util/text_file.hpp"

namespace manifold {
namespace {

using Json = nlohmann::ordered_json;

/// The data file `name` of the game, as JSON, which the tests change to check one thing.
Json shipped(std::string_view name) {
    return Json::parse(read_text_file(std::filesystem::path(MANIFOLD_DATA_DIR) / name));
}

/// The message of the GameDataError that `parse` throws, or a failure if it throws none.
template <typename Parse>
std::string problems_of(const Parse& parse) {
    try {
        parse();
    } catch (const GameDataError& error) {
        return error.what();
    }
    ADD_FAILURE() << "no GameDataError";
    return {};
}

/// Whether `text` contains `part`, with a message that shows the text otherwise.
testing::AssertionResult contains(const std::string& text, std::string_view part) {
    if (text.find(part) != std::string::npos) {
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure() << "\"" << part << "\" isn't in:\n" << text;
}

TEST(GameDataTest, TheGameLoadsItsDataFilesWithoutWarnings) {
    testing::internal::CaptureStderr();
    const GameData data = load_game_data(MANIFOLD_DATA_DIR);
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(log.find("[warning]"), std::string::npos) << log;
    EXPECT_FALSE(data.game.level.items.items().empty());
    for (const SpellType& spell : data.game.magic.spells) {
        EXPECT_FALSE(spell.name.empty());
    }
}

TEST(GameDataTest, ReadsTheSpells) {
    Json json = shipped(kSpellsFileName);
    json["levitate"] = {{"name", "Float"}, {"cost", 2.5}, {"duration", 30}};
    MagicConfig magic;
    parse_spells(json.dump(), "spells.json", magic);
    EXPECT_EQ(magic.spell(Spell::kLevitate), (SpellType{.name = "Float", .cost = 2.5f, .duration = 30.0f}));
}

TEST(GameDataTest, ListsEveryProblemOfTheSpells) {
    Json json = shipped(kSpellsFileName);
    json.erase("levitate");
    json["candlelight"]["cost"] = -1;
    json["candlelight"]["name"] = "";
    json["candlelight"].erase("duration");
    json["candlelight"]["colour"] = "white";
    json["fireball"] = Json::object();
    MagicConfig magic;
    const std::string problems = problems_of([&] { parse_spells(json.dump(), "spells.json", magic); });
    EXPECT_TRUE(contains(problems, "spells.json: levitate is missing"));
    EXPECT_TRUE(contains(problems, "spells.json: candlelight.name is \"\", not a string that isn't empty"));
    EXPECT_TRUE(contains(problems, "spells.json: candlelight.cost is -1, not a number of 0 or more"));
    EXPECT_TRUE(contains(problems, "spells.json: candlelight.duration is missing"));
    EXPECT_TRUE(contains(problems, "spells.json: candlelight.colour is unknown"));
    EXPECT_TRUE(contains(problems, "spells.json: fireball is unknown"));
}

TEST(GameDataTest, RefusesTextThatIsNotAJsonObject) {
    MagicConfig magic;
    EXPECT_TRUE(contains(problems_of([&] { parse_spells("{", "spells.json", magic); }),
                         "spells.json: the file is not valid JSON"));
    EXPECT_EQ(problems_of([&] { parse_spells("[]", "spells.json", magic); }),
              "spells.json: the file is not a JSON object");
}

TEST(GameDataTest, ReadsTheLevelLayouts) {
    Json json = shipped(kLevelLayoutsFileName);
    json["rooms"]["probability"] = 0.75;
    json["rooms"]["layer_count"] = 5;
    json["rooms"]["tile_size"] = 2.5;
    json["debug"]["probability"] = 0.25;
    json["debug"]["torches_per_chest"] = 0;
    LevelConfig level;
    parse_level_layouts(json.dump(), "level_layouts.json", level);
    EXPECT_FLOAT_EQ(level.rooms_probability, 0.75f);
    EXPECT_EQ(level.rooms.layer_count, 5);
    EXPECT_FLOAT_EQ(level.rooms.tile_size, 2.5f);
    EXPECT_FLOAT_EQ(level.debug_probability, 0.25f);
    EXPECT_EQ(level.debug.torches_per_chest, 0);
}

TEST(GameDataTest, ListsEveryProblemOfTheLevelLayouts) {
    Json json = shipped(kLevelLayoutsFileName);
    json["rooms"]["min_holes"] = 4;
    json["rooms"]["max_holes"] = 3;
    json["rooms"]["layer_count"] = 1.5;
    json["debug"]["x_min"] = 20;
    json["debug"]["x_max"] = 20;
    json["debug"].erase("ramp_width");
    LevelConfig level;
    const std::string problems = problems_of([&] { parse_level_layouts(json.dump(), "layouts.json", level); });
    EXPECT_TRUE(contains(problems, "layouts.json: rooms.layer_count is 1.5, not an integer from 1 to 1000"));
    // The orders of an object are checked only once all its fields are right.
    EXPECT_FALSE(contains(problems, "min_holes"));
    EXPECT_TRUE(contains(problems, "layouts.json: debug.ramp_width is missing"));

    json["rooms"]["layer_count"] = 2;
    json["debug"]["ramp_width"] = 2;
    const std::string orders = problems_of([&] { parse_level_layouts(json.dump(), "layouts.json", level); });
    EXPECT_TRUE(contains(orders, "layouts.json: rooms.min_holes 4 is above rooms.max_holes 3"));
    EXPECT_TRUE(contains(orders, "layouts.json: debug.x_min 20 is not below debug.x_max 20"));
}

TEST(GameDataTest, NeedsALayoutWithAPositiveProbability) {
    Json json = shipped(kLevelLayoutsFileName);
    json["rooms"]["probability"] = 0;
    json["debug"]["probability"] = 0;
    LevelConfig level;
    EXPECT_EQ(problems_of([&] { parse_level_layouts(json.dump(), "layouts.json", level); }),
              "layouts.json: the file has no layout with a positive probability");
}

TEST(GameDataTest, ReadsTheConstants) {
    Json json = shipped(kConstantsFileName);
    json["player"]["turn_speed"] = 180;
    json["player"]["walk_speed"] = 3;
    json["party"]["size"] = 4;
    json["party"]["mana_regeneration"] = 0;
    json["level_objects"]["torch_flames"] = {{"orange", 0}, {"blue", 1}, {"green", 3}};
    json["level_objects"]["golden_chest"]["max_total_value"] = 9000;
    json["lighting"]["candlelight"]["color"] = {0.5, 0.25, 2};
    json["lighting"]["torch"]["range"] = 12;
    GameConfig game;
    LightSettings lights;
    parse_constants(json.dump(), "constants.json", game, lights);
    EXPECT_FLOAT_EQ(game.turn_speed, std::numbers::pi_v<float>);
    EXPECT_FLOAT_EQ(game.walk_speed, 3.0f);
    EXPECT_EQ(game.party.size, 4U);
    EXPECT_FLOAT_EQ(game.magic.mana_regeneration, 0.0f);
    EXPECT_EQ(game.level.object_chances.torch_flames, (std::array<double, 3>{0.0, 1.0, 3.0}));
    EXPECT_EQ(game.level.chest_loot.golden.max_total_value, 9000);
    EXPECT_EQ(lights.player_color, glm::vec3(0.5f, 0.25f, 2.0f));
    EXPECT_FLOAT_EQ(lights.torch.range, 12.0f);
}

TEST(GameDataTest, ListsEveryProblemOfTheConstants) {
    Json json = shipped(kConstantsFileName);
    json["player"]["radius"] = 0;
    json["party"]["size"] = "three";
    json["level_objects"]["torch_flames"] = {{"orange", 0}, {"blue", 0}, {"green", 0}};
    json["level_objects"]["wooden_chest"]["min_draws"] = 4;
    json["level_objects"]["wooden_chest"]["max_draws"] = 3;
    json["lighting"]["candlelight"]["color"] = {1, 1};
    json["lighting"]["exit"].erase("range");
    json["lighting"]["shadows"] = true;
    json["lighting"]["torch"]["range"] = 0.05;
    GameConfig game;
    LightSettings lights;
    const std::string problems = problems_of([&] { parse_constants(json.dump(), "constants.json", game, lights); });
    EXPECT_TRUE(contains(problems, "constants.json: player.radius is 0, not a positive number"));
    EXPECT_TRUE(contains(problems, "constants.json: party.size is \"three\", not an integer from 1 to 6"));
    EXPECT_TRUE(contains(problems, "constants.json: level_objects.torch_flames has no flame with a positive weight"));
    EXPECT_TRUE(contains(problems,
                         "constants.json: level_objects.wooden_chest.min_draws 4 is above "
                         "level_objects.wooden_chest.max_draws 3"));
    EXPECT_TRUE(contains(problems, "constants.json: lighting.candlelight.color is [1,1], not an array of 3 numbers"));
    EXPECT_TRUE(contains(problems, "constants.json: lighting.exit.range is missing"));
    EXPECT_TRUE(contains(problems, "constants.json: lighting.shadows is unknown"));
    EXPECT_TRUE(contains(problems, "constants.json: lighting.torch.range is 0.05, not a number of 0.1 or more"));
}

TEST(GameDataTest, ChecksTheLimitsOfTheConstants) {
    Json json = shipped(kConstantsFileName);
    json["player"]["walk_speed"] = 5;
    json["player"]["run_speed"] = 4;
    // Positive, but 0 as a float.
    json["player"]["radius"] = 1e-50;
    json["party"]["size"] = 7;
    GameConfig game;
    LightSettings lights;
    std::string problems = problems_of([&] { parse_constants(json.dump(), "constants.json", game, lights); });
    EXPECT_TRUE(contains(problems, "constants.json: player.radius is 1e-50, not a positive number"));
    EXPECT_TRUE(contains(problems, "constants.json: party.size is 7, not an integer from 1 to 6"));

    json["player"]["radius"] = 0.5;
    json["party"]["size"] = 6;
    problems = problems_of([&] { parse_constants(json.dump(), "constants.json", game, lights); });
    EXPECT_EQ(problems, "constants.json: player.walk_speed 5 is above player.run_speed 4");
}

TEST(GameDataTest, ChecksTheLimitsOfTheLevelLayouts) {
    Json json = shipped(kLevelLayoutsFileName);
    json["rooms"]["grid_width"] = 20;
    json["rooms"]["grid_height"] = 30;
    json["rooms"]["min_room_size"] = 1;
    json["rooms"]["max_room_size"] = 19;
    json["debug"]["obstacle_count"] = 1000000;
    json["debug"]["x_max"] = 1e300;
    LevelConfig level;
    const std::string problems = problems_of([&] { parse_level_layouts(json.dump(), "layouts.json", level); });
    EXPECT_TRUE(contains(problems,
                         "layouts.json: rooms has rooms of up to 19 tiles, which don't fit with their walls "
                         "into a grid of 20 x 30 tiles"));
    EXPECT_TRUE(contains(problems, "layouts.json: debug.obstacle_count is 1000000, not an integer from 0 to 1000"));
    EXPECT_TRUE(contains(problems, "layouts.json: debug.x_max is 1e+300, not a number from -3.40282e+38 to "));
}

TEST(GameDataTest, WarnsAboutLootThatCanLeaveAChestEmpty) {
    Json json = shipped(kConstantsFileName);
    json["level_objects"]["golden_chest"]["max_item_value"] = 5000;
    json["level_objects"]["golden_chest"]["max_total_value"] = 4000;
    GameConfig game;
    LightSettings lights;
    testing::internal::CaptureStderr();
    parse_constants(json.dump(), "constants.json", game, lights);
    EXPECT_TRUE(contains(testing::internal::GetCapturedStderr(), "golden chests"));
}

/// A copy of the game's data files in a directory of its own, which a test can change.
class GameDataDirTest : public testing::Test {
protected:
    void SetUp() override {
        const testing::TestInfo* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path() /
               ("manifold_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
        std::filesystem::copy(MANIFOLD_DATA_DIR, dir_, std::filesystem::copy_options::recursive);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    const std::filesystem::path& dir() const { return dir_; }

    /// Replaces the data file `name` with `json`.
    void write(std::string_view name, const Json& json) const { std::ofstream(dir_ / name) << json.dump(2); }

private:
    std::filesystem::path dir_;
};

TEST_F(GameDataDirTest, ListsTheProblemsOfAllFiles) {
    std::filesystem::remove(dir() / kSpellsFileName);
    Json constants = shipped(kConstantsFileName);
    constants["player"].erase("height");
    write(kConstantsFileName, constants);
    const std::string problems = problems_of([&] { static_cast<void>(load_game_data(dir())); });
    EXPECT_TRUE(contains(problems, "Can't read the game data"));
    EXPECT_TRUE(contains(problems, std::string(kSpellsFileName)));
    EXPECT_TRUE(contains(problems, "player.height is missing"));
}

TEST_F(GameDataDirTest, ChecksSettingsOfSeveralFilesTogether) {
    Json constants = shipped(kConstantsFileName);
    constants["player"]["height"] = 3.5;
    constants["party"]["max_mana"] = 4;
    write(kConstantsFileName, constants);
    Json layouts = shipped(kLevelLayoutsFileName);
    layouts["rooms"]["height"] = 3.0;
    write(kLevelLayoutsFileName, layouts);
    Json spells = shipped(kSpellsFileName);
    spells["levitate"]["cost"] = 10;
    write(kSpellsFileName, spells);
    const std::string problems = problems_of([&] { static_cast<void>(load_game_data(dir())); });
    EXPECT_TRUE(contains(problems, "player.height 3.5 is not below rooms.height 3 of level_layouts.json"));
    EXPECT_TRUE(contains(problems, "party.max_mana 4 is below levitate.cost 10 of spells.json"));
}

TEST_F(GameDataDirTest, NeedsLayoutsThatCanGenerateALevel) {
    // Each setting is right on its own, but the tiles are too narrow for the player.
    Json layouts = shipped(kLevelLayoutsFileName);
    layouts["rooms"]["tile_size"] = 1.0;
    write(kLevelLayoutsFileName, layouts);
    Json constants = shipped(kConstantsFileName);
    constants["player"]["radius"] = 0.6;
    constants["player"]["height"] = 1.8;
    write(kConstantsFileName, constants);
    const std::string problems = problems_of([&] { static_cast<void>(load_game_data(dir())); });
    EXPECT_TRUE(contains(problems, "the rooms layout can't generate a level"));
    EXPECT_FALSE(contains(problems, "the debug layout"));
}

}  // namespace
}  // namespace manifold
