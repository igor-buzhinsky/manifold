#pragma once

#include <span>

#include <glad/gl.h>
#include <glm/vec3.hpp>

#include "render/camera.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"

namespace manifold {

/// A light that shines equally in all directions from one point.
struct PointLight {
    glm::vec3 position{0.0f};  ///< In world coordinates.
    glm::vec3 color{1.0f};     ///< Linear RGB.
    /// Brightness at a distance of 1 m. It falls off with the square of the distance.
    float intensity = 1.0f;
};

/// All light in the scene.
struct Lighting {
    glm::vec3 ambient{0.0f};  ///< Light that reaches every surface from all directions, linear RGB.
    PointLight light;         ///< The player's light.
};

/// Draws the world with OpenGL.
///
/// Needs a current OpenGL 4.1 core context, see Window for its whole lifetime.
class Renderer {
public:
    /// Compiles the shaders and uploads the world mesh to the GPU.
    ///
    /// \param world_mesh Triangles to draw, see build_world_mesh().
    /// \throws std::runtime_error if a shader fails to compile or link.
    explicit Renderer(std::span<const Vertex> world_mesh);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /// Clears the framebuffer to black and draws the world.
    ///
    /// \param camera   Where the world is seen from.
    /// \param lighting The lights to use.
    /// \param viewport Part of the framebuffer to draw into. The rest stays black.
    void draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport) const;

private:
    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    GLsizei vertex_count_ = 0;
};

}  // namespace manifold
