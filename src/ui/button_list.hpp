#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_event.hpp"

namespace manifold {

/// A button of a ButtonList.
struct Button {
    std::string label;
    bool enabled = true;  ///< A disabled button is greyed out and can't get the focus or be activated.
};

/// A column of buttons, e.g. the items of a menu, used with the keys and the mouse.
///
/// One button at most has the focus: it is highlighted, and NavKey::kConfirm activates it. Up and Down move the
/// focus to the previous or next enabled button, wrapping around, and the mouse moves it to the enabled button it is
/// over. A click activates a button when the main button is pressed and released over it.
///
/// The list keeps no layout: it is placed anew for each event and each drawing, e.g. by the screen it is on, so
/// that it follows the size of the picture.
class ButtonList {
public:
    /// Sizes at scale 1, in interface pixels, see Placement::scale.
    static constexpr float kButtonHeight = 24.0f;
    static constexpr float kGap = 4.0f;  ///< Between two buttons.

    /// Makes the list. The first enabled button gets the focus; if none is enabled, none has it.
    explicit ButtonList(std::vector<Button> buttons);

    /// Height of a list of `count` buttons, in pixels of the picture.
    static float height(std::size_t count, float scale);

    /// The rectangles of the buttons, top to bottom, as wide as `placement.area` and stacked from its top-left
    /// corner, with kGap between them. The height of the area isn't used.
    std::vector<Rect> layout(const Placement& placement) const;

    /// Handles an event, with the list at `placement`.
    ///
    /// \return The index of the button that the event activated, if any. NavKey::kCancel is left to the caller.
    std::optional<std::size_t> handle(const UiEvent& event, const Placement& placement);

    /// Adds the buttons at `placement` to `mesh`: each a rectangle with its label in the middle. The button with the
    /// focus is lighter and has a border, and one held down with the mouse is darker.
    void add_to(OverlayMesh& mesh, const Placement& placement) const;

    const std::vector<Button>& buttons() const { return buttons_; }
    /// The index of the button with the focus, if any.
    std::optional<std::size_t> focus() const { return focus_; }

private:
    /// Moves the focus to the next enabled button in `step` direction (1 or -1), wrapping around.
    void move_focus(int step);

    /// The enabled button that `point` is over, if any.
    std::optional<std::size_t> button_at(const glm::vec2& point, const Placement& placement) const;

    std::vector<Button> buttons_;
    std::optional<std::size_t> focus_;
    std::optional<std::size_t> pressed_;  ///< The button that the main pointer button went down on, if it is held.
};

}  // namespace manifold
