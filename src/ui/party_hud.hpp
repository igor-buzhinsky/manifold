#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "party/party.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"

namespace manifold {

// The party's portraits in the bottom-left corner of the picture, side by side, each with a thin red bar below it that
// shows the character's hit points. All sizes are whole interface pixels, see ui_scale().

/// Width and height of a portrait on screen, in interface pixels: an eighth of the 240 x 320 files.
inline constexpr float kHudPortraitWidth = 30.0f;
inline constexpr float kHudPortraitHeight = 40.0f;
/// Space between the portraits, and between the bars, in interface pixels.
inline constexpr float kHudPortraitGap = 2.0f;
/// Space between the portraits and the left and bottom edges of the picture, in interface pixels.
inline constexpr float kHudMargin = 4.0f;
/// Height of the bar of hit points, and the space between it and the portrait above, in interface pixels.
inline constexpr float kHudBarHeight = 2.0f;
inline constexpr float kHudBarGap = 1.0f;

/// Where the parts of a character are drawn, in pixels of the picture.
struct CharacterPlacement {
    Rect portrait;
    /// The whole bar of hit points, as wide as the portrait. Its left part is filled as far as the character has hit
    /// points, see add_party_hud().
    Rect hp_bar;
};

/// Where the portraits of a party of `count` characters and their bars go in a picture of `picture` size: from left to
/// right in the order of the party, the bars kHudMargin above the bottom edge.
std::vector<CharacterPlacement> layout_party_hud(const PixelSize& picture, std::size_t count);

/// Whether `point` is on the party's portraits and bars, or in the gaps between them: in the smallest rectangle around
/// all of them, see layout_party_hud(). Clicks there don't reach the game behind.
///
/// \param picture Size of the picture.
/// \param count   How many characters the party has.
/// \param point   In pixels from the top-left corner of the picture, y down.
bool party_hud_contains(const PixelSize& picture, std::size_t count, const glm::vec2& point);

/// Adds the shapes of the party to `mesh`: a dark rectangle where each portrait goes, which shows when it has no
/// image, and each bar of hit points, dark with the part of Character::hp_fraction() from its left red. The red part is
/// rounded to whole pixels, and is at least one pixel wide while the character has hit points.
///
/// The images of the portraits are drawn over the dark rectangles separately.
///
/// \param[out] mesh  Where the shapes are appended.
/// \param placements Where each character goes, see layout_party_hud().
/// \param party      The characters, as many as `placements`.
void add_party_hud(OverlayMesh& mesh, std::span<const CharacterPlacement> placements, std::span<const Character> party);

}  // namespace manifold
