#pragma once

#include <cstdint>
#include <string_view>

#include <glm/vec3.hpp>

namespace manifold {

/// Length of a torch's handle, from the wall to the burning end, in metres.
inline constexpr float kTorchLength = 0.45f;
/// Radius of a torch's handle, in metres.
inline constexpr float kTorchRadius = 0.03f;
/// Angle between a torch's handle and the vertical, in radians (about 29 degrees).
inline constexpr float kTorchTilt = 0.5f;
/// Width of a torch's flame, in metres.
inline constexpr float kFlameWidth = 0.18f;
/// Height of a torch's flame, in metres.
inline constexpr float kFlameHeight = 0.32f;

/// What burns in a torch, which sets the color of its flame and its light.
enum class TorchFlame : std::uint8_t {
    kOrange,  ///< An ordinary fire.
    kBlue,    ///< A cold, unnatural blue with violet edges.
    kGreen,   ///< A sickly, poisonous green.
};

/// Short name of a flame, for logs: "orange", "blue" or "green".
std::string_view torch_flame_name(TorchFlame flame);

/// A torch fixed to a wall: a handle that leans away from the wall, with a flame at its upper end.
///
/// Torches are only decoration and light: they don't collide with anything.
struct Torch {
    glm::vec3 mount{0.0f};  ///< Where the handle touches the wall, in world coordinates.
    float yaw = 0.0f;       ///< Direction away from the wall: radians counter-clockwise from +x.
    TorchFlame flame = TorchFlame::kOrange;

    /// Horizontal unit vector away from the wall.
    glm::vec3 outward() const;
    /// Unit vector along the handle, from the mount to the burning end.
    glm::vec3 axis() const;
    /// Center of the burning end of the handle.
    glm::vec3 tip() const;
    /// Center of the bottom edge of the flame. It is a little below the tip, so the flame covers it.
    glm::vec3 flame_bottom() const;
    /// Where the torch's light is: in the lower, brightest part of the flame.
    glm::vec3 light_position() const;
};

}  // namespace manifold
