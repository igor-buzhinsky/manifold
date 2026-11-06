#pragma once

#include <numbers>

#include <glm/vec3.hpp>

#include "world/geometry.hpp"

namespace manifold {

struct PlayerParams {
    glm::vec3 position{0.0f};
    float yaw = 0.0f;     // Facing +x.
    float radius = 0.3f;  // Metres.
    float height = 1.8f;  // Metres.
};

// The player character. The position is the point between the feet, in metres.
// It is 3D already, although for now the player always stays on the floor (z = 0).
class Player {
public:
    // How far the eyes are below the top of the head.
    static constexpr float kEyeDepth = 0.12f;
    // How far the player can look up or down: 30 degrees.
    static constexpr float kMaxPitch = std::numbers::pi_v<float> / 6.0f;

    explicit Player(const PlayerParams& params)
        : position_(params.position), radius_(params.radius), height_(params.height), yaw_(params.yaw) {}

    const glm::vec3& position() const { return position_; }
    void set_position(const glm::vec3& position) { position_ = position; }

    // Radius of the player's collision body.
    float radius() const { return radius_; }
    float height() const { return height_; }

    // Direction the player faces, in radians counter-clockwise from +x around +z.
    float yaw() const { return yaw_; }
    // Vertical view angle in radians, positive is up. Always within [-kMaxPitch, kMaxPitch].
    float pitch() const { return pitch_; }

    // Turns the view by the given angles in radians. Positive `delta_yaw` turns left, positive
    // `delta_pitch` looks up. Yaw is kept within [0, 2 pi), pitch is clamped.
    void rotate(float delta_yaw, float delta_pitch);

    // Where the camera is.
    glm::vec3 eye_position() const { return position_ + glm::vec3(0.0f, 0.0f, height_ - kEyeDepth); }

    // The player's collision body.
    VerticalCylinder body() const { return {.base = position_, .radius = radius_, .height = height_}; }

    // The area the player occupies on the floor.
    Circle footprint() const { return {.center = {position_.x, position_.y}, .radius = radius_}; }

private:
    glm::vec3 position_;
    float radius_;
    float height_;
    float yaw_;
    float pitch_ = 0.0f;
};

}  // namespace manifold
