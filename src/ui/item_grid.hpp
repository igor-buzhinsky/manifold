#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "items/item_catalog.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"

namespace manifold {

/// A grid of square cells that shows the items of an Inventory, one per cell, row by row from the top left, with a
/// scroll bar on its right.
///
/// It shows `visible_rows` rows at a time, and has as many rows as the items need, with at least one empty cell after
/// them, so that there is always an empty cell to put an item into. It scrolls by whole rows. The grid keeps which row
/// is at the top, but no layout: it is placed anew for each event and each drawing, like ButtonList.
class ItemGrid {
public:
    /// Sizes at scale 1, in interface pixels, see Placement::scale.
    static constexpr float kCellSize = 20.0f;
    static constexpr float kCellGap = 1.0f;      ///< Between two cells.
    static constexpr float kIconInset = 1.0f;    ///< Between the edge of a cell and its icon.
    static constexpr float kScrollGap = 2.0f;    ///< Between the cells and the scroll bar.
    static constexpr float kScrollWidth = 3.0f;  ///< Of the scroll bar.

    /// \param columns      Cells in a row, at least 1.
    /// \param visible_rows Rows shown at a time, at least 1.
    ItemGrid(std::size_t columns, std::size_t visible_rows);

    std::size_t columns() const { return columns_; }
    std::size_t visible_rows() const { return visible_rows_; }

    /// Width and height of the grid with its scroll bar, in pixels of the picture.
    glm::vec2 size(float scale) const;

    /// How many rows the grid has for `count` items: enough for all of them and an empty cell after them, and at
    /// least visible_rows().
    std::size_t rows(std::size_t count) const;

    /// The row shown at the top for `count` items: the one scrolled to, or the last one that still fills the grid if
    /// the items became fewer since.
    std::size_t first_row(std::size_t count) const;

    /// Scrolls by `rows` rows, down if positive, as far as there are rows for `count` items.
    void scroll(int rows, std::size_t count);

    /// Scrolls as little as needed to show cell `cell` with `count` items.
    void scroll_to(std::size_t cell, std::size_t count);

    /// The cell under `point`, counted row by row from the first cell of the first row, if it is on a cell that is
    /// shown. The cells may be empty.
    ///
    /// \param count     How many items there are.
    /// \param placement Where the grid is: its top-left corner, and the scale.
    std::optional<std::size_t> cell_at(const glm::vec2& point, std::size_t count, const Placement& placement) const;

    /// The rectangle of cell `cell` if it is shown with `count` items, see cell_at().
    std::optional<Rect> cell_rect(std::size_t cell, std::size_t count, const Placement& placement) const;

    /// Whether `point` is on the grid or its scroll bar, e.g. to scroll it with the wheel.
    bool contains(const glm::vec2& point, const Placement& placement) const;

    /// Adds the cells that are shown and the scroll bar to `mesh`. The cell of `selected`, if it is shown, is lighter
    /// and has a border.
    ///
    /// \param count The number of items, which fill the first cells.
    void add_to(OverlayMesh& mesh, const Placement& placement, std::size_t count,
                std::optional<std::size_t> selected) const;

    /// Adds the icons of the items that are shown to `icons`, each inside its cell.
    void add_icons(std::vector<IconPlacement>& icons, const Placement& placement, std::span<const ItemId> items) const;

private:
    std::size_t columns_;
    std::size_t visible_rows_;
    std::size_t first_row_ = 0;  ///< The row scrolled to, see first_row().
};

}  // namespace manifold
