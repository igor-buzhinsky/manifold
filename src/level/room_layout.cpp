#include "level/room_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <numbers>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "level/level.hpp"
#include "util/log.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// A tile position on the grid.
struct TilePosition {
    int x = 0;
    int y = 0;
};

/// Index of tile (x, y) in RoomPlan::tiles. The tile must be inside the grid.
std::size_t tile_index(const RoomPlan& plan, int x, int y) {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(plan.width)) + static_cast<std::size_t>(x);
}

/// Calls `visit(x, y)` for every inside tile of a room.
template <typename Visit>
void for_each_inside_tile(const GridRoom& room, Visit visit) {
    for (int y = room.y; y < room.y + room.height; ++y) {
        for (int x = room.x; x < room.x + room.width; ++x) {
            visit(x, y);
        }
    }
}

/// Calls `visit(x, y)` for every wall tile of a room, including the corners.
template <typename Visit>
void for_each_wall_tile(const GridRoom& room, Visit visit) {
    for (int x = room.x - 1; x <= room.x + room.width; ++x) {
        visit(x, room.y - 1);
        visit(x, room.y + room.height);
    }
    for (int y = room.y; y < room.y + room.height; ++y) {
        visit(room.x - 1, y);
        visit(room.x + room.width, y);
    }
}

/// Checks whether a room fits: its walls are inside the grid, its inside is empty, and its walls don't
/// cover the floor of other rooms. Walls may overlap the walls of other rooms.
bool can_place(const RoomPlan& plan, const GridRoom& room) {
    if (room.x < 1 || room.y < 1 || room.x + room.width > plan.width - 1 || room.y + room.height > plan.height - 1) {
        return false;
    }
    bool fits = true;
    for_each_inside_tile(room, [&](int x, int y) { fits = fits && plan.tile(x, y) == Tile::kEmpty; });
    for_each_wall_tile(room, [&](int x, int y) { fits = fits && plan.tile(x, y) != Tile::kFloor; });
    return fits;
}

/// Checks whether a room that is not placed yet could get an opening to an existing room: some wall tile
/// of it, except the corners, has the floor of another room right behind it.
bool touches_existing_room(const RoomPlan& plan, const GridRoom& room) {
    for (int y = room.y; y < room.y + room.height; ++y) {
        if (plan.tile(room.x - 2, y) == Tile::kFloor || plan.tile(room.x + room.width + 1, y) == Tile::kFloor) {
            return true;
        }
    }
    for (int x = room.x; x < room.x + room.width; ++x) {
        if (plan.tile(x, room.y - 2) == Tile::kFloor || plan.tile(x, room.y + room.height + 1) == Tile::kFloor) {
            return true;
        }
    }
    return false;
}

/// Adds a room to the plan. The room must fit, see can_place().
void place_room(RoomPlan& plan, const GridRoom& room) {
    const auto room_index = static_cast<int>(plan.rooms.size());
    plan.rooms.push_back(room);
    for_each_inside_tile(room, [&](int x, int y) {
        plan.tiles[tile_index(plan, x, y)] = Tile::kFloor;
        plan.room_of_tile[tile_index(plan, x, y)] = room_index;
    });
    for_each_wall_tile(room, [&](int x, int y) { plan.tiles[tile_index(plan, x, y)] = Tile::kWall; });
}

/// Returns a uniformly distributed integer in [min, max].
int random_int(LevelRng& rng, int min, int max) { return std::uniform_int_distribution<int>(min, max)(rng); }

/// Turns one random wall tile between every two neighboring rooms into an opening.
///
/// A wall tile can become an opening if the tiles on its two opposite sides are inside two different rooms.
void add_openings(RoomPlan& plan, LevelRng& rng) {
    // An ordered map keeps the order of random choices, and so the level, the same for the same seed.
    std::map<std::pair<int, int>, std::vector<TilePosition>> candidates;
    for (int y = 0; y < plan.height; ++y) {
        for (int x = 0; x < plan.width; ++x) {
            if (plan.tile(x, y) != Tile::kWall) {
                continue;
            }
            for (const TilePosition step : {TilePosition{.x = 1, .y = 0}, TilePosition{.x = 0, .y = 1}}) {
                const int first = plan.room_at(x - step.x, y - step.y);
                const int second = plan.room_at(x + step.x, y + step.y);
                if (first >= 0 && second >= 0 && first != second) {
                    candidates[std::minmax(first, second)].push_back({.x = x, .y = y});
                }
            }
        }
    }
    for (const auto& [rooms, tiles] : candidates) {
        const TilePosition chosen =
            tiles[static_cast<std::size_t>(random_int(rng, 0, static_cast<int>(tiles.size()) - 1))];
        plan.tiles[tile_index(plan, chosen.x, chosen.y)] = Tile::kOpening;
    }
}

/// Checks whether tile (x, y) or one of the four tiles that share a side with it is an opening.
bool near_opening(const RoomPlan& plan, int x, int y) {
    return plan.tile(x, y) == Tile::kOpening || plan.tile(x - 1, y) == Tile::kOpening ||
           plan.tile(x + 1, y) == Tile::kOpening || plan.tile(x, y - 1) == Tile::kOpening ||
           plan.tile(x, y + 1) == Tile::kOpening;
}

/// Checks whether the floor of a room without the tiles of `hole` is still connected and not empty.
bool stays_connected(const GridRoom& room, const Hole& hole) {
    const auto index = [&room](int x, int y) {
        return (static_cast<std::size_t>(y - room.y) * static_cast<std::size_t>(room.width)) +
               static_cast<std::size_t>(x - room.x);
    };
    const auto is_free = [&](int x, int y) {
        return x >= room.x && x < room.x + room.width && y >= room.y && y < room.y + room.height &&
               !hole.contains(x, y);
    };

    // Flood fill from the first free tile, then check that all free tiles were reached.
    std::vector<bool> reached(static_cast<std::size_t>(room.width) * static_cast<std::size_t>(room.height), false);
    std::vector<TilePosition> stack;
    int free_tiles = 0;
    for_each_inside_tile(room, [&](int x, int y) {
        if (is_free(x, y)) {
            ++free_tiles;
            if (free_tiles == 1) {
                reached[index(x, y)] = true;
                stack.push_back({.x = x, .y = y});
            }
        }
    });
    int reached_tiles = 0;
    while (!stack.empty()) {
        const TilePosition tile = stack.back();
        stack.pop_back();
        ++reached_tiles;
        for (const TilePosition step : {TilePosition{.x = 1, .y = 0}, TilePosition{.x = -1, .y = 0},
                                        TilePosition{.x = 0, .y = 1}, TilePosition{.x = 0, .y = -1}}) {
            const int x = tile.x + step.x;
            const int y = tile.y + step.y;
            if (is_free(x, y) && !reached[index(x, y)]) {
                reached[index(x, y)] = true;
                stack.push_back({.x = x, .y = y});
            }
        }
    }
    return free_tiles > 0 && reached_tiles == free_tiles;
}

/// Finds the room of a layer that could get `hole`.
///
/// \return Index of the room, or -1 if the hole isn't inside one room, touches an opening, or would cut the
///         room's floor into parts.
int room_for_hole(const RoomPlan& plan, const Hole& hole) {
    const int room = plan.room_at(hole.x, hole.y);
    if (room < 0) {
        return -1;
    }
    for (int y = hole.y; y < hole.y + hole.height; ++y) {
        for (int x = hole.x; x < hole.x + hole.width; ++x) {
            if (plan.room_at(x, y) != room || near_opening(plan, x, y)) {
                return -1;
            }
        }
    }
    return stays_connected(plan.rooms.at(static_cast<std::size_t>(room)), hole) ? room : -1;
}

/// Marks which rooms of each layer have a hole in their floor or ceiling.
using RoomFlags = std::vector<std::vector<bool>>;

/// Adds random holes between layer `lower` and the layer above it.
///
/// \param plan             The plan with all layers. Holes are added to it.
/// \param lower            Index of the lower layer.
/// \param config           Settings with the hole size and the number of holes.
/// \param[out] has_hole    Rooms that have a hole. Rooms with one get no further holes.
/// \param rng              Random number generator.
/// \return False if fewer than `config.min_holes` holes fit.
bool add_holes(LayeredPlan& plan, int lower, const RoomLayoutConfig& config, RoomFlags& has_hole, LevelRng& rng) {
    const auto lower_index = static_cast<std::size_t>(lower);
    const RoomPlan& below = plan.layers.at(lower_index);
    const RoomPlan& above = plan.layers.at(lower_index + 1);
    std::vector<Hole> candidates;
    for (const bool along_x : {true, false}) {
        const int width = along_x ? config.hole_length : 1;
        const int height = along_x ? 1 : config.hole_length;
        for (int y = 0; y + height <= below.height; ++y) {
            for (int x = 0; x + width <= below.width; ++x) {
                const Hole hole{.layer = lower, .x = x, .y = y, .width = width, .height = height};
                if (room_for_hole(below, hole) >= 0 && room_for_hole(above, hole) >= 0) {
                    candidates.push_back(hole);
                }
            }
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), rng);

    const int wanted = random_int(rng, config.min_holes, config.max_holes);
    int added = 0;
    for (const Hole& hole : candidates) {
        if (added == wanted) {
            break;
        }
        auto&& below_flag = has_hole.at(lower_index).at(static_cast<std::size_t>(below.room_at(hole.x, hole.y)));
        auto&& above_flag = has_hole.at(lower_index + 1).at(static_cast<std::size_t>(above.room_at(hole.x, hole.y)));
        if (below_flag || above_flag) {
            continue;
        }
        below_flag = true;
        above_flag = true;
        plan.holes.push_back(hole);
        ++added;
    }
    return added >= config.min_holes;
}

/// A rectangle of tiles, including its last row and column.
struct TileBox {
    int x_min = 0;
    int x_max = -1;
    int y_min = 0;
    int y_max = -1;
};

/// The smallest box that contains all non-empty tiles of all layers.
TileBox used_tiles(const LayeredPlan& plan) {
    TileBox box{.x_min = plan.layers.at(0).width, .y_min = plan.layers.at(0).height};
    for (const RoomPlan& layer : plan.layers) {
        for (int y = 0; y < layer.height; ++y) {
            for (int x = 0; x < layer.width; ++x) {
                if (layer.tile(x, y) != Tile::kEmpty) {
                    box = {.x_min = std::min(box.x_min, x),
                           .x_max = std::max(box.x_max, x),
                           .y_min = std::min(box.y_min, y),
                           .y_max = std::max(box.y_max, y)};
                }
            }
        }
    }
    return box;
}

/// The hole in the floor or the ceiling of `layer` at tile (x, y), if any.
std::optional<Hole> find_hole(const LayeredPlan& plan, int layer, int x, int y) {
    const std::optional<Hole> hole = plan.hole_at(layer, x, y);
    return hole ? hole : plan.hole_at(layer - 1, x, y);
}

/// The character that shows a hole on the printed plan: `O`, or an arrow pointing up its ramp.
char hole_symbol(const Hole& hole) {
    switch (hole.ramp) {
        case RampRise::kNone:
            return 'O';
        case RampRise::kForward:
            return hole.along_x() ? '>' : '^';
        case RampRise::kBackward:
            return hole.along_x() ? '<' : 'v';
    }
    return '?';
}

/// The character that shows a tile on the printed plan.
char tile_symbol(Tile tile) {
    switch (tile) {
        case Tile::kEmpty:
            return ' ';
        case Tile::kFloor:
            return '.';
        case Tile::kWall:
            return '#';
        case Tile::kOpening:
            return '+';
    }
    return '?';
}

/// The directions a ramp under `hole` could go up: its low end must lead into the hole's room in the lower
/// layer, and its high end into the hole's room in the upper layer.
std::vector<RampRise> possible_ramps(const LayeredPlan& plan, const Hole& hole) {
    const RoomPlan& below = plan.layers.at(static_cast<std::size_t>(hole.layer));
    const RoomPlan& above = plan.layers.at(static_cast<std::size_t>(hole.layer) + 1);
    const int lower_room = below.room_at(hole.x, hole.y);
    const int upper_room = above.room_at(hole.x, hole.y);
    // The tiles right before the hole and right after it, along its long side.
    const TilePosition before =
        hole.along_x() ? TilePosition{.x = hole.x - 1, .y = hole.y} : TilePosition{.x = hole.x, .y = hole.y - 1};
    const TilePosition after = hole.along_x() ? TilePosition{.x = hole.x + hole.width, .y = hole.y}
                                              : TilePosition{.x = hole.x, .y = hole.y + hole.height};
    std::vector<RampRise> possible;
    if (below.room_at(before.x, before.y) == lower_room && above.room_at(after.x, after.y) == upper_room) {
        possible.push_back(RampRise::kForward);
    }
    if (below.room_at(after.x, after.y) == lower_room && above.room_at(before.x, before.y) == upper_room) {
        possible.push_back(RampRise::kBackward);
    }
    return possible;
}

/// Puts ramps under random holes between layer `lower` and the layer above it.
///
/// \return False if fewer than `config.min_ramps` holes can have a ramp.
bool add_ramps(LayeredPlan& plan, int lower, const RoomLayoutConfig& config, LevelRng& rng) {
    std::vector<std::pair<Hole*, std::vector<RampRise>>> candidates;
    for (Hole& hole : plan.holes) {
        if (hole.layer == lower) {
            std::vector<RampRise> possible = possible_ramps(plan, hole);
            if (!possible.empty()) {
                candidates.emplace_back(&hole, std::move(possible));
            }
        }
    }
    if (static_cast<int>(candidates.size()) < config.min_ramps) {
        return false;
    }
    std::shuffle(candidates.begin(), candidates.end(), rng);
    const int wanted =
        std::min(random_int(rng, config.min_ramps, config.max_ramps), static_cast<int>(candidates.size()));
    for (int i = 0; i < wanted; ++i) {
        auto& [hole, possible] = candidates[static_cast<std::size_t>(i)];
        hole->ramp = possible[static_cast<std::size_t>(random_int(rng, 0, static_cast<int>(possible.size()) - 1))];
    }
    return true;
}

/// Plans all layers and the holes between them once.
///
/// \return The plan, or nothing if the holes don't fit.
std::optional<LayeredPlan> try_plan_layers(const RoomLayoutConfig& config, LevelRng& rng) {
    LayeredPlan plan;
    RoomFlags has_hole;
    for (int layer = 0; layer < config.layer_count; ++layer) {
        plan.layers.push_back(plan_rooms(config, rng));
        has_hole.emplace_back(plan.layers.back().rooms.size(), false);
    }
    for (int lower = 0; lower + 1 < config.layer_count; ++lower) {
        if (!add_holes(plan, lower, config, has_hole, rng)) {
            return std::nullopt;
        }
    }
    for (int lower = 0; lower + 1 < config.layer_count; ++lower) {
        if (!add_ramps(plan, lower, config, rng)) {
            return std::nullopt;
        }
    }

    // The player starts at the center of a random floor tile. Above a hole in the floor the player would
    // fall right away, and below a hole in the ceiling there may be a ramp, so tiles of holes are skipped.
    std::vector<std::pair<int, TilePosition>> starts;
    for (std::size_t index = 0; index < plan.layers.size(); ++index) {
        const RoomPlan& layer = plan.layers[index];
        const auto layer_number = static_cast<int>(index);
        for (int y = 0; y < layer.height; ++y) {
            for (int x = 0; x < layer.width; ++x) {
                if (layer.tile(x, y) == Tile::kFloor && !plan.is_hole(layer_number, x, y) &&
                    !plan.is_hole(layer_number - 1, x, y)) {
                    starts.emplace_back(layer_number, TilePosition{.x = x, .y = y});
                }
            }
        }
    }
    if (starts.empty()) {
        return std::nullopt;
    }
    const auto& [start_layer, start] =
        starts.at(static_cast<std::size_t>(random_int(rng, 0, static_cast<int>(starts.size()) - 1)));
    plan.start_layer = start_layer;
    plan.start_x = start.x;
    plan.start_y = start.y;

    // The player faces along x or y, but not right into a wall. If there are walls all around, any of the
    // four directions will do.
    constexpr std::array<TilePosition, 4> kSteps{
        {{.x = 1, .y = 0}, {.x = 0, .y = 1}, {.x = -1, .y = 0}, {.x = 0, .y = -1}}};
    const RoomPlan& start_plan = plan.layers.at(static_cast<std::size_t>(start_layer));
    std::vector<int> directions;
    for (int direction = 0; direction < static_cast<int>(kSteps.size()); ++direction) {
        const TilePosition& step = kSteps.at(static_cast<std::size_t>(direction));
        if (start_plan.tile(start.x + step.x, start.y + step.y) != Tile::kWall) {
            directions.push_back(direction);
        }
    }
    if (directions.empty()) {
        directions = {0, 1, 2, 3};
    }
    plan.start_direction =
        directions.at(static_cast<std::size_t>(random_int(rng, 0, static_cast<int>(directions.size()) - 1)));
    return plan;
}

}  // namespace

RoomPlan::RoomPlan(int width, int height)
    : width(width),
      height(height),
      tiles(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), Tile::kEmpty),
      room_of_tile(tiles.size(), -1) {}

std::optional<Hole> LayeredPlan::hole_at(int layer, int x, int y) const {
    const auto found = std::find_if(holes.begin(), holes.end(),
                                    [&](const Hole& hole) { return hole.layer == layer && hole.contains(x, y); });
    return found == holes.end() ? std::nullopt : std::optional<Hole>(*found);
}

Tile RoomPlan::tile(int x, int y) const { return contains(x, y) ? tiles[tile_index(*this, x, y)] : Tile::kEmpty; }

int RoomPlan::room_at(int x, int y) const { return contains(x, y) ? room_of_tile[tile_index(*this, x, y)] : -1; }

RoomPlan plan_rooms(const RoomLayoutConfig& config, LevelRng& rng) {
    RoomPlan plan(config.grid_width, config.grid_height);
    const auto random_size = [&] { return random_int(rng, config.min_room_size, config.max_room_size); };

    // The first room goes anywhere. A room needs one more tile on each side for its walls.
    GridRoom first{.width = random_size(), .height = random_size()};
    if (first.width + 2 > plan.width || first.height + 2 > plan.height) {
        throw std::runtime_error("level generation failed: the grid is too small for a room");
    }
    first.x = random_int(rng, 1, plan.width - 1 - first.width);
    first.y = random_int(rng, 1, plan.height - 1 - first.height);
    place_room(plan, first);

    // Every further room is tried next to a random existing room, so that the rooms stay connected.
    for (int attempt = 0; attempt < config.placement_attempts; ++attempt) {
        const GridRoom& anchor =
            plan.rooms[static_cast<std::size_t>(random_int(rng, 0, static_cast<int>(plan.rooms.size()) - 1))];
        GridRoom room{.width = random_size(), .height = random_size()};
        room.x = random_int(rng, anchor.x - room.width - 1, anchor.x + anchor.width + 1);
        room.y = random_int(rng, anchor.y - room.height - 1, anchor.y + anchor.height + 1);
        if (can_place(plan, room) && touches_existing_room(plan, room)) {
            place_room(plan, room);
        }
    }

    add_openings(plan, rng);
    return plan;
}

LayeredPlan plan_layers(const RoomLayoutConfig& config, LevelRng& rng) {
    if (config.layer_count < 1 || config.hole_length < 1 || config.min_holes < 0 ||
        config.min_holes > config.max_holes || config.max_plan_attempts < 1) {
        throw std::invalid_argument("invalid room layout settings for layers and holes");
    }
    // Every two neighboring layers get at least min_holes holes, so that many can have ramps.
    if (config.min_ramps < 0 || config.min_ramps > config.max_ramps || config.min_ramps > config.min_holes) {
        throw std::invalid_argument(
            "invalid room layout settings for ramps: need 0 <= min_ramps <= max_ramps and min_ramps <= min_holes");
    }
    for (int attempt = 1; attempt <= config.max_plan_attempts; ++attempt) {
        std::optional<LayeredPlan> plan = try_plan_layers(config, rng);
        if (!plan) {
            continue;
        }
        plan->attempts = attempt;
        if (attempt > 2) {
            log_warning("Room layout: the holes between layers fit only after {} attempts", attempt);
        } else {
            log_info("Room layout: planned in {} attempt(s)", attempt);
        }
        return std::move(*plan);
    }
    throw std::runtime_error("level generation failed: the holes between layers don't fit");
}

std::string draw_plan(const LayeredPlan& plan) {
    const TileBox box = used_tiles(plan);
    std::string text;
    for (std::size_t index = 0; index < plan.layers.size(); ++index) {
        const RoomPlan& layer = plan.layers[index];
        const auto layer_number = static_cast<int>(index);
        text += "Layer " + std::to_string(layer_number) + ":\n";
        for (int y = box.y_max; y >= box.y_min; --y) {
            for (int x = box.x_min; x <= box.x_max; ++x) {
                if (layer_number == plan.start_layer && x == plan.start_x && y == plan.start_y) {
                    text += '@';
                } else if (const std::optional<Hole> hole = find_hole(plan, layer_number, x, y)) {
                    text += hole_symbol(*hole);
                } else {
                    text += tile_symbol(layer.tile(x, y));
                }
            }
            text += '\n';
        }
    }
    return text;
}

namespace {

/// Where the tiles and the layers of a plan are in the world.
struct GridPlacement {
    float tile = 0.0f;           ///< Side of a tile, in metres.
    float x_min = 0.0f;          ///< World x of the grid's edge at tile x = 0.
    float y_min = 0.0f;          ///< World y of the grid's edge at tile y = 0.
    std::vector<float> floor_z;  ///< Height of each layer's floor.

    /// The world position of a point given in tiles, e.g. (x + 0.5, y + 0.5) for the center of tile (x, y).
    glm::vec3 at(float x, float y, float z) const { return {x_min + (x * tile), y_min + (y * tile), z}; }
    /// The world position of the center of tile (x, y), at height z.
    glm::vec3 tile_center(int x, int y, float z) const {
        return at(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, z);
    }
};

/// Adds a block from the floor to the ceiling for every wall tile of every layer.
void add_walls(World& world, const LayeredPlan& plan, const GridPlacement& grid, float height) {
    for (std::size_t index = 0; index < plan.layers.size(); ++index) {
        const RoomPlan& layer = plan.layers[index];
        for (int y = 0; y < layer.height; ++y) {
            for (int x = 0; x < layer.width; ++x) {
                if (layer.tile(x, y) == Tile::kWall) {
                    world.add_obstacle({.base_center = grid.tile_center(x, y, grid.floor_z[index]),
                                        .half_size = grid.tile / 2.0f,
                                        .height = height,
                                        .yaw = 0.0f});
                }
            }
        }
    }
}

/// Adds the floors between layers as thin blocks, except at holes and where neither layer has anything.
void add_slabs(World& world, const LayeredPlan& plan, const GridPlacement& grid, float height, float thickness) {
    for (std::size_t index = 0; index + 1 < plan.layers.size(); ++index) {
        const RoomPlan& below = plan.layers[index];
        const RoomPlan& above = plan.layers[index + 1];
        const float slab_z = grid.floor_z[index] + height;
        for (int y = 0; y < below.height; ++y) {
            for (int x = 0; x < below.width; ++x) {
                const bool used = below.tile(x, y) != Tile::kEmpty || above.tile(x, y) != Tile::kEmpty;
                if (used && !plan.is_hole(static_cast<int>(index), x, y)) {
                    world.add_obstacle({.base_center = grid.tile_center(x, y, slab_z),
                                        .half_size = grid.tile / 2.0f,
                                        .height = thickness,
                                        .yaw = 0.0f});
                }
            }
        }
    }
}

/// Adds the ramps. Each fills its hole in the lower layer, from its floor up to the floor of the layer above.
void build_ramps(World& world, const LayeredPlan& plan, const GridPlacement& grid) {
    const float pi = std::numbers::pi_v<float>;
    for (const Hole& hole : plan.holes) {
        if (hole.ramp == RampRise::kNone) {
            continue;
        }
        const auto lower = static_cast<std::size_t>(hole.layer);
        const float forward_yaw = hole.along_x() ? 0.0f : pi / 2.0f;
        world.add_ramp({.base_center = grid.at(static_cast<float>(hole.x) + (static_cast<float>(hole.width) / 2.0f),
                                               static_cast<float>(hole.y) + (static_cast<float>(hole.height) / 2.0f),
                                               grid.floor_z.at(lower)),
                        .half_length = static_cast<float>(std::max(hole.width, hole.height)) * grid.tile / 2.0f,
                        .half_width = grid.tile / 2.0f,
                        .rise = grid.floor_z.at(lower + 1) - grid.floor_z.at(lower),
                        .yaw = hole.ramp == RampRise::kForward ? forward_yaw : forward_yaw + pi});
    }
}

/// Hangs torches on the sides of wall tiles that face the floor of a room, see RoomLayout.
///
/// The torch chances must be valid, see RoomLayout::generate().
void add_torches(World& world, const LayeredPlan& plan, const GridPlacement& grid, const RoomLayoutConfig& config,
                 LevelRng& rng) {
    const TorchFlame flame = choose_torch_flame(rng);
    // uniform_real_distribution needs min < max, so equal chances are taken as they are.
    const float chance =
        config.min_torch_chance == config.max_torch_chance
            ? config.min_torch_chance
            : std::uniform_real_distribution<float>(config.min_torch_chance, config.max_torch_chance)(rng);
    std::bernoulli_distribution has_torch(chance);
    const std::array<TilePosition, 4> directions{
        {{.x = 1, .y = 0}, {.x = 0, .y = 1}, {.x = -1, .y = 0}, {.x = 0, .y = -1}}};
    for (std::size_t index = 0; index < plan.layers.size(); ++index) {
        const RoomPlan& layer = plan.layers[index];
        for (int y = 0; y < layer.height; ++y) {
            for (int x = 0; x < layer.width; ++x) {
                if (layer.tile(x, y) != Tile::kWall) {
                    continue;
                }
                for (const TilePosition& direction : directions) {
                    const int front_x = x + direction.x;
                    const int front_y = y + direction.y;
                    const std::optional<Hole> hole = plan.hole_at(static_cast<int>(index), front_x, front_y);
                    const bool ramp_in_front = hole && hole->ramp != RampRise::kNone;
                    if (layer.room_at(front_x, front_y) < 0 || ramp_in_front || !has_torch(rng)) {
                        continue;
                    }
                    const glm::vec3 outward{static_cast<float>(direction.x), static_cast<float>(direction.y), 0.0f};
                    const glm::vec3 floor_center = grid.tile_center(x, y, grid.floor_z[index]);
                    world.add_torch({.mount = floor_center + (outward * (grid.tile / 2.0f)) +
                                              glm::vec3(0.0f, 0.0f, config.torch_height),
                                     .yaw = std::atan2(outward.y, outward.x),
                                     .flame = flame});
                }
            }
        }
    }
}

}  // namespace

Level RoomLayout::generate(float clearance, LevelRng& rng) const {
    // The player starts in the center of a tile, which is at least half a tile away from any wall.
    if (config_.tile_size / 2.0f <= clearance) {
        throw std::runtime_error("level generation failed: tiles are too small for the player");
    }
    // NaN fails every comparison, so it is checked for separately.
    if (std::isnan(config_.min_torch_chance) || std::isnan(config_.max_torch_chance) ||
        config_.min_torch_chance < 0.0f || config_.max_torch_chance > 1.0f ||
        config_.min_torch_chance > config_.max_torch_chance) {
        throw std::invalid_argument("torch chances must be in [0, 1], the smallest first");
    }
    const LayeredPlan plan = plan_layers(config_, rng);
    const RoomPlan& bottom = plan.layers.at(0);

    // The grid is centered on the origin. Each layer's floor is the top of the slab below it. Adding up
    // the heights, instead of multiplying, makes the floors match the slab tops exactly.
    GridPlacement grid{.tile = config_.tile_size,
                       .x_min = -static_cast<float>(bottom.width) * config_.tile_size / 2.0f,
                       .y_min = -static_cast<float>(bottom.height) * config_.tile_size / 2.0f,
                       .floor_z = {0.0f}};
    while (grid.floor_z.size() < plan.layers.size()) {
        grid.floor_z.push_back(grid.floor_z.back() + config_.height + config_.slab_thickness);
    }

    World world({.x_min = grid.x_min,
                 .x_max = -grid.x_min,
                 .y_min = grid.y_min,
                 .y_max = -grid.y_min,
                 .height = grid.floor_z.back() + config_.height});
    add_walls(world, plan, grid, config_.height);
    add_slabs(world, plan, grid, config_.height, config_.slab_thickness);
    build_ramps(world, plan, grid);
    add_torches(world, plan, grid, config_, rng);

    const glm::vec3 start = grid.at(static_cast<float>(plan.start_x) + 0.5f, static_cast<float>(plan.start_y) + 0.5f,
                                    grid.floor_z.at(static_cast<std::size_t>(plan.start_layer)));
    return {.layout = LayoutKind::kRooms,
            .world = std::move(world),
            .player_start = start,
            .player_yaw = static_cast<float>(plan.start_direction) * std::numbers::pi_v<float> / 2.0f,
            .plan = draw_plan(plan)};
}

}  // namespace manifold
