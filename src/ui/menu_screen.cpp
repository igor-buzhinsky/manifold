#include "ui/menu_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>

#include "render/viewport.hpp"
#include "ui/button_list.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"
#include "ui/ui_style.hpp"

namespace manifold {

namespace {

/// The buttons of the items of a menu.
std::vector<Button> make_buttons(const std::vector<MenuItem>& items) {
    std::vector<Button> buttons;
    buttons.reserve(items.size());
    for (const MenuItem& item : items) {
        buttons.push_back({.label = item.label, .enabled = item.enabled});
    }
    return buttons;
}

/// The commands of the items of a menu, in the same order.
std::vector<UiCommand> item_commands(const std::vector<MenuItem>& items) {
    std::vector<UiCommand> commands;
    commands.reserve(items.size());
    for (const MenuItem& item : items) {
        commands.push_back(item.command);
    }
    return commands;
}

}  // namespace

MenuScreen::MenuScreen(MenuConfig config)
    : title_(std::move(config.title)),
      commands_(item_commands(config.items)),
      cancel_(config.cancel),
      hides_game_(config.hides_game),
      buttons_(make_buttons(config.items)) {}

Placement MenuScreen::buttons_placement(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    // As wide as the longest label or the title needs, in whole interface pixels.
    float width = kMinButtonWidth * scale;
    for (const Button& button : buttons_.buttons()) {
        width = std::max(width, text_width(button.label, scale) + (2.0f * kLabelPadding * scale));
    }
    width = std::max(width, text_width(title_, scale));
    width = std::ceil(width / scale) * scale;
    const float height = ButtonList::height(buttons_.buttons().size(), scale);
    const float panel_width = width + (2.0f * kPadding * scale);
    const float panel_height = ((kTextLineHeight + kTitleGap + (2.0f * kPadding)) * scale) + height;
    // The panel is centered, at whole interface pixels.
    const float left =
        (std::floor((static_cast<float>(picture.width) - panel_width) / 2.0f / scale) * scale) + (kPadding * scale);
    const float top = (std::floor((static_cast<float>(picture.height) - panel_height) / 2.0f / scale) * scale) +
                      ((kPadding + kTextLineHeight + kTitleGap) * scale);
    return {.area = {.top_left = {left, top}, .size = {width, height}}, .scale = scale};
}

Rect MenuScreen::panel(const PixelSize& picture) const {
    const Placement buttons = buttons_placement(picture);
    const float scale = buttons.scale;
    const glm::vec2 top_left =
        buttons.area.top_left - (glm::vec2(kPadding, kPadding + kTextLineHeight + kTitleGap) * scale);
    const glm::vec2 bottom_right = buttons.area.top_left + buttons.area.size + glm::vec2(kPadding * scale);
    return {.top_left = top_left, .size = bottom_right - top_left};
}

std::optional<UiCommand> MenuScreen::handle(const UiEvent& event, const PixelSize& picture) {
    if (const NavKey* key = std::get_if<NavKey>(&event); key != nullptr && *key == NavKey::kCancel) {
        return cancel_;
    }
    if (const std::optional<std::size_t> chosen = buttons_.handle(event, buttons_placement(picture))) {
        return commands_[*chosen];
    }
    return std::nullopt;
}

void MenuScreen::add_to(OverlayMesh& mesh, const PixelSize& picture) const {
    const glm::vec2 picture_size{static_cast<float>(picture.width), static_cast<float>(picture.height)};
    mesh.add_rectangle({0.0f, 0.0f}, picture_size, hides_game_ ? kUiBlackColor : kUiDimColor);

    const Placement buttons = buttons_placement(picture);
    const float scale = buttons.scale;
    const Rect frame = panel(picture);
    // A border of one interface pixel, as a larger rectangle behind the panel.
    mesh.add_rectangle(frame.top_left - glm::vec2(scale), frame.size + glm::vec2(2.0f * scale), kUiBorderColor);
    mesh.add_rectangle(frame.top_left, frame.size, kUiPanelColor);
    const float title_x = buttons.area.top_left.x +
                          (std::floor((buttons.area.size.x - text_width(title_, scale)) / 2.0f / scale) * scale);
    mesh.add_text({title_x, frame.top_left.y + (kPadding * scale)}, title_, scale, kUiTitleColor);
    buttons_.add_to(mesh, buttons);
}

}  // namespace manifold
