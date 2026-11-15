#include "world/spatial_grid.hpp"

#include <cstddef>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace manifold {
namespace {

using Indices = std::vector<std::size_t>;

/// A grid of 5 x 5 cells, each 2 m wide, over [0, 10] x [0, 10].
SpatialGrid make_grid() { return SpatialGrid({.min = {0.0f, 0.0f}, .max = {10.0f, 10.0f}}, 2.0f); }

/// The indices found in a box.
Indices find(const SpatialGrid& grid, const Box2& box) {
    Indices indices{99};  // Must be replaced, not appended to.
    grid.find(box, indices);
    return indices;
}

TEST(SpatialGridTest, EmptyGridFindsNothing) {
    EXPECT_TRUE(find(make_grid(), {.min = {0, 0}, .max = {10, 10}}).empty());
}

TEST(SpatialGridTest, FindsItemsInOverlappedCellsOnly) {
    SpatialGrid grid = make_grid();
    grid.insert(0, {.min = {0.5f, 0.5f}, .max = {1.0f, 1.0f}});
    grid.insert(1, {.min = {8.5f, 8.5f}, .max = {9.0f, 9.0f}});
    EXPECT_EQ(find(grid, {.min = {1.5f, 1.5f}, .max = {1.9f, 1.9f}}), Indices{0});  // Same cell, not overlapping.
    EXPECT_EQ(find(grid, {.min = {9.0f, 9.0f}, .max = {9.5f, 9.5f}}), Indices{1});
    EXPECT_TRUE(find(grid, {.min = {4.5f, 4.5f}, .max = {5.0f, 5.0f}}).empty());
    EXPECT_EQ(find(grid, {.min = {1.0f, 1.0f}, .max = {9.0f, 9.0f}}), (Indices{0, 1}));
}

TEST(SpatialGridTest, FindsItemSpanningCellsOnceInEachOfThem) {
    SpatialGrid grid = make_grid();
    grid.insert(3, {.min = {1.0f, 1.0f}, .max = {7.0f, 3.0f}});
    EXPECT_EQ(find(grid, {.min = {6.5f, 2.5f}, .max = {6.6f, 2.6f}}), Indices{3});
    EXPECT_EQ(find(grid, {.min = {0.0f, 0.0f}, .max = {10.0f, 10.0f}}), Indices{3});
    EXPECT_TRUE(find(grid, {.min = {8.5f, 2.5f}, .max = {9.0f, 3.5f}}).empty());
}

TEST(SpatialGridTest, ReturnsIndicesInIncreasingOrder) {
    SpatialGrid grid = make_grid();
    grid.insert(5, {.min = {6.5f, 0.5f}, .max = {7.0f, 1.0f}});
    grid.insert(2, {.min = {0.5f, 6.5f}, .max = {1.0f, 7.0f}});
    grid.insert(4, {.min = {0.5f, 0.5f}, .max = {7.0f, 1.0f}});
    EXPECT_EQ(find(grid, {.min = {0.0f, 0.0f}, .max = {10.0f, 10.0f}}), (Indices{2, 4, 5}));
}

TEST(SpatialGridTest, FindsItemsTouchingTheBox) {
    SpatialGrid grid = make_grid();
    grid.insert(0, {.min = {3.0f, 3.0f}, .max = {4.0f, 4.0f}});
    EXPECT_EQ(find(grid, {.min = {4.0f, 4.0f}, .max = {5.0f, 5.0f}}), Indices{0});
}

TEST(SpatialGridTest, ItemsAndSearchesOutsideTheAreaUseEdgeCells) {
    SpatialGrid grid = make_grid();
    grid.insert(0, {.min = {-30.0f, 4.5f}, .max = {-20.0f, 5.0f}});
    grid.insert(1, {.min = {12.0f, 12.0f}, .max = {13.0f, 13.0f}});
    EXPECT_EQ(find(grid, {.min = {-25.0f, 4.0f}, .max = {-24.0f, 5.0f}}), Indices{0});
    EXPECT_EQ(find(grid, {.min = {12.5f, 12.5f}, .max = {20.0f, 20.0f}}), Indices{1});
    EXPECT_EQ(find(grid, {.min = {-50.0f, -50.0f}, .max = {50.0f, 50.0f}}), (Indices{0, 1}));
}

TEST(SpatialGridTest, HandlesHugeAndInfiniteCoordinates) {
    SpatialGrid grid = make_grid();
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    grid.insert(0, {.min = {-kInfinity, 1.0f}, .max = {kInfinity, 1.0f}});
    EXPECT_EQ(find(grid, {.min = {1e30f, 0.0f}, .max = {1e30f, 2.0f}}), Indices{0});
    EXPECT_TRUE(find(grid, {.min = {-1e30f, 5.0f}, .max = {-1e30f, 6.0f}}).empty());
}

TEST(SpatialGridTest, WorksForAnEmptyArea) {
    SpatialGrid grid({.min = {1.0f, 1.0f}, .max = {1.0f, 1.0f}}, 2.0f);
    grid.insert(0, {.min = {5.0f, 5.0f}, .max = {6.0f, 6.0f}});
    EXPECT_EQ(find(grid, {.min = {-5.0f, -5.0f}, .max = {-4.0f, -4.0f}}), Indices{0});
}

}  // namespace
}  // namespace manifold
