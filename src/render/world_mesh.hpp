#pragma once

#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/torch.hpp"
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

/// Color of the level exit, linear RGB: bright blue.
inline constexpr glm::vec3 kExitColor{0.05f, 0.2f, 1.0f};

/// Creates triangles for the floor, the ceiling, the boundary walls, the obstacles and the ramps of a world.
///
/// Every three vertices form a triangle, counter-clockwise when seen from the side the normal points to.
/// Normals of the boundary walls point into the level, normals of obstacles point out of them.
/// The top of an obstacle is only added if it doesn't reach the ceiling. Bottoms of obstacles and ramps
/// are only added if they are above the floor.
///
/// Texture coordinates come from the position on the surface's plane, so the texture is never stretched,
/// also on ramp slopes. On vertical surfaces the texture is upright: its top is up.
///
/// The torches and the exit are left out, see build_torch_mesh() and build_exit_mesh().
std::vector<Vertex> build_world_mesh(const World& world);

/// Creates the faces that only shadow maps need, which build_world_mesh() leaves out.
///
/// Shadow maps keep only the faces that point away from the light (back faces), so every surface that
/// blocks light needs one behind it. These faces close the gaps:
/// - the outer side of a shell around the level boundary: six quads `thickness` outside it, facing
///   outwards, behind the one-sided floor, ceiling and boundary walls;
/// - the tops of obstacles that reach the ceiling;
/// - the bottoms of obstacles and ramps that stand on the floor.
///
/// Triangles are counter-clockwise as in build_world_mesh().
///
/// \param world     The world.
/// \param thickness Distance of the shell from the boundary, in metres.
std::vector<Vertex> build_shadow_faces(const World& world, float thickness);

/// Creates triangles for the handles of torches.
///
/// A handle is an untextured six-sided prism in kTorchHandleColor, with a wider end in kTorchHeadColor.
/// Triangles are counter-clockwise as in build_world_mesh(). The flames are not part of the mesh: the
/// renderer draws them as sprites.
std::vector<Vertex> build_torch_mesh(std::span<const Torch> torches);

/// Creates triangles for the cube of the level exit, untextured in kExitColor.
///
/// Triangles are counter-clockwise as in build_world_mesh(), with normals pointing out of the cube.
std::vector<Vertex> build_exit_mesh(const LevelExit& exit);

}  // namespace manifold
