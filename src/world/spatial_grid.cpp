#include "world/spatial_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>

#include "world/geometry.hpp"

namespace manifold {

namespace {

/// At most this many columns and rows, so that a huge area doesn't use up the memory. Items beyond go to the edge.
constexpr std::size_t kMaxCellsPerSide = 1024;

}  // namespace

SpatialGrid::SpatialGrid(const Box2& area, float cell_size)
    : origin_(area.min),
      cell_size_(cell_size),
      columns_(cell_index(area.max.x - area.min.x, cell_size, kMaxCellsPerSide) + 1),
      rows_(cell_index(area.max.y - area.min.y, cell_size, kMaxCellsPerSide) + 1),
      cells_(columns_ * rows_) {}

void SpatialGrid::insert(std::size_t index, const Box2& box) {
    const CellRange range = cells_of(box);
    for (std::size_t row = range.first_row; row <= range.last_row; ++row) {
        for (std::size_t column = range.first_column; column <= range.last_column; ++column) {
            cells_[(row * columns_) + column].push_back(index);
        }
    }
}

void SpatialGrid::find(const Box2& box, std::vector<std::size_t>& indices) const {
    indices.clear();
    const CellRange range = cells_of(box);
    for (std::size_t row = range.first_row; row <= range.last_row; ++row) {
        for (std::size_t column = range.first_column; column <= range.last_column; ++column) {
            const std::vector<std::size_t>& cell = cells_[(row * columns_) + column];
            indices.insert(indices.end(), cell.begin(), cell.end());
        }
    }
    // Items that span several cells are found once in each of them.
    std::ranges::sort(indices);
    const auto duplicates = std::ranges::unique(indices);
    indices.erase(duplicates.begin(), duplicates.end());
}

SpatialGrid::CellRange SpatialGrid::cells_of(const Box2& box) const {
    return {.first_column = cell_index(box.min.x - origin_.x, cell_size_, columns_),
            .last_column = cell_index(box.max.x - origin_.x, cell_size_, columns_),
            .first_row = cell_index(box.min.y - origin_.y, cell_size_, rows_),
            .last_row = cell_index(box.max.y - origin_.y, cell_size_, rows_)};
}

std::size_t SpatialGrid::cell_index(float offset, float cell_size, std::size_t count) {
    // Compared as floats first, so that offsets far outside of the grid, infinite or NaN don't overflow the cast.
    const float cell = std::floor(offset / cell_size);
    if (!(cell > 0.0f)) {
        return 0;
    }
    if (cell >= static_cast<float>(count - 1)) {
        return count - 1;
    }
    return static_cast<std::size_t>(cell);
}

}  // namespace manifold
