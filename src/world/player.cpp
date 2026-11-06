#include "world/player.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace manifold {

void Player::rotate(float delta_yaw, float delta_pitch) {
    constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
    yaw_ = std::fmod(yaw_ + delta_yaw, kFullTurn);
    if (yaw_ < 0.0f) {
        yaw_ += kFullTurn;
    }
    pitch_ = std::clamp(pitch_ + delta_pitch, -kMaxPitch, kMaxPitch);
}

}  // namespace manifold
