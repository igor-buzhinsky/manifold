#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_event.hpp"

namespace manifold {

/// What the player chose in an ItemSplitDialog.
enum class SplitAction : std::uint8_t {
    kNone,    ///< Nothing yet: the dialog stays open.
    kMove,    ///< Move ItemSplitDialog::amount() items.
    kCancel,  ///< Move nothing.
};

/// A small panel in the middle of the picture that asks how many items of a stack to move, from 1 to all of them,
/// which it starts with. Under the name of the item, a slider between a minus and a plus button chooses the number,
/// which is shown below it, and the buttons Move and Cancel close it.
///
/// - Left and Right, the minus and plus buttons and the mouse wheel over the panel change the number by one, Down and
///   Up by ten, within its range. A press on the slider, or dragging it, chooses the number at the pointer.
/// - Enter or the Move button moves the items, Escape or the Cancel button or a press outside the panel cancels.
///
/// Like a Screen, it keeps its state but no layout, and is laid out anew for each event and each drawing. It is part of
/// a screen rather than a screen of its own, see InventoryScreen.
class ItemSplitDialog {
public:
    /// Sizes at scale 1, in interface pixels, see Placement::scale.
    static constexpr float kPadding = 8.0f;  ///< Between the edge of the panel and what is in it.
    static constexpr float kGap = 4.0f;      ///< Between the rows of the panel, and between buttons.
    static constexpr float kMinContentWidth = 140.0f;
    static constexpr float kStepButtonSize = 12.0f;  ///< Width and height of the minus and plus buttons.
    static constexpr float kButtonHeight = 14.0f;    ///< Of Move and Cancel.
    static constexpr float kLabelPadding = 6.0f;     ///< Left and right of the label in a button.
    static constexpr float kKnobWidth = 3.0f;        ///< Of the part of the slider that shows the number.
    static constexpr int kBigStep = 10;              ///< How much Up and Down change the number.

    static constexpr const char* kMoveLabel = "Move";
    static constexpr const char* kCancelLabel = "Cancel";

    /// \param item_name The name of the item, which the panel shows.
    /// \param max_count How many items the stack has, at least 2.
    ItemSplitDialog(std::string item_name, int max_count);

    /// The number of items to move, from 1 to max_count().
    int amount() const { return amount_; }
    int max_count() const { return max_count_; }

    /// Handles an event, with the panel laid out over a picture of `picture` size. The I and T keys do nothing, so
    /// that the screen that opened it can handle them.
    SplitAction handle(const UiEvent& event, const PixelSize& picture);

    /// Adds the panel, laid out over a picture of `picture` size, to `mesh`.
    void add_to(OverlayMesh& mesh, const PixelSize& picture) const;

    /// The rectangles of the panel and its controls over a picture of `picture` size.
    Rect panel(const PixelSize& picture) const;
    Rect minus_button(const PixelSize& picture) const;
    Rect plus_button(const PixelSize& picture) const;
    /// The slider between the minus and plus buttons, as high as them.
    Rect slider(const PixelSize& picture) const;
    Rect move_button(const PixelSize& picture) const;
    Rect cancel_button(const PixelSize& picture) const;

private:
    /// The controls that the pointer can be over.
    enum class Control : std::uint8_t { kMinus, kPlus, kSlider, kMove, kCancel };

    /// Handles a key, see the class description.
    SplitAction handle_key(NavKey key);
    /// Handles the pointer, see the class description.
    SplitAction handle_pointer(const PointerEvent& pointer, const PixelSize& picture);
    /// The control under `point`, if any.
    std::optional<Control> control_at(const glm::vec2& point, const PixelSize& picture) const;
    /// Changes the number by `step`, within its range.
    void change_by(int step);
    /// Sets the number to the one of the slider at the x coordinate `x`.
    void set_from_slider(float x, const PixelSize& picture);
    /// The width of the content of the panel, without the padding, at scale 1.
    float content_width() const;
    /// Adds a button with `label` in `rect` to `mesh`, lighter and with a border if the pointer is over it.
    void add_button(OverlayMesh& mesh, const Rect& rect, const char* label, Control control, float scale) const;

    std::string item_name_;
    int max_count_;
    int amount_;
    std::optional<Control> hovered_;  ///< The control under the pointer.
    bool dragging_ = false;           ///< Whether the main button was pressed on the slider and is still down.
};

}  // namespace manifold
