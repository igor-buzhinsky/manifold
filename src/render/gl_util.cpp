#include "render/gl_util.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

#include <glad/gl.h>

namespace manifold {

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

const void* buffer_offset(std::size_t offset) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    return reinterpret_cast<const void*>(offset);
}

void set_vertex_attribute(GLuint location, GLint components, std::size_t stride, std::size_t offset) {
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          buffer_offset(offset));
}

}  // namespace manifold
