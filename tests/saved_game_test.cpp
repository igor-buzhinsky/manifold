#include "save/saved_game.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "level/level.hpp"
#include "party/party.hpp"
#include "party/spells.hpp"
#include "util/asset_mapping.hpp"
#include "world/player.hpp"

namespace manifold {
namespace {

/// A party of two: one with a portrait and hurt, one without a portrait.
std::vector<Character> make_party() {
    return {{.portrait = Portrait{.key = AssetKey::kFemalePortraits, .index = 2},
             .gender = Gender::kFemale,
             .hp = 30,
             .max_hp = 100},
            {.portrait = std::nullopt, .gender = Gender::kMale, .hp = 100, .max_hp = 100}};
}

/// Magic with some mana spent, and both spells in effect, Levitate cast first.
PartyMagic make_magic() {
    return {.mana = 42.5f,
            .max_mana = 120.0f,
            .spells = {{.spell = Spell::kLevitate, .remaining = 3.25f},
                       {.spell = Spell::kCandlelight, .remaining = 60.0f}}};
}

TEST(SavedGameTest, JsonRoundTripKeepsAllFields) {
    const SavedGame save{.seed = std::numeric_limits<std::uint64_t>::max(),
                         .level_number = 3,
                         .layout = LayoutKind::kDebug,
                         .time = 12.25,
                         .party = make_party(),
                         .magic = make_magic(),
                         .inventory = {"small_ruby", "large_diamond", "small_ruby"}};
    EXPECT_EQ(parse_saved_game(save_to_json(save)), save);
    EXPECT_NE(save_to_json(save).find(R"("inventory": [
    "small_ruby",
    "large_diamond",
    "small_ruby"
  ])"),
              std::string::npos)
        << save_to_json(save);
    const SavedGame without_layout{
        .seed = 7, .level_number = 0, .layout = std::nullopt, .time = 0.0, .party = make_party(), .magic = {}};
    EXPECT_EQ(parse_saved_game(save_to_json(without_layout)), without_layout);
}

TEST(SavedGameTest, JsonHasTheLayoutOnlyIfItWasGiven) {
    const std::string json = save_to_json(
        {.seed = 5, .level_number = 1, .layout = std::nullopt, .time = 2.5, .party = make_party(), .magic = {}});
    EXPECT_NE(json.find("\"version\": 5"), std::string::npos) << json;
    EXPECT_NE(json.find("\"seed\": 5"), std::string::npos) << json;
    EXPECT_NE(json.find(R"("portrait": {
        "key": "PORTRAITS_FEMALE",
        "index": 2
      })"),
              std::string::npos)
        << json;
    EXPECT_NE(json.find("\"portrait\": null"), std::string::npos) << json;
    EXPECT_NE(json.find("\"gender\": \"male\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"hp\": 30"), std::string::npos) << json;
    EXPECT_NE(json.find("\"max_hp\": 100"), std::string::npos) << json;
    EXPECT_NE(json.find("\"mana\": 100"), std::string::npos) << json;
    EXPECT_NE(json.find("\"max_mana\": 100"), std::string::npos) << json;
    EXPECT_NE(json.find("\"spells\": []"), std::string::npos) << json;
    EXPECT_EQ(json.find("layout"), std::string::npos) << json;
    EXPECT_NE(save_to_json({.seed = 5,
                            .level_number = 0,
                            .layout = LayoutKind::kRooms,
                            .time = 0.0,
                            .party = make_party(),
                            .magic = {}})
                  .find("\"layout\": \"rooms\""),
              std::string::npos);
}

/// The party and its magic of a valid save, as JSON, to add to the fields of saves in tests.
constexpr const char* kPartyJson =
    R"("party": [{"portrait": null, "gender": "male", "hp": 1, "max_hp": 2}], "mana": 100, "max_mana": 100, )"
    R"("spells": [])";

/// A save as JSON with `fields` and kPartyJson.
std::string with_party(const std::string& fields) { return "{" + fields + ", " + kPartyJson + "}"; }

TEST(SavedGameTest, ParsingIgnoresUnknownFields) {
    const SavedGame save =
        parse_saved_game(with_party(R"("version": 5, "seed": 9, "level_number": 2, "time": 1, "note": "hello")"));
    EXPECT_EQ(save, (SavedGame{.seed = 9,
                               .level_number = 2,
                               .layout = std::nullopt,
                               .time = 1.0,
                               .party = {{.portrait = std::nullopt, .gender = Gender::kMale, .hp = 1, .max_hp = 2}},
                               .magic = {}}));
}

TEST(SavedGameTest, SavesWithoutAnInventoryHaveNoItems) {
    EXPECT_TRUE(
        parse_saved_game(with_party(R"("version": 5, "seed": 9, "level_number": 2, "time": 1)")).inventory.empty());
    EXPECT_EQ(
        parse_saved_game(with_party(R"("version": 5, "seed": 9, "level_number": 2, "time": 1, "inventory": ["A"])"))
            .inventory,
        std::vector<std::string>{"A"});
    for (const char* inventory : {R"("inventory": "A")", R"("inventory": [1])", R"("inventory": [null])"}) {
        EXPECT_THROW(parse_saved_game(with_party(
                         std::string(R"("version": 5, "seed": 9, "level_number": 2, "time": 1, )") + inventory)),
                     std::runtime_error)
            << inventory;
    }
}

TEST(SavedGameTest, ParsingRejectsInvalidSaves) {
    const std::array<std::string, 3> invalid_json = {"", "not json", "[1, 2]"};
    for (const std::string& json : invalid_json) {
        EXPECT_THROW(parse_saved_game(json), std::runtime_error) << json;
    }
    // Each has a valid party, so that it fails for its other fields.
    const std::array invalid_fields = {
        R"("seed": 1, "level_number": 0, "time": 0)",
        R"("version": 1, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 2, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 3, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 4, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 6, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 5.5, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 5, "level_number": 0, "time": 0)",
        R"("version": 5, "seed": -1, "level_number": 0, "time": 0)",
        R"("version": 5, "seed": 1.5, "level_number": 0, "time": 0)",
        R"("version": 5, "seed": "1", "level_number": 0, "time": 0)",
        R"("version": 5, "seed": 1, "level_number": -1, "time": 0)",
        R"("version": 5, "seed": 1, "level_number": 3000000000, "time": 0)",
        R"("version": 5, "seed": 1, "time": 0)",
        R"("version": 5, "seed": 1, "level_number": 0)",
        R"("version": 5, "seed": 1, "level_number": 0, "time": -1)",
        R"("version": 5, "seed": 1, "level_number": 0, "time": "0")",
        R"("version": 5, "seed": 1, "level_number": 0, "time": 0, "layout": "maze")",
        R"("version": 5, "seed": 1, "level_number": 0, "time": 0, "layout": 1)",
    };
    for (const char* fields : invalid_fields) {
        EXPECT_THROW(parse_saved_game(with_party(fields)), std::runtime_error) << fields;
    }
}

TEST(SavedGameTest, ParsingRejectsInvalidParties) {
    const std::string fields =
        R"({"version": 5, "seed": 1, "level_number": 0, "time": 0, "mana": 1, "max_mana": 1, "spells": [])";
    EXPECT_THROW(parse_saved_game(fields + "}"), std::runtime_error);
    const std::array invalid_parties = {
        R"([])",
        R"({})",
        R"([1])",
        R"([{"gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": 1, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": "male/a.png", "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"key": "PORTRAITS_MALE"}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"index": 0}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"key": "DOOR_OPEN_SOUNDS", "index": 0}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"key": "PORTRAITS_OTHER", "index": 0}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"key": 1, "index": 0}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"key": "PORTRAITS_MALE", "index": -1}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"key": "PORTRAITS_MALE", "index": 0.5}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": {"key": "PORTRAITS_MALE", "index": 4294967296}, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": null, "hp": 1, "max_hp": 1}])",
        R"([{"portrait": null, "gender": "other", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": null, "gender": "male", "max_hp": 1}])",
        R"([{"portrait": null, "gender": "male", "hp": 2, "max_hp": 1}])",
        R"([{"portrait": null, "gender": "male", "hp": -1, "max_hp": 1}])",
        R"([{"portrait": null, "gender": "male", "hp": 0.5, "max_hp": 1}])",
        R"([{"portrait": null, "gender": "male", "hp": 0}])",
        R"([{"portrait": null, "gender": "male", "hp": 0, "max_hp": 0}])",
        R"([{"portrait": null, "gender": "male", "hp": 0, "max_hp": 3000000000}])",
        R"([{"portrait": null, "gender": "male", "hp": 0, "max_hp": 18446744073709551615}])",
    };
    for (const char* party : invalid_parties) {
        EXPECT_THROW(parse_saved_game(fields + R"(, "party": )" + party + "}"), std::runtime_error) << party;
    }
}

TEST(SavedGameTest, JsonHasTheSpellsInEffectByName) {
    const std::string json = save_to_json({.seed = 5,
                                           .level_number = 1,
                                           .layout = std::nullopt,
                                           .time = 2.5,
                                           .party = make_party(),
                                           .magic = make_magic()});
    EXPECT_NE(json.find("\"spell\": \"levitate\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"remaining\": 3.25"), std::string::npos) << json;
    EXPECT_NE(json.find("\"spell\": \"candlelight\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"mana\": 42.5"), std::string::npos) << json;
}

TEST(SavedGameTest, ParsingRejectsInvalidMagic) {
    const std::string fields = R"({"version": 5, "seed": 1, "level_number": 0, "time": 0, )"
                               R"("party": [{"portrait": null, "gender": "male", "hp": 1, "max_hp": 2}])";
    const SavedGame valid = parse_saved_game(
        fields + R"(, "mana": 0, "max_mana": 10, "spells": [{"spell": "candlelight", "remaining": 60}]})");
    EXPECT_EQ(
        valid.magic,
        (PartyMagic{.mana = 0.0f, .max_mana = 10.0f, .spells = {{.spell = Spell::kCandlelight, .remaining = 60.0f}}}));
    const std::array invalid_magic = {
        R"("max_mana": 10, "spells": [])",
        R"("mana": 1, "spells": [])",
        R"("mana": 1, "max_mana": 10)",
        R"("mana": 11, "max_mana": 10, "spells": [])",
        R"("mana": -1, "max_mana": 10, "spells": [])",
        R"("mana": "1", "max_mana": 10, "spells": [])",
        R"("mana": 0, "max_mana": 0, "spells": [])",
        R"("mana": 0, "max_mana": 1e300, "spells": [])",
        R"("mana": 1, "max_mana": 10, "spells": {})",
        R"("mana": 1, "max_mana": 10, "spells": [1])",
        R"("mana": 1, "max_mana": 10, "spells": [{"remaining": 1}])",
        R"("mana": 1, "max_mana": 10, "spells": [{"spell": "fireball", "remaining": 1}])",
        R"("mana": 1, "max_mana": 10, "spells": [{"spell": "levitate"}])",
        R"("mana": 1, "max_mana": 10, "spells": [{"spell": "levitate", "remaining": 0}])",
        R"("mana": 1, "max_mana": 10, "spells": [{"spell": "levitate", "remaining": -1}])",
        R"("mana": 1, "max_mana": 10, "spells": [{"spell": "levitate", "remaining": "1"}])",
        R"("mana": 1, "max_mana": 10, "spells": [{"spell": "levitate", "remaining": 1}, )"
        R"({"spell": "levitate", "remaining": 2}])",
    };
    for (const char* magic : invalid_magic) {
        EXPECT_THROW(parse_saved_game(fields + ", " + magic + "}"), std::runtime_error) << magic;
    }
}

/// Progress of a level with a hurt party, a player in the air, a closing door and two chests, one empty.
LevelProgress make_progress() {
    std::vector<Character> party = make_party();
    party[1].hp = 7;
    return {.time = 40.5,
            .party = std::move(party),
            .magic = {.mana = 10.0f, .max_mana = 120.0f, .spells = {{.spell = Spell::kLevitate, .remaining = 1.5f}}},
            .inventory = {"crown"},
            .player = {.position = {1.25f, -3.5f, 0.75f},
                       .yaw = 4.0f,
                       .pitch = -0.25f,
                       .vertical_speed = -2.5f,
                       .horizontal_velocity = {0.5f, -1.0f},
                       .on_ground = false},
            .doors = {{.openness = 0.25f, .target = 0.0f}, {.openness = 1.0f, .target = 1.0f}},
            .chests = {{"pebble", "gold_bar"}, {}}};
}

/// A save of the start of a level at time 12.25, without progress.
SavedGame make_level_start() {
    return {.seed = 9,
            .level_number = 2,
            .layout = std::nullopt,
            .time = 12.25,
            .party = make_party(),
            .magic = make_magic(),
            .inventory = {"pebble"}};
}

TEST(SavedGameTest, SaveFilesHaveTheirNames) {
    EXPECT_STREQ(save_file_name(SaveKind::kProgress), "save.json");
    EXPECT_STREQ(save_file_name(SaveKind::kLevelStart), "save_level_start.json");
}

TEST(SavedGameTest, JsonRoundTripKeepsTheProgress) {
    SavedGame save = make_level_start();
    EXPECT_EQ(save_to_json(save).find("progress"), std::string::npos);
    save.progress = make_progress();
    const std::string json = save_to_json(save);
    EXPECT_EQ(parse_saved_game(json), save) << json;
    EXPECT_NE(json.find(R"("position": [
        1.25,
        -3.5,
        0.75
      ])"),
              std::string::npos)
        << json;
    EXPECT_NE(json.find(R"("on_ground": false)"), std::string::npos) << json;
    EXPECT_NE(json.find(R"("openness": 0.25,
        "target": 0.0)"),
              std::string::npos)
        << json;
    // The level's start stays as it was.
    SavedGame start = parse_saved_game(json);
    start.progress.reset();
    EXPECT_EQ(start, make_level_start());
}

TEST(SavedGameTest, ProgressKeepsTheExactFloatsOfThePlayer) {
    SavedGame save = make_level_start();
    save.progress = make_progress();
    // Floats without a short decimal form, and a yaw just below 2 pi.
    save.progress->player.position = {0.1f, 1.0f / 3.0f, 1e-7f};
    save.progress->player.yaw = std::nextafter(2.0f * std::numbers::pi_v<float>, 0.0f);
    save.progress->player.pitch = Player::kMaxPitch;
    save.progress->doors[0].openness = 0.3f;
    EXPECT_EQ(parse_saved_game(save_to_json(save)), save);
}

TEST(SavedGameTest, ProgressOfAPlayerWhoTurnedATinyBitRightFromZeroCanBeRead) {
    Player player({.yaw = 0.0f});
    player.rotate(-1e-8f, 0.0f);
    SavedGame save = make_level_start();
    save.progress = make_progress();
    save.progress->player.yaw = player.yaw();
    EXPECT_EQ(parse_saved_game(save_to_json(save)), save);
}

TEST(SavedGameTest, ParsingRejectsInvalidProgress) {
    SavedGame save = make_level_start();
    save.progress = make_progress();
    const std::string valid = save_to_json(save);
    ASSERT_NO_THROW(parse_saved_game(valid));
    // Each replaces a part of the valid save.
    const std::vector<std::pair<std::string, std::string>> replacements = {
        {R"("time": 40.5)", R"("time": 12)"},
        {R"("time": 40.5)", R"("time": "40.5")"},
        {R"("time": 40.5,)", ""},
        {R"("inventory": [
      "crown"
    ],)",
         ""},
        {R"("crown")", "1"},
        {R"("hp": 7)", R"("hp": 200)"},
        {R"("mana": 10.0)", R"("mana": 1000.0)"},
        {R"("spell": "levitate")", R"("spell": "fireball")"},
        {"1.25,", "\"1.25\","},
        {"1.25,", ""},
        {R"("yaw": 4.0)", R"("yaw": 7.0)"},
        {R"("yaw": 4.0)", R"("yaw": -0.5)"},
        {R"("pitch": -0.25)", R"("pitch": -1.0)"},
        {R"("vertical_speed": -2.5)", R"("vertical_speed": 1e300)"},
        {R"("on_ground": false)", R"("on_ground": 0)"},
        {R"("openness": 0.25)", R"("openness": 1.5)"},
        {R"("target": 0.0)", R"("target": 0.5)"},
        {R"("target": 0.0)", R"("target": "0")"},
        {R"("gold_bar")", "null"},
    };
    for (const auto& [from, to] : replacements) {
        std::string json = valid;
        const std::size_t at = json.find(from);
        ASSERT_NE(at, std::string::npos) << from;
        json.replace(at, from.size(), to);
        EXPECT_THROW(parse_saved_game(json), std::runtime_error) << from << " -> " << to;
    }
    for (const char* field : {"party", "player", "doors", "chests"}) {
        std::string json = valid;
        const std::size_t progress = json.find(R"("progress")");
        const std::size_t at = json.find(std::string("\"") + field + "\"", progress);
        ASSERT_NE(at, std::string::npos) << field;
        json.replace(at + 1, 1, "X");
        EXPECT_THROW(parse_saved_game(json), std::runtime_error) << field;
    }
    for (const char* progress : {R"("progress": 1)", R"("progress": [])", R"("progress": {})"}) {
        EXPECT_THROW(parse_saved_game(with_party(std::string(R"("version": 5, "seed": 9, "level_number": 2, )"
                                                             R"("time": 1, )") +
                                                 progress)),
                     std::runtime_error)
            << progress;
    }
}

/// Creates an empty directory for one test and removes it afterwards.
class SaveFileTest : public testing::Test {
protected:
    void SetUp() override {
        const testing::TestInfo* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path() /
               ("manifold_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    const std::filesystem::path& dir() const { return dir_; }

private:
    std::filesystem::path dir_;
};

TEST_F(SaveFileTest, WritingReplacesTheSaveWithoutLeavingATemporaryFile) {
    const std::filesystem::path path = dir() / "save.json";
    const SavedGame first{
        .seed = 1, .level_number = 0, .layout = LayoutKind::kDebug, .time = 0.0, .party = make_party(), .magic = {}};
    const SavedGame second{
        .seed = 2, .level_number = 1, .layout = std::nullopt, .time = 30.5, .party = make_party(), .magic = {}};
    write_save_file(path, first);
    EXPECT_EQ(read_save_file(path), first);
    write_save_file(path, second);
    EXPECT_EQ(read_save_file(path), second);
    EXPECT_FALSE(std::filesystem::exists(dir() / "save.json.tmp"));
}

TEST_F(SaveFileTest, ReadingFailsForMissingOrBrokenFiles) {
    EXPECT_THROW(read_save_file(dir() / "missing.json"), std::runtime_error);
    const std::filesystem::path broken = dir() / "broken.json";
    std::ofstream(broken) << R"({"version": 5, "seed": 1)";
    EXPECT_THROW(read_save_file(broken), std::runtime_error);
}

TEST_F(SaveFileTest, WritingFailsIfTheDirectoryIsMissing) {
    const SavedGame save{.seed = 1, .level_number = 0, .layout = std::nullopt, .time = 0.0, .party = {}, .magic = {}};
    EXPECT_THROW(write_save_file(dir() / "missing" / "save.json", save), std::runtime_error);
}

}  // namespace
}  // namespace manifold
