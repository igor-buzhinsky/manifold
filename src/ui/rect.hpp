#pragma once

#include <glm/vec2.hpp>

namespace manifold {

/// An axis-aligned rectangle of the interface, in pixels from the top-left corner of the picture, y down.
struct Rect {
    glm::vec2 top_left{0.0f};
    glm::vec2 size{0.0f};  ///< Width and height, not negative.

    bool operator==(const Rect&) const = default;

    /// Whether `point` is inside, counting the top and left edges but not the bottom and right ones, so that
    /// rectangles side by side never both contain a point.
    bool contains(const glm::vec2& point) const {
        return point.x >= top_left.x && point.y >= top_left.y && point.x < top_left.x + size.x &&
               point.y < top_left.y + size.y;
    }
};

/// Where a control of the interface is put: its rectangle and how large its pixels are drawn.
struct Placement {
    Rect area;
    /// Size of an interface pixel in pixels of the picture, see ui_scale(). All sizes of a control are whole
    /// interface pixels, so that the blocky font is drawn evenly, and stays sharp where the scale is a whole number.
    float scale = 1.0f;
};

}  // namespace manifold
