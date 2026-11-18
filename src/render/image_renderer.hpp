#pragma once

#include <glad/gl.h>

#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "ui/rect.hpp"

namespace manifold {

/// An image in a texture, to draw with ImageRenderer, e.g. a portrait.
///
/// Needs a current OpenGL 4.1 core context, see Window for its whole lifetime. It can be moved, but not copied.
class ImageTexture {
public:
    /// Uploads the image, with mipmaps and smooth filtering, since images are mostly drawn smaller than they are.
    explicit ImageTexture(const Image& image);
    ~ImageTexture();
    ImageTexture(const ImageTexture&) = delete;
    ImageTexture& operator=(const ImageTexture&) = delete;
    ImageTexture(ImageTexture&& other) noexcept;
    ImageTexture& operator=(ImageTexture&& other) noexcept;

    /// The texture name, or 0 for a texture that was moved from.
    GLuint texture() const { return texture_; }

private:
    GLuint texture_ = 0;
};

/// Draws images over the picture, each stretched over a rectangle, e.g. the portraits of the party.
///
/// Needs a current OpenGL 4.1 core context, see Window for its whole lifetime.
class ImageRenderer {
public:
    /// Compiles the shaders and creates the vertex buffer.
    ///
    /// \throws std::runtime_error if a shader fails to compile or link.
    ImageRenderer();
    ~ImageRenderer();
    ImageRenderer(const ImageRenderer&) = delete;
    ImageRenderer& operator=(const ImageRenderer&) = delete;
    ImageRenderer(ImageRenderer&&) = delete;
    ImageRenderer& operator=(ImageRenderer&&) = delete;

    /// Draws an image into the viewport of the bound framebuffer, over what is there, blended by its opacity.
    ///
    /// \param image    What to draw. Its top row is at the top of `area`.
    /// \param area     Where to draw it, in pixels from the top-left corner of the viewport, like OverlayMesh.
    /// \param viewport Where the picture is.
    void draw(const ImageTexture& image, const Rect& area, const Viewport& viewport) const;

private:
    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;  ///< The corners of one rectangle, filled on every draw.
};

}  // namespace manifold
