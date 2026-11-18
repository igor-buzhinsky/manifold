#include "render/viewport.hpp"

#include <glm/vec2.hpp>
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

TEST(ViewportTest, WindowPointsAreScaledToPixelsAndMovedPastTheBars) {
    // A high-DPI window of 1000 x 900 units, 2000 x 1800 pixels, with bars above and below the 16:9 picture.
    const PixelSize framebuffer{.width = 2000, .height = 1800};
    const Viewport viewport = fit_viewport(framebuffer, kWide);
    ASSERT_EQ(viewport.height, 1125);
    const int bar = (1800 - 1125) / 2;
    const PixelSize window{.width = 1000, .height = 900};
    EXPECT_EQ(window_to_viewport({0.0f, 0.0f}, window, framebuffer, viewport),
              glm::vec2(0.0f, -static_cast<float>(1800 - bar - 1125)));
    EXPECT_EQ(window_to_viewport({500.0f, 450.0f}, window, framebuffer, viewport),
              glm::vec2(1000.0f, 900.0f - static_cast<float>(1800 - bar - 1125)));
}

TEST(ViewportTest, WindowPointsWithSideBars) {
    const PixelSize framebuffer{.width = 2000, .height = 900};
    const Viewport viewport = fit_viewport(framebuffer, kWide);
    EXPECT_EQ(window_to_viewport({200.0f, 10.0f}, framebuffer, framebuffer, viewport), glm::vec2(0.0f, 10.0f));
    // A window of zero size, e.g. while minimized, leaves the point unscaled.
    EXPECT_EQ(window_to_viewport({200.0f, 10.0f}, {}, framebuffer, viewport), glm::vec2(0.0f, 10.0f));
}

}  // namespace
}  // namespace manifold
