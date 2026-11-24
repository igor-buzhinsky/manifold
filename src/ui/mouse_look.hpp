#pragma once

#include <glm/vec2.hpp>

namespace manifold {

/// How far mouse look turns the view for a motion of the mouse.
///
/// \param motion           The motion in units of the mouse, x right and y down, as SDL gives it in relative mode.
/// \param degrees_per_unit The mouse sensitivity, see Settings::mouse_sensitivity.
/// \return The change of yaw and of pitch in radians, as Player::rotate() takes them: a motion to the right turns
///         right, and one up looks up.
glm::vec2 mouse_look_rotation(const glm::vec2& motion, float degrees_per_unit);

}  // namespace manifold
