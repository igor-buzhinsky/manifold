#pragma once

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"

namespace manifold {

/// Size of a crosshair pixel, in screen pixels, for a picture of a given height: 1 up to 1079 pixels, then growing
/// with the height, e.g. 2 for 1080 and 3 for 2160.
///
/// \param picture_height Height of the picture, in pixels.
int crosshair_scale(int picture_height);

/// Adds a crosshair at the center of the picture: a thin white plus with a black outline, so it shows on light and on
/// dark surfaces.
///
/// The black outline is added first, then the white lines over it. All sizes are whole pixels times
/// crosshair_scale(): the lines are 1 thick and reach 6 from the central pixel to each side, and the outline is 1 wide.
///
/// \param[out] mesh The rectangles are appended here.
/// \param picture   Size of the picture. The central square of the plus, crosshair_scale() pixels wide, is centered on
///                  the center of the picture, or half a pixel right of and below it where whole pixels can't be.
void add_crosshair(OverlayMesh& mesh, const PixelSize& picture);

}  // namespace manifold
