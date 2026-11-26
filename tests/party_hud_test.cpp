#include "ui/party_hud.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "party/party.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_style.hpp"
#include "util/asset_mapping.hpp"

namespace manifold {
namespace {

TEST(PartyHudTest, PortraitsGoSideBySideInTheBottomLeftCornerWithBarsBelow) {
    const PixelSize picture{.width = 1000, .height = 600};
    const float scale = ui_scale(picture);
    ASSERT_EQ(scale, 2.0f);
    const std::vector<CharacterPlacement> placements = layout_party_hud(picture, 3);
    ASSERT_EQ(placements.size(), 3U);
    for (std::size_t i = 0; i < placements.size(); ++i) {
        const Rect& portrait = placements[i].portrait;
        const Rect& bar = placements[i].hp_bar;
        EXPECT_EQ(portrait.size, glm::vec2(60.0f, 80.0f));
        EXPECT_EQ(bar.size, glm::vec2(60.0f, 4.0f));
        EXPECT_EQ(bar.top_left.x, portrait.top_left.x);
        // The bar is one interface pixel below the portrait, and four above the bottom edge.
        EXPECT_EQ(bar.top_left.y, portrait.top_left.y + 80.0f + 2.0f);
        EXPECT_EQ(bar.top_left.y + bar.size.y, 600.0f - 8.0f);
        if (i > 0) {
            // Two interface pixels between neighbours.
            const Rect& left = placements[i - 1].portrait;
            EXPECT_EQ(portrait.top_left.x, left.top_left.x + left.size.x + 4.0f);
            EXPECT_EQ(portrait.top_left.y, left.top_left.y);
        }
    }
    EXPECT_EQ(placements[0].portrait.top_left.x, 8.0f);
    EXPECT_TRUE(layout_party_hud(picture, 0).empty());
}

TEST(PartyHudTest, ContainsThePortraitsTheBarsAndTheGapsBetweenThem) {
    const PixelSize picture{.width = 1000, .height = 600};
    const std::vector<CharacterPlacement> placements = layout_party_hud(picture, 3);
    const Rect& first = placements[0].portrait;
    const Rect& last_bar = placements[2].hp_bar;
    EXPECT_TRUE(party_hud_contains(picture, 3, first.top_left));
    EXPECT_TRUE(party_hud_contains(picture, 3, last_bar.top_left + last_bar.size - glm::vec2(1.0f)));
    // The gap between the first two portraits, and the gap between a portrait and its bar.
    EXPECT_TRUE(party_hud_contains(picture, 3, {first.top_left.x + first.size.x + 1.0f, first.top_left.y + 10.0f}));
    EXPECT_TRUE(party_hud_contains(picture, 3, {first.top_left.x + 10.0f, first.top_left.y + first.size.y + 1.0f}));
    // Outside: left of, above, right of and below the portraits.
    EXPECT_FALSE(party_hud_contains(picture, 3, first.top_left - glm::vec2(1.0f, 0.0f)));
    EXPECT_FALSE(party_hud_contains(picture, 3, first.top_left - glm::vec2(0.0f, 1.0f)));
    EXPECT_FALSE(party_hud_contains(picture, 3, {last_bar.top_left.x + last_bar.size.x, last_bar.top_left.y}));
    EXPECT_FALSE(party_hud_contains(picture, 3, {first.top_left.x, last_bar.top_left.y + last_bar.size.y}));
    EXPECT_FALSE(party_hud_contains(picture, 0, first.top_left));
}

/// Six vertices per rectangle.
constexpr std::size_t kRectangleVertices = 6;

TEST(PartyHudTest, BarsAreFilledAsFarAsTheHpGo) {
    const PixelSize picture{.width = 1000, .height = 600};
    const std::vector<CharacterPlacement> placements = layout_party_hud(picture, 3);
    const std::vector<Character> party = {
        {.portrait = Portrait{.key = AssetKey::kMalePortraits, .index = 0},
         .gender = Gender::kMale,
         .hp = 100,
         .max_hp = 100},
        {.portrait = std::nullopt, .gender = Gender::kFemale, .hp = 25, .max_hp = 100},
        {.portrait = Portrait{.key = AssetKey::kMalePortraits, .index = 1},
         .gender = Gender::kMale,
         .hp = 0,
         .max_hp = 100}};
    OverlayMesh mesh;
    add_party_hud(mesh, placements, party);
    // A place for each portrait and a bar for each, and a red part for those with hit points.
    ASSERT_EQ(mesh.vertices().size(), (3 + 3 + 2) * kRectangleVertices);
    // The red part of the second character: the fourth rectangle of the second character's three, after the first
    // character's three.
    const std::size_t red = (3 + 2) * kRectangleVertices;
    float right = 0.0f;
    for (std::size_t i = red; i < red + kRectangleVertices; ++i) {
        right = std::max(right, mesh.vertices()[i].position.x);
    }
    EXPECT_EQ(right, placements[1].hp_bar.top_left.x + 15.0f);
}

TEST(PartyHudTest, AnyHpLeftShowsAtLeastOnePixel) {
    const std::vector<CharacterPlacement> placements = layout_party_hud({.width = 1000, .height = 600}, 1);
    OverlayMesh mesh;
    add_party_hud(mesh, placements,
                  std::vector<Character>{{.portrait = std::nullopt, .gender = Gender::kMale, .hp = 1, .max_hp = 1000}});
    ASSERT_EQ(mesh.vertices().size(), 3 * kRectangleVertices);
    float right = 0.0f;
    for (std::size_t i = 2 * kRectangleVertices; i < 3 * kRectangleVertices; ++i) {
        right = std::max(right, mesh.vertices()[i].position.x);
    }
    EXPECT_EQ(right, placements[0].hp_bar.top_left.x + 1.0f);
}

}  // namespace
}  // namespace manifold
