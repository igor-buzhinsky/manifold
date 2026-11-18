#pragma once

#include <cstddef>

#include <glad/gl.h>

namespace manifold {

/// Compiles one shader stage.
///
/// \param type   GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
/// \param source GLSL source code.
/// \throws std::runtime_error with the compiler log if compilation fails.
GLuint compile_shader(GLenum type, const char* source);

/// Compiles and links a shader program.
///
/// \throws std::runtime_error with the compiler or linker log if anything fails.
GLuint link_program(const char* vertex_source, const char* fragment_source);

/// Clears the whole bound framebuffer to black, e.g. when a screen of the interface hides the game.
void clear_to_black();

/// Converts an offset into the bound array buffer to the pointer that OpenGL takes for it.
const void* buffer_offset(std::size_t offset);

/// Describes a float vector field of the bound array buffer to the currently bound vertex array.
///
/// \param location   Attribute location in the vertex shader.
/// \param components Number of floats in the field, e.g. 3 for a vec3.
/// \param stride     Size of one element of the buffer, e.g. sizeof(Vertex), in bytes.
/// \param offset     Offset of the field in an element, in bytes.
void set_vertex_attribute(GLuint location, GLint components, std::size_t stride, std::size_t offset);

}  // namespace manifold
