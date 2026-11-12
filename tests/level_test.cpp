#include "level/level.hpp"

#include <map>

#include <gtest/gtest.h>

#include "world/torch.hpp"

namespace manifold {
namespace {

TEST(LevelTest, TorchFlamesHaveConfiguredChances) {
    LevelRng rng(7);
    constexpr int kDraws = 4000;
    std::map<TorchFlame, int> counts;
    for (int i = 0; i < kDraws; ++i) {
        ++counts[choose_torch_flame(rng)];
    }
    EXPECT_NEAR(counts[TorchFlame::kOrange] / static_cast<double>(kDraws), 0.5, 0.03);
    EXPECT_NEAR(counts[TorchFlame::kBlue] / static_cast<double>(kDraws), 0.25, 0.03);
    EXPECT_NEAR(counts[TorchFlame::kGreen] / static_cast<double>(kDraws), 0.25, 0.03);
}

}  // namespace
}  // namespace manifold
