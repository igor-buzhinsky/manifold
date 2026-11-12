#pragma once

#include <span>

#include <glad/gl.h>

#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"
#include "world/torch.hpp"

namespace manifold {

/// Draws the world and the torch flames with OpenGL.
///
/// Needs a current OpenGL 4.1 core context, see Window for its whole lifetime.
class Renderer {
public:
    /// Compiles the shaders and uploads the world mesh, its texture and the flames to the GPU.
    ///
    /// \param world_mesh Triangles to draw, see build_world_mesh().
    /// \param texture    The texture of all surfaces, repeated as the mesh's texture coordinates say.
    /// \param torches    The torches whose flames to draw. Torch i flickers with torch_phase(i).
    /// \throws std::runtime_error if a shader fails to compile or link.
    Renderer(std::span<const Vertex> world_mesh, const Image& texture, std::span<const Torch> torches);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /// Clears the framebuffer to black and draws the world, then the flames.
    ///
    /// \param camera   Where the world is seen from.
    /// \param lighting The lights to use. Lights after the first kMaxLights are left out.
    /// \param viewport Part of the framebuffer to draw into. The rest stays black.
    /// \param time     Seconds since the start of the game, which animates the flames.
    void draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport, float time) const;

private:
    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    GLsizei vertex_count_ = 0;
    GLuint texture_ = 0;
    GLuint light_buffer_ = 0;  ///< Uniform buffer with the lights, filled on every draw.
    GLuint flame_program_ = 0;
    GLuint flame_vertex_array_ = 0;
    GLuint flame_buffer_ = 0;  ///< One FlameInstance per torch.
    GLsizei flame_count_ = 0;
};

}  // namespace manifold
