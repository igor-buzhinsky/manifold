#include "util/asset_files.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
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

TEST(AssetFilesPickTest, NoCandidatesGiveNothing) { EXPECT_FALSE(pick_asset_file({}, 1).has_value()); }

TEST(AssetFilesPickTest, PickDependsOnlyOnSeedAndReachesEveryCandidate) {
    const std::vector<std::filesystem::path> candidates{"a.ogg", "b.ogg", "c.ogg", "d.ogg", "e.ogg"};
    std::set<std::filesystem::path> picked;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        const std::optional<std::filesystem::path> path = pick_asset_file(candidates, seed);
        ASSERT_TRUE(path.has_value());
        EXPECT_EQ(path, pick_asset_file(candidates, seed));
        picked.insert(*path);
    }
    EXPECT_EQ(picked, std::set<std::filesystem::path>(candidates.begin(), candidates.end()));
}

}  // namespace
}  // namespace manifold
