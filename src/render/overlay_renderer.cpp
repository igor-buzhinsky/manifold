#include "render/overlay_renderer.hpp"

#include <cstddef>
#include <span>

#include <glad/gl.h>

#include "render/gl_util.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"

namespace manifold {

namespace {

/// Converts pixels from the top-left corner, y down, to OpenGL's coordinates.
constexpr const char* kVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;

uniform vec2 u_size;  // Width and height of the viewport, in pixels.

out vec4 v_color;

void main() {
    v_color = a_color;
    gl_Position = vec4(a_position.x / u_size.x * 2.0 - 1.0, 1.0 - a_position.y / u_size.y * 2.0, 0.0, 1.0);
}
)glsl";

/// Uses the color as it is: it is already sRGB.
constexpr const char* kFragmentShader = R"glsl(
#version 410 core
in vec4 v_color;

out vec4 frag_color;

void main() {
    frag_color = v_color;
}
)glsl";

}  // namespace

OverlayRenderer::OverlayRenderer() : program_(link_program(kVertexShader, kFragmentShader)) {
    glGenVertexArrays(1, &vertex_array_);
    glBindVertexArray(vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    set_vertex_attribute(0, 2, sizeof(OverlayVertex), offsetof(OverlayVertex, position));
    set_vertex_attribute(1, 4, sizeof(OverlayVertex), offsetof(OverlayVertex, color));
    glBindVertexArray(0);
}

OverlayRenderer::~OverlayRenderer() {
    glDeleteBuffers(1, &vertex_buffer_);
    glDeleteVertexArrays(1, &vertex_array_);
    glDeleteProgram(program_);
}

void OverlayRenderer::draw(const OverlayMesh& mesh, const Viewport& viewport) const {
    if (mesh.vertices().empty()) {
        return;
    }
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_size"), static_cast<float>(viewport.width),
                static_cast<float>(viewport.height));
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    const std::span<const OverlayVertex> vertices(mesh.vertices());
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data(), GL_STREAM_DRAW);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}

}  // namespace manifold
