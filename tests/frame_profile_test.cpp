#include "util/frame_profile.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(FrameProfileTest, StatsOfNoDurationsAreZero) {
    const DurationStats stats = compute_duration_stats({});
    EXPECT_EQ(stats.count, 0U);
    EXPECT_EQ(stats.mean, 0.0f);
    EXPECT_EQ(stats.max, 0.0f);
}

TEST(FrameProfileTest, StatsUseTheNearestRank) {
    std::vector<float> durations;
    // 20 values from 20 down to 1, so that sorting matters.
    for (int i = 20; i >= 1; --i) {
        durations.push_back(static_cast<float>(i));
    }
    const DurationStats stats = compute_duration_stats(durations);
    EXPECT_EQ(stats.count, 20U);
    EXPECT_FLOAT_EQ(stats.mean, 10.5f);
    EXPECT_FLOAT_EQ(stats.median, 10.0f);
    EXPECT_FLOAT_EQ(stats.p95, 19.0f);
    EXPECT_FLOAT_EQ(stats.max, 20.0f);
}

TEST(FrameProfileTest, StatsOfOneDuration) {
    const std::vector<float> durations{0.25f};
    const DurationStats stats = compute_duration_stats(durations);
    EXPECT_FLOAT_EQ(stats.median, 0.25f);
    EXPECT_FLOAT_EQ(stats.p95, 0.25f);
}

TEST(FrameProfileTest, IsDoneAfterTheFramesAskedFor) {
    FrameProfile profile(2);
    profile.add(FramePhase::kUpdate, 0.001f);
    profile.add(FramePhase::kFrame, 0.01f);
    EXPECT_FALSE(profile.done());
    profile.add(FramePhase::kGpu, 0.005f);
    EXPECT_FALSE(profile.done());
    profile.add(FramePhase::kFrame, 0.03f);
    EXPECT_TRUE(profile.done());
    EXPECT_EQ(profile.durations(FramePhase::kFrame).size(), 2U);
    EXPECT_EQ(profile.durations(FramePhase::kGpu).size(), 1U);
    EXPECT_TRUE(profile.durations(FramePhase::kSwap).empty());
}

TEST(FrameProfileTest, SummaryListsThePhasesWithDurations) {
    FrameProfile profile(2);
    profile.add(FramePhase::kFrame, 0.01f);
    profile.add(FramePhase::kFrame, 0.03f);
    profile.add(FramePhase::kGpu, 0.005f);
    const std::string summary = profile.summary("800 x 450 pixels");
    EXPECT_NE(summary.find("Profile of 2 frames, 800 x 450 pixels: 50.0 FPS"), std::string::npos) << summary;
    EXPECT_NE(summary.find("frame      20.00   10.00   30.00   30.00"), std::string::npos) << summary;
    EXPECT_NE(summary.find("gpu         5.00"), std::string::npos) << summary;
    EXPECT_EQ(summary.find("update"), std::string::npos) << summary;
    EXPECT_EQ(summary.find("swap"), std::string::npos) << summary;
}

}  // namespace
}  // namespace manifold
