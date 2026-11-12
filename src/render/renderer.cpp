#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "render/camera.hpp"
#include "render/lighting.hpp"
#include "render/shadow_map.hpp"
#include "render/texture_image.hpp"
#include "render/viewport.hpp"
#include "render/world_mesh.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// Passes world positions, normals, texture coordinates and materials to the fragment shader.
constexpr const char* kVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec3 a_color;
layout(location = 4) in float a_texture_weight;

uniform mat4 u_view_projection;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_color;
out float v_texture_weight;

void main() {
    v_position = a_position;
    v_normal = a_normal;
    v_uv = a_uv;
    v_color = a_color;
    v_texture_weight = a_texture_weight;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)glsl";

/// The size of the light array in kFragmentShader.
static_assert(kMaxLights == 32, "update u_lights in kFragmentShader");

/// Lights each pixel with the ambient light and the point lights.
///
/// Lighting is computed in linear RGB and then tonemapped and converted to sRGB for the display.
constexpr const char* kFragmentShader = R"glsl(
#version 410 core
in vec3 v_position;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_color;
in float v_texture_weight;

struct Light {
    vec4 position_range;   // xyz: position, w: range.
    vec4 color_intensity;  // rgb: color, a: intensity.
    vec4 shadow;           // x: layer of the light's shadow map in u_shadow_maps, or -1 for no shadows.
};

layout(std140) uniform Lights {
    Light u_lights[32];
    int u_light_count;
};

uniform sampler2D u_texture;
uniform samplerCubeArrayShadow u_shadow_maps;
uniform vec3 u_ambient;

out vec4 frag_color;

// Surfaces count as this much closer to the light than they are, in metres, so that rounding in the
// shadow map doesn't shadow surfaces that should be lit. Lit surfaces are only compared with back faces
// behind them, so the bias can be small. A larger one would let light through where a floor meets a wall.
const float kShadowBias = 0.005;

// Surfaces closer to a light than 0.3 m are lit as if they were 0.3 m away. This keeps the walls right
// behind torch flames from turning white.
const float kMinDistanceSquared = 0.09;

// How much of light i reaches this point: 1 if nothing is in the way, 0 if it is in shadow.
//
// The shadow map holds, for each direction, the distance from the light to the back faces of what is in
// the way, divided by the range. Linear filtering mixes the results of the 2 x 2 nearest texels, which
// softens the edges of shadows.
//
// This is called in non-uniform control flow, where implicit derivatives are undefined. That is harmless
// only because the shadow maps have no mipmaps, so texture() doesn't use the level of detail. Adding
// mipmaps would need the lookup to move out of the branches.
float light_reaching(int i) {
    float layer = u_lights[i].shadow.x;
    if (layer < 0.0) {
        return 1.0;
    }
    vec3 from_light = v_position - u_lights[i].position_range.xyz;
    float reference = (length(from_light) - kShadowBias) / u_lights[i].position_range.w;
    return texture(u_shadow_maps, vec4(from_light, layer), reference);
}

vec3 linear_to_srgb(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, color));
}

void main() {
    vec3 normal = normalize(v_normal);
    // The texture has an sRGB format, so sampling returns linear RGB.
    vec3 albedo = v_color * mix(vec3(1.0), texture(u_texture, v_uv).rgb, v_texture_weight);

    vec3 irradiance = u_ambient;
    for (int i = 0; i < u_light_count; ++i) {
        vec3 to_light = u_lights[i].position_range.xyz - v_position;
        float range = u_lights[i].position_range.w;
        float distance_squared = dot(to_light, to_light);
        float diffuse = max(dot(normal, to_light * inversesqrt(max(distance_squared, 1e-6))), 0.0);
        if (diffuse == 0.0) {
            continue;  // Facing away from the light, which also saves the shadow map lookup.
        }
        // Inverse-square falloff, faded smoothly to zero at the range: (1 - (d / range)^4)^2.
        float ratio = distance_squared / (range * range);
        float window = clamp(1.0 - ratio * ratio, 0.0, 1.0);
        vec3 light = u_lights[i].color_intensity.rgb * u_lights[i].color_intensity.a;
        irradiance += light * diffuse * window * window / max(distance_squared, kMinDistanceSquared) *
                      light_reaching(i);
    }
    vec3 radiance = albedo * irradiance;

    // Reinhard tonemapping maps any brightness into [0, 1).
    vec3 mapped = radiance / (1.0 + radiance);
    frag_color = vec4(linear_to_srgb(mapped), 1.0);
}
)glsl";

/// Draws the world from a point light into one face of its cube shadow map.
constexpr const char* kShadowVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_position;

uniform mat4 u_view_projection;  // See cube_face_view_projection().

out vec3 v_position;

void main() {
    v_position = a_position;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)glsl";

/// Records the distance from the light, divided by its range, as the depth.
///
/// The depth buffer then keeps the nearest of these distances in each direction. Storing distances,
/// rather than the usual perspective depth, lets kFragmentShader compare them directly.
constexpr const char* kShadowFragmentShader = R"glsl(
#version 410 core
in vec3 v_position;

uniform vec3 u_light_position;
uniform float u_range;

void main() {
    gl_FragDepth = length(v_position - u_light_position) / u_range;
}
)glsl";

/// Draws each flame as an upright quad that turns around the vertical to face the camera.
///
/// It is drawn instanced: one instance per flame, four vertices each, as a triangle strip.
constexpr const char* kFlameVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_bottom;
layout(location = 1) in float a_phase;

uniform mat4 u_view_projection;
uniform vec3 u_right;  // Horizontal unit vector to the right of the camera.
uniform vec2 u_size;   // Width and height of a flame, in metres.

out vec2 v_uv;
flat out float v_phase;

void main() {
    // Bottom left, bottom right, top left, top right.
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1);
    v_uv = corner;
    v_phase = a_phase;
    vec3 position = a_bottom + u_right * ((corner.x - 0.5) * u_size.x) + vec3(0.0, 0.0, corner.y * u_size.y);
    gl_Position = u_view_projection * vec4(position, 1.0);
}
)glsl";

/// Paints a flickering, pixelated flame, without lighting.
///
/// The flame is a teardrop that sways and changes height over time, in three bands of color from the hot
/// core to the edge. Pixels outside of it are discarded, so flames need no sorting by depth.
constexpr const char* kFlameFragmentShader = R"glsl(
#version 410 core
in vec2 v_uv;  // x across the flame, y from the bottom up, both in [0, 1].
flat in float v_phase;

uniform float u_time;

out vec4 frag_color;

const vec2 kPixels = vec2(6.0, 11.0);  // Big pixels across and up the flame, for the retro look.
const float kPi = 3.14159265;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec2 cell = floor(v_uv * kPixels);
    vec2 p = (cell + 0.5) / kPixels;
    float top = 0.8 + 0.2 * sin(11.0 * u_time + 3.0 * v_phase);
    float h = p.y / top;
    if (h >= 1.0) {
        discard;
    }
    // The sway grows towards the top of the flame.
    float sway = p.y * (0.15 * sin(6.0 * u_time + v_phase + 4.0 * p.y) + 0.07 * sin(13.0 * u_time + 2.0 * v_phase));
    float x = abs(p.x - 0.5 - sway) * 2.0;
    // Round at the bottom, pointed at the top.
    float half_width = sin(kPi * sqrt(h));
    // The noise changes 12 times a second. Wrapping the step keeps the argument of sin() in hash() small,
    // where it is precise.
    float noise = hash(cell + vec2(mod(floor(u_time * 12.0), 256.0), v_phase));
    float heat = (1.0 - x / max(half_width, 1e-3)) * (1.0 - 0.4 * h) + 0.25 * (noise - 0.5);
    if (heat < 0.1) {
        discard;
    }
    // Already sRGB: the flame is as bright as the display allows.
    vec3 color = heat > 0.65 ? vec3(1.0, 0.95, 0.7) : heat > 0.35 ? vec3(1.0, 0.65, 0.15) : vec3(0.85, 0.25, 0.05);
    frag_color = vec4(color, 1.0);
}
)glsl";

/// One light in the `Lights` uniform block, laid out as std140 wants it.
struct GpuLight {
    glm::vec4 position_range;
    glm::vec4 color_intensity;
    glm::vec4 shadow;  ///< x: layer in the shadow map array, or -1 for no shadows.
};

/// The `Lights` uniform block, laid out as std140 wants it.
struct GpuLights {
    std::array<GpuLight, kMaxLights> lights;
    std::int32_t count;
    std::array<std::int32_t, 3> padding;  ///< A std140 block is a multiple of 16 bytes long.
};
static_assert(sizeof(GpuLights) == (kMaxLights * 48) + 16);

/// The uniform buffer binding that the `Lights` block uses.
constexpr GLuint kLightsBinding = 0;

/// Width and height of each face of a shadow map, in texels.
///
/// At 6 m, the range of a torch, a texel is about 5 cm wide. Lower resolution suits the retro look and
/// keeps the shadow maps of all slots in about 38 MB.
constexpr GLsizei kShadowMapSize = 256;

/// How many shadow maps are kept. More than kMaxLights, so that walking back and forth doesn't make
/// shadow maps be drawn again and again.
constexpr std::size_t kShadowSlots = 48;

/// Distance of the shell around the level boundary (see build_shadow_faces()) from the boundary, in metres.
constexpr float kShellThickness = 0.1f;

/// One flame, as uploaded to the GPU for kFlameVertexShader.
struct FlameInstance {
    glm::vec3 bottom;  ///< Center of the flame's bottom edge, see Torch::flame_bottom().
    float phase;       ///< See torch_phase().
};

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

/// Describes a float vector field of the bound array buffer to the currently bound vertex array.
///
/// \param location   Attribute location in the vertex shader.
/// \param components Number of floats in the field, e.g. 3 for a vec3.
/// \param stride     Size of one element of the buffer, e.g. sizeof(Vertex), in bytes.
/// \param offset     Offset of the field in an element, in bytes.
void set_vertex_attribute(GLuint location, GLint components, std::size_t stride, std::size_t offset) {
    glEnableVertexAttribArray(location);
    // OpenGL takes the offset into the bound buffer as a pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    const auto* pointer = reinterpret_cast<const void*>(offset);
    glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), pointer);
}

/// The flames of the torches, in the same order.
std::vector<FlameInstance> make_flames(std::span<const Torch> torches) {
    std::vector<FlameInstance> flames;
    flames.reserve(torches.size());
    for (std::size_t i = 0; i < torches.size(); ++i) {
        flames.push_back({.bottom = torches[i].flame_bottom(), .phase = torch_phase(i)});
    }
    return flames;
}

/// Converts lights to the layout of the `Lights` uniform block, leaving out those after kMaxLights.
///
/// \param lights       The lights.
/// \param shadow_slots The shadow map slot of each light, if it has one. Lights after its end have none.
GpuLights to_gpu_lights(std::span<const PointLight> lights, std::span<const std::optional<std::size_t>> shadow_slots) {
    GpuLights block{};
    const std::size_t count = std::min(lights.size(), kMaxLights);
    for (std::size_t i = 0; i < count; ++i) {
        const PointLight& light = lights[i];
        const std::optional<std::size_t> slot = i < shadow_slots.size() ? shadow_slots[i] : std::nullopt;
        const float layer = slot ? static_cast<float>(*slot) : -1.0f;
        block.lights.at(i) = {.position_range = {light.position, light.range},
                              .color_intensity = {light.color, light.intensity},
                              .shadow = {layer, 0.0f, 0.0f, 0.0f}};
    }
    block.count = static_cast<std::int32_t>(count);
    return block;
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

/// Creates the depth cube map array that holds kShadowSlots shadow maps, with depth comparison for
/// samplerCubeArrayShadow.
///
/// \return The texture name. The texture stays bound to GL_TEXTURE_CUBE_MAP_ARRAY.
GLuint create_shadow_maps() {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);
    // 16 bits store distances up to the range in steps of range / 65536, about 0.1 mm for a torch.
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT16, kShadowMapSize, kShadowMapSize,
                 static_cast<GLsizei>(kShadowSlots) * kCubeFaces, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    return texture;
}

/// Creates a framebuffer that draws only depth, into one face of `shadow_maps` at a time.
///
/// \throws std::runtime_error if the framebuffer is incomplete.
GLuint create_shadow_framebuffer(GLuint shadow_maps) {
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_maps, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &framebuffer);
        throw std::runtime_error("shadow map framebuffer is incomplete: status " + std::to_string(status));
    }
    return framebuffer;
}

}  // namespace

Renderer::Renderer(const World& world, const Image& texture)
    : program_(link_program(kVertexShader, kFragmentShader)),
      texture_(upload_texture(texture)),
      flame_program_(link_program(kFlameVertexShader, kFlameFragmentShader)),
      flame_count_(static_cast<GLsizei>(world.torches().size())),
      shadow_program_(link_program(kShadowVertexShader, kShadowFragmentShader)),
      shadow_maps_(create_shadow_maps()),
      shadow_framebuffer_(create_shadow_framebuffer(shadow_maps_)),
      shadow_cache_(kShadowSlots) {
    glGenVertexArrays(1, &vertex_array_);
    glBindVertexArray(vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    // One buffer holds the world, then the torch handles, then the faces only shadow maps need. The main
    // pass draws the first two, the shadow pass the first and the last.
    std::vector<Vertex> vertices = build_world_mesh(world);
    world_vertex_count_ = static_cast<GLsizei>(vertices.size());
    const std::vector<Vertex> torch_mesh = build_torch_mesh(world.torches());
    vertices.insert(vertices.end(), torch_mesh.begin(), torch_mesh.end());
    vertex_count_ = static_cast<GLsizei>(vertices.size());
    const std::vector<Vertex> shadow_faces = build_shadow_faces(world, kShellThickness);
    vertices.insert(vertices.end(), shadow_faces.begin(), shadow_faces.end());
    shadow_face_vertex_count_ = static_cast<GLsizei>(shadow_faces.size());
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(std::span(vertices).size_bytes()), vertices.data(),
                 GL_STATIC_DRAW);
    set_vertex_attribute(0, 3, sizeof(Vertex), offsetof(Vertex, position));
    set_vertex_attribute(1, 3, sizeof(Vertex), offsetof(Vertex, normal));
    set_vertex_attribute(2, 2, sizeof(Vertex), offsetof(Vertex, uv));
    set_vertex_attribute(3, 3, sizeof(Vertex), offsetof(Vertex, color));
    set_vertex_attribute(4, 1, sizeof(Vertex), offsetof(Vertex, texture_weight));

    // Each flame is one instance: its attributes advance once per instance, not per vertex.
    const std::vector<FlameInstance> flames = make_flames(world.torches());
    glGenVertexArrays(1, &flame_vertex_array_);
    glBindVertexArray(flame_vertex_array_);
    glGenBuffers(1, &flame_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, flame_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(std::span(flames).size_bytes()), flames.data(),
                 GL_STATIC_DRAW);
    set_vertex_attribute(0, 3, sizeof(FlameInstance), offsetof(FlameInstance, bottom));
    set_vertex_attribute(1, 1, sizeof(FlameInstance), offsetof(FlameInstance, phase));
    glVertexAttribDivisor(0, 1);
    glVertexAttribDivisor(1, 1);
    glBindVertexArray(0);

    glGenBuffers(1, &light_buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, light_buffer_);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GpuLights), nullptr, GL_DYNAMIC_DRAW);
    glUniformBlockBinding(program_, glGetUniformBlockIndex(program_, "Lights"), kLightsBinding);

    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "u_texture"), 0);
    glUniform1i(glGetUniformLocation(program_, "u_shadow_maps"), 1);
    glUseProgram(flame_program_);
    glUniform2f(glGetUniformLocation(flame_program_, "u_size"), kFlameWidth, kFlameHeight);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    // Linear filtering across the edges of cube faces, so shadows don't show the seams.
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

Renderer::~Renderer() {
    glDeleteFramebuffers(1, &shadow_framebuffer_);
    glDeleteTextures(1, &shadow_maps_);
    glDeleteProgram(shadow_program_);
    glDeleteBuffers(1, &flame_buffer_);
    glDeleteVertexArrays(1, &flame_vertex_array_);
    glDeleteProgram(flame_program_);
    glDeleteBuffers(1, &light_buffer_);
    glDeleteTextures(1, &texture_);
    glDeleteBuffers(1, &vertex_buffer_);
    glDeleteVertexArrays(1, &vertex_array_);
    glDeleteProgram(program_);
}

std::vector<std::optional<std::size_t>> Renderer::update_shadow_maps(std::span<const PointLight> lights) {
    // The ids of the lights that cast shadows, and which light each belongs to.
    const std::size_t count = std::min(lights.size(), kMaxLights);
    std::vector<std::uint32_t> ids;
    std::vector<std::size_t> owners;
    for (std::size_t i = 0; i < count; ++i) {
        if (lights[i].shadow_id) {
            ids.push_back(*lights[i].shadow_id);
            owners.push_back(i);
        }
    }
    const ShadowCache::Assignment assignment = shadow_cache_.assign(ids);
    std::vector<std::optional<std::size_t>> slots(count);
    for (std::size_t k = 0; k < ids.size(); ++k) {
        slots[owners[k]] = assignment.slots[k];
    }
    if (assignment.to_draw.empty()) {
        return slots;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffer_);
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glUseProgram(shadow_program_);
    glBindVertexArray(vertex_array_);
    // Torch handles cast no shadows: a real flame is much larger than our point light, so the handle under
    // it would cast a long, hard shadow down the wall that a real torch doesn't. Only the world and the
    // faces from build_shadow_faces() are drawn.
    // Only back faces, as seen from the light, go into the shadow map. A lit surface is then never compared
    // with itself, which would shadow parts of it at random (shadow acne). Surfaces that face away from the
    // light are compared with themselves, but they get no light anyway.
    glCullFace(GL_FRONT);
    for (std::size_t k = 0; k < ids.size(); ++k) {
        const std::optional<std::size_t> slot = assignment.slots[k];
        if (!slot || std::ranges::find(assignment.to_draw, *slot) == assignment.to_draw.end()) {
            continue;
        }
        const PointLight& light = lights[owners[k]];
        glUniform3fv(glGetUniformLocation(shadow_program_, "u_light_position"), 1, glm::value_ptr(light.position));
        glUniform1f(glGetUniformLocation(shadow_program_, "u_range"), light.range);
        for (int face = 0; face < kCubeFaces; ++face) {
            const auto layer = (static_cast<GLint>(*slot) * kCubeFaces) + face;
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_maps_, 0, layer);
            glClear(GL_DEPTH_BUFFER_BIT);
            const glm::mat4 view_projection = cube_face_view_projection(light.position, light.range, face);
            glUniformMatrix4fv(glGetUniformLocation(shadow_program_, "u_view_projection"), 1, GL_FALSE,
                               glm::value_ptr(view_projection));
            glDrawArrays(GL_TRIANGLES, 0, world_vertex_count_);
            glDrawArrays(GL_TRIANGLES, vertex_count_, shadow_face_vertex_count_);
        }
    }
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return slots;
}

void Renderer::draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport, float time) {
    const std::vector<std::optional<std::size_t>> shadow_slots = update_shadow_maps(lighting.lights);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    const glm::mat4 view_projection = camera.projection() * camera.view();

    const GpuLights lights = to_gpu_lights(lighting.lights, shadow_slots);
    glBindBuffer(GL_UNIFORM_BUFFER, light_buffer_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GpuLights), &lights);
    glBindBufferBase(GL_UNIFORM_BUFFER, kLightsBinding, light_buffer_);

    glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_, "u_view_projection"), 1, GL_FALSE,
                       glm::value_ptr(view_projection));
    glUniform3fv(glGetUniformLocation(program_, "u_ambient"), 1, glm::value_ptr(lighting.ambient));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadow_maps_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(vertex_array_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);

    if (flame_count_ > 0) {
        // Flames turn to the camera only around the vertical, so they stay upright when looking up or down.
        const glm::vec3 right{std::sin(camera.yaw), -std::cos(camera.yaw), 0.0f};
        glUseProgram(flame_program_);
        glUniformMatrix4fv(glGetUniformLocation(flame_program_, "u_view_projection"), 1, GL_FALSE,
                           glm::value_ptr(view_projection));
        glUniform3fv(glGetUniformLocation(flame_program_, "u_right"), 1, glm::value_ptr(right));
        glUniform1f(glGetUniformLocation(flame_program_, "u_time"), time);
        glBindVertexArray(flame_vertex_array_);
        glDisable(GL_CULL_FACE);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, flame_count_);
        glEnable(GL_CULL_FACE);
    }
    glBindVertexArray(0);
}

}  // namespace manifold
