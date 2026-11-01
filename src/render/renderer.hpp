#pragma once

#include <span>

#include <epoxy/gl.h>
#include <glm/vec3.hpp>

#include "render/camera.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"

namespace manifold {

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    // Brightness at a distance of 1 m. It falls off with the square of the distance.
    float intensity = 1.0f;
};

struct Lighting {
    glm::vec3 ambient{0.0f};  // Light that reaches every surface from all directions.
    PointLight light;
};

// Draws the world with OpenGL. Needs a current OpenGL 3.3 core context.
class Renderer {
public:
    explicit Renderer(std::span<const Vertex> world_mesh);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Clears the framebuffer to black and draws the world into `viewport`.
    void draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport) const;

private:
    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    GLsizei vertex_count_ = 0;
};

}  // namespace manifold
