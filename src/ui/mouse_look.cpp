#include "ui/mouse_look.hpp"

#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>

namespace manifold {

glm::vec2 mouse_look_rotation(const glm::vec2& motion, float degrees_per_unit) {
    // Positive yaw turns left and positive pitch looks up, while the mouse's x is right and its y down.
    return -motion * glm::radians(degrees_per_unit);
}

}  // namespace manifold
