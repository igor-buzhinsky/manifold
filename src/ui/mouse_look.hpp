#pragma once

#include <cstdint>
#include <optional>

#include <glm/vec2.hpp>

namespace manifold {

/// How far mouse look turns the view for a motion of the mouse.
///
/// \param motion           The motion in units of the mouse, x right and y down, as SDL gives it in relative mode.
/// \param degrees_per_unit The mouse sensitivity, see Settings::mouse_sensitivity.
/// \return The change of yaw and of pitch in radians, as Player::rotate() takes them: a motion to the right turns
///         right, and one up looks up.
glm::vec2 mouse_look_rotation(const glm::vec2& motion, float degrees_per_unit);

/// How close in time a press of the mouse button and the window getting the focus are taken as one click, which gave
/// the window the focus, in nanoseconds: 0.2 s.
inline constexpr std::uint64_t kFocusClickTime = 200'000'000;

/// Whether a press of the mouse button is the click that gave the window the focus, and not one meant for the game:
/// whether it came within kFocusClickTime of the focus, before or after it, since the two can arrive in either order.
///
/// \param press_time The time of the press, in nanoseconds, as SDL gives the times of events.
/// \param focus_time When the window last got the focus, in the same nanoseconds, if it did.
bool is_focus_click(std::uint64_t press_time, std::optional<std::uint64_t> focus_time);

}  // namespace manifold
