#include "ui/item_split_dialog.hpp"

#include <limits>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_event.hpp"

namespace manifold {
namespace {

constexpr PixelSize kPicture{.width = 800, .height = 450};

/// The middle of a rectangle.
glm::vec2 middle(const Rect& rect) { return rect.top_left + (rect.size / 2.0f); }

/// A press of the main button at `point`.
UiEvent press(const glm::vec2& point) { return PointerEvent{.action = PointerAction::kPress, .position = point}; }

TEST(ItemSplitDialogTest, StartsWithAllAndKeysChangeTheNumberWithinItsRange) {
    ItemSplitDialog dialog("Pebble", 25);
    EXPECT_EQ(dialog.amount(), 25);
    EXPECT_EQ(dialog.handle(NavKey::kRight, kPicture), SplitAction::kNone);
    EXPECT_EQ(dialog.amount(), 25);
    dialog.handle(NavKey::kLeft, kPicture);
    EXPECT_EQ(dialog.amount(), 24);
    dialog.handle(NavKey::kDown, kPicture);
    dialog.handle(NavKey::kDown, kPicture);
    EXPECT_EQ(dialog.amount(), 4);
    dialog.handle(NavKey::kDown, kPicture);
    EXPECT_EQ(dialog.amount(), 1);
    dialog.handle(NavKey::kUp, kPicture);
    EXPECT_EQ(dialog.amount(), 11);
    // I and T are left to the screen that opened it.
    EXPECT_EQ(dialog.handle(NavKey::kInventory, kPicture), SplitAction::kNone);
    EXPECT_EQ(dialog.handle(NavKey::kTakeAll, kPicture), SplitAction::kNone);
    EXPECT_EQ(dialog.handle(NavKey::kConfirm, kPicture), SplitAction::kMove);
    EXPECT_EQ(dialog.handle(NavKey::kCancel, kPicture), SplitAction::kCancel);
    EXPECT_EQ(dialog.amount(), 11);
}

TEST(ItemSplitDialogTest, ButtonsAndTheWheelChangeTheNumberAndCloseIt) {
    ItemSplitDialog dialog("Pebble", 5);
    dialog.handle(press(middle(dialog.minus_button(kPicture))), kPicture);
    dialog.handle(press(middle(dialog.minus_button(kPicture))), kPicture);
    EXPECT_EQ(dialog.amount(), 3);
    dialog.handle(press(middle(dialog.plus_button(kPicture))), kPicture);
    EXPECT_EQ(dialog.amount(), 4);
    const glm::vec2 inside = middle(dialog.panel(kPicture));
    dialog.handle(ScrollEvent{.steps = -2.0f, .position = inside}, kPicture);
    EXPECT_EQ(dialog.amount(), 2);
    dialog.handle(ScrollEvent{.steps = 0.25f, .position = inside}, kPicture);
    EXPECT_EQ(dialog.amount(), 3);
    dialog.handle(ScrollEvent{.steps = 1.0f, .position = {0.0f, 0.0f}}, kPicture);  // Not over the panel.
    EXPECT_EQ(dialog.amount(), 3);
    EXPECT_EQ(dialog.handle(press(middle(dialog.move_button(kPicture))), kPicture), SplitAction::kMove);
    EXPECT_EQ(dialog.handle(press(middle(dialog.cancel_button(kPicture))), kPicture), SplitAction::kCancel);
    // A press on the panel between its controls does nothing, and one outside cancels.
    const Rect panel = dialog.panel(kPicture);
    EXPECT_EQ(dialog.handle(press(panel.top_left + glm::vec2(1.0f)), kPicture), SplitAction::kNone);
    EXPECT_EQ(dialog.handle(press(panel.top_left - glm::vec2(1.0f)), kPicture), SplitAction::kCancel);
    EXPECT_EQ(dialog.amount(), 3);
}

TEST(ItemSplitDialogTest, TheSliderChoosesTheNumberAtThePointerWhileItIsDragged) {
    ItemSplitDialog dialog("Pebble", 11);
    const Rect slider = dialog.slider(kPicture);
    const float y = middle(slider).y;
    dialog.handle(press({slider.top_left.x, y}), kPicture);
    EXPECT_EQ(dialog.amount(), 1);
    // Dragging follows the pointer, also beyond the ends of the slider.
    dialog.handle(PointerEvent{.action = PointerAction::kMove, .position = middle(slider)}, kPicture);
    EXPECT_EQ(dialog.amount(), 6);
    dialog.handle(PointerEvent{.action = PointerAction::kMove, .position = {slider.top_left.x + 1000.0f, y}}, kPicture);
    EXPECT_EQ(dialog.amount(), 11);
    dialog.handle(PointerEvent{.action = PointerAction::kRelease, .position = {0.0f, y}}, kPicture);
    // A press on the middle of the knob keeps the number.
    const float knob_middle = slider.top_left.x + slider.size.x - (ItemSplitDialog::kKnobWidth * 1.5f / 2.0f);
    ItemSplitDialog large("Pebble", 1000);
    large.handle(press({knob_middle, y}), kPicture);
    EXPECT_EQ(large.amount(), 1000);
    dialog.handle(PointerEvent{.action = PointerAction::kMove, .position = {slider.top_left.x, y}}, kPicture);
    EXPECT_EQ(dialog.amount(), 11);
}

TEST(ItemSplitDialogTest, TheNumberStaysInItsRangeForTheLargestStacks) {
    for (const int max : {16777221, std::numeric_limits<int>::max()}) {
        ItemSplitDialog dialog("Pebble", max);
        dialog.handle(NavKey::kRight, kPicture);
        dialog.handle(NavKey::kUp, kPicture);
        EXPECT_EQ(dialog.amount(), max);
        const Rect slider = dialog.slider(kPicture);
        dialog.handle(press({slider.top_left.x + slider.size.x - 0.01f, middle(slider).y}), kPicture);
        dialog.handle(PointerEvent{.action = PointerAction::kMove, .position = {slider.top_left.x + 1000.0f, 0.0f}},
                      kPicture);
        EXPECT_EQ(dialog.amount(), max);
        dialog.handle(PointerEvent{.action = PointerAction::kMove, .position = {slider.top_left.x - 1000.0f, 0.0f}},
                      kPicture);
        EXPECT_EQ(dialog.amount(), 1);
    }
}

TEST(ItemSplitDialogTest, PanelIsCenteredAndHoldsItsControls) {
    const ItemSplitDialog dialog("A long name of an item that is wider than the panel would be", 3);
    const Rect panel = dialog.panel(kPicture);
    EXPECT_NEAR(panel.top_left.x + (panel.size.x / 2.0f), 400.0f, 2.0f);
    EXPECT_NEAR(panel.top_left.y + (panel.size.y / 2.0f), 225.0f, 2.0f);
    const auto inside = [&panel](const Rect& rect) {
        return rect.top_left.x >= panel.top_left.x && rect.top_left.y >= panel.top_left.y &&
               rect.top_left.x + rect.size.x <= panel.top_left.x + panel.size.x &&
               rect.top_left.y + rect.size.y <= panel.top_left.y + panel.size.y;
    };
    for (const Rect& control : {dialog.minus_button(kPicture), dialog.plus_button(kPicture), dialog.slider(kPicture),
                                dialog.move_button(kPicture), dialog.cancel_button(kPicture)}) {
        EXPECT_TRUE(inside(control));
    }
    EXPECT_GT(panel.size.x, text_width("A long name of an item that is wider than the panel would be", 1.5f));
    EXPECT_LT(dialog.move_button(kPicture).top_left.x + dialog.move_button(kPicture).size.x,
              dialog.cancel_button(kPicture).top_left.x);
    OverlayMesh mesh;
    dialog.add_to(mesh, kPicture);
    EXPECT_FALSE(mesh.vertices().empty());
}

}  // namespace
}  // namespace manifold
