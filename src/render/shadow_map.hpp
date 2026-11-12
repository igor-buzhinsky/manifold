#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace manifold {

/// Number of faces of a cube map.
inline constexpr int kCubeFaces = 6;

/// Closest distance from a light that its shadow map records, in metres.
inline constexpr float kShadowNearPlane = 0.05f;

/// The matrix that draws one face of a point light's cube shadow map.
///
/// Faces are in OpenGL order: +x, -x, +y, -y, +z, -z. The picture of each face is laid out as OpenGL
/// samples cube maps, so a direction from the light finds what was drawn in it. The projection has a
/// 90-degree field of view and an aspect ratio of 1.
///
/// \param light Position of the light, in world coordinates.
/// \param range Farthest distance that the shadow map records, in metres.
/// \param face  Index of the face, in [0, kCubeFaces).
glm::mat4 cube_face_view_projection(const glm::vec3& light, float range, int face);

/// Remembers which lights have a shadow map in which slot of a fixed set of slots.
///
/// Lights are static, so a shadow map stays valid as long as its light keeps its slot. When more lights
/// need a slot than are free, the slots of the lights that were used longest ago are reused.
class ShadowCache {
public:
    /// What a call of assign() decided.
    struct Assignment {
        /// The slot of each requested light, in the order of the request, or nothing if no slot was left.
        std::vector<std::optional<std::size_t>> slots;
        /// The slots whose shadow maps must be drawn (again), because they got a new light.
        std::vector<std::size_t> to_draw;
    };

    /// Creates a cache with all slots free.
    explicit ShadowCache(std::size_t slot_count);

    /// Gives a slot to each light that a frame needs.
    ///
    /// Lights that already have a slot keep it. The others get a free slot, or the slot of the light that
    /// was used longest ago, but never a slot that another light of the same request uses.
    ///
    /// \param ids Distinct ids of the lights. If there are more than slots, some get no slot: lights that
    ///            had no slot before, the last ones in `ids` first.
    Assignment assign(std::span<const std::uint32_t> ids);

    std::size_t slot_count() const { return slots_.size(); }

private:
    /// What a slot holds.
    struct Slot {
        std::optional<std::uint32_t> id;  ///< The light whose shadow map is in the slot, if any.
        std::uint64_t last_used = 0;      ///< The request that last used the slot, counted from 1.
    };

    std::vector<Slot> slots_;
    std::uint64_t request_ = 0;  ///< How many times assign() was called.
};

}  // namespace manifold
