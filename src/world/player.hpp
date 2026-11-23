#pragma once

#include <numbers>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/geometry.hpp"

namespace manifold {

/// Initial state and build of the player.
struct PlayerParams {
    glm::vec3 position{0.0f};  ///< The point between the feet.
    float yaw = 0.0f;          ///< Radians, counter-clockwise from +x. Zero faces +x.
    float radius = 0.6f;       ///< Radius of the collision body, in metres.
    float height = 1.8f;       ///< Metres.
};

/// The player character.
///
/// The position is the point between the feet, in metres.
class Player {
public:
    /// How far the eyes are below the top of the head, in metres.
    static constexpr float kEyeDepth = 0.12f;
    /// How far the player can look up or down: 40 degrees.
    static constexpr float kMaxPitch = 40.0f * std::numbers::pi_v<float> / 180.0f;

    /// Creates the player looking straight ahead.
    explicit Player(const PlayerParams& params)
        : position_(params.position), radius_(params.radius), height_(params.height), yaw_(params.yaw) {}

    const glm::vec3& position() const { return position_; }
    void set_position(const glm::vec3& position) { position_ = position; }

    /// Vertical speed in metres per second, positive is up. Negative while falling.
    float vertical_speed() const { return vertical_speed_; }
    void set_vertical_speed(float vertical_speed) { vertical_speed_ = vertical_speed; }

    /// Horizontal velocity in metres per second, from the movement of the last update. In the air, the player keeps it,
    /// see Game::update().
    const glm::vec2& horizontal_velocity() const { return horizontal_velocity_; }
    void set_horizontal_velocity(const glm::vec2& velocity) { horizontal_velocity_ = velocity; }

    /// Whether the player stood on something at the end of the last update, or stands on the floor at the start of a
    /// level.
    bool on_ground() const { return on_ground_; }
    void set_on_ground(bool on_ground) { on_ground_ = on_ground; }

    /// Radius of the player's collision body.
    float radius() const { return radius_; }
    float height() const { return height_; }

    /// Direction the player faces, in radians counter-clockwise from +x around +z. Within [0, 2 pi).
    float yaw() const { return yaw_; }
    /// Vertical view angle in radians, positive is up. Within [-kMaxPitch, kMaxPitch].
    float pitch() const { return pitch_; }

    /// Turns the player's view.
    ///
    /// Yaw is kept within [0, 2 pi), pitch is clamped to [-kMaxPitch, kMaxPitch].
    ///
    /// \param delta_yaw   Change of yaw in radians. Positive turns left.
    /// \param delta_pitch Change of pitch in radians. Positive looks up.
    void rotate(float delta_yaw, float delta_pitch);

    /// Where the camera is: kEyeDepth below the top of the head.
    glm::vec3 eye_position() const { return position_ + glm::vec3(0.0f, 0.0f, height_ - kEyeDepth); }

    /// Unit vector in the direction the player looks, from yaw() and pitch().
    glm::vec3 view_direction() const;

    /// The player's collision body.
    VerticalCylinder body() const { return {.base = position_, .radius = radius_, .height = height_}; }

    /// The area the player occupies on the floor.
    Circle footprint() const { return {.center = {position_.x, position_.y}, .radius = radius_}; }

private:
    glm::vec3 position_;
    float radius_;
    float height_;
    float yaw_;
    float pitch_ = 0.0f;
    float vertical_speed_ = 0.0f;
    glm::vec2 horizontal_velocity_{0.0f};
    bool on_ground_ = false;
};

}  // namespace manifold
