#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/chest.hpp"
#include "world/door.hpp"
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

/// Color of obstacles with a Surface::kDark surface, linear RGB.
inline constexpr glm::vec3 kDarkSurfaceColor{0.015f, 0.015f, 0.015f};
/// Color of a torch's wooden handle, linear RGB.
inline constexpr glm::vec3 kTorchHandleColor{0.2f, 0.1f, 0.04f};
/// Color of the charred, burning end of a torch, linear RGB.
inline constexpr glm::vec3 kTorchHeadColor{0.05f, 0.035f, 0.025f};

/// Color of the body of a wooden chest, linear RGB.
inline constexpr glm::vec3 kWoodenChestBodyColor{0.16f, 0.075f, 0.03f};
/// Color of the lid of a wooden chest, linear RGB: darker than the body.
inline constexpr glm::vec3 kWoodenChestLidColor{0.07f, 0.035f, 0.015f};
/// Color of the body of a golden chest, linear RGB.
inline constexpr glm::vec3 kGoldenChestBodyColor{0.55f, 0.3f, 0.01f};
/// Color of the lid of a golden chest, linear RGB: brighter than the body.
inline constexpr glm::vec3 kGoldenChestLidColor{0.9f, 0.6f, 0.03f};

/// Creates triangles for the floor, the ceiling, the boundary walls, the obstacles and the ramps of a world.
///
/// Obstacles with a Surface::kDark surface are untextured, in kDarkSurfaceColor. Everything else has the level
/// texture.
///
/// Every three vertices form a triangle, counter-clockwise when seen from the side the normal points to.
/// Normals of the boundary walls point into the level, normals of obstacles point out of them.
/// The top of an obstacle is only added if it doesn't reach the ceiling. Bottoms of obstacles and ramps
/// are only added if they are above the floor.
///
/// Texture coordinates come from the position on the surface's plane, so the texture is never stretched,
/// also on ramp slopes. On vertical surfaces the texture is upright: its top is up.
///
/// The torches, the chests and the exit are left out, see build_torch_mesh(), build_chest_mesh() and
/// build_exit_mesh().
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

/// Creates triangles for chests: two untextured boxes each, the body and the lid on it, see Chest::body() and
/// Chest::lid(), with all six faces. A wooden chest is in kWoodenChestBodyColor and kWoodenChestLidColor, a golden
/// one in kGoldenChestBodyColor and kGoldenChestLidColor. Triangles are counter-clockwise as in build_world_mesh(),
/// with normals pointing out of the boxes.
std::vector<Vertex> build_chest_mesh(std::span<const Chest> chests);

/// Creates triangles for the cube of the level exit, centered on the origin. The renderer spins it and moves it
/// into place, see exit_spin_angle().
///
/// The cube is LevelExit::kSize wide. Triangles are counter-clockwise as in build_world_mesh(), with normals
/// pointing out of the cube. Each face has its own texture coordinates from (0, 0) to (1, 1), for the portal
/// pattern that the renderer paints on it. Colors and texture weights are not used.
std::vector<Vertex> build_exit_mesh();

/// Number of vertices that build_door_mesh() creates for each door.
inline constexpr std::size_t kDoorVertexCount = 72;

/// Creates triangles for the halves of doors, see Door::halves().
///
/// Each half is a box with all six faces, kDoorVertexCount / 2 vertices. Triangles are counter-clockwise as in
/// build_world_mesh(), with normals pointing out of the box. The texture is not tiled in world coordinates but moves
/// with the half: one copy of it is as wide as the half, with its top at the top of the half, and a whole number of
/// copies is as high as the half, each about as high as it is wide. See make_door_texture(). Colors are white and the
/// texture weight is 1.
///
/// \param doors  The doors.
/// \param states How far each door is open, in the order of `doors`. Doors without a state are drawn closed.
/// \return kDoorVertexCount vertices for each door, in the order of `doors`.
std::vector<Vertex> build_door_mesh(std::span<const Door> doors, std::span<const DoorState> states);

/// How long the exit cube takes to turn once around the vertical, in seconds.
inline constexpr float kExitSpinPeriod = 8.0f;

/// How far the exit cube has turned around the vertical through its center at a moment.
///
/// \param time Seconds since the start of the game.
/// \return Radians counter-clockwise from +x, in [0, 2 pi).
float exit_spin_angle(float time);

}  // namespace manifold
