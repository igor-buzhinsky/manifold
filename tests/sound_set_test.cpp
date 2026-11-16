#include "audio/sound_set.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(SoundSetTest, LoadsAllSoundsAndCountsRound) {
    const SoundSet set(std::filesystem::path(MANIFOLD_SOUNDS_DIR) / "door_open", "door open");
    ASSERT_EQ(set.size(), 2U);
    ASSERT_NE(set.get(0), nullptr);
    ASSERT_NE(set.get(1), nullptr);
    EXPECT_NE(set.get(0), set.get(1));
    EXPECT_EQ(set.get(2), set.get(0));
    EXPECT_EQ(set.get(4294967295U), set.get(1));
    EXPECT_GT(set.get(0)->duration(), 0.0f);
}

TEST(SoundSetTest, SkipsBrokenFilesAndIsEmptyWithoutSounds) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "manifold_SoundSetTest_broken";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "broken.ogg") << "not an ogg";
    testing::internal::CaptureStderr();
    const SoundSet set(dir, "test");
    const SoundSet missing(dir / "missing", "test");
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(set.size(), 0U);
    EXPECT_EQ(set.get(0), nullptr);
    EXPECT_EQ(missing.size(), 0U);
    EXPECT_NE(log.find("[warning] No test sounds found"), std::string::npos) << log;
    std::filesystem::remove_all(dir);
}

TEST(SoundSetTest, DefaultIsEmpty) { EXPECT_EQ(SoundSet().get(7), nullptr); }

}  // namespace
}  // namespace manifold
