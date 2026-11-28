#include "render/image_renderer.hpp"

#include <array>
#include <cstddef>
#include <utility>

#include <glad/gl.h>
#include <glm/vec2.hpp>

#include "render/gl_util.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "ui/rect.hpp"

namespace manifold {

namespace {

/// A corner of the rectangle of an image.
struct ImageVertex {
    glm::vec2 position;  ///< In pixels from the top-left corner of the picture, y down.
    glm::vec2 uv;        ///< Texture coordinates, v down from the top row of the image.
};

/// Converts pixels from the top-left corner, y down, to OpenGL's coordinates.
constexpr const char* kVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;

uniform vec2 u_size;  // Width and height of the viewport, in pixels.

out vec2 v_uv;

void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_position.x / u_size.x * 2.0 - 1.0, 1.0 - a_position.y / u_size.y * 2.0, 0.0, 1.0);
}
)glsl";

/// The texture is sRGB, so sampling gives linear colors, which are filtered correctly. They are converted back to sRGB,
/// since nothing else converts the overlay's colors. A gray image gets the luminance of its linear colors, darkened.
constexpr const char* kFragmentShader = R"glsl(
#version 410 core
in vec2 v_uv;

uniform sampler2D u_image;
uniform bool u_gray;  // Whether to draw the image gray, see ImageRenderer::draw().

out vec4 frag_color;

vec3 to_srgb(vec3 linear) {
    vec3 low = linear * 12.92;
    vec3 high = 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), linear));
}

void main() {
    vec4 color = texture(u_image, v_uv);
    if (u_gray) {
        color.rgb = vec3(dot(color.rgb, vec3(0.2126, 0.7152, 0.0722)) * 0.5);
    }
    frag_color = vec4(to_srgb(color.rgb), color.a);
}
)glsl";

}  // namespace

ImageTexture::ImageTexture(const Image& image) {
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // The rows go from the top down, so v = 0 is the top row, as ImageVertex::uv has it.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

ImageTexture::~ImageTexture() { glDeleteTextures(1, &texture_); }

ImageTexture::ImageTexture(ImageTexture&& other) noexcept : texture_(std::exchange(other.texture_, 0)) {}

ImageTexture& ImageTexture::operator=(ImageTexture&& other) noexcept {
    if (this != &other) {
        glDeleteTextures(1, &texture_);
        texture_ = std::exchange(other.texture_, 0);
    }
    return *this;
}

ImageRenderer::ImageRenderer() : program_(link_program(kVertexShader, kFragmentShader)) {
    glGenVertexArrays(1, &vertex_array_);
    glBindVertexArray(vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    set_vertex_attribute(0, 2, sizeof(ImageVertex), offsetof(ImageVertex, position));
    set_vertex_attribute(1, 2, sizeof(ImageVertex), offsetof(ImageVertex, uv));
    glBindVertexArray(0);
}

ImageRenderer::~ImageRenderer() {
    glDeleteBuffers(1, &vertex_buffer_);
    glDeleteVertexArrays(1, &vertex_array_);
    glDeleteProgram(program_);
}

void ImageRenderer::draw(const ImageTexture& image, const Rect& area, const Viewport& viewport, bool gray) const {
    const glm::vec2 top_left = area.top_left;
    const glm::vec2 bottom_right = area.top_left + area.size;
    // Two triangles, without culling.
    const std::array<ImageVertex, 6> vertices{{
        {.position = top_left, .uv = {0.0f, 0.0f}},
        {.position = {bottom_right.x, top_left.y}, .uv = {1.0f, 0.0f}},
        {.position = bottom_right, .uv = {1.0f, 1.0f}},
        {.position = top_left, .uv = {0.0f, 0.0f}},
        {.position = bottom_right, .uv = {1.0f, 1.0f}},
        {.position = {top_left.x, bottom_right.y}, .uv = {0.0f, 1.0f}},
    }};
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_size"), static_cast<float>(viewport.width),
                static_cast<float>(viewport.height));
    glUniform1i(glGetUniformLocation(program_, "u_image"), 0);
    glUniform1i(glGetUniformLocation(program_, "u_gray"), gray ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image.texture());
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STREAM_DRAW);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace manifold
