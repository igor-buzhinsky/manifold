#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "world/player.hpp"

namespace manifold {

// The visible area always has this width-to-height ratio, regardless of the window shape.
inline constexpr float kAspectRatio = 16.0f / 9.0f;

// A perspective camera in world coordinates (metres, z up).
struct Camera {
    glm::vec3 position{0.0f};
    float yaw = 0.0f;           // Radians, counter-clockwise from +x around +z.
    float pitch = 0.0f;         // Radians, positive looks up.
    float vertical_fov = 1.2f;  // Radians, about 69 degrees.
    float near_plane = 0.05f;
    float far_plane = 200.0f;

    glm::vec3 forward() const;
    glm::mat4 view() const;
    glm::mat4 projection() const;
};

// A camera at the player's eyes, looking where the player looks.
Camera make_player_camera(const Player& player);

}  // namespace manifold
