#include "save/saved_game.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "level/level.hpp"

namespace manifold {
namespace {

TEST(SavedGameTest, JsonRoundTripKeepsAllFields) {
    const SavedGame save{.seed = std::numeric_limits<std::uint64_t>::max(),
                         .level_number = 3,
                         .layout = LayoutKind::kDebug,
                         .time = 12.25};
    EXPECT_EQ(parse_saved_game(save_to_json(save)), save);
    const SavedGame without_layout{.seed = 7, .level_number = 0, .layout = std::nullopt, .time = 0.0};
    EXPECT_EQ(parse_saved_game(save_to_json(without_layout)), without_layout);
}

TEST(SavedGameTest, JsonHasTheLayoutOnlyIfItWasGiven) {
    const std::string json = save_to_json({.seed = 5, .level_number = 1, .layout = std::nullopt, .time = 2.5});
    EXPECT_NE(json.find("\"version\": 1"), std::string::npos) << json;
    EXPECT_NE(json.find("\"seed\": 5"), std::string::npos) << json;
    EXPECT_EQ(json.find("layout"), std::string::npos) << json;
    EXPECT_NE(save_to_json({.seed = 5, .level_number = 0, .layout = LayoutKind::kRooms, .time = 0.0})
                  .find("\"layout\": \"rooms\""),
              std::string::npos);
}

TEST(SavedGameTest, ParsingIgnoresUnknownFields) {
    const SavedGame save =
        parse_saved_game(R"({"version": 1, "seed": 9, "level_number": 2, "time": 1, "note": "hello"})");
    EXPECT_EQ(save, (SavedGame{.seed = 9, .level_number = 2, .layout = std::nullopt, .time = 1.0}));
}

TEST(SavedGameTest, ParsingRejectsInvalidSaves) {
    const std::array invalid = {
        "",
        "not json",
        "[1, 2]",
        R"({"seed": 1, "level_number": 0, "time": 0})",
        R"({"version": 2, "seed": 1, "level_number": 0, "time": 0})",
        R"({"version": 1.5, "seed": 1, "level_number": 0, "time": 0})",
        R"({"version": 1, "level_number": 0, "time": 0})",
        R"({"version": 1, "seed": -1, "level_number": 0, "time": 0})",
        R"({"version": 1, "seed": 1.5, "level_number": 0, "time": 0})",
        R"({"version": 1, "seed": "1", "level_number": 0, "time": 0})",
        R"({"version": 1, "seed": 1, "level_number": -1, "time": 0})",
        R"({"version": 1, "seed": 1, "level_number": 3000000000, "time": 0})",
        R"({"version": 1, "seed": 1, "time": 0})",
        R"({"version": 1, "seed": 1, "level_number": 0})",
        R"({"version": 1, "seed": 1, "level_number": 0, "time": -1})",
        R"({"version": 1, "seed": 1, "level_number": 0, "time": "0"})",
        R"({"version": 1, "seed": 1, "level_number": 0, "time": 0, "layout": "maze"})",
        R"({"version": 1, "seed": 1, "level_number": 0, "time": 0, "layout": 1})",
    };
    for (const char* json : invalid) {
        EXPECT_THROW(parse_saved_game(json), std::runtime_error) << json;
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
    const SavedGame first{.seed = 1, .level_number = 0, .layout = LayoutKind::kDebug, .time = 0.0};
    const SavedGame second{.seed = 2, .level_number = 1, .layout = std::nullopt, .time = 30.5};
    write_save_file(path, first);
    EXPECT_EQ(read_save_file(path), first);
    write_save_file(path, second);
    EXPECT_EQ(read_save_file(path), second);
    EXPECT_FALSE(std::filesystem::exists(dir() / "save.json.tmp"));
}

TEST_F(SaveFileTest, ReadingFailsForMissingOrBrokenFiles) {
    EXPECT_THROW(read_save_file(dir() / "missing.json"), std::runtime_error);
    const std::filesystem::path broken = dir() / "broken.json";
    std::ofstream(broken) << R"({"version": 1, "seed": 1)";
    EXPECT_THROW(read_save_file(broken), std::runtime_error);
}

TEST_F(SaveFileTest, WritingFailsIfTheDirectoryIsMissing) {
    const SavedGame save{.seed = 1, .level_number = 0, .layout = std::nullopt, .time = 0.0};
    EXPECT_THROW(write_save_file(dir() / "missing" / "save.json", save), std::runtime_error);
}

}  // namespace
}  // namespace manifold
