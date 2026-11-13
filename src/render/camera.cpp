#include "render/camera.hpp"

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "world/player.hpp"

namespace manifold {

glm::vec3 Camera::forward() const {
    return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
}

glm::mat4 Camera::view() const {
    // The camera's own up direction, perpendicular to forward(). Unlike +z, it also works when looking
    // straight up or down.
    const glm::vec3 up{-std::sin(pitch) * std::cos(yaw), -std::sin(pitch) * std::sin(yaw), std::cos(pitch)};
    return glm::lookAt(position, position + forward(), up);
}

glm::mat4 Camera::projection() const { return glm::perspective(vertical_fov, kAspectRatio, near_plane, far_plane); }

Camera make_player_camera(const Player& player) {
    return {.position = player.eye_position(), .yaw = player.yaw(), .pitch = player.pitch()};
}

}  // namespace manifold
