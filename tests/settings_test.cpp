#include "settings/settings.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(ParseSettingsTest, ReadsFullscreen) {
    EXPECT_TRUE(parse_settings(R"({"fullscreen": true})", {}).fullscreen);
    EXPECT_FALSE(parse_settings(R"({"fullscreen": false})", {.fullscreen = true}).fullscreen);
}

TEST(ParseSettingsTest, ReadsMouseLook) {
    EXPECT_FALSE(Settings{}.mouse_look);
    EXPECT_TRUE(parse_settings(R"({"mouse_look": true})", {}).mouse_look);
    EXPECT_FALSE(parse_settings(R"({"mouse_look": false})", {.mouse_look = true}).mouse_look);
}

TEST(ParseSettingsTest, ReadsMouseSensitivityAsAnyPositiveNumber) {
    EXPECT_FLOAT_EQ(parse_settings(R"({"mouse_sensitivity": 0.25})", {}).mouse_sensitivity, 0.25f);
    EXPECT_FLOAT_EQ(parse_settings(R"({"mouse_sensitivity": 2})", {}).mouse_sensitivity, 2.0f);
}

TEST(ParseSettingsTest, RejectsAMouseSensitivityThatIsNotAPositiveFloat) {
    for (const char* json :
         {R"({"mouse_sensitivity": 0})", R"({"mouse_sensitivity": -0.1})", R"({"mouse_sensitivity": 1e100})",
          R"({"mouse_sensitivity": 1e-100})", R"({"mouse_sensitivity": "0.1"})", R"({"mouse_sensitivity": null})"}) {
        EXPECT_THROW(parse_settings(json, {}), std::runtime_error) << json;
    }
}

TEST(ParseSettingsTest, KeepsMissingFieldsOfTheBase) {
    EXPECT_TRUE(parse_settings("{}", {.fullscreen = true}).fullscreen);
    EXPECT_FALSE(parse_settings("{}", {}).fullscreen);
    const Settings base{.mouse_look = true, .mouse_sensitivity = 0.5f};
    EXPECT_EQ(parse_settings(R"({"fullscreen": true})", base),
              (Settings{.fullscreen = true, .mouse_look = true, .mouse_sensitivity = 0.5f}));
}

TEST(ParseSettingsTest, IgnoresUnknownFields) {
    EXPECT_TRUE(parse_settings(R"({"fulscreen": false, "fullscreen": true})", {}).fullscreen);
}

TEST(ParseSettingsTest, RejectsInvalidJson) {
    EXPECT_THROW(parse_settings("{", {}), std::runtime_error);
    EXPECT_THROW(parse_settings("", {}), std::runtime_error);
}

TEST(ParseSettingsTest, RejectsWhatIsNotAnObject) {
    EXPECT_THROW(parse_settings("[]", {}), std::runtime_error);
    EXPECT_THROW(parse_settings("true", {}), std::runtime_error);
}

TEST(ParseSettingsTest, RejectsAFieldOfAWrongType) {
    EXPECT_THROW(parse_settings(R"({"fullscreen": 1})", {}), std::runtime_error);
    EXPECT_THROW(parse_settings(R"({"fullscreen": "true"})", {}), std::runtime_error);
    EXPECT_THROW(parse_settings(R"({"fullscreen": null})", {}), std::runtime_error);
    EXPECT_THROW(parse_settings(R"({"mouse_look": 1})", {}), std::runtime_error);
}

/// Creates an empty directory for one test and removes it afterwards.
class SettingsFilesTest : public testing::Test {
protected:
    void SetUp() override {
        const testing::TestInfo* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path() /
               ("manifold_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    /// Writes `text` to the file `name` in the test directory.
    std::filesystem::path write_file(const std::string& name, const std::string& text) const {
        const std::filesystem::path path = dir_ / name;
        std::ofstream(path) << text;
        return path;
    }

    const std::filesystem::path& dir() const { return dir_; }

private:
    std::filesystem::path dir_;
};

TEST_F(SettingsFilesTest, GivesTheDefaultsWithoutFiles) {
    EXPECT_EQ(load_settings({}), Settings{});
    const std::vector<std::filesystem::path> files = {dir() / "missing.json"};
    EXPECT_EQ(load_settings(files), Settings{});
}

TEST_F(SettingsFilesTest, LaterFilesHavePriority) {
    const std::vector<std::filesystem::path> files = {write_file("game.json", R"({"fullscreen": true})"),
                                                      write_file("user.json", R"({"fullscreen": false})")};
    EXPECT_FALSE(load_settings(files).fullscreen);
}

TEST_F(SettingsFilesTest, KeepsWhatALaterFileDoesNotSet) {
    const std::vector<std::filesystem::path> files = {write_file("game.json", R"({"fullscreen": true})"),
                                                      write_file("user.json", "{}"), dir() / "missing.json"};
    EXPECT_TRUE(load_settings(files).fullscreen);
}

TEST_F(SettingsFilesTest, SkipsAnInvalidFileAsAWhole) {
    const std::vector<std::filesystem::path> files = {
        write_file("game.json", R"({"fullscreen": true})"),
        write_file("user.json", R"({"fullscreen": false, "other": 1, "fullscreen_too": [)")};
    EXPECT_TRUE(load_settings(files).fullscreen);
}

TEST_F(SettingsFilesTest, SkipsADirectory) {
    std::filesystem::create_directories(dir() / "settings.json");
    const std::vector<std::filesystem::path> files = {write_file("game.json", R"({"fullscreen": true})"),
                                                      dir() / "settings.json"};
    EXPECT_TRUE(load_settings(files).fullscreen);
}

TEST_F(SettingsFilesTest, FindsTheGameFileNextToTheExecutable) {
    const std::filesystem::path fallback = dir() / "source" / "settings.json";
    EXPECT_EQ(find_game_settings_file(dir(), fallback), fallback);
    EXPECT_EQ(find_game_settings_file({}, fallback), fallback);
    const std::filesystem::path next_to_executable = write_file(std::string(kSettingsFileName), "{}");
    EXPECT_EQ(find_game_settings_file(dir(), fallback), next_to_executable);
}

}  // namespace
}  // namespace manifold
