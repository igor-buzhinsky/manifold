#include "console/console_window.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "util/log.hpp"

namespace manifold {
namespace {

/// Types a command and presses Enter.
std::optional<std::string> submit(ConsoleWindow& window, const std::string& text) {
    window.input().insert(text);
    return window.handle(ConsoleKey::kSubmit);
}

TEST(ConsoleWindowTest, SubmitReturnsTheCommandAndClearsTheInput) {
    ConsoleWindow window;
    EXPECT_EQ(submit(window, "camera player"), "camera player");
    EXPECT_EQ(window.input().text(), "");
    EXPECT_EQ(submit(window, "   "), std::nullopt);
    EXPECT_EQ(submit(window, ""), std::nullopt);
}

TEST(ConsoleWindowTest, HistoryGoesBackAndForwardToTheDraft) {
    ConsoleWindow window;
    submit(window, "first");
    submit(window, "second");
    submit(window, "second");  // Repeats are kept once.
    window.input().insert("draft");
    window.handle(ConsoleKey::kHistoryBack);
    EXPECT_EQ(window.input().text(), "second");
    window.handle(ConsoleKey::kHistoryBack);
    EXPECT_EQ(window.input().text(), "first");
    window.handle(ConsoleKey::kHistoryBack);  // Already the oldest.
    EXPECT_EQ(window.input().text(), "first");
    window.handle(ConsoleKey::kHistoryForward);
    EXPECT_EQ(window.input().text(), "second");
    window.handle(ConsoleKey::kHistoryForward);
    EXPECT_EQ(window.input().text(), "draft");
    window.handle(ConsoleKey::kHistoryForward);  // Nothing newer.
    EXPECT_EQ(window.input().text(), "draft");
}

TEST(ConsoleWindowTest, ScrollsWithinTheOutput) {
    ConsoleWindow window;
    window.handle(ConsoleKey::kScrollUp);
    EXPECT_EQ(window.scroll(), 0U);  // No output yet.
    for (int i = 0; i < 25; ++i) {
        log_info("ignored");  // Without a listener, the window doesn't see the log.
        window.add_output(LogLevel::kInfo, "line " + std::to_string(i));
    }
    window.handle(ConsoleKey::kScrollUp);
    window.handle(ConsoleKey::kScrollUp);
    window.handle(ConsoleKey::kScrollUp);
    EXPECT_EQ(window.scroll(), 24U);  // At most up to the oldest line.
    window.add_output(LogLevel::kInfo, "new");
    EXPECT_EQ(window.scroll(), 25U);  // Still showing the same lines.
    window.handle(ConsoleKey::kScrollDown);
    EXPECT_EQ(window.scroll(), 15U);
    submit(window, "help");
    EXPECT_EQ(window.scroll(), 0U);  // Running a command shows the newest output.
}

TEST(ConsoleWindowTest, KeepsOnlyTheNewestOutput) {
    ConsoleWindow window;
    window.add_output(LogLevel::kInfo, "a\nb");
    for (std::size_t i = 0; i < ConsoleWindow::kMaxOutputLines; ++i) {
        window.add_output(LogLevel::kWarning, "x");
    }
    for (int i = 0; i < 100; ++i) {
        window.handle(ConsoleKey::kScrollUp);
    }
    EXPECT_EQ(window.scroll(), ConsoleWindow::kMaxOutputLines - 1);
}

TEST(ConsoleWindowTest, MeshCoversTheTopHalfAndGrowsWithOutput) {
    ConsoleWindow window;
    const PixelSize size{.width = 1600, .height = 900};
    const OverlayMesh empty = window.build_mesh(size);
    ASSERT_FALSE(empty.vertices().empty());
    float bottom = 0.0f;
    for (const OverlayVertex& vertex : empty.vertices()) {
        EXPECT_GE(vertex.position.x, 0.0f);
        EXPECT_LE(vertex.position.x, 1600.0f);
        EXPECT_GE(vertex.position.y, 0.0f);
        bottom = std::max(bottom, vertex.position.y);
    }
    EXPECT_LE(bottom, 452.0f);  // The background and its border line.

    window.add_output(LogLevel::kError, "something failed");
    EXPECT_GT(window.build_mesh(size).vertices().size(), empty.vertices().size());
    // So much output that only part of it fits: the mesh stays inside the window.
    for (int i = 0; i < 200; ++i) {
        window.add_output(LogLevel::kInfo, std::string(300, 'w'));
    }
    const OverlayMesh full = window.build_mesh(size);
    for (const OverlayVertex& vertex : full.vertices()) {
        EXPECT_GE(vertex.position.y, 0.0f);
        EXPECT_LE(vertex.position.y, 452.0f);
    }
}

}  // namespace
}  // namespace manifold
