#include "audio/sound_set.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "util/asset_index.hpp"
#include "util/asset_mapping.hpp"

namespace manifold {
namespace {

/// The door sounds of the game, in the directory of the Kenney pack.
std::filesystem::path door_sound(const std::string& name) {
    return std::filesystem::path(MANIFOLD_ASSETS_DIR) / "sounds" / "door_close" / "kenney-rpg-audio" / name;
}

TEST(SoundSetTest, LoadsEachPathInOrder) {
    const std::vector<std::filesystem::path> paths{door_sound("doorClose_1.ogg"), door_sound("doorClose_2.ogg"),
                                                   door_sound("doorClose_1.ogg")};
    testing::internal::CaptureStderr();
    const SoundSet set(paths, "TEST_SOUNDS");
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_NE(log.find("[info] 3 of 3 TEST_SOUNDS loaded"), std::string::npos) << log;
    ASSERT_EQ(set.size(), 3U);
    ASSERT_NE(set.get(0), nullptr);
    ASSERT_NE(set.get(1), nullptr);
    ASSERT_NE(set.get(2), nullptr);
    EXPECT_NE(set.get(0), set.get(2));
    EXPECT_EQ(set.get(0)->samples, set.get(2)->samples);
    EXPECT_NE(set.get(0)->samples, set.get(1)->samples);
    EXPECT_GT(set.get(0)->duration(), 0.0f);
}

TEST(SoundSetTest, IndexBeyondTheSoundsGivesNoneWithAWarning) {
    const std::vector<std::filesystem::path> paths{door_sound("doorClose_1.ogg")};
    testing::internal::CaptureStderr();
    const SoundSet set(paths, "TEST_SOUNDS");
    testing::internal::GetCapturedStderr();
    testing::internal::CaptureStderr();
    EXPECT_EQ(set.get(1), nullptr);
    EXPECT_EQ(set.get(4294967295U), nullptr);
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_NE(log.find("[warning] TEST_SOUNDS has no sound 1: it has 1"), std::string::npos) << log;
}

TEST(SoundSetTest, BrokenAndMissingFilesKeepTheirPlacesWithoutSounds) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "manifold_SoundSetTest_broken";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "broken.ogg") << "not an ogg";
    const std::vector<std::filesystem::path> paths{dir / "broken.ogg", dir / "missing.ogg",
                                                   door_sound("doorClose_1.ogg")};
    testing::internal::CaptureStderr();
    const SoundSet set(paths, "TEST_SOUNDS");
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(set.size(), 3U);
    EXPECT_EQ(set.get(0), nullptr);
    EXPECT_EQ(set.get(1), nullptr);
    EXPECT_NE(set.get(2), nullptr);
    EXPECT_NE(log.find("[warning] Can't load " + (dir / "broken.ogg").string()), std::string::npos) << log;
    EXPECT_NE(log.find("[warning] Can't load " + (dir / "missing.ogg").string()), std::string::npos) << log;
    EXPECT_NE(log.find("[info] 1 of 3 TEST_SOUNDS loaded"), std::string::npos) << log;
    std::filesystem::remove_all(dir);
}

TEST(SoundSetTest, LoadsTheFilesOfAKey) {
    testing::internal::CaptureStderr();
    const AssetMapping mapping =
        AssetMapping::parse(R"({"DOOR_CLOSE_SOUNDS": ["sounds/door_close/kenney-rpg-audio/doorClose_3.ogg"]})",
                            MANIFOLD_ASSETS_DIR, "test");
    const SoundSet set(mapping, AssetKey::kDoorCloseSounds);
    const std::string log = testing::internal::GetCapturedStderr();
    EXPECT_EQ(set.size(), 1U);
    EXPECT_NE(set.get(0), nullptr);
    EXPECT_NE(log.find("[info] 1 of 1 DOOR_CLOSE_SOUNDS loaded"), std::string::npos) << log;
}

TEST(SoundSetTest, PickOtherNeverRepeatsAndReachesAllOthers) {
    const std::vector<std::filesystem::path> paths(4, door_sound("doorClose_1.ogg"));
    testing::internal::CaptureStderr();
    const SoundSet set(paths, "TEST_SOUNDS");
    testing::internal::GetCapturedStderr();
    ASSERT_EQ(set.size(), 4U);
    std::set<AssetIndex> picked;
    for (std::uint64_t random = 0; random < 30; ++random) {
        const AssetIndex index = set.pick_other(2, random);
        EXPECT_LT(index, 4U);
        EXPECT_NE(index, 2U);
        picked.insert(index);
    }
    EXPECT_EQ(picked, (std::set<AssetIndex>{0, 1, 3}));
    EXPECT_EQ(SoundSet().pick_other(5, 7), 0U);
}

TEST(SoundSetTest, DefaultIsEmptyAndQuiet) {
    testing::internal::CaptureStderr();
    EXPECT_EQ(SoundSet().get(7), nullptr);
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
}

}  // namespace
}  // namespace manifold
