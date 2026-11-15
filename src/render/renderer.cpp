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
#include "render/gl_util.hpp"
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
uniform float u_cut_height;  // See DrawOptions::cut_height. Only used while GL_CLIP_DISTANCE0 is on.

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
    gl_ClipDistance[0] = u_cut_height - a_position.z;
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
uniform bool u_lighting;  // If false, the surface has its plain color, without lights or shadows.
uniform bool u_cutaway;   // If true, back faces are the insides of solids that are cut open.
uniform float u_shadow_bias;  // See kShadowBias.

out vec4 frag_color;

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
    float reference = (length(from_light) - u_shadow_bias) / u_lights[i].position_range.w;
    return texture(u_shadow_maps, vec4(from_light, layer), reference);
}

vec3 linear_to_srgb(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, color));
}

// Color of the cut through a solid, as seen from above, already sRGB.
const vec3 kCutColor = vec3(0.12, 0.12, 0.14);

void main() {
    if (u_cutaway && !gl_FrontFacing) {
        frag_color = vec4(kCutColor, 1.0);
        return;
    }
    vec3 normal = normalize(v_normal);
    // The texture has an sRGB format, so sampling returns linear RGB.
    vec3 albedo = v_color * mix(vec3(1.0), texture(u_texture, v_uv).rgb, v_texture_weight);
    if (!u_lighting) {
        frag_color = vec4(linear_to_srgb(albedo), 1.0);
        return;
    }

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
layout(location = 2) in int a_flame;

uniform mat4 u_view_projection;
uniform vec3 u_right;  // Horizontal unit vector to the right of the camera.
uniform vec2 u_size;   // Width and height of a flame, in metres.
uniform float u_cut_height;  // See DrawOptions::cut_height. Only used while GL_CLIP_DISTANCE0 is on.

out vec2 v_uv;
flat out float v_phase;
flat out int v_flame;

void main() {
    v_flame = a_flame;
    // Bottom left, bottom right, top left, top right.
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1);
    v_uv = corner;
    v_phase = a_phase;
    vec3 position = a_bottom + u_right * ((corner.x - 0.5) * u_size.x) + vec3(0.0, 0.0, corner.y * u_size.y);
    gl_ClipDistance[0] = u_cut_height - position.z;
    gl_Position = u_view_projection * vec4(position, 1.0);
}
)glsl";

/// Paints a flickering, pixelated flame, without lighting.
///
/// The flame is a teardrop that sways and changes height over time, in three bands of color from the hot
/// core to the edge. The colors depend on the flame, in the order of TorchFlame. Pixels outside of it are
/// discarded, so flames need no sorting by depth.
constexpr const char* kFlameFragmentShader = R"glsl(
#version 410 core
in vec2 v_uv;  // x across the flame, y from the bottom up, both in [0, 1].
flat in float v_phase;
flat in int v_flame;  // TorchFlame: 0 orange, 1 blue, 2 green.

uniform float u_time;

out vec4 frag_color;

const vec2 kPixels = vec2(6.0, 11.0);  // Big pixels across and up the flame, for the retro look.
const float kPi = 3.14159265;

// Colors of the core, the middle and the edge of each kind of flame, already sRGB.
const vec3 kCore[3] = vec3[](vec3(1.0, 0.95, 0.7), vec3(0.85, 0.95, 1.0), vec3(0.9, 1.0, 0.7));
const vec3 kMiddle[3] = vec3[](vec3(1.0, 0.65, 0.15), vec3(0.3, 0.55, 1.0), vec3(0.45, 0.9, 0.2));
const vec3 kEdge[3] = vec3[](vec3(0.85, 0.25, 0.05), vec3(0.4, 0.15, 0.75), vec3(0.15, 0.4, 0.1));

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
    vec3 color = heat > 0.65 ? kCore[v_flame] : heat > 0.35 ? kMiddle[v_flame] : kEdge[v_flame];
    frag_color = vec4(color, 1.0);
}
)glsl";

/// Spins the cube of the level exit around the vertical through its center, and moves it into place.
constexpr const char* kExitVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_position;  // Relative to the center of the cube.
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

uniform mat4 u_view_projection;
uniform vec3 u_center;  // Center of the cube in the world.
uniform float u_angle;  // How far the cube has turned, in radians counter-clockwise.
uniform float u_cut_height;  // See DrawOptions::cut_height. Only used while GL_CLIP_DISTANCE0 is on.

out vec3 v_position;
out vec3 v_normal;
out vec2 v_uv;

void main() {
    mat2 turn = mat2(cos(u_angle), sin(u_angle), -sin(u_angle), cos(u_angle));
    v_position = u_center + vec3(turn * a_position.xy, a_position.z);
    v_normal = vec3(turn * a_normal.xy, a_normal.z);
    v_uv = a_uv;
    gl_ClipDistance[0] = u_cut_height - v_position.z;
    gl_Position = u_view_projection * vec4(v_position, 1.0);
}
)glsl";

/// Paints the faces of the exit cube as a pixelated portal: a glowing frame around a swirl of blue that turns
/// inwards, with sparkles. It glows by itself, without lighting.
///
/// The darker bands of the swirl are partly see-through, so the far faces of the cube show behind them. Faces
/// seen from the side are more see-through than faces seen from the front, which softens the outline.
constexpr const char* kExitFragmentShader = R"glsl(
#version 410 core
in vec3 v_position;
in vec3 v_normal;
in vec2 v_uv;  // From (0, 0) to (1, 1) across each face.

uniform vec3 u_camera;  // Where the camera is.
uniform float u_time;   // Seconds since the start of the game.
uniform float u_pulse;  // Brightness of the exit's light relative to its peak, see exit_pulse(): 0.6 to 1.

out vec4 frag_color;

const float kPixels = 8.0;  // Big pixels across a face, for the retro look.

// Colors of the frame and of the swirl from its brightest band to its darkest, already sRGB.
const vec3 kFrame = vec3(0.55, 0.8, 1.0);
const vec3 kCore = vec3(0.85, 0.95, 1.0);
const vec3 kMiddle = vec3(0.2, 0.45, 1.0);
const vec3 kEdge = vec3(0.04, 0.1, 0.55);

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    // Texture coordinates of exactly 1 at the far edges of a face belong to the last cell.
    vec2 cell = min(floor(v_uv * kPixels), vec2(kPixels - 1.0));
    vec2 p = (cell + 0.5) / kPixels - 0.5;
    vec3 color;
    float alpha;
    if (cell.x == 0.0 || cell.y == 0.0 || cell.x == kPixels - 1.0 || cell.y == kPixels - 1.0) {
        color = kFrame;
        alpha = 0.9;
    } else {
        // Two spiral arms that turn and move inwards over time.
        float swirl = 0.5 + 0.5 * sin(18.0 * length(p) + 2.0 * atan(p.y, p.x) + 3.0 * u_time);
        // The noise changes 8 times a second. Wrapping the step keeps the argument of sin() in hash() small.
        float sparkle = hash(cell + vec2(mod(floor(u_time * 8.0), 256.0), 0.0));
        if (sparkle > 0.95 || swirl > 0.75) {
            color = kCore;
            alpha = 0.85;
        } else if (swirl > 0.4) {
            color = kMiddle;
            alpha = 0.6;
        } else {
            color = kEdge;
            alpha = 0.4;
        }
    }
    float facing = abs(dot(normalize(v_normal), normalize(u_camera - v_position)));
    alpha *= mix(0.35, 1.0, facing);
    frag_color = vec4(color * (0.55 + 0.45 * u_pulse), alpha);
}
)glsl";

/// Draws the marker of DrawOptions in one color.
constexpr const char* kMarkerVertexShader = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_position;

uniform mat4 u_view_projection;

void main() {
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)glsl";

/// Paints the marker bright magenta, which stands out against the level.
constexpr const char* kMarkerFragmentShader = R"glsl(
#version 410 core
out vec4 frag_color;

void main() {
    frag_color = vec4(1.0, 0.2, 0.9, 1.0);
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
    glm::vec3 bottom;    ///< Center of the flame's bottom edge, see Torch::flame_bottom().
    float phase;         ///< See torch_phase().
    std::int32_t flame;  ///< The TorchFlame, as an index into the color arrays of kFlameFragmentShader.
};

// kFlameFragmentShader has the colors of each TorchFlame at the index of its value.
static_assert(static_cast<int>(TorchFlame::kOrange) == 0 && static_cast<int>(TorchFlame::kBlue) == 1 &&
                  static_cast<int>(TorchFlame::kGreen) == 2,
              "update the flame colors in kFlameFragmentShader");

/// The flames of the torches, in the same order.
std::vector<FlameInstance> make_flames(std::span<const Torch> torches) {
    std::vector<FlameInstance> flames;
    flames.reserve(torches.size());
    for (std::size_t i = 0; i < torches.size(); ++i) {
        flames.push_back({.bottom = torches[i].flame_bottom(),
                          .phase = torch_phase(i),
                          .flame = static_cast<std::int32_t>(torches[i].flame)});
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
      shadow_cache_(kShadowSlots),
      shadow_ranges_(kShadowSlots, 0.0f),
      exit_program_(link_program(kExitVertexShader, kExitFragmentShader)),
      marker_program_(link_program(kMarkerVertexShader, kMarkerFragmentShader)) {
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
    // An integer attribute needs glVertexAttribIPointer, which keeps it from being converted to float.
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_INT, sizeof(FlameInstance), buffer_offset(offsetof(FlameInstance, flame)));
    glVertexAttribDivisor(0, 1);
    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);
    glBindVertexArray(0);

    if (const std::optional<LevelExit>& exit = world.exit()) {
        exit_center_ = exit->center();
        const std::vector<Vertex> exit_mesh = build_exit_mesh();
        exit_vertex_count_ = static_cast<GLsizei>(exit_mesh.size());
        glGenVertexArrays(1, &exit_vertex_array_);
        glBindVertexArray(exit_vertex_array_);
        glGenBuffers(1, &exit_buffer_);
        glBindBuffer(GL_ARRAY_BUFFER, exit_buffer_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(std::span(exit_mesh).size_bytes()), exit_mesh.data(),
                     GL_STATIC_DRAW);
        set_vertex_attribute(0, 3, sizeof(Vertex), offsetof(Vertex, position));
        set_vertex_attribute(1, 3, sizeof(Vertex), offsetof(Vertex, normal));
        set_vertex_attribute(2, 2, sizeof(Vertex), offsetof(Vertex, uv));
        glBindVertexArray(0);
    }

    glGenVertexArrays(1, &marker_vertex_array_);
    glBindVertexArray(marker_vertex_array_);
    glGenBuffers(1, &marker_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, marker_buffer_);
    set_vertex_attribute(0, 3, sizeof(glm::vec3), 0);
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
    glDeleteBuffers(1, &marker_buffer_);
    glDeleteVertexArrays(1, &marker_vertex_array_);
    glDeleteProgram(marker_program_);
    // Deleting the names 0 does nothing, so this is safe also when the level has no exit.
    glDeleteBuffers(1, &exit_buffer_);
    glDeleteVertexArrays(1, &exit_vertex_array_);
    glDeleteProgram(exit_program_);
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
    // The lights whose maps are drawn: those with a new slot, and those whose range changed, e.g. in the
    // debug overlay, because a map stores distances divided by the range.
    std::vector<bool> to_draw(ids.size(), false);
    std::size_t draw_count = 0;
    for (std::size_t k = 0; k < ids.size(); ++k) {
        slots[owners[k]] = assignment.slots[k];
        if (const std::optional<std::size_t> slot = assignment.slots[k]) {
            to_draw[k] = std::ranges::find(assignment.to_draw, *slot) != assignment.to_draw.end() ||
                         shadow_ranges_[*slot] != lights[owners[k]].range;
            draw_count += to_draw[k] ? 1 : 0;
        }
    }
    stats_.shadow_maps_drawn = draw_count;
    if (draw_count == 0) {
        return slots;
    }

    GLint previous_framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffer_);
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glUseProgram(shadow_program_);
    glBindVertexArray(vertex_array_);
    // Torch handles cast no shadows: a real flame is much larger than our point light, so the handle under
    // it would cast a long, hard shadow down the wall that a real torch doesn't. Only the world and the faces
    // from build_shadow_faces() are drawn.
    // Only back faces, as seen from the light, go into the shadow map. A lit surface is then never compared
    // with itself, which would shadow parts of it at random (shadow acne). Surfaces that face away from the
    // light are compared with themselves, but they get no light anyway.
    glCullFace(GL_FRONT);
    for (std::size_t k = 0; k < ids.size(); ++k) {
        if (!to_draw[k]) {
            continue;
        }
        const std::size_t slot = *assignment.slots[k];
        const PointLight& light = lights[owners[k]];
        shadow_ranges_[slot] = light.range;
        glUniform3fv(glGetUniformLocation(shadow_program_, "u_light_position"), 1, glm::value_ptr(light.position));
        glUniform1f(glGetUniformLocation(shadow_program_, "u_range"), light.range);
        for (int face = 0; face < kCubeFaces; ++face) {
            const auto layer = (static_cast<GLint>(slot) * kCubeFaces) + face;
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
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    return slots;
}

void Renderer::draw_exit(const Camera& camera, const glm::mat4& view_projection, float time,
                         const DrawOptions& options) {
    if (!exit_center_) {
        return;
    }
    glUseProgram(exit_program_);
    glUniformMatrix4fv(glGetUniformLocation(exit_program_, "u_view_projection"), 1, GL_FALSE,
                       glm::value_ptr(view_projection));
    glUniform3fv(glGetUniformLocation(exit_program_, "u_center"), 1, glm::value_ptr(*exit_center_));
    glUniform1f(glGetUniformLocation(exit_program_, "u_angle"), exit_spin_angle(time));
    glUniform1f(glGetUniformLocation(exit_program_, "u_cut_height"), options.cut_height.value_or(0.0f));
    glUniform3fv(glGetUniformLocation(exit_program_, "u_camera"), 1, glm::value_ptr(camera.position));
    glUniform1f(glGetUniformLocation(exit_program_, "u_time"), time);
    glUniform1f(glGetUniformLocation(exit_program_, "u_pulse"), exit_pulse(time));
    glBindVertexArray(exit_vertex_array_);
    // The cube is see-through, so it is drawn after everything opaque, without writing depth: the far faces
    // first, then the near faces over them.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glCullFace(GL_FRONT);
    glDrawArrays(GL_TRIANGLES, 0, exit_vertex_count_);
    glCullFace(GL_BACK);
    glDrawArrays(GL_TRIANGLES, 0, exit_vertex_count_);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::draw(const Camera& camera, const Lighting& lighting, const Viewport& viewport, float time,
                    const DrawOptions& options) {
    // Without lighting, no lights are needed, nor their shadow maps.
    const std::span<const PointLight> used_lights =
        options.lighting ? std::span<const PointLight>(lighting.lights) : std::span<const PointLight>();
    const std::vector<std::optional<std::size_t>> shadow_slots = update_shadow_maps(used_lights);
    stats_.lights = std::min(used_lights.size(), kMaxLights);
    stats_.shadowed_lights = static_cast<std::size_t>(
        std::ranges::count_if(shadow_slots, [](const std::optional<std::size_t>& slot) { return slot.has_value(); }));

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    const glm::mat4 view_projection = camera.projection() * camera.view();

    const GpuLights lights = to_gpu_lights(used_lights, shadow_slots);
    glBindBuffer(GL_UNIFORM_BUFFER, light_buffer_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GpuLights), &lights);
    glBindBufferBase(GL_UNIFORM_BUFFER, kLightsBinding, light_buffer_);

    glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_, "u_view_projection"), 1, GL_FALSE,
                       glm::value_ptr(view_projection));
    glUniform3fv(glGetUniformLocation(program_, "u_ambient"), 1, glm::value_ptr(lighting.ambient));
    glUniform1i(glGetUniformLocation(program_, "u_lighting"), options.lighting ? 1 : 0);
    glUniform1i(glGetUniformLocation(program_, "u_cutaway"), options.cut_height ? 1 : 0);
    glUniform1f(glGetUniformLocation(program_, "u_shadow_bias"), options.shadow_bias);
    glUniform1f(glGetUniformLocation(program_, "u_cut_height"), options.cut_height.value_or(0.0f));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadow_maps_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBindVertexArray(vertex_array_);
    if (options.cut_height) {
        glEnable(GL_CLIP_DISTANCE0);
        // The insides of cut solids are seen through the cut: their back faces are drawn first, in the cut
        // color. Neighboring blocks share the planes of their sides, so a block's inside lies in the same
        // plane as the outside of its neighbor. Pulling the insides a little towards the camera keeps them
        // in front there, so the cut looks solid.
        glCullFace(GL_FRONT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.0f, -4.0f);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glCullFace(GL_BACK);
    }
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);

    if (flame_count_ > 0) {
        // Flames turn to the camera only around the vertical, so they stay upright when looking up or down.
        const glm::vec3 right{std::sin(camera.yaw), -std::cos(camera.yaw), 0.0f};
        glUseProgram(flame_program_);
        glUniformMatrix4fv(glGetUniformLocation(flame_program_, "u_view_projection"), 1, GL_FALSE,
                           glm::value_ptr(view_projection));
        glUniform3fv(glGetUniformLocation(flame_program_, "u_right"), 1, glm::value_ptr(right));
        glUniform1f(glGetUniformLocation(flame_program_, "u_time"), time);
        glUniform1f(glGetUniformLocation(flame_program_, "u_cut_height"), options.cut_height.value_or(0.0f));
        glBindVertexArray(flame_vertex_array_);
        glDisable(GL_CULL_FACE);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, flame_count_);
        glEnable(GL_CULL_FACE);
    }
    draw_exit(camera, view_projection, time, options);
    glDisable(GL_CLIP_DISTANCE0);

    if (!options.marker.empty()) {
        // On top of everything, and from both sides, so it can be seen through floors and walls.
        glUseProgram(marker_program_);
        glUniformMatrix4fv(glGetUniformLocation(marker_program_, "u_view_projection"), 1, GL_FALSE,
                           glm::value_ptr(view_projection));
        glBindVertexArray(marker_vertex_array_);
        glBindBuffer(GL_ARRAY_BUFFER, marker_buffer_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(options.marker.size_bytes()), options.marker.data(),
                     GL_STREAM_DRAW);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(options.marker.size()));
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }
    glBindVertexArray(0);
}

}  // namespace manifold
