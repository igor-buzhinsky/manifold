#include "ui/button_list.hpp"

#include <cstddef>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_event.hpp"

namespace manifold {
namespace {

/// Where the lists of these tests are: 100 pixels wide at (10, 20), with interface pixels of 2 pixels.
constexpr Placement kPlacement{.area = {.top_left = {10.0f, 20.0f}, .size = {100.0f, 0.0f}}, .scale = 2.0f};

/// Three buttons, the middle one disabled unless `middle_enabled`.
ButtonList make_list(bool middle_enabled = false) {
    return ButtonList({{.label = "One"}, {.label = "Two", .enabled = middle_enabled}, {.label = "Three"}});
}

/// The center of button `index` of a list at kPlacement.
glm::vec2 center(const ButtonList& list, std::size_t index) {
    const Rect rect = list.layout(kPlacement)[index];
    return rect.top_left + (rect.size / 2.0f);
}

PointerEvent pointer(PointerAction action, const glm::vec2& position) {
    return {.action = action, .position = position};
}

TEST(ButtonListTest, StacksButtonsWithGapsAndKnowsItsHeight) {
    const ButtonList list = make_list();
    const std::vector<Rect> rects = list.layout(kPlacement);
    ASSERT_EQ(rects.size(), 3U);
    EXPECT_EQ(rects[0].top_left, glm::vec2(10.0f, 20.0f));
    EXPECT_EQ(rects[0].size, glm::vec2(100.0f, ButtonList::kButtonHeight * 2.0f));
    EXPECT_EQ(rects[1].top_left.y, 20.0f + ((ButtonList::kButtonHeight + ButtonList::kGap) * 2.0f));
    EXPECT_FLOAT_EQ(rects[2].top_left.y + rects[2].size.y - 20.0f, ButtonList::height(3, 2.0f));
    EXPECT_EQ(ButtonList::height(0, 2.0f), 0.0f);
}

TEST(ButtonListTest, FocusStartsAtTheFirstEnabledButton) {
    EXPECT_EQ(make_list().focus(), 0U);
    EXPECT_EQ(ButtonList({{.label = "Off", .enabled = false}, {.label = "On"}}).focus(), 1U);
    EXPECT_EQ(ButtonList({{.label = "Off", .enabled = false}}).focus(), std::nullopt);
}

TEST(ButtonListTest, KeysMoveTheFocusOverDisabledButtonsAndWrapAround) {
    ButtonList list = make_list();
    list.handle(NavKey::kDown, kPlacement);
    EXPECT_EQ(list.focus(), 2U);
    list.handle(NavKey::kDown, kPlacement);
    EXPECT_EQ(list.focus(), 0U);
    list.handle(NavKey::kUp, kPlacement);
    EXPECT_EQ(list.focus(), 2U);
    EXPECT_EQ(list.handle(NavKey::kConfirm, kPlacement), 2U);
    EXPECT_EQ(list.handle(NavKey::kCancel, kPlacement), std::nullopt);
}

TEST(ButtonListTest, KeysDoNothingWithoutEnabledButtons) {
    ButtonList list({{.label = "Off", .enabled = false}});
    list.handle(NavKey::kDown, kPlacement);
    EXPECT_EQ(list.focus(), std::nullopt);
    EXPECT_EQ(list.handle(NavKey::kConfirm, kPlacement), std::nullopt);
}

TEST(ButtonListTest, PointerMovesTheFocusToEnabledButtonsOnly) {
    ButtonList list = make_list();
    list.handle(pointer(PointerAction::kMove, center(list, 2)), kPlacement);
    EXPECT_EQ(list.focus(), 2U);
    list.handle(pointer(PointerAction::kMove, center(list, 1)), kPlacement);  // Disabled.
    EXPECT_EQ(list.focus(), 2U);
    list.handle(pointer(PointerAction::kMove, {0.0f, 0.0f}), kPlacement);  // Outside.
    EXPECT_EQ(list.focus(), 2U);
}

TEST(ButtonListTest, ClickActivatesWhenPressedAndReleasedOverTheSameButton) {
    ButtonList list = make_list(true);
    EXPECT_EQ(list.handle(pointer(PointerAction::kPress, center(list, 1)), kPlacement), std::nullopt);
    EXPECT_EQ(list.focus(), 1U);
    EXPECT_EQ(list.handle(pointer(PointerAction::kRelease, center(list, 1)), kPlacement), 1U);

    // Released over another button, or outside: nothing.
    list.handle(pointer(PointerAction::kPress, center(list, 0)), kPlacement);
    EXPECT_EQ(list.handle(pointer(PointerAction::kRelease, center(list, 2)), kPlacement), std::nullopt);
    list.handle(pointer(PointerAction::kPress, center(list, 0)), kPlacement);
    EXPECT_EQ(list.handle(pointer(PointerAction::kRelease, {0.0f, 0.0f}), kPlacement), std::nullopt);
    // A release without a press, e.g. of a click that began before the list was shown: nothing.
    EXPECT_EQ(list.handle(pointer(PointerAction::kRelease, center(list, 0)), kPlacement), std::nullopt);
}

TEST(ButtonListTest, ClickOnADisabledButtonDoesNothing) {
    ButtonList list = make_list();
    list.handle(pointer(PointerAction::kPress, center(list, 1)), kPlacement);
    EXPECT_EQ(list.handle(pointer(PointerAction::kRelease, center(list, 1)), kPlacement), std::nullopt);
    EXPECT_EQ(list.focus(), 0U);
}

TEST(ButtonListTest, DrawsEachButtonWithItsLabelAndABorderForTheFocus) {
    const ButtonList list = make_list();
    OverlayMesh mesh;
    list.add_to(mesh, kPlacement);
    // One rectangle (6 vertices) per button and one for the border, plus the text.
    EXPECT_GT(mesh.vertices().size(), 4U * 6U);
    // The border of the focused first button is the first rectangle, one interface pixel larger.
    EXPECT_EQ(mesh.vertices()[0].position, glm::vec2(8.0f, 18.0f));
}

}  // namespace
}  // namespace manifold
