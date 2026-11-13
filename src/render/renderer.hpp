#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <glad/gl.h>
#include <glm/vec3.hpp>

#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/shadow_map.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "world/world.hpp"

namespace manifold {

/// Options of Renderer::draw() for inspecting a level.
struct DrawOptions {
    bool lighting = true;  ///< If false, surfaces have their plain texture colors, without lights or shadows.
    /// Triangles drawn in one bright color on top of everything, e.g. build_arrow(), or none.
    std::span<const glm::vec3> marker;
};

/// Draws the world and the torch flames with OpenGL, with shadows from the lights that cast them.
///
/// Each light with a `shadow_id` gets a cube shadow map, which is kept for later frames as long as the
/// light is drawn often enough, see ShadowCache. Lights and the world must therefore not move.
///
/// Needs a current OpenGL 4.1 core context, see Window for its whole lifetime.
class Renderer {
public:
    /// Compiles the shaders and uploads the meshes of the world, its texture and the flames to the GPU.
    ///
    /// The world casts shadows, see build_world_mesh() and build_shadow_faces(). Torches don't: their
    /// handles (see build_torch_mesh()) and flames are only drawn. Torch i flickers with torch_phase(i).
    ///
    /// \param world   The world to draw. Only its geometry is copied, so it doesn't need to outlive the
    ///                renderer.
    /// \param texture The texture of all surfaces, repeated as the mesh's texture coordinates say.
    /// \throws std::runtime_error if a shader fails to compile or link.
    Renderer(const World& world, const Image& texture);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /// Draws the shadow maps that are missing, then clears the bound framebuffer to black and draws the world,
    /// the flames and the marker.
    ///
    /// \param camera   Where the world is seen from.
    /// \param lighting The lights to use. Lights after the first kMaxLights are left out.
    /// \param viewport Part of the framebuffer to draw into. The rest stays black.
    /// \param time     Seconds since the start of the game, which animates the flames.
    /// \param options  Options for inspecting the level.
    void draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport, float time,
              const DrawOptions& options = {});

private:
    /// Gives each light that casts shadows a shadow map, and draws the maps that are new.
    ///
    /// Changes the viewport, the current program, the bound vertex array and, if any map is drawn, the
    /// depth attachment of the shadow framebuffer. The framebuffer that was bound before is bound again at the
    /// end.
    ///
    /// \return The shadow map slot of each of the first kMaxLights lights, if it has one.
    std::vector<std::optional<std::size_t>> update_shadow_maps(std::span<const PointLight> lights);

    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    GLsizei world_vertex_count_ = 0;  ///< Vertices of the world, which come first in the vertex buffer.
    GLsizei vertex_count_ = 0;        ///< Vertices of the world and the torch handles.
    /// Vertices of build_shadow_faces(), which come after the torch handles.
    GLsizei shadow_face_vertex_count_ = 0;
    GLuint texture_ = 0;
    GLuint light_buffer_ = 0;  ///< Uniform buffer with the lights, filled on every draw.
    GLuint flame_program_ = 0;
    GLuint flame_vertex_array_ = 0;
    GLuint flame_buffer_ = 0;  ///< One FlameInstance per torch.
    GLsizei flame_count_ = 0;
    GLuint shadow_program_ = 0;
    GLuint shadow_maps_ = 0;  ///< Cube map array with the shadow map of each slot of `shadow_cache_`.
    GLuint shadow_framebuffer_ = 0;
    ShadowCache shadow_cache_;
    GLuint marker_program_ = 0;
    GLuint marker_vertex_array_ = 0;
    GLuint marker_buffer_ = 0;  ///< The marker's triangles, filled on every draw that has a marker.
};

}  // namespace manifold
