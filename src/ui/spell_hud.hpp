#pragma once

#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>

#include "party/spells.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"

namespace manifold {

// The party's magic on the screen: the names of the spells in effect in white boxes in the top-right corner, one below
// the other, and a blue bar of mana in the bottom-right corner. All sizes are whole interface pixels, see ui_scale(),
// and the space to the edges of the picture is kHudMargin, as for the portraits, see party_hud.hpp.

/// Space between the text of a spell's name and the left and right sides of its box, in interface pixels.
inline constexpr float kHudSpellPaddingX = 2.0f;
/// Space between the letters of a spell's name and the top and bottom of its box, in interface pixels.
inline constexpr float kHudSpellPaddingY = 1.0f;
/// Space between the boxes of two spells, in interface pixels.
inline constexpr float kHudSpellGap = 1.0f;
/// Width and height of the bar of mana, in interface pixels: as wide as two bars of hit points, and four times as high,
/// see kHudPortraitWidth and kHudBarHeight.
inline constexpr float kHudManaBarWidth = 60.0f;
inline constexpr float kHudManaBarHeight = 8.0f;
/// A spell's box blinks while the spell has at most this many seconds of game time left, see spell_box_visible().
inline constexpr float kHudSpellBlinkTime = 10.0f;
/// Seconds of game time of one blink of a spell's box: shown for the first half, hidden for the second.
inline constexpr float kHudSpellBlinkPeriod = 1.0f;

/// Where the boxes of `count` spells go in a picture of `picture` size: from the top down, kHudSpellGap apart, the
/// first kHudMargin below the top edge, and all kHudMargin left of the right edge. All boxes are equally wide, as wide
/// as the box of the spell with the longest SpellType::name in `config`, so that they don't change as spells come and
/// go.
std::vector<Rect> layout_spell_hud(const PixelSize& picture, const MagicConfig& config, std::size_t count);

/// Where the bar of mana goes in a picture of `picture` size: kHudMargin left of the right edge and above the bottom
/// edge, which puts its bottom at the height of that of the bars of hit points.
Rect layout_mana_bar(const PixelSize& picture);

/// Whether `point` is on the boxes of the spells in effect, also those that spell_box_visible() hides, or in the gaps
/// between them, or on the bar of mana, see layout_spell_hud() and layout_mana_bar(). Clicks there don't reach the game
/// behind.
///
/// \param picture Size of the picture.
/// \param magic   The party's magic, whose spells in effect have boxes.
/// \param config  The names of the spells, which set the width of the boxes.
/// \param point   In pixels from the top-left corner of the picture, y down.
bool spell_hud_contains(const PixelSize& picture, const PartyMagic& magic, const MagicConfig& config,
                        const glm::vec2& point);

/// Whether the box of a spell with `remaining` seconds of game time left is drawn: always while at least
/// kHudSpellBlinkTime are left, then for the first half of each kHudSpellBlinkPeriod, counted from the end of the
/// spell, so that the box is hidden in the last half period. The blinking follows the game time, so it stops while the
/// game is paused.
bool spell_box_visible(float remaining);

/// Adds the party's magic to `mesh`: a white box with the black SpellType::name of each spell in effect, in the order
/// of PartyMagic::spells, leaving out those that spell_box_visible() hides but keeping their places, and the bar of
/// mana, dark blue with the part of PartyMagic::mana_fraction() from its left blue. The width of the blue part is
/// rounded to whole pixels, and is at least one pixel while the party has any mana.
///
/// \param[out] mesh Where the shapes are appended.
/// \param picture   Size of the picture.
/// \param magic     The party's magic.
/// \param config    The names of the spells.
void add_spell_hud(OverlayMesh& mesh, const PixelSize& picture, const PartyMagic& magic, const MagicConfig& config);

}  // namespace manifold
