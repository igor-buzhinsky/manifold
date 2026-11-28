#include "ui/item_split_dialog.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_event.hpp"
#include "ui/ui_style.hpp"

namespace manifold {

namespace {

/// The line of the slider, and the part of it that shows the number.
constexpr glm::vec4 kTrackColor{0.1f, 0.1f, 0.11f, 1.0f};
constexpr glm::vec4 kKnobColor = kUiTitleColor;
/// Height of the line of the slider at scale 1, in interface pixels.
constexpr float kTrackHeight = 2.0f;

/// Rounds `pixels` down to whole interface pixels of `scale`, so that the blocky font is drawn evenly.
float floor_to_scale(float pixels, float scale) { return std::floor(pixels / scale) * scale; }

/// The width of a button with `label` at scale 1.
float button_width(const char* label) { return text_width(label, 1.0f) + (2.0f * ItemSplitDialog::kLabelPadding); }

/// The text under the slider, e.g. "12 of 20".
std::string amount_text(int amount, int max_count) {
    return std::to_string(amount) + " of " + std::to_string(max_count);
}

}  // namespace

ItemSplitDialog::ItemSplitDialog(std::string item_name, int max_count)
    : item_name_(std::move(item_name)), max_count_(std::max(max_count, 1)), amount_(max_count_) {}

float ItemSplitDialog::content_width() const {
    const float buttons = button_width(kMoveLabel) + kGap + button_width(kCancelLabel);
    return std::max({kMinContentWidth, text_width(item_name_, 1.0f), buttons});
}

Rect ItemSplitDialog::panel(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const float height = kTextLineHeight + kGap + kStepButtonSize + kGap + kTextLineHeight + kGap + kButtonHeight;
    const glm::vec2 size = (glm::vec2(content_width(), height) + glm::vec2(2.0f * kPadding)) * scale;
    // Centered, at whole interface pixels.
    const glm::vec2 top_left{floor_to_scale((static_cast<float>(picture.width) - size.x) / 2.0f, scale),
                             floor_to_scale((static_cast<float>(picture.height) - size.y) / 2.0f, scale)};
    return {.top_left = top_left, .size = size};
}

Rect ItemSplitDialog::minus_button(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const glm::vec2 top_left =
        panel(picture).top_left + (glm::vec2(kPadding, kPadding + kTextLineHeight + kGap) * scale);
    return {.top_left = top_left, .size = glm::vec2(kStepButtonSize * scale)};
}

Rect ItemSplitDialog::plus_button(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const Rect minus = minus_button(picture);
    return {.top_left = minus.top_left + glm::vec2((content_width() - kStepButtonSize) * scale, 0.0f),
            .size = minus.size};
}

Rect ItemSplitDialog::slider(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const Rect minus = minus_button(picture);
    const float gap = kGap * scale;
    const float left = minus.top_left.x + minus.size.x + gap;
    return {.top_left = {left, minus.top_left.y}, .size = {plus_button(picture).top_left.x - gap - left, minus.size.y}};
}

Rect ItemSplitDialog::move_button(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const Rect frame = panel(picture);
    const float buttons = button_width(kMoveLabel) + kGap + button_width(kCancelLabel);
    // Both buttons together are centered.
    const float left = frame.top_left.x + floor_to_scale((frame.size.x - (buttons * scale)) / 2.0f, scale);
    const float top = frame.top_left.y + frame.size.y - ((kPadding + kButtonHeight) * scale);
    return {.top_left = {left, top}, .size = glm::vec2(button_width(kMoveLabel), kButtonHeight) * scale};
}

Rect ItemSplitDialog::cancel_button(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const Rect move = move_button(picture);
    return {.top_left = move.top_left + glm::vec2(move.size.x + (kGap * scale), 0.0f),
            .size = glm::vec2(button_width(kCancelLabel), kButtonHeight) * scale};
}

std::optional<ItemSplitDialog::Control> ItemSplitDialog::control_at(const glm::vec2& point,
                                                                    const PixelSize& picture) const {
    if (minus_button(picture).contains(point)) {
        return Control::kMinus;
    }
    if (plus_button(picture).contains(point)) {
        return Control::kPlus;
    }
    if (slider(picture).contains(point)) {
        return Control::kSlider;
    }
    if (move_button(picture).contains(point)) {
        return Control::kMove;
    }
    if (cancel_button(picture).contains(point)) {
        return Control::kCancel;
    }
    return std::nullopt;
}

void ItemSplitDialog::change_by(int step) {
    // In long long, since the number can be the largest int.
    amount_ =
        static_cast<int>(std::clamp(static_cast<long long>(amount_) + step, 1LL, static_cast<long long>(max_count_)));
}

void ItemSplitDialog::set_from_slider(float x, const PixelSize& picture) {
    const Rect track = slider(picture);
    // As the knob is drawn, see add_to(): its middle is at the place of the number, so a press on it keeps the number.
    const float knob_width = kKnobWidth * ui_scale(picture);
    const float offset = x - track.top_left.x - (knob_width / 2.0f);
    const double fraction = std::clamp(static_cast<double>(offset / (track.size.x - knob_width)), 0.0, 1.0);
    // In double, which holds every int exactly, unlike float, and clamped against rounding.
    const long long amount = 1 + std::llround(fraction * static_cast<double>(max_count_ - 1));
    amount_ = static_cast<int>(std::clamp(amount, 1LL, static_cast<long long>(max_count_)));
}

SplitAction ItemSplitDialog::handle_key(NavKey key) {
    switch (key) {
        case NavKey::kLeft:
            change_by(-1);
            break;
        case NavKey::kRight:
            change_by(1);
            break;
        case NavKey::kDown:
            change_by(-kBigStep);
            break;
        case NavKey::kUp:
            change_by(kBigStep);
            break;
        case NavKey::kConfirm:
            return SplitAction::kMove;
        case NavKey::kCancel:
            return SplitAction::kCancel;
        case NavKey::kInventory:
        case NavKey::kTakeAll:
            break;
    }
    return SplitAction::kNone;
}

SplitAction ItemSplitDialog::handle_pointer(const PointerEvent& pointer, const PixelSize& picture) {
    hovered_ = control_at(pointer.position, picture);
    switch (pointer.action) {
        case PointerAction::kMove:
            if (dragging_) {
                set_from_slider(pointer.position.x, picture);
            }
            return SplitAction::kNone;
        case PointerAction::kRelease:
            dragging_ = false;
            return SplitAction::kNone;
        case PointerAction::kPress:
            break;
    }
    if (!panel(picture).contains(pointer.position)) {
        return SplitAction::kCancel;
    }
    if (!hovered_) {
        return SplitAction::kNone;
    }
    switch (*hovered_) {
        case Control::kMinus:
            change_by(-1);
            break;
        case Control::kPlus:
            change_by(1);
            break;
        case Control::kSlider:
            dragging_ = true;
            set_from_slider(pointer.position.x, picture);
            break;
        case Control::kMove:
            return SplitAction::kMove;
        case Control::kCancel:
            return SplitAction::kCancel;
    }
    return SplitAction::kNone;
}

SplitAction ItemSplitDialog::handle(const UiEvent& event, const PixelSize& picture) {
    if (const NavKey* key = std::get_if<NavKey>(&event)) {
        return handle_key(*key);
    }
    if (const auto* pointer = std::get_if<PointerEvent>(&event)) {
        return handle_pointer(*pointer, picture);
    }
    if (const auto* scroll = std::get_if<ScrollEvent>(&event);
        scroll != nullptr && panel(picture).contains(scroll->position)) {
        // Away from the user is more. Small steps of touchpads change it by one each.
        int steps = static_cast<int>(std::lround(scroll->steps));
        if (steps == 0 && scroll->steps != 0.0f) {
            steps = scroll->steps > 0.0f ? 1 : -1;
        }
        change_by(steps);
    }
    return SplitAction::kNone;
}

void ItemSplitDialog::add_button(OverlayMesh& mesh, const Rect& rect, const char* label, Control control,
                                 float scale) const {
    const bool hovered = hovered_ == control;
    if (hovered) {
        mesh.add_rectangle(rect.top_left - glm::vec2(scale), rect.size + glm::vec2(2.0f * scale), kUiBorderColor);
    }
    mesh.add_rectangle(rect.top_left, rect.size, hovered ? kUiFocusColor : kUiButtonColor);
    // The label centered, at whole interface pixels.
    const glm::vec2 offset{floor_to_scale((rect.size.x - text_width(label, scale)) / 2.0f, scale),
                           floor_to_scale((rect.size.y - (kTextLetterHeight * scale)) / 2.0f, scale)};
    mesh.add_text(rect.top_left + offset + glm::vec2(scale, 0.0f), label, scale, kUiTextColor);
}

void ItemSplitDialog::add_to(OverlayMesh& mesh, const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const Rect frame = panel(picture);
    // A border of one interface pixel, as a larger rectangle behind the panel.
    mesh.add_rectangle(frame.top_left - glm::vec2(scale), frame.size + glm::vec2(2.0f * scale), kUiBorderColor);
    mesh.add_rectangle(frame.top_left, frame.size, kUiPanelColor);

    const float center_x = frame.top_left.x + (frame.size.x / 2.0f);
    const auto centered = [&](const std::string& text, float y, const glm::vec4& color) {
        mesh.add_text({center_x - floor_to_scale(text_width(text, scale) / 2.0f, scale), y}, text, scale, color);
    };
    centered(item_name_, frame.top_left.y + (kPadding * scale), kUiTitleColor);

    add_button(mesh, minus_button(picture), "-", Control::kMinus, scale);
    add_button(mesh, plus_button(picture), "+", Control::kPlus, scale);
    const Rect track = slider(picture);
    const float track_y = track.top_left.y + floor_to_scale((track.size.y - (kTrackHeight * scale)) / 2.0f, scale);
    mesh.add_rectangle({track.top_left.x, track_y}, {track.size.x, kTrackHeight * scale}, kTrackColor);
    // The knob at the place of the number, within the slider.
    const float fraction = max_count_ > 1 ? static_cast<float>(amount_ - 1) / static_cast<float>(max_count_ - 1) : 1.0f;
    const float knob_width = kKnobWidth * scale;
    const float knob_x = track.top_left.x + floor_to_scale(fraction * (track.size.x - knob_width), scale);
    mesh.add_rectangle({knob_x, track.top_left.y}, {knob_width, track.size.y},
                       hovered_ == Control::kSlider || dragging_ ? kUiTextColor : kKnobColor);

    centered(amount_text(amount_, max_count_), track.top_left.y + track.size.y + (kGap * scale), kUiTextColor);

    add_button(mesh, move_button(picture), kMoveLabel, Control::kMove, scale);
    add_button(mesh, cancel_button(picture), kCancelLabel, Control::kCancel, scale);
}

}  // namespace manifold
