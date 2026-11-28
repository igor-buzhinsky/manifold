#pragma once

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"

namespace manifold {

// The picture turns red for a moment when a fall hurts the party, see Game::last_fall_impact(). The red is strongest at
// the landing and fades out over kDamageFlashDuration of game time, so it stays while the game is paused.

/// How long the red of a landing lasts, in seconds of game time.
inline constexpr float kDamageFlashDuration = 0.5f;
/// The opacity of the red at the landing for the least damage and for kDamageFlashFullDamage or more.
inline constexpr float kDamageFlashMinOpacity = 0.25f;
inline constexpr float kDamageFlashMaxOpacity = 0.6f;
/// The damage of a character, in hit points, from which the red is strongest: that of an ordinary person.
inline constexpr float kDamageFlashFullDamage = 100.0f;

/// The opacity of the red, from 0 to kDamageFlashMaxOpacity, `elapsed` seconds after a landing that took `damage` hit
/// points of the character hurt most: from kDamageFlashMinOpacity to kDamageFlashMaxOpacity in proportion to the
/// damage at the landing, then falling linearly to 0 at kDamageFlashDuration. 0 before the landing, e.g. for a negative
/// `elapsed`, and without damage.
float damage_flash_opacity(float elapsed, int damage);

/// Adds a red rectangle over the whole picture with `opacity`, if it is positive.
///
/// \param[out] mesh Where the rectangle is appended.
/// \param picture   Size of the picture.
/// \param opacity   From 0 to 1, see damage_flash_opacity().
void add_damage_flash(OverlayMesh& mesh, const PixelSize& picture, float opacity);

}  // namespace manifold
