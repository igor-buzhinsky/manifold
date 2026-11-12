#include "world/torch.hpp"

#include <cmath>

#include <glm/vec3.hpp>

namespace manifold {

namespace {

/// How far the flame reaches below the tip, so that it hides the end of the handle, in metres.
constexpr float kFlameSink = 0.04f;
/// Height of the light above the tip, in metres.
constexpr float kLightAboveTip = 0.1f;

}  // namespace

glm::vec3 Torch::outward() const { return {std::cos(yaw), std::sin(yaw), 0.0f}; }

glm::vec3 Torch::axis() const {
    return (std::sin(kTorchTilt) * outward()) + glm::vec3(0.0f, 0.0f, std::cos(kTorchTilt));
}

glm::vec3 Torch::tip() const { return mount + (kTorchLength * axis()); }

glm::vec3 Torch::flame_bottom() const { return tip() - glm::vec3(0.0f, 0.0f, kFlameSink); }

glm::vec3 Torch::light_position() const { return tip() + glm::vec3(0.0f, 0.0f, kLightAboveTip); }

}  // namespace manifold
