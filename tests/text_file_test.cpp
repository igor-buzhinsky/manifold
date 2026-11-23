#include "util/text_file.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace manifold {
namespace {

/// A directory for one test, removed afterwards.
class TextFileTest : public testing::Test {
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

TEST_F(TextFileTest, ReadsTheWholeFileAsItIs) {
    std::string text = "line 1\r\nline 2\n";
    text += '\0';
    text += "end";
    std::ofstream(dir() / "a.txt", std::ios::binary) << text;
    EXPECT_EQ(read_text_file(dir() / "a.txt"), text);
}

TEST_F(TextFileTest, ReadsAnEmptyFile) {
    std::ofstream(dir() / "empty.txt").close();
    EXPECT_EQ(read_text_file(dir() / "empty.txt"), "");
}

TEST_F(TextFileTest, RejectsAMissingFileOrADirectory) {
    EXPECT_THROW(read_text_file(dir() / "missing.txt"), std::runtime_error);
    EXPECT_THROW(read_text_file(dir()), std::runtime_error);
}

}  // namespace
}  // namespace manifold
