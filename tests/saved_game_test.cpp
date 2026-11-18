#include "save/saved_game.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "level/level.hpp"
#include "party/party.hpp"

namespace manifold {
namespace {

/// A party of two: one with a portrait and hurt, one without a portrait.
std::vector<Character> make_party() {
    return {{.portrait = "female/met/a.png", .gender = Gender::kFemale, .hp = 30, .max_hp = 100},
            {.portrait = "", .gender = Gender::kMale, .hp = 100, .max_hp = 100}};
}

TEST(SavedGameTest, JsonRoundTripKeepsAllFields) {
    const SavedGame save{.seed = std::numeric_limits<std::uint64_t>::max(),
                         .level_number = 3,
                         .layout = LayoutKind::kDebug,
                         .time = 12.25,
                         .party = make_party()};
    EXPECT_EQ(parse_saved_game(save_to_json(save)), save);
    const SavedGame without_layout{
        .seed = 7, .level_number = 0, .layout = std::nullopt, .time = 0.0, .party = make_party()};
    EXPECT_EQ(parse_saved_game(save_to_json(without_layout)), without_layout);
}

TEST(SavedGameTest, JsonHasTheLayoutOnlyIfItWasGiven) {
    const std::string json =
        save_to_json({.seed = 5, .level_number = 1, .layout = std::nullopt, .time = 2.5, .party = make_party()});
    EXPECT_NE(json.find("\"version\": 2"), std::string::npos) << json;
    EXPECT_NE(json.find("\"seed\": 5"), std::string::npos) << json;
    EXPECT_NE(json.find("\"portrait\": \"female/met/a.png\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"gender\": \"male\""), std::string::npos) << json;
    EXPECT_NE(json.find("\"hp\": 30"), std::string::npos) << json;
    EXPECT_NE(json.find("\"max_hp\": 100"), std::string::npos) << json;
    EXPECT_EQ(json.find("layout"), std::string::npos) << json;
    EXPECT_NE(
        save_to_json({.seed = 5, .level_number = 0, .layout = LayoutKind::kRooms, .time = 0.0, .party = make_party()})
            .find("\"layout\": \"rooms\""),
        std::string::npos);
}

/// The party of a valid save, as JSON, to add to the fields of saves in tests.
constexpr const char* kPartyJson = R"("party": [{"portrait": "", "gender": "male", "hp": 1, "max_hp": 2}])";

/// A save as JSON with `fields` and kPartyJson.
std::string with_party(const std::string& fields) { return "{" + fields + ", " + kPartyJson + "}"; }

TEST(SavedGameTest, ParsingIgnoresUnknownFields) {
    const SavedGame save =
        parse_saved_game(with_party(R"("version": 2, "seed": 9, "level_number": 2, "time": 1, "note": "hello")"));
    EXPECT_EQ(save, (SavedGame{.seed = 9,
                               .level_number = 2,
                               .layout = std::nullopt,
                               .time = 1.0,
                               .party = {{.portrait = "", .gender = Gender::kMale, .hp = 1, .max_hp = 2}}}));
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
        R"("version": 3, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 2.5, "seed": 1, "level_number": 0, "time": 0)",
        R"("version": 2, "level_number": 0, "time": 0)",
        R"("version": 2, "seed": -1, "level_number": 0, "time": 0)",
        R"("version": 2, "seed": 1.5, "level_number": 0, "time": 0)",
        R"("version": 2, "seed": "1", "level_number": 0, "time": 0)",
        R"("version": 2, "seed": 1, "level_number": -1, "time": 0)",
        R"("version": 2, "seed": 1, "level_number": 3000000000, "time": 0)",
        R"("version": 2, "seed": 1, "time": 0)",
        R"("version": 2, "seed": 1, "level_number": 0)",
        R"("version": 2, "seed": 1, "level_number": 0, "time": -1)",
        R"("version": 2, "seed": 1, "level_number": 0, "time": "0")",
        R"("version": 2, "seed": 1, "level_number": 0, "time": 0, "layout": "maze")",
        R"("version": 2, "seed": 1, "level_number": 0, "time": 0, "layout": 1)",
    };
    for (const char* fields : invalid_fields) {
        EXPECT_THROW(parse_saved_game(with_party(fields)), std::runtime_error) << fields;
    }
}

TEST(SavedGameTest, ParsingRejectsInvalidParties) {
    const std::string fields = R"({"version": 2, "seed": 1, "level_number": 0, "time": 0)";
    EXPECT_THROW(parse_saved_game(fields + "}"), std::runtime_error);
    const std::array invalid_parties = {
        R"([])",
        R"({})",
        R"([1])",
        R"([{"gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": 1, "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": "/etc/a.png", "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": "male/../../a.png", "gender": "male", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": "", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": "", "gender": "other", "hp": 1, "max_hp": 1}])",
        R"([{"portrait": "", "gender": "male", "max_hp": 1}])",
        R"([{"portrait": "", "gender": "male", "hp": 2, "max_hp": 1}])",
        R"([{"portrait": "", "gender": "male", "hp": -1, "max_hp": 1}])",
        R"([{"portrait": "", "gender": "male", "hp": 0.5, "max_hp": 1}])",
        R"([{"portrait": "", "gender": "male", "hp": 0}])",
        R"([{"portrait": "", "gender": "male", "hp": 0, "max_hp": 0}])",
        R"([{"portrait": "", "gender": "male", "hp": 0, "max_hp": 3000000000}])",
        R"([{"portrait": "", "gender": "male", "hp": 0, "max_hp": 18446744073709551615}])",
    };
    for (const char* party : invalid_parties) {
        EXPECT_THROW(parse_saved_game(fields + R"(, "party": )" + party + "}"), std::runtime_error) << party;
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
        .seed = 1, .level_number = 0, .layout = LayoutKind::kDebug, .time = 0.0, .party = make_party()};
    const SavedGame second{.seed = 2, .level_number = 1, .layout = std::nullopt, .time = 30.5, .party = make_party()};
    write_save_file(path, first);
    EXPECT_EQ(read_save_file(path), first);
    write_save_file(path, second);
    EXPECT_EQ(read_save_file(path), second);
    EXPECT_FALSE(std::filesystem::exists(dir() / "save.json.tmp"));
}

TEST_F(SaveFileTest, ReadingFailsForMissingOrBrokenFiles) {
    EXPECT_THROW(read_save_file(dir() / "missing.json"), std::runtime_error);
    const std::filesystem::path broken = dir() / "broken.json";
    std::ofstream(broken) << R"({"version": 2, "seed": 1)";
    EXPECT_THROW(read_save_file(broken), std::runtime_error);
}

TEST_F(SaveFileTest, WritingFailsIfTheDirectoryIsMissing) {
    const SavedGame save{.seed = 1, .level_number = 0, .layout = std::nullopt, .time = 0.0, .party = {}};
    EXPECT_THROW(write_save_file(dir() / "missing" / "save.json", save), std::runtime_error);
}

}  // namespace
}  // namespace manifold
