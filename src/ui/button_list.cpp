#include "ui/button_list.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/ui_event.hpp"
#include "ui/ui_style.hpp"

namespace manifold {

ButtonList::ButtonList(std::vector<Button> buttons) : buttons_(std::move(buttons)) {
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        if (buttons_[i].enabled) {
            focus_ = i;
            break;
        }
    }
}

float ButtonList::height(std::size_t count, float scale) {
    if (count == 0) {
        return 0.0f;
    }
    const auto buttons = static_cast<float>(count);
    return ((buttons * kButtonHeight) + ((buttons - 1.0f) * kGap)) * scale;
}

std::vector<Rect> ButtonList::layout(const Placement& placement) const {
    std::vector<Rect> rects;
    rects.reserve(buttons_.size());
    glm::vec2 top_left = placement.area.top_left;
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        rects.push_back({.top_left = top_left, .size = {placement.area.size.x, kButtonHeight * placement.scale}});
        top_left.y += (kButtonHeight + kGap) * placement.scale;
    }
    return rects;
}

std::optional<std::size_t> ButtonList::button_at(const glm::vec2& point, const Placement& placement) const {
    const std::vector<Rect> rects = layout(placement);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        if (buttons_[i].enabled && rects[i].contains(point)) {
            return i;
        }
    }
    return std::nullopt;
}

void ButtonList::move_focus(int step) {
    if (!focus_) {
        return;  // No button is enabled.
    }
    const auto count = static_cast<std::ptrdiff_t>(buttons_.size());
    auto index = static_cast<std::ptrdiff_t>(*focus_);
    // At most one round, which ends at the button with the focus if it is the only enabled one.
    for (std::ptrdiff_t tried = 0; tried < count; ++tried) {
        index = (index + step + count) % count;
        if (buttons_[static_cast<std::size_t>(index)].enabled) {
            focus_ = static_cast<std::size_t>(index);
            return;
        }
    }
}

std::optional<std::size_t> ButtonList::handle(const UiEvent& event, const Placement& placement) {
    if (const NavKey* key = std::get_if<NavKey>(&event)) {
        switch (*key) {
            case NavKey::kUp:
                move_focus(-1);
                break;
            case NavKey::kDown:
                move_focus(1);
                break;
            case NavKey::kConfirm:
                return focus_;
            case NavKey::kLeft:
            case NavKey::kRight:
            case NavKey::kCancel:
            case NavKey::kInventory:
            case NavKey::kTakeAll:
                break;
        }
        return std::nullopt;
    }
    const auto* pointer = std::get_if<PointerEvent>(&event);
    if (pointer == nullptr) {
        return std::nullopt;  // The buttons don't scroll.
    }
    const std::optional<std::size_t> under = button_at(pointer->position, placement);
    switch (pointer->action) {
        case PointerAction::kMove:
            if (under) {
                focus_ = under;
            }
            break;
        case PointerAction::kPress:
            pressed_ = under;
            if (under) {
                focus_ = under;
            }
            break;
        case PointerAction::kRelease:
            if (under && std::exchange(pressed_, std::nullopt) == under) {
                return under;
            }
            pressed_.reset();
            break;
    }
    return std::nullopt;
}

void ButtonList::add_to(OverlayMesh& mesh, const Placement& placement) const {
    const float scale = placement.scale;
    const std::vector<Rect> rects = layout(placement);
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        const Rect& rect = rects[i];
        const Button& button = buttons_[i];
        const bool focused = focus_ == i;
        if (focused) {
            // A border of one interface pixel, as a larger rectangle behind the button.
            mesh.add_rectangle(rect.top_left - glm::vec2(scale), rect.size + glm::vec2(2.0f * scale), kUiBorderColor);
        }
        glm::vec4 color = kUiButtonColor;
        if (pressed_ == i) {
            color = kUiPressedColor;
        } else if (focused) {
            color = kUiFocusColor;
        }
        mesh.add_rectangle(rect.top_left, rect.size, color);
        // Centered, rounded down to whole interface pixels so that the font stays sharp. Vertically, the letters are
        // centered rather than the line, which has empty rows below them.
        const float text_x =
            rect.top_left.x + (std::floor((rect.size.x - text_width(button.label, scale)) / 2.0f / scale) * scale);
        const float text_y =
            rect.top_left.y + (std::floor((rect.size.y - (kTextLetterHeight * scale)) / 2.0f / scale) * scale);
        mesh.add_text({text_x, text_y}, button.label, scale, button.enabled ? kUiTextColor : kUiDisabledTextColor);
    }
}

}  // namespace manifold
