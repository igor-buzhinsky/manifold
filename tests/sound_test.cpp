#include "audio/sound.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

/// A sound of the game, in the source tree.
const std::filesystem::path kPortalSound =
    std::filesystem::path(MANIFOLD_ASSETS_DIR) / "sounds" / "portal" / "kenney-digital-audio" / "spaceTrash1.ogg";

TEST(SoundTest, DecodesOggVorbis) {
    const std::optional<Sound> sound = load_sound(kPortalSound);
    ASSERT_TRUE(sound.has_value());
    EXPECT_GE(sound->channels, 1);
    EXPECT_LE(sound->channels, 2);
    EXPECT_GE(sound->sample_rate, 8000);
    EXPECT_EQ(sound->samples.size() % static_cast<std::size_t>(sound->channels), 0U);
    EXPECT_GT(sound->duration(), 0.05f);
    EXPECT_LT(sound->duration(), 10.0f);
    EXPECT_TRUE(std::ranges::any_of(sound->samples, [](std::int16_t sample) { return sample != 0; }));
}

TEST(SoundTest, BrokenOrMissingFileGivesNothing) {
    const std::filesystem::path broken = std::filesystem::temp_directory_path() / "manifold_SoundTest_broken.ogg";
    std::ofstream(broken) << "not an ogg";
    testing::internal::CaptureStderr();
    EXPECT_FALSE(load_sound(broken).has_value());
    EXPECT_FALSE(load_sound(broken.parent_path() / "manifold_SoundTest_missing.ogg").has_value());
    EXPECT_NE(testing::internal::GetCapturedStderr().find("[warning]"), std::string::npos);
    std::filesystem::remove(broken);
}

TEST(SoundTest, DurationFromSamplesChannelsAndRate) {
    const Sound stereo{.channels = 2, .sample_rate = 100, .samples = std::vector<std::int16_t>(300)};
    EXPECT_FLOAT_EQ(stereo.duration(), 1.5f);
    EXPECT_EQ(Sound{}.duration(), 0.0f);
}

}  // namespace
}  // namespace manifold
