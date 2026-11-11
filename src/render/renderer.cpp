#include "render/renderer.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include "render/camera.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"

namespace manifold {

namespace {

/// Passes world positions, normals and texture coordinates to the fragment shader.
constexpr const char* kVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

uniform mat4 u_view_projection;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_uv;

void main() {
    v_position = a_position;
    v_normal = a_normal;
    v_uv = a_uv;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)glsl";

/// Lights each pixel with the ambient light and the point light.
///
/// Lighting is computed in linear RGB and then tonemapped and converted to sRGB for the display.
constexpr const char* kFragmentShader = R"glsl(
#version 410 core
in vec3 v_position;
in vec3 v_normal;
in vec2 v_uv;

uniform sampler2D u_texture;
uniform vec3 u_ambient;
uniform vec3 u_light_position;
uniform vec3 u_light_color;
uniform float u_light_intensity;

out vec4 frag_color;

vec3 linear_to_srgb(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, color));
}

void main() {
    vec3 normal = normalize(v_normal);
    // The texture has an sRGB format, so sampling returns linear RGB.
    vec3 albedo = texture(u_texture, v_uv).rgb;

    vec3 to_light = u_light_position - v_position;
    float distance_squared = max(dot(to_light, to_light), 0.01);
    float diffuse = max(dot(normal, to_light * inversesqrt(distance_squared)), 0.0);
    vec3 radiance = albedo * (u_ambient + u_light_color * u_light_intensity * diffuse / distance_squared);

    // Reinhard tonemapping maps any brightness into [0, 1).
    vec3 mapped = radiance / (1.0 + radiance);
    frag_color = vec4(linear_to_srgb(mapped), 1.0);
}
)glsl";

/// Compiles one shader stage.
///
/// \param type   GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
/// \param source GLSL source code.
/// \throws std::runtime_error with the compiler log if compilation fails.
GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        std::string log(1024, '\0');
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("shader compilation failed: " + log);
    }
    return shader;
}

/// Compiles and links a shader program.
///
/// \throws std::runtime_error with the compiler or linker log if anything fails.
GLuint link_program(const char* vertex_source, const char* fragment_source) {
    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        std::string log(1024, '\0');
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("shader linking failed: " + log);
    }
    return program;
}

/// Describes a float vector field of Vertex to the currently bound vertex array.
///
/// \param location   Attribute location in the vertex shader.
/// \param components Number of floats in the field, e.g. 3 for a vec3.
/// \param offset     Offset of the field in Vertex, in bytes.
void set_vertex_attribute(GLuint location, GLint components, std::size_t offset) {
    glEnableVertexAttribArray(location);
    // OpenGL takes the offset into the bound buffer as a pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    const auto* pointer = reinterpret_cast<const void*>(offset);
    glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE, sizeof(Vertex), pointer);
}

/// Uploads an image as a repeating texture with nearest-neighbour filtering and mipmaps.
///
/// \return The texture name. The texture stays bound to GL_TEXTURE_2D.
GLuint upload_texture(const Image& image) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Image rows go from the top down, while OpenGL expects the bottom row first. This is undone by
    // texture_coordinates() in world_mesh.cpp, where v grows downwards.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Sharp texels up close for the retro look. Mipmaps keep distant surfaces from shimmering.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    return texture;
}

}  // namespace

Renderer::Renderer(std::span<const Vertex> world_mesh, const Image& texture)
    : program_(link_program(kVertexShader, kFragmentShader)),
      vertex_count_(static_cast<GLsizei>(world_mesh.size())),
      texture_(upload_texture(texture)) {
    glGenVertexArrays(1, &vertex_array_);
    glBindVertexArray(vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(world_mesh.size_bytes()), world_mesh.data(), GL_STATIC_DRAW);
    set_vertex_attribute(0, 3, offsetof(Vertex, position));
    set_vertex_attribute(1, 3, offsetof(Vertex, normal));
    set_vertex_attribute(2, 2, offsetof(Vertex, uv));
    glBindVertexArray(0);

    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "u_texture"), 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

Renderer::~Renderer() {
    glDeleteTextures(1, &texture_);
    glDeleteBuffers(1, &vertex_buffer_);
    glDeleteVertexArrays(1, &vertex_array_);
    glDeleteProgram(program_);
}

void Renderer::draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport) const {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

    glUseProgram(program_);
    const glm::mat4 view_projection = camera.projection() * camera.view();
    glUniformMatrix4fv(glGetUniformLocation(program_, "u_view_projection"), 1, GL_FALSE,
                       glm::value_ptr(view_projection));
    glUniform3fv(glGetUniformLocation(program_, "u_ambient"), 1, glm::value_ptr(lighting.ambient));
    glUniform3fv(glGetUniformLocation(program_, "u_light_position"), 1, glm::value_ptr(lighting.light.position));
    glUniform3fv(glGetUniformLocation(program_, "u_light_color"), 1, glm::value_ptr(lighting.light.color));
    glUniform1f(glGetUniformLocation(program_, "u_light_intensity"), lighting.light.intensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(vertex_array_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    glBindVertexArray(0);
}

}  // namespace manifold
