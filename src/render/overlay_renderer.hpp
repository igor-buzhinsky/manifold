#pragma once

#include <glad/gl.h>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"

namespace manifold {

/// Draws an OverlayMesh over the picture, e.g. the console window.
///
/// Needs a current OpenGL 4.1 core context, see Window for its whole lifetime.
class OverlayRenderer {
public:
    /// Compiles the shaders and creates the vertex buffer.
    ///
    /// \throws std::runtime_error if a shader fails to compile or link.
    OverlayRenderer();
    ~OverlayRenderer();
    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;
    OverlayRenderer(OverlayRenderer&&) = delete;
    OverlayRenderer& operator=(OverlayRenderer&&) = delete;

    /// Draws the mesh into the viewport of the bound framebuffer, over what is there, blended by opacity.
    ///
    /// \param mesh     What to draw, in pixels from the top-left corner of the viewport.
    /// \param viewport Where to draw it.
    void draw(const OverlayMesh& mesh, const Viewport& viewport) const;

private:
    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;  ///< The mesh, filled on every draw.
};

}  // namespace manifold
