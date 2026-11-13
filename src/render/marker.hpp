#pragma once

#include <vector>

#include <glm/vec3.hpp>

namespace manifold {

/// Length of the arrow of build_arrow(), from its tail to its tip, in metres.
inline constexpr float kArrowLength = 1.2f;

/// Builds a flat arrow that lies just above the point `feet` and points in the direction `yaw`.
///
/// The arrow is centered on `feet`, so it shows where a player stands and faces.
///
/// \param feet The point between the player's feet.
/// \param yaw  Direction of the tip: radians counter-clockwise from +x.
/// \return Triangles, three corners each, counter-clockwise when seen from above.
std::vector<glm::vec3> build_arrow(const glm::vec3& feet, float yaw);

}  // namespace manifold
