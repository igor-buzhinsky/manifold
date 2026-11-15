#pragma once

#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>

#include "world/geometry.hpp"

namespace manifold {

/// Finds the items near a place quickly, by sorting the items into the square cells of a grid in the horizontal plane.
///
/// An item is known by its index and by the box around it, and is added to every cell that the box overlaps. Items
/// and searches outside of the grid's area use the nearest cells at the edge, so they work too, only more slowly.
class SpatialGrid {
public:
    /// \param area      The area that the cells cover. Should be where most items and searches are.
    /// \param cell_size Side of a cell, in metres. Must be positive.
    SpatialGrid(const Box2& area, float cell_size);

    /// Adds an item. The same index can be added more than once, e.g. with different boxes.
    void insert(std::size_t index, const Box2& box);

    /// Finds the items whose cells overlap a box.
    ///
    /// This includes all items whose boxes overlap or touch the box, and maybe some more nearby.
    ///
    /// \param box          Where to search.
    /// \param[out] indices Replaced with the indices of the items found, in increasing order and each once.
    void find(const Box2& box, std::vector<std::size_t>& indices) const;

private:
    /// The first and last column and row of the cells that a box overlaps.
    struct CellRange {
        std::size_t first_column;
        std::size_t last_column;
        std::size_t first_row;
        std::size_t last_row;
    };

    CellRange cells_of(const Box2& box) const;
    /// Column or row of the cell that holds a coordinate, measured from the start of the area.
    static std::size_t cell_index(float offset, float cell_size, std::size_t count);

    glm::vec2 origin_;
    float cell_size_;
    std::size_t columns_;
    std::size_t rows_;
    std::vector<std::vector<std::size_t>> cells_;  ///< Row by row, columns along x.
};

}  // namespace manifold
