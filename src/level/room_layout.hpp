#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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
    /// Long side of a hole between two layers, in tiles. The short side is 1. A ramp under a hole is only
    /// walkable if it is longer than it is high.
    int hole_length = 2;
    int min_holes = 3;            ///< Fewest holes between every two neighboring layers.
    int max_holes = 6;            ///< Most holes between every two neighboring layers.
    int min_ramps = 2;            ///< Fewest holes with a ramp under them, between every two neighboring layers.
    int max_ramps = 4;            ///< Most holes with a ramp under them, between every two neighboring layers.
    int max_plan_attempts = 100;  ///< How often to plan the level again when the holes don't fit.
    /// Smallest chance that a side of a wall tile next to the floor of a room gets a torch, in [0, 1].
    float min_torch_chance = 0.05f;
    /// Largest chance that a side of a wall tile next to the floor of a room gets a torch, in [0, 1].
    float max_torch_chance = 0.1f;
    float torch_height = 1.6f;  ///< Height of a torch's mount above the floor of its layer, in metres.
    /// Smallest chance that an opening gets a door, in [0, 1].
    float min_door_chance = 0.1f;
    /// Largest chance that an opening gets a door, in [0, 1].
    float max_door_chance = 1.0f;
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

/// Which way a ramp under a hole goes up, if there is one.
enum class RampRise : std::uint8_t {
    kNone,      ///< No ramp: the hole is only a way down.
    kForward,   ///< Up towards +x or +y, along the long side of the hole.
    kBackward,  ///< Up towards -x or -y, along the long side of the hole.
};

/// A rectangular hole in the floor between two neighboring layers, possibly with a ramp under it.
///
/// A ramp fills the hole's tiles in the lower layer and goes up from the floor of the lower layer to the
/// floor of the upper layer.
struct Hole {
    int layer = 0;                    ///< The lower of the two layers. The hole is in its ceiling.
    int x = 0;                        ///< First tile along x.
    int y = 0;                        ///< First tile along y.
    int width = 1;                    ///< Size along x, in tiles.
    int height = 1;                   ///< Size along y, in tiles.
    RampRise ramp = RampRise::kNone;  ///< The ramp under the hole.

    /// Whether the long side of the hole runs along x.
    bool along_x() const { return width > height; }

    /// Checks whether tile (x, y) is part of the hole.
    bool contains(int tile_x, int tile_y) const {
        return tile_x >= x && tile_x < x + width && tile_y >= y && tile_y < y + height;
    }
};

/// The plan of a level with several layers of rooms on top of each other.
struct LayeredPlan {
    std::vector<RoomPlan> layers;  ///< From the bottom up.
    std::vector<Hole> holes;       ///< Holes between the layers.
    int start_layer = 0;           ///< Index in `layers` of the layer where the player starts.
    int start_x = 0;               ///< Tile of the player start along x.
    int start_y = 0;               ///< Tile of the player start along y.
    /// Direction the player faces at the start, in quarter turns counter-clockwise from +x: 0 is +x, 1 is +y,
    /// 2 is -x and 3 is -y.
    int start_direction = 0;
    int exit_layer = 0;  ///< Index in `layers` of the layer with the level exit.
    int exit_x = 0;      ///< Tile of the level exit along x.
    int exit_y = 0;      ///< Tile of the level exit along y.
    int attempts = 0;    ///< How many times the level was planned until the holes, the player start and the exit fit.

    /// The hole in the ceiling of `layer` that tile (x, y) is part of, if any.
    std::optional<Hole> hole_at(int layer, int x, int y) const;
    /// Checks whether tile (x, y) is part of a hole in the ceiling of `layer`.
    bool is_hole(int layer, int x, int y) const { return hole_at(layer, x, y).has_value(); }
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
/// holes between `config.min_holes` and `config.max_holes`. A hole is 1 tile wide and
/// `config.hole_length` tiles long, along x or y at random. It lies inside one room of the lower layer and
/// one room of the upper layer, and in both of them:
/// - it doesn't share a side with any opening;
/// - the rest of the room's floor stays connected, so the hole (or a ramp there later) doesn't block the
///   way through the room.
///
/// Every room has at most one hole in its floor or ceiling.
///
/// Then a random number of holes between `config.min_ramps` and `config.max_ramps`, chosen at random, get
/// a ramp. The low end of a ramp must lead into the room of the lower layer, and its high end into the room
/// of the upper layer. If both directions are possible, one is chosen at random.
///
/// If the holes or ramps don't fit, or there is no tile to start on or for the exit, all layers are planned again.
/// The number of attempts is logged, as a warning when more than two were needed.
///
/// The player starts in the center of a random floor tile that is not part of a hole, neither in its floor
/// nor in its ceiling (where a ramp may be). All such tiles have the same chance. The player faces along +x,
/// +y, -x or -y at random, but not a tile right next to the start that is a wall or part of a hole in its
/// floor or ceiling, unless all four neighbors are such tiles.
///
/// The level exit is in the center of a random floor tile of another layer than the start. The tile is not part
/// of a hole in its floor or ceiling, and doesn't share a side with an opening, so the exit doesn't block a way
/// between rooms. All such tiles have the same chance. With only one layer, the exit is on the start layer, but
/// not on the start tile.
///
/// \throws std::invalid_argument if the settings are invalid or can't be met, e.g. min_holes > max_holes,
///         or min_ramps > min_holes.
/// \throws std::runtime_error if the grid is too small for a room, or the holes, the player start or the exit don't
///         fit after `config.max_plan_attempts` attempts.
LayeredPlan plan_layers(const RoomLayoutConfig& config, LevelRng& rng);

/// Draws the plan as text: each layer from the bottom up, one character per tile, with +y up and +x right.
///
/// Uses `#` for walls, `.` for floor, `+` for openings, `O` for holes to the layer below or above, `>`,
/// `<`, `^` and `v` for ramps, pointing up the ramp, `@` for the player start and `E` for the level exit. Empty tiles
/// around the rooms of all layers are cropped.
std::string draw_plan(const LayeredPlan& plan);

/// Layers of rooms on a grid of square tiles, connected by openings in their walls and by holes in the
/// floors between the layers.
///
/// Each wall tile becomes a non-rotated block from the floor to the ceiling of its layer. The floors
/// between layers are made of thin blocks, one per tile, except at holes and where both layers are empty.
/// Ramps are wedges under their holes.
/// Torches hang in the middle of wall tile sides that face the floor of a room, but not where a ramp runs
/// along the wall. Each such side gets a torch with the same chance, chosen for the level uniformly from
/// [`min_torch_chance`, `max_torch_chance`]. All torches of a level have the same flame, see
/// choose_torch_flame().
/// Each opening gets a door with the same chance, chosen for the level uniformly from [`min_door_chance`,
/// `max_door_chance`]. A door fills its opening, with the wall running across the way between the rooms, see Door.
/// The player starts in the center of a random tile, facing a random direction, and the exit is in the center
/// of a random tile of another layer, see plan_layers(). The level info tells how tiles map to world coordinates,
/// where the player starts and where the exit is, how many openings have doors, and draws the plan, see draw_plan().
class RoomLayout final : public LevelLayout {
public:
    explicit RoomLayout(const RoomLayoutConfig& config) : config_(config) {}

    /// \copydoc LevelLayout::generate
    /// \throws std::runtime_error also if the tiles are too narrow for the clearance.
    /// \throws std::invalid_argument if the torch or door chances are not in [0, 1], or min_torch_chance >
    ///         max_torch_chance, or min_door_chance > max_door_chance.
    Level generate(float clearance, LevelRng& rng) const override;

private:
    RoomLayoutConfig config_;
};

}  // namespace manifold
