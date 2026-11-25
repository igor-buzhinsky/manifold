#include "ui/item_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "items/item_catalog.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"
#include "ui/ui_style.hpp"

namespace manifold {

namespace {

/// Background of a cell, empty or not.
constexpr glm::vec4 kCellColor{0.1f, 0.1f, 0.11f, 1.0f};
/// Border of the selected cell.
constexpr glm::vec4 kSelectedBorderColor = kUiTitleColor;
/// The track of the scroll bar, and the part of it that shows which rows are shown.
constexpr glm::vec4 kScrollTrackColor{0.1f, 0.1f, 0.11f, 1.0f};
constexpr glm::vec4 kScrollThumbColor = kUiBorderColor;

}  // namespace

ItemGrid::ItemGrid(std::size_t columns, std::size_t visible_rows)
    : columns_(std::max<std::size_t>(columns, 1)), visible_rows_(std::max<std::size_t>(visible_rows, 1)) {}

glm::vec2 ItemGrid::size(float scale) const {
    const auto columns = static_cast<float>(columns_);
    const auto rows = static_cast<float>(visible_rows_);
    const float width = (columns * kCellSize) + ((columns - 1.0f) * kCellGap) + kScrollGap + kScrollWidth;
    const float height = (rows * kCellSize) + ((rows - 1.0f) * kCellGap);
    return glm::vec2(width, height) * scale;
}

std::size_t ItemGrid::rows(std::size_t count) const {
    // The empty cell after the items is the cell `count`.
    return std::max(visible_rows_, (count / columns_) + 1);
}

std::size_t ItemGrid::first_row(std::size_t count) const { return std::min(first_row_, rows(count) - visible_rows_); }

void ItemGrid::scroll(int rows, std::size_t count) {
    const auto first = static_cast<long long>(first_row(count)) + rows;
    first_row_ = static_cast<std::size_t>(std::max(first, 0LL));
    first_row_ = first_row(count);
}

void ItemGrid::scroll_to(std::size_t cell, std::size_t count) {
    const std::size_t row = cell / columns_;
    std::size_t first = first_row(count);
    if (row < first) {
        first = row;
    } else if (row >= first + visible_rows_) {
        first = row + 1 - visible_rows_;
    }
    first_row_ = first;
    first_row_ = first_row(count);
}

std::optional<std::size_t> ItemGrid::cell_at(const glm::vec2& point, std::size_t count,
                                             const Placement& placement) const {
    const glm::vec2 offset = (point - placement.area.top_left) / placement.scale;
    const float step = kCellSize + kCellGap;
    if (offset.x < 0.0f || offset.y < 0.0f) {
        return std::nullopt;
    }
    const auto column = static_cast<std::size_t>(offset.x / step);
    const auto row = static_cast<std::size_t>(offset.y / step);
    // Points in the gaps after a cell are on no cell.
    if (column >= columns_ || row >= visible_rows_ || offset.x - (static_cast<float>(column) * step) >= kCellSize ||
        offset.y - (static_cast<float>(row) * step) >= kCellSize) {
        return std::nullopt;
    }
    return ((first_row(count) + row) * columns_) + column;
}

std::optional<Rect> ItemGrid::cell_rect(std::size_t cell, std::size_t count, const Placement& placement) const {
    const std::size_t first = first_row(count);
    const std::size_t row = cell / columns_;
    if (row < first || row >= first + visible_rows_) {
        return std::nullopt;
    }
    const float step = (kCellSize + kCellGap) * placement.scale;
    const glm::vec2 top_left = placement.area.top_left + glm::vec2(static_cast<float>(cell % columns_) * step,
                                                                   static_cast<float>(row - first) * step);
    return Rect{.top_left = top_left, .size = glm::vec2(kCellSize * placement.scale)};
}

bool ItemGrid::contains(const glm::vec2& point, const Placement& placement) const {
    return Rect{.top_left = placement.area.top_left, .size = size(placement.scale)}.contains(point);
}

void ItemGrid::add_to(OverlayMesh& mesh, const Placement& placement, std::size_t count,
                      std::optional<std::size_t> selected) const {
    const float scale = placement.scale;
    const std::size_t first_cell = first_row(count) * columns_;
    for (std::size_t cell = first_cell; cell < first_cell + (visible_rows_ * columns_); ++cell) {
        const std::optional<Rect> rect = cell_rect(cell, count, placement);
        if (!rect) {
            continue;
        }
        if (selected == cell) {
            // A border of one interface pixel, as a larger rectangle behind the cell.
            mesh.add_rectangle(rect->top_left - glm::vec2(scale), rect->size + glm::vec2(2.0f * scale),
                               kSelectedBorderColor);
        }
        mesh.add_rectangle(rect->top_left, rect->size, selected == cell ? kUiFocusColor : kCellColor);
    }

    // The track as high as the grid, and in it the part of the rows that is shown, in whole interface pixels.
    const glm::vec2 grid_size = size(scale);
    const glm::vec2 track_top_left = placement.area.top_left + glm::vec2(grid_size.x - (kScrollWidth * scale), 0.0f);
    const glm::vec2 track_size{kScrollWidth * scale, grid_size.y};
    mesh.add_rectangle(track_top_left, track_size, kScrollTrackColor);
    const auto all_rows = static_cast<float>(rows(count));
    const float track_pixels = grid_size.y / scale;
    const float thumb_top = std::round(track_pixels * static_cast<float>(first_row(count)) / all_rows);
    const float thumb_height = std::max(1.0f, std::round(track_pixels * static_cast<float>(visible_rows_) / all_rows));
    mesh.add_rectangle(track_top_left + glm::vec2(0.0f, thumb_top * scale),
                       {track_size.x, std::min(thumb_height, track_pixels - thumb_top) * scale}, kScrollThumbColor);
}

void ItemGrid::add_icons(std::vector<IconPlacement>& icons, const Placement& placement,
                         std::span<const ItemId> items) const {
    const float inset = kIconInset * placement.scale;
    for (std::size_t cell = 0; cell < items.size(); ++cell) {
        if (const std::optional<Rect> rect = cell_rect(cell, items.size(), placement)) {
            icons.push_back({.item = items[cell],
                             .area = {.top_left = rect->top_left + glm::vec2(inset),
                                      .size = rect->size - glm::vec2(2.0f * inset)}});
        }
    }
}

}  // namespace manifold
