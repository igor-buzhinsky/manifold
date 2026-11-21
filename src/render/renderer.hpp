#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <glad/gl.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/shadow_map.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "world/door.hpp"
#include "world/world.hpp"

namespace manifold {

/// Options of Renderer::draw() for inspecting a level.
struct DrawOptions {
    bool lighting = true;  ///< If false, surfaces have their plain texture colors, without lights or shadows.
    /// Triangles drawn in one bright color on top of everything, e.g. build_arrow(), or none.
    std::span<const glm::vec3> marker;
    /// If set, the world and the flames above this height are left out, so a camera above can see into a
    /// layer. Where solids are cut open, their insides are drawn in a dark color. Shadows don't change.
    std::optional<float> cut_height;
    float shadow_bias = kShadowBias;  ///< See kShadowBias.
};

/// What the last Renderer::draw() did, e.g. to show it in the debug overlay.
struct RenderStats {
    std::size_t lights = 0;             ///< Lights drawn.
    std::size_t shadowed_lights = 0;    ///< Lights drawn that have a shadow map.
    std::size_t shadow_maps_drawn = 0;  ///< Shadow maps drawn (again) before the picture.
};

/// Draws the world and the torch flames with OpenGL, with shadows from the lights that cast them.
///
/// Each light with a `shadow_id` gets a cube shadow map, which is kept for later frames as long as the
/// light is drawn often enough, see ShadowCache. Lights and the world must therefore not move. A map is
/// drawn again when the range of its light changes.
///
/// Needs a current OpenGL 4.1 core context, see Window for its whole lifetime.
class Renderer {
public:
    /// Compiles the shaders and uploads the meshes of the world, its texture and the flames to the GPU.
    ///
    /// The world, with the chests (see build_chest_mesh()), casts shadows, see build_world_mesh() and
    /// build_shadow_faces(), and so do the door halves (see build_door_mesh()), which are lit like the world, with the
    /// texture of make_door_texture() for the world's door look. Torches and the exit don't: torch handles (see
    /// build_torch_mesh()), flames and the exit (see build_exit_mesh()) are only drawn. Torch i flickers with
    /// torch_phase(i). The exit spins (see exit_spin_angle()), and is painted as an animated, partly see-through portal
    /// that pulses with its light (see exit_pulse()).
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
    /// the doors, the flames, the exit and the marker.
    ///
    /// \param camera   Where the world is seen from.
    /// \param lighting The lights to use. Lights after the first kMaxLights are left out.
    /// \param viewport Part of the framebuffer to draw into. The rest stays black.
    /// \param time     Seconds since the start of the game, which animates the flames.
    /// \param doors    How far each door of the world is open, see Game::doors(). Doors without a state are drawn
    ///                 closed.
    /// \param options  Options for inspecting the level.
    void draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport, float time,
              std::span<const DoorState> doors, const DrawOptions& options = {});

    /// What the last call of draw() did.
    const RenderStats& stats() const { return stats_; }

private:
    /// Draws the exit, if the level has one, blended over what is drawn already. Leaves GL_CLIP_DISTANCE0 as it
    /// is, so the exit is cut like the rest.
    ///
    /// Changes the current program and the bound vertex array.
    void draw_exit(const Camera& camera, const glm::mat4& view_projection, float time, const DrawOptions& options);

    /// Uploads the door halves again if a door has moved since they were last uploaded.
    ///
    /// The shadow maps that see a door that moved are marked to be drawn again, see light_reaches_door().
    void update_doors(std::span<const DoorState> doors);

    /// Gives each light that casts shadows a shadow map, and draws the maps that are new, whose light's range
    /// changed, or that update_doors() marked.
    ///
    /// Changes the viewport, the current program, the bound vertex array and, if any map is drawn, the
    /// depth attachment of the shadow framebuffer. The framebuffer that was bound before is bound again at the
    /// end.
    ///
    /// Also counts the maps drawn in `stats_`.
    ///
    /// \return The shadow map slot of each of the first kMaxLights lights, if it has one.
    std::vector<std::optional<std::size_t>> update_shadow_maps(std::span<const PointLight> lights);

    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    /// Vertices of the world and the chests, which come first in the vertex buffer.
    GLsizei world_vertex_count_ = 0;
    GLsizei vertex_count_ = 0;  ///< Vertices of the world, the chests and the torch handles.
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
    /// The range of the light whose shadow map is in each slot, when the map was drawn. Zero for no map, or for a
    /// map that must be drawn again because a door moved.
    std::vector<float> shadow_ranges_;
    /// The position of the light whose shadow map is in each slot, when the map was drawn.
    std::vector<glm::vec3> shadow_positions_;
    std::optional<glm::vec3> exit_center_;  ///< Center of the exit's cube, or none if the level has no exit.
    GLuint exit_program_ = 0;
    GLuint exit_vertex_array_ = 0;
    GLuint exit_buffer_ = 0;  ///< The cube of build_exit_mesh().
    GLsizei exit_vertex_count_ = 0;
    std::vector<Door> doors_;           ///< The doors of the world.
    std::vector<float> door_openness_;  ///< How far each door was open when the door buffer was filled.
    GLuint door_vertex_array_ = 0;
    GLuint door_buffer_ = 0;  ///< The halves of all doors, see build_door_mesh().
    GLuint door_texture_ = 0;
    GLuint marker_program_ = 0;
    GLuint marker_vertex_array_ = 0;
    GLuint marker_buffer_ = 0;  ///< The marker's triangles, filled on every draw that has a marker.
    RenderStats stats_;
};

}  // namespace manifold
