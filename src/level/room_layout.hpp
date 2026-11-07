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
    float tile_size = 1.5f;         ///< Side of a square tile, in metres.
    float height = 3.0f;            ///< Height of the ceiling, in metres.
    int min_room_size = 1;          ///< Smallest inside width or depth of a room, in tiles.
    int max_room_size = 5;          ///< Largest inside width or depth of a room, in tiles.
    int placement_attempts = 5000;  ///< How often to try placing another room. More attempts fill more space.
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

/// The plan of a room level on a grid of square tiles.
///
/// Tile (x, y) covers the square from (x, y) to (x + 1, y + 1) in tile units, so +y is "up" on the printed
/// plan.
struct RoomPlan {
    int width = 0;                  ///< Number of tiles along x.
    int height = 0;                 ///< Number of tiles along y.
    std::vector<Tile> tiles;        ///< Row by row, starting at y = 0.
    std::vector<int> room_of_tile;  ///< Index in `rooms` of the room each tile is inside, or -1.
    std::vector<GridRoom> rooms;    ///< All rooms.
    std::size_t start_room = 0;     ///< Index in `rooms` of the room where the player starts.

    /// Creates a grid with all tiles empty.
    RoomPlan(int width, int height);

    /// Checks whether the tile is inside the grid.
    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    /// The tile at (x, y). Tiles outside the grid are empty.
    Tile tile(int x, int y) const;
    /// Index of the room that tile (x, y) is inside, or -1 if there is none.
    int room_at(int x, int y) const;
};

/// Plans a level of rooms on a grid.
///
/// Rooms are added one by one, each sharing a wall with an existing room, so all rooms are connected.
/// Rooms don't overlap, but neighbors share their walls. Space between rooms can stay empty. Every two
/// rooms that share a straight piece of wall get one opening in it, at a random tile of that piece.
///
/// \throws std::runtime_error if the grid is too small for a room.
RoomPlan plan_rooms(const RoomLayoutConfig& config, LevelRng& rng);

/// Draws the plan as text, one character per tile, with +y up and +x right.
///
/// Uses `#` for walls, `.` for floor, `+` for openings and `@` for the player start. Empty tiles around the
/// rooms are cropped.
std::string draw_plan(const RoomPlan& plan);

/// Rooms on a grid of square tiles, connected by openings in their walls.
///
/// Each wall tile becomes a non-rotated block from the floor to the ceiling. The player starts in the
/// center of a random room. The plan is drawn as text, see draw_plan().
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
