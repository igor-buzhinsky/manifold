#include "ui/inventory_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "render/viewport.hpp"
#include "ui/item_grid.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"
#include "ui/ui_style.hpp"

namespace manifold {

namespace {

/// The title of the party's grid when a container is open.
constexpr const char* kPartyTitle = "Party";
/// The title of the panel of the party alone.
constexpr const char* kInventoryTitle = "Inventory";

/// Rounds `pixels` down to whole interface pixels of `scale`, so that the blocky font stays sharp.
float floor_to_scale(float pixels, float scale) { return std::floor(pixels / scale) * scale; }

}  // namespace

InventoryScreen::InventoryScreen(const ItemCatalog& catalog, Inventory& party) : catalog_(&catalog) {
    sides_.push_back({.inventory = &party, .title = kInventoryTitle, .grid = ItemGrid(kColumns, kRows)});
}

InventoryScreen::InventoryScreen(const ItemCatalog& catalog, Inventory& party, Inventory& container,
                                 std::string container_title)
    : catalog_(&catalog) {
    sides_.push_back({.inventory = &container,
                      .title = std::move(container_title),
                      .grid = ItemGrid(kContainerColumns, kContainerRows)});
    sides_.push_back({.inventory = &party, .title = kPartyTitle, .grid = ItemGrid(kContainerColumns, kContainerRows)});
}

std::optional<ItemSelection> InventoryScreen::selection() const {
    if (selection_ && selection_->index < sides_[selection_->side].inventory->size()) {
        return selection_;
    }
    return std::nullopt;
}

glm::vec2 InventoryScreen::content_size(float scale) const {
    float width = 0.0f;
    float grid_height = 0.0f;
    for (const Side& side : sides_) {
        const glm::vec2 grid = side.grid.size(scale);
        width += grid.x;
        grid_height = std::max(grid_height, grid.y);
    }
    width += static_cast<float>(sides_.size() - 1) * kSideGap * scale;
    // The titles, the grids, the row of the button or the weight, and the line of the selected item.
    const float rows = kTextLineHeight + kGap + kGap + kButtonHeight + kGap + kTextLineHeight;
    return {width, grid_height + (rows * scale)};
}

Rect InventoryScreen::panel(const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    const glm::vec2 size = content_size(scale) + glm::vec2(2.0f * kPadding * scale);
    // Centered, at whole interface pixels.
    const glm::vec2 top_left{floor_to_scale((static_cast<float>(picture.width) - size.x) / 2.0f, scale),
                             floor_to_scale((static_cast<float>(picture.height) - size.y) / 2.0f, scale)};
    return {.top_left = top_left, .size = size};
}

Placement InventoryScreen::grid_placement(std::size_t side, const PixelSize& picture) const {
    const float scale = ui_scale(picture);
    glm::vec2 top_left = panel(picture).top_left + (glm::vec2(kPadding, kPadding + kTextLineHeight + kGap) * scale);
    for (std::size_t i = 0; i < side; ++i) {
        top_left.x += sides_[i].grid.size(scale).x + (kSideGap * scale);
    }
    return {.area = {.top_left = top_left, .size = sides_.at(side).grid.size(scale)}, .scale = scale};
}

std::optional<Rect> InventoryScreen::take_all_button(const PixelSize& picture) const {
    if (sides_.size() < 2) {
        return std::nullopt;
    }
    const Placement grid = grid_placement(0, picture);
    const float scale = grid.scale;
    const float width = text_width(kTakeAllLabel, scale) + (2.0f * kLabelPadding * scale);
    return Rect{.top_left = grid.area.top_left + glm::vec2(0.0f, grid.area.size.y + (kGap * scale)),
                .size = {width, kButtonHeight * scale}};
}

void InventoryScreen::select(ItemSelection selection) {
    selection_ = selection;
    Side& side = sides_[selection.side];
    side.grid.scroll_to(selection.index, side.inventory->size());
}

void InventoryScreen::navigate(NavKey key) {
    const std::optional<ItemSelection> current = selection();
    if (!current) {
        // The first item of the first grid that has any.
        for (std::size_t side = 0; side < sides_.size(); ++side) {
            if (!sides_[side].inventory->empty()) {
                select({.side = side, .index = 0});
                return;
            }
        }
        return;
    }
    const Side& side = sides_[current->side];
    const std::size_t columns = side.grid.columns();
    const std::size_t count = side.inventory->size();
    const std::size_t index = current->index;
    const std::size_t row = index / columns;
    const std::size_t column = index % columns;
    // The item of `other` side nearest to `row`, in its first or last column.
    const auto across = [this, row](std::size_t other, bool last_column) -> std::optional<ItemSelection> {
        const Side& target = sides_[other];
        if (target.inventory->empty()) {
            return std::nullopt;
        }
        const std::size_t target_columns = target.grid.columns();
        const std::size_t wanted = (row * target_columns) + (last_column ? target_columns - 1 : 0);
        return ItemSelection{.side = other, .index = std::min(wanted, target.inventory->size() - 1)};
    };
    std::optional<ItemSelection> next;
    switch (key) {
        case NavKey::kUp:
            if (row > 0) {
                next = ItemSelection{.side = current->side, .index = index - columns};
            }
            break;
        case NavKey::kDown:
            if (index + columns < count) {
                next = ItemSelection{.side = current->side, .index = index + columns};
            } else if (row < (count - 1) / columns) {
                // Down to the last item, which is in the next row but not below.
                next = ItemSelection{.side = current->side, .index = count - 1};
            }
            break;
        case NavKey::kLeft:
            if (column > 0) {
                next = ItemSelection{.side = current->side, .index = index - 1};
            } else if (current->side > 0) {
                next = across(current->side - 1, true);
            }
            break;
        case NavKey::kRight:
            if (column + 1 < columns && index + 1 < count) {
                next = ItemSelection{.side = current->side, .index = index + 1};
            } else if (current->side + 1 < sides_.size()) {
                next = across(current->side + 1, false);
            }
            break;
        default:
            break;
    }
    if (next) {
        select(*next);
    }
}

void InventoryScreen::move_selected_to(std::size_t side) {
    const std::optional<ItemSelection> current = selection();
    if (!current) {
        return;
    }
    const std::size_t index = move_item(*sides_[current->side].inventory, current->index, *sides_[side].inventory);
    select({.side = side, .index = index});
}

void InventoryScreen::take_all() {
    if (sides_.size() < 2) {
        return;
    }
    Inventory& container = *sides_[0].inventory;
    Inventory& party = *sides_[party_side()].inventory;
    while (!container.empty()) {
        move_item(container, 0, party);
    }
    if (selection_ && selection_->side == 0) {
        selection_.reset();
    }
}

void InventoryScreen::move_selected_across() {
    const std::optional<ItemSelection> current = selection();
    if (!current || sides_.size() < 2) {
        return;
    }
    const std::size_t from = current->side;
    move_item(*sides_[from].inventory, current->index, *sides_[1 - from].inventory);
    // The item after it comes into its place, or the one before it if it was the last.
    const std::size_t count = sides_[from].inventory->size();
    if (count == 0) {
        selection_.reset();
    } else {
        select({.side = from, .index = std::min(current->index, count - 1)});
    }
}

std::optional<UiCommand> InventoryScreen::handle_key(NavKey key) {
    switch (key) {
        case NavKey::kCancel:
        case NavKey::kInventory:
            return UiCommand::kResume;
        case NavKey::kTakeAll:
            take_all();
            break;
        case NavKey::kConfirm:
            move_selected_across();
            break;
        case NavKey::kUp:
        case NavKey::kDown:
        case NavKey::kLeft:
        case NavKey::kRight:
            navigate(key);
            break;
    }
    return std::nullopt;
}

void InventoryScreen::handle_scroll(const ScrollEvent& scroll, const PixelSize& picture) {
    // Away from the user scrolls up. Small steps of touchpads scroll by a row each.
    int rows = static_cast<int>(std::lround(-scroll.steps));
    if (rows == 0 && scroll.steps != 0.0f) {
        rows = scroll.steps > 0.0f ? -1 : 1;
    }
    for (std::size_t side = 0; side < sides_.size(); ++side) {
        if (sides_[side].grid.contains(scroll.position, grid_placement(side, picture))) {
            sides_[side].grid.scroll(rows, sides_[side].inventory->size());
        }
    }
}

void InventoryScreen::handle_pointer(const PointerEvent& pointer, const PixelSize& picture) {
    const std::optional<Rect> button = take_all_button(picture);
    take_all_hovered_ = button && button->contains(pointer.position);
    if (pointer.action != PointerAction::kPress) {
        return;
    }
    if (take_all_hovered_) {
        take_all();
        return;
    }
    for (std::size_t side = 0; side < sides_.size(); ++side) {
        const Side& target = sides_[side];
        const std::optional<std::size_t> cell =
            target.grid.cell_at(pointer.position, target.inventory->size(), grid_placement(side, picture));
        if (!cell) {
            continue;
        }
        if (*cell < target.inventory->size()) {
            select({.side = side, .index = *cell});
        } else {
            move_selected_to(side);
        }
        return;
    }
}

std::optional<UiCommand> InventoryScreen::handle(const UiEvent& event, const PixelSize& picture) {
    if (const NavKey* key = std::get_if<NavKey>(&event)) {
        return handle_key(*key);
    }
    if (const auto* scroll = std::get_if<ScrollEvent>(&event)) {
        handle_scroll(*scroll, picture);
    } else if (const auto* pointer = std::get_if<PointerEvent>(&event)) {
        handle_pointer(*pointer, picture);
    }
    return std::nullopt;
}

void InventoryScreen::add_to(OverlayMesh& mesh, const PixelSize& picture) const {
    const glm::vec2 picture_size{static_cast<float>(picture.width), static_cast<float>(picture.height)};
    mesh.add_rectangle({0.0f, 0.0f}, picture_size, kUiDimColor);
    const Rect frame = panel(picture);
    const float scale = ui_scale(picture);
    // A border of one interface pixel, as a larger rectangle behind the panel.
    mesh.add_rectangle(frame.top_left - glm::vec2(scale), frame.size + glm::vec2(2.0f * scale), kUiBorderColor);
    mesh.add_rectangle(frame.top_left, frame.size, kUiPanelColor);

    const std::optional<ItemSelection> selected = selection();
    float below_grids = 0.0f;
    for (std::size_t side = 0; side < sides_.size(); ++side) {
        const Side& shown = sides_[side];
        const Placement grid = grid_placement(side, picture);
        below_grids = grid.area.top_left.y + grid.area.size.y + (kGap * scale);
        // The title of the party alone is centered over the panel, those of two grids are over each.
        float title_x = grid.area.top_left.x;
        if (sides_.size() == 1) {
            title_x += floor_to_scale((grid.area.size.x - text_width(shown.title, scale)) / 2.0f, scale);
        }
        mesh.add_text({title_x, frame.top_left.y + (kPadding * scale)}, shown.title, scale, kUiTitleColor);
        shown.grid.add_to(mesh, grid, shown.inventory->size(),
                          selected && selected->side == side ? std::optional(selected->index) : std::nullopt);

        // The total weight below the party's grid, to the right of the Take all button below the container's.
        if (side == party_side()) {
            const std::string weight = (sides_.size() == 1 ? "Total weight: " : "Weight: ") +
                                       format_weight(shown.inventory->weight(*catalog_));
            const float text_y =
                below_grids + floor_to_scale(((kButtonHeight - kTextLetterHeight) * scale) / 2.0f, scale);
            mesh.add_text({grid.area.top_left.x, text_y}, weight, scale, kUiTextColor);
        }
    }

    if (const std::optional<Rect> button = take_all_button(picture)) {
        const bool enabled = !sides_[0].inventory->empty();
        if (enabled && take_all_hovered_) {
            mesh.add_rectangle(button->top_left - glm::vec2(scale), button->size + glm::vec2(2.0f * scale),
                               kUiBorderColor);
        }
        mesh.add_rectangle(button->top_left, button->size,
                           enabled && take_all_hovered_ ? kUiFocusColor : kUiButtonColor);
        const float text_y =
            button->top_left.y + floor_to_scale((button->size.y - (kTextLetterHeight * scale)) / 2.0f, scale);
        mesh.add_text({button->top_left.x + (kLabelPadding * scale), text_y}, kTakeAllLabel, scale,
                      enabled ? kUiTextColor : kUiDisabledTextColor);
    }

    // The selected item's name and weight on the last line.
    if (selected) {
        const ItemType& item = catalog_->at(sides_[selected->side].inventory->items()[selected->index]);
        const std::string text = item.name + ", weight " + format_weight(item.weight);
        const float text_y = below_grids + ((kButtonHeight + kGap) * scale);
        mesh.add_text({frame.top_left.x + (kPadding * scale), text_y}, text, scale, kUiTextColor);
    }
}

void InventoryScreen::add_icons(std::vector<IconPlacement>& icons, const PixelSize& picture) const {
    for (std::size_t side = 0; side < sides_.size(); ++side) {
        sides_[side].grid.add_icons(icons, grid_placement(side, picture), sides_[side].inventory->items());
    }
}

}  // namespace manifold
