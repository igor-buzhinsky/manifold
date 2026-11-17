#include "level/level.hpp"

#include <map>
#include <set>
#include <string>

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

TEST(LevelTest, TextureKindsHaveTheNamesOfTheirDirectories) {
    std::set<std::string> names;
    for (const TextureKind kind : kTextureKinds) {
        names.insert(texture_kind_name(kind));
    }
    EXPECT_EQ(names, (std::set<std::string>{"grass", "wood", "metal", "stone"}));
    EXPECT_STREQ(texture_kind_name(TextureKind::kWood), "wood");
}

}  // namespace
}  // namespace manifold
