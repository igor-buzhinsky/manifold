#include "util/asset_files.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

/// Creates an empty directory for one test and removes it afterwards.
class AssetFilesTest : public testing::Test {
protected:
    void SetUp() override {
        const testing::TestInfo* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path() /
               ("manifold_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    /// Writes a small file at `relative` inside the test directory, creating directories as needed.
    std::filesystem::path write_file(const std::string& relative) const {
        const std::filesystem::path path = dir_ / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "data";
        return path;
    }

    const std::filesystem::path& dir() const { return dir_; }

private:
    std::filesystem::path dir_;
};

TEST_F(AssetFilesTest, FindsFilesWithTheExtensionAtAnyDepthSorted) {
    const auto b = write_file("pack2/b.ogg");
    const auto a = write_file("pack1/nested/deeper/a.OGG");
    const auto c = write_file("c.ogg");
    write_file("pack2/readme.md");
    write_file("pack2/b.ogg.txt");
    write_file("pack1/image.png");
    std::filesystem::create_directories(dir() / "folder.ogg");
    EXPECT_EQ(find_asset_files(dir(), ".ogg"), (std::vector<std::filesystem::path>{c, a, b}));
    EXPECT_EQ(find_asset_files(dir(), ".OGG"), find_asset_files(dir(), ".ogg"));
}

TEST_F(AssetFilesTest, MissingDirectoryGivesNoFiles) {
    EXPECT_TRUE(find_asset_files(dir() / "missing", ".ogg").empty());
}

TEST_F(AssetFilesTest, AssetsNextToTheExecutableComeFirst) {
    std::filesystem::create_directories(dir() / "game" / "assets");
    EXPECT_EQ(find_assets_dir(dir() / "game", dir() / "source"), dir() / "game" / "assets");
}

TEST_F(AssetFilesTest, WithoutAssetsNextToTheExecutableTheFallbackIsUsed) {
    std::filesystem::create_directories(dir() / "game");
    EXPECT_EQ(find_assets_dir(dir() / "game", dir() / "source"), dir() / "source");
    // A file named like the directory doesn't count.
    write_file("game/assets");
    EXPECT_EQ(find_assets_dir(dir() / "game", dir() / "source"), dir() / "source");
    // Nor does a missing executable directory, or an unknown one.
    EXPECT_EQ(find_assets_dir(dir() / "missing", dir() / "source"), dir() / "source");
    EXPECT_EQ(find_assets_dir({}, dir() / "source"), dir() / "source");
}

}  // namespace
}  // namespace manifold
