#pragma once

#include <glm/vec2.hpp>

namespace manifold {

/// Size of an image or framebuffer in pixels.
struct PixelSize {
    int width = 0;
    int height = 0;
};

/// A rectangle in framebuffer pixels, with the origin in the bottom-left corner like in OpenGL.
struct Viewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

/// Finds the largest centered rectangle with the given aspect ratio that fits into the framebuffer.
///
/// The rest of the framebuffer is left for black bars, on the sides or above and below.
///
/// \param framebuffer  Size of the whole framebuffer.
/// \param aspect_ratio Wanted ratio of width to height.
Viewport fit_viewport(const PixelSize& framebuffer, float aspect_ratio);

/// Converts a point of the window, e.g. of the mouse, to pixels of the picture in a viewport.
///
/// \param point       In window units from the top-left corner of the window, y down, as SDL gives mouse positions.
/// \param window      Size of the window in window units, which can differ from pixels on high-DPI screens.
/// \param framebuffer Size of the window's framebuffer in pixels.
/// \param viewport    Where the picture is in the framebuffer, see fit_viewport().
/// \return The point in pixels from the top-left corner of the picture, y down, as OverlayMesh uses. It is outside
///         the picture for points on the black bars. A window of zero size gives the point unscaled.
glm::vec2 window_to_viewport(const glm::vec2& point, const PixelSize& window, const PixelSize& framebuffer,
                             const Viewport& viewport);

/// The center of the pixel that `point` is in, e.g. to test a point of the mouse against shapes as they are drawn: the
/// GPU fills a pixel if its center is inside a shape, which matters where edges fall between pixels, as at fractional
/// scales of the interface, see ui_scale().
///
/// \param point In pixels, such as those of window_to_viewport().
glm::vec2 pixel_center(const glm::vec2& point);

}  // namespace manifold
