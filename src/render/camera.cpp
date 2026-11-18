#include "render/camera.hpp"

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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

glm::vec3 Camera::ray_direction(const glm::vec2& point) const {
    // The point in camera coordinates on the plane at distance 1 in front of the camera, which looks along -z with
    // y up.
    const float half_height = std::tan(vertical_fov / 2.0f);
    const glm::vec3 local{((2.0f * point.x) - 1.0f) * half_height * kAspectRatio,
                          (1.0f - (2.0f * point.y)) * half_height, -1.0f};
    // The view matrix only rotates and moves, so its inverse turns the direction back into world coordinates.
    return glm::normalize(glm::vec3(glm::inverse(view()) * glm::vec4(local, 0.0f)));
}

Camera make_player_camera(const Player& player) {
    return {.position = player.eye_position(), .yaw = player.yaw(), .pitch = player.pitch()};
}

}  // namespace manifold
