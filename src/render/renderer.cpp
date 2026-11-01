#include "render/renderer.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>

#include <epoxy/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include "render/camera.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"

namespace manifold {

namespace {

constexpr const char* kVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;

uniform mat4 u_view_projection;

out vec3 v_position;
out vec3 v_normal;
out vec3 v_color;

void main() {
    v_position = a_position;
    v_normal = a_normal;
    v_color = a_color;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)glsl";

// Lighting is computed in linear RGB and then tonemapped and converted to sRGB for the display.
constexpr const char* kFragmentShader = R"glsl(
#version 330 core
in vec3 v_position;
in vec3 v_normal;
in vec3 v_color;

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

    // A 1 m checker pattern gives a sense of scale and depth until there are textures.
    // The small offset along the normal keeps cells stable on surfaces that lie on cell borders.
    vec3 cell = floor(v_position - 0.001 * normal);
    float checker = mod(cell.x + cell.y + cell.z, 2.0);
    vec3 albedo = v_color * mix(0.8, 1.0, checker);

    vec3 to_light = u_light_position - v_position;
    float distance_squared = max(dot(to_light, to_light), 0.01);
    float diffuse = max(dot(normal, to_light * inversesqrt(distance_squared)), 0.0);
    vec3 radiance = albedo * (u_ambient + u_light_color * u_light_intensity * diffuse / distance_squared);

    // Reinhard tonemapping maps any brightness into [0, 1).
    vec3 mapped = radiance / (1.0 + radiance);
    frag_color = vec4(linear_to_srgb(mapped), 1.0);
}
)glsl";

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

void set_vertex_attribute(GLuint location, std::size_t offset) {
    glEnableVertexAttribArray(location);
    // OpenGL takes the offset into the bound buffer as a pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offset));
}

}  // namespace

Renderer::Renderer(std::span<const Vertex> world_mesh)
    : program_(link_program(kVertexShader, kFragmentShader)), vertex_count_(static_cast<GLsizei>(world_mesh.size())) {
    glGenVertexArrays(1, &vertex_array_);
    glBindVertexArray(vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(world_mesh.size_bytes()), world_mesh.data(), GL_STATIC_DRAW);
    set_vertex_attribute(0, offsetof(Vertex, position));
    set_vertex_attribute(1, offsetof(Vertex, normal));
    set_vertex_attribute(2, offsetof(Vertex, color));
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

Renderer::~Renderer() {
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

    glBindVertexArray(vertex_array_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    glBindVertexArray(0);
}

}  // namespace manifold
