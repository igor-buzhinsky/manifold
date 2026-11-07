#pragma once

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

}  // namespace manifold
