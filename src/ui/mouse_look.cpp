#include "ui/mouse_look.hpp"

#include <cstdint>
#include <optional>

#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>

namespace manifold {

glm::vec2 mouse_look_rotation(const glm::vec2& motion, float degrees_per_unit) {
    // Positive yaw turns left and positive pitch looks up, while the mouse's x is right and its y down.
    return -motion * glm::radians(degrees_per_unit);
}

bool is_focus_click(std::uint64_t press_time, std::optional<std::uint64_t> focus_time) {
    if (!focus_time) {
        return false;
    }
    const std::uint64_t distance = press_time > *focus_time ? press_time - *focus_time : *focus_time - press_time;
    return distance <= kFocusClickTime;
}

}  // namespace manifold
