#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "level/level.hpp"

namespace manifold {

/// Settings for RoomLayout.
struct RoomLayoutConfig {
    int grid_width = 80;            ///< Number of tiles along x.
    int grid_height = 80;           ///< Number of tiles along y.
    float tile_size = 2.0f;         ///< Side of a square tile, in metres.
    float height = 3.0f;            ///< Height of a layer from its floor to its ceiling, in metres.
    int min_room_size = 1;          ///< Smallest inside width or depth of a room, in tiles.
    int max_room_size = 5;          ///< Largest inside width or depth of a room, in tiles.
    int placement_attempts = 5000;  ///< How often to try placing another room. More attempts fill more space.
    int layer_count = 3;            ///< Number of layers stacked on top of each other.
    float slab_thickness = 0.3f;    ///< Thickness of the floor between two layers, in metres.
    int hole_size = 2;              ///< Width and depth of a hole between two layers, in tiles.
    int min_holes = 3;              ///< Fewest holes between every two neighboring layers.
    int max_holes = 6;              ///< Most holes between every two neighboring layers.
    int max_plan_attempts = 100;    ///< How often to plan the level again when the holes don't fit.
};

/// What a tile of the room grid contains.
enum class Tile : std::uint8_t {
    kEmpty,    ///< Not part of any room. Unreachable for the player.
    kFloor,    ///< Inside a room.
    kWall,     ///< A wall block.
    kOpening,  ///< A gap in a wall that connects two rooms.
};

/// A rectangular room on the grid. Its walls lie on the tiles just outside of it.
struct GridRoom {
    int x = 0;       ///< First inside tile along x.
    int y = 0;       ///< First inside tile along y.
    int width = 0;   ///< Inside size along x, in tiles.
    int height = 0;  ///< Inside size along y, in tiles.
};

/// The plan of one layer of rooms on a grid of square tiles.
///
/// Tile (x, y) covers the square from (x, y) to (x + 1, y + 1) in tile units, so +y is "up" on the printed
/// plan.
struct RoomPlan {
    int width = 0;                  ///< Number of tiles along x.
    int height = 0;                 ///< Number of tiles along y.
    std::vector<Tile> tiles;        ///< Row by row, starting at y = 0.
    std::vector<int> room_of_tile;  ///< Index in `rooms` of the room each tile is inside, or -1.
    std::vector<GridRoom> rooms;    ///< All rooms.

    /// Creates a grid with all tiles empty.
    RoomPlan(int width, int height);

    /// Checks whether the tile is inside the grid.
    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    /// The tile at (x, y). Tiles outside the grid are empty.
    Tile tile(int x, int y) const;
    /// Index of the room that tile (x, y) is inside, or -1 if there is none.
    int room_at(int x, int y) const;
};

/// A square hole in the floor between two neighboring layers.
struct Hole {
    int layer = 0;  ///< The lower of the two layers. The hole is in its ceiling.
    int x = 0;      ///< First tile along x.
    int y = 0;      ///< First tile along y.
};

/// The plan of a level with several layers of rooms on top of each other.
struct LayeredPlan {
    std::vector<RoomPlan> layers;  ///< From the bottom up.
    std::vector<Hole> holes;       ///< Holes between the layers.
    int hole_size = 0;             ///< Width and depth of every hole, in tiles.
    int start_layer = 0;           ///< Index in `layers` of the layer where the player starts.
    std::size_t start_room = 0;    ///< Index of the start room in that layer's rooms.
    int attempts = 0;              ///< How many times the level was planned until the holes fit.

    /// Checks whether tile (x, y) is part of a hole in the ceiling of `layer`.
    bool is_hole(int layer, int x, int y) const;
};

/// Plans one layer of rooms on a grid.
///
/// Rooms are added one by one, each sharing a wall with an existing room, so all rooms are connected.
/// Rooms don't overlap, but neighbors share their walls. Space between rooms can stay empty. Every two
/// rooms that share a straight piece of wall get one opening in it, at a random tile of that piece.
///
/// \throws std::runtime_error if the grid is too small for a room.
RoomPlan plan_rooms(const RoomLayoutConfig& config, LevelRng& rng);

/// Plans a level of several layers, connected by holes in the floors between them.
///
/// Every layer is planned with plan_rooms(). Then every two neighboring layers get a random number of
/// holes between `config.min_holes` and `config.max_holes`. A hole lies inside one room of the lower layer
/// and one room of the upper layer, doesn't share a side with any opening in either layer, and every
/// room has at most one hole in its floor or ceiling. If the holes don't fit, all layers are planned
/// again. The number of attempts is logged, as a warning when more than two were needed.
///
/// The player starts in the center of a random room without a hole in its floor. All such rooms have the
/// same chance, so layers with more rooms are more likely.
///
/// \throws std::invalid_argument if the settings are invalid, e.g. min_holes > max_holes.
/// \throws std::runtime_error if the grid is too small for a room, or the holes don't fit after
///         `config.max_plan_attempts` attempts.
LayeredPlan plan_layers(const RoomLayoutConfig& config, LevelRng& rng);

/// Draws the plan as text: each layer from the bottom up, one character per tile, with +y up and +x right.
///
/// Uses `#` for walls, `.` for floor, `+` for openings, `O` for holes to the layer below or above, and `@`
/// for the player start. Empty tiles around the rooms of all layers are cropped.
std::string draw_plan(const LayeredPlan& plan);

/// Layers of rooms on a grid of square tiles, connected by openings in their walls and by holes in the
/// floors between the layers.
///
/// Each wall tile becomes a non-rotated block from the floor to the ceiling of its layer. The floors
/// between layers are made of thin blocks, one per tile, except at holes and where both layers are empty.
/// The player starts in the center of a random room, see plan_layers(). The plan is drawn as text, see
/// draw_plan().
class RoomLayout final : public LevelLayout {
public:
    explicit RoomLayout(const RoomLayoutConfig& config) : config_(config) {}

    /// \copydoc LevelLayout::generate
    /// \throws std::runtime_error also if the tiles are too narrow for the clearance.
    Level generate(float clearance, LevelRng& rng) const override;

private:
    RoomLayoutConfig config_;
};

}  // namespace manifold
