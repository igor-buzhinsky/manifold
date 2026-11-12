#pragma once

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/world.hpp"

namespace manifold {

/// A vertex of the world mesh, as uploaded to the GPU.
struct Vertex {
    glm::vec3 position;  ///< In world coordinates.
    glm::vec3 normal;    ///< Unit vector pointing out of the visible side of the surface.
    /// Texture coordinates. One unit is one copy of the texture, which repeats every kTextureTileSize.
    glm::vec2 uv;
    glm::vec3 color{1.0f};  ///< Linear RGB that multiplies the texture, or replaces it, see `texture_weight`.
    /// How much of the texture shows: 1 for the texture times `color`, 0 for `color` alone.
    float texture_weight = 1.0f;
};

/// Width and height of one copy of the level texture on a surface, in metres.
constexpr float kTextureTileSize = 2.0f;

/// Color of a torch's wooden handle, linear RGB.
inline constexpr glm::vec3 kTorchHandleColor{0.2f, 0.1f, 0.04f};
/// Color of the charred, burning end of a torch, linear RGB.
inline constexpr glm::vec3 kTorchHeadColor{0.05f, 0.035f, 0.025f};

/// Creates triangles for the floor, the ceiling, the boundary walls, the obstacles, the ramps and the torch
/// handles of a world.
///
/// Every three vertices form a triangle, counter-clockwise when seen from the side the normal points to.
/// Normals of the boundary walls point into the level, normals of obstacles point out of them.
/// The top of an obstacle is only added if it doesn't reach the ceiling. Bottoms of obstacles and ramps
/// are only added if they are above the floor.
///
/// Texture coordinates come from the position on the surface's plane, so the texture is never stretched,
/// also on ramp slopes. On vertical surfaces the texture is upright: its top is up.
///
/// Torch handles are untextured six-sided prisms in kTorchHandleColor, with a wider end in kTorchHeadColor.
/// Their flames are not part of the mesh: the renderer draws them as sprites.
std::vector<Vertex> build_world_mesh(const World& world);

}  // namespace manifold
