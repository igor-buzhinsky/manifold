#include "render/viewport.hpp"

#include <gtest/gtest.h>

namespace manifold {
namespace {

constexpr float kWide = 16.0f / 9.0f;

TEST(ViewportTest, ExactAspectFillsFramebuffer) {
    const Viewport viewport = fit_viewport({.width = 1600, .height = 900}, kWide);
    EXPECT_EQ(viewport.x, 0);
    EXPECT_EQ(viewport.y, 0);
    EXPECT_EQ(viewport.width, 1600);
    EXPECT_EQ(viewport.height, 900);
}

TEST(ViewportTest, WiderFramebufferGetsBarsOnSides) {
    const Viewport viewport = fit_viewport({.width = 2000, .height = 900}, kWide);
    EXPECT_EQ(viewport.width, 1600);
    EXPECT_EQ(viewport.height, 900);
    EXPECT_EQ(viewport.x, 200);
    EXPECT_EQ(viewport.y, 0);
}

TEST(ViewportTest, TallerFramebufferGetsBarsAboveAndBelow) {
    const Viewport viewport = fit_viewport({.width = 1600, .height = 1200}, kWide);
    EXPECT_EQ(viewport.width, 1600);
    EXPECT_EQ(viewport.height, 900);
    EXPECT_EQ(viewport.x, 0);
    EXPECT_EQ(viewport.y, 150);
}

}  // namespace
}  // namespace manifold
