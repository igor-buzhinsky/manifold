#include "world/player.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/vec3.hpp>

namespace manifold {

void Player::rotate(float delta_yaw, float delta_pitch) {
    constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
    yaw_ = std::fmod(yaw_ + delta_yaw, kFullTurn);
    if (yaw_ < 0.0f) {
        yaw_ += kFullTurn;
    }
    // A tiny negative yaw rounds up to a full turn when it is added.
    if (yaw_ >= kFullTurn) {
        yaw_ = 0.0f;
    }
    pitch_ = std::clamp(pitch_ + delta_pitch, -kMaxPitch, kMaxPitch);
}

glm::vec3 Player::view_direction() const {
    return {std::cos(pitch_) * std::cos(yaw_), std::cos(pitch_) * std::sin(yaw_), std::sin(pitch_)};
}

}  // namespace manifold
