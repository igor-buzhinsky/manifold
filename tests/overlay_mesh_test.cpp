#include "ui/overlay_mesh.hpp"

#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(OverlayMeshTest, RectangleIsTwoTrianglesCoveringIt) {
    OverlayMesh mesh;
    mesh.add_rectangle({10.0f, 20.0f}, {30.0f, 40.0f}, {1.0f, 0.0f, 0.0f, 0.5f});
    ASSERT_EQ(mesh.vertices().size(), 6U);
    for (const OverlayVertex& vertex : mesh.vertices()) {
        EXPECT_TRUE(vertex.position.x == 10.0f || vertex.position.x == 40.0f);
        EXPECT_TRUE(vertex.position.y == 20.0f || vertex.position.y == 60.0f);
        EXPECT_EQ(vertex.color, glm::vec4(1.0f, 0.0f, 0.0f, 0.5f));
    }
}

TEST(OverlayMeshTest, TextStaysWithinItsWidthAndLineHeight) {
    OverlayMesh mesh;
    const glm::vec2 top_left{100.0f, 50.0f};
    const float scale = 2.0f;
    mesh.add_text(top_left, "Hello, world", scale, glm::vec4(1.0f));
    ASSERT_FALSE(mesh.vertices().empty());
    EXPECT_EQ(mesh.vertices().size() % 6, 0U);
    const float width = text_width("Hello, world", scale);
    for (const OverlayVertex& vertex : mesh.vertices()) {
        EXPECT_GE(vertex.position.x, top_left.x);
        EXPECT_LE(vertex.position.x, top_left.x + width);
        EXPECT_GE(vertex.position.y, top_left.y);
        EXPECT_LE(vertex.position.y, top_left.y + (kTextLineHeight * scale));
    }
}

TEST(OverlayMeshTest, TextWidthGrowsWithTextAndScale) {
    EXPECT_FLOAT_EQ(text_width("", 1.0f), 0.0f);
    EXPECT_GT(text_width("ab", 1.0f), text_width("a", 1.0f));
    EXPECT_FLOAT_EQ(text_width("ab", 3.0f), 3.0f * text_width("ab", 1.0f));
    // Characters the font doesn't have are drawn as '?', so they have its width.
    EXPECT_FLOAT_EQ(text_width("\xC3", 1.0f), text_width("?", 1.0f));
}

TEST(OverlayMeshTest, WrapsAtSpacesThenBetweenCharacters) {
    const float letter = text_width("a", 1.0f);
    // Room for five letters.
    const std::vector<std::string> lines = wrap_text("aaa aaa aaaaaaaa\n\nb", 5.0f * letter, 1.0f);
    EXPECT_EQ(lines, (std::vector<std::string>{"aaa", "aaa", "aaaaa", "aaa", "", "b"}));
    for (const std::string& line : lines) {
        EXPECT_LE(text_width(line, 1.0f), 5.0f * letter);
    }
    // Too narrow for anything: one character per line.
    EXPECT_EQ(wrap_text("abc", 0.0f, 1.0f), (std::vector<std::string>{"a", "b", "c"}));
}

}  // namespace
}  // namespace manifold
