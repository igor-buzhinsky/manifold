#include "util/frame_times.hpp"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(FrameTimesTest, EmptyHasNoTimes) {
    const FrameTimes times;
    EXPECT_TRUE(times.values().empty());
    EXPECT_EQ(times.average(), 0.0f);
    EXPECT_EQ(times.longest(), 0.0f);
}

TEST(FrameTimesTest, KeepsTimesInOrder) {
    FrameTimes times;
    times.add(0.01f);
    times.add(0.03f);
    times.add(0.02f);
    EXPECT_EQ(times.values(), (std::vector<float>{0.01f, 0.03f, 0.02f}));
    EXPECT_FLOAT_EQ(times.average(), 0.02f);
    EXPECT_FLOAT_EQ(times.longest(), 0.03f);
}

TEST(FrameTimesTest, DropsTheOldestWhenFull) {
    FrameTimes times;
    const std::size_t count = FrameTimes::kCapacity + 5;
    for (std::size_t i = 0; i < count; ++i) {
        times.add(static_cast<float>(i));
    }
    const std::vector<float> values = times.values();
    ASSERT_EQ(values.size(), FrameTimes::kCapacity);
    for (std::size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(values[i], static_cast<float>(i + 5)) << i;
    }
    EXPECT_EQ(times.longest(), static_cast<float>(count - 1));
}

}  // namespace
}  // namespace manifold
