#include "ui/spell_hud.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "party/spells.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/party_hud.hpp"
#include "ui/rect.hpp"
#include "ui/ui_style.hpp"

namespace manifold {
namespace {

constexpr PixelSize kPicture{.width = 1000, .height = 600};

TEST(SpellHudTest, SpellsGoDownFromTheTopRightCornerInBoxesOfOneWidth) {
    ASSERT_EQ(ui_scale(kPicture), 2.0f);
    const std::vector<Rect> boxes = layout_spell_hud(kPicture, 2);
    ASSERT_EQ(boxes.size(), 2U);
    EXPECT_EQ(boxes[0].top_left.y, 8.0f);
    EXPECT_EQ(boxes[0].top_left.x + boxes[0].size.x, 1000.0f - 8.0f);
    // Nine pixels of letters and one above and below them, in interface pixels.
    EXPECT_EQ(boxes[0].size.y, 22.0f);
    EXPECT_EQ(boxes[1].size, boxes[0].size);
    EXPECT_EQ(boxes[1].top_left, boxes[0].top_left + glm::vec2(0.0f, 22.0f + 2.0f));
    // Wide enough for the longest name.
    EXPECT_GE(boxes[0].size.x, text_width(spell_title(Spell::kCandlelight), 2.0f) + 8.0f);
    EXPECT_TRUE(layout_spell_hud(kPicture, 0).empty());
}

TEST(SpellHudTest, ManaBarIsInTheBottomRightCornerLevelWithTheHitPoints) {
    const Rect bar = layout_mana_bar(kPicture);
    EXPECT_EQ(bar.size, glm::vec2(2.0f * kHudPortraitWidth, 4.0f * kHudBarHeight) * 2.0f);
    EXPECT_EQ(bar.top_left + bar.size, glm::vec2(1000.0f - 8.0f, 600.0f - 8.0f));
    const Rect hp_bar = layout_party_hud(kPicture, 1)[0].hp_bar;
    EXPECT_EQ(bar.top_left.y + bar.size.y, hp_bar.top_left.y + hp_bar.size.y);
}

TEST(SpellHudTest, ContainsTheBoxesTheGapsAndTheManaBar) {
    PartyMagic magic;
    const Rect bar = layout_mana_bar(kPicture);
    EXPECT_TRUE(spell_hud_contains(kPicture, magic, bar.top_left));
    EXPECT_FALSE(spell_hud_contains(kPicture, magic, bar.top_left - glm::vec2(1.0f)));
    const Rect first = layout_spell_hud(kPicture, 1)[0];
    EXPECT_FALSE(spell_hud_contains(kPicture, magic, first.top_left));
    magic.cast(Spell::kCandlelight);
    magic.cast(Spell::kLevitate);
    EXPECT_TRUE(spell_hud_contains(kPicture, magic, first.top_left));
    // The gap between the boxes.
    EXPECT_TRUE(spell_hud_contains(kPicture, magic, first.top_left + glm::vec2(1.0f, first.size.y + 1.0f)));
    EXPECT_FALSE(spell_hud_contains(kPicture, magic, first.top_left - glm::vec2(1.0f, 0.0f)));
    EXPECT_FALSE(spell_hud_contains(kPicture, magic, first.top_left + glm::vec2(0.0f, (2.0f * first.size.y) + 2.0f)));
}

/// How many rectangles `width` pixels wide the mesh has, each two triangles of six vertices, see
/// OverlayMesh::add_rectangle().
std::size_t count_rectangles(const OverlayMesh& mesh, float width) {
    std::size_t count = 0;
    const std::vector<OverlayVertex>& vertices = mesh.vertices();
    for (std::size_t i = 0; i + 6 <= vertices.size(); i += 6) {
        float min_x = vertices[i].position.x;
        float max_x = min_x;
        for (std::size_t j = i; j < i + 6; ++j) {
            min_x = std::min(min_x, vertices[j].position.x);
            max_x = std::max(max_x, vertices[j].position.x);
        }
        if (max_x - min_x == width) {
            ++count;
        }
    }
    return count;
}

TEST(SpellHudTest, ManaBarIsFilledByTheFractionOfMana) {
    PartyMagic magic;
    magic.set_mana(25.0f);
    OverlayMesh mesh;
    add_spell_hud(mesh, kPicture, magic);
    // Without spells, only the two rectangles of the bar: the whole bar and its filled quarter.
    ASSERT_EQ(mesh.vertices().size(), 12U);
    const float width = layout_mana_bar(kPicture).size.x;
    EXPECT_EQ(count_rectangles(mesh, width), 1U);
    EXPECT_EQ(count_rectangles(mesh, width / 4.0f), 1U);

    magic.set_mana(0.0f);
    OverlayMesh empty;
    add_spell_hud(empty, kPicture, magic);
    EXPECT_EQ(empty.vertices().size(), 6U);
    magic.set_mana(0.01f);
    OverlayMesh nearly_empty;
    add_spell_hud(nearly_empty, kPicture, magic);
    EXPECT_EQ(count_rectangles(nearly_empty, 1.0f), 1U);
}

TEST(SpellHudTest, EachSpellInEffectGetsAWhiteBoxWithItsName) {
    PartyMagic magic;
    magic.cast(Spell::kLevitate);
    OverlayMesh mesh;
    add_spell_hud(mesh, kPicture, magic);
    ASSERT_GT(mesh.vertices().size(), 18U);
    // The box comes first, white, and its name over it in black.
    EXPECT_EQ(mesh.vertices()[0].color, glm::vec4(1.0f));
    EXPECT_EQ(mesh.vertices()[6].color, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

TEST(SpellHudTest, BoxBlinksInTheLastTenSeconds) {
    EXPECT_TRUE(spell_box_visible(60.0f));
    EXPECT_TRUE(spell_box_visible(10.5f));
    EXPECT_TRUE(spell_box_visible(10.0f));
    EXPECT_TRUE(spell_box_visible(9.9f));
    EXPECT_TRUE(spell_box_visible(9.5f));
    EXPECT_FALSE(spell_box_visible(9.4f));
    EXPECT_FALSE(spell_box_visible(9.0f));
    EXPECT_TRUE(spell_box_visible(8.75f));
    EXPECT_TRUE(spell_box_visible(0.5f));
    EXPECT_FALSE(spell_box_visible(0.25f));
}

TEST(SpellHudTest, HiddenBoxKeepsItsPlace) {
    PartyMagic magic;
    magic.cast(Spell::kCandlelight);
    magic.cast(Spell::kLevitate);
    magic.spells[0].remaining = 0.25f;
    OverlayMesh mesh;
    add_spell_hud(mesh, kPicture, magic);
    // Only the second box is drawn, at its own place below the hidden first one.
    const std::vector<Rect> boxes = layout_spell_hud(kPicture, 2);
    ASSERT_GT(mesh.vertices().size(), 6U);
    EXPECT_EQ(mesh.vertices()[0].color, glm::vec4(1.0f));
    float top = mesh.vertices()[0].position.y;
    for (std::size_t i = 0; i < 6; ++i) {
        top = std::min(top, mesh.vertices()[i].position.y);
    }
    EXPECT_EQ(top, boxes[1].top_left.y);
    EXPECT_EQ(count_rectangles(mesh, boxes[1].size.x), 1U);
    EXPECT_TRUE(spell_hud_contains(kPicture, magic, boxes[0].top_left));
}

}  // namespace
}  // namespace manifold
