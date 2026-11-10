#include "level/room_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
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

/// Finds the room that could get a hole with its first tile at (x, y).
///
/// \return Index of the room, or -1 if the hole isn't inside one room or touches an opening.
int room_for_hole(const RoomPlan& plan, int x, int y, int size) {
    const int room = plan.room_at(x, y);
    if (room < 0) {
        return -1;
    }
    for (int dy = 0; dy < size; ++dy) {
        for (int dx = 0; dx < size; ++dx) {
            if (plan.room_at(x + dx, y + dy) != room || near_opening(plan, x + dx, y + dy)) {
                return -1;
            }
        }
    }
    return room;
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
    for (int y = 0; y + config.hole_size <= below.height; ++y) {
        for (int x = 0; x + config.hole_size <= below.width; ++x) {
            if (room_for_hole(below, x, y, config.hole_size) >= 0 &&
                room_for_hole(above, x, y, config.hole_size) >= 0) {
                candidates.push_back({.layer = lower, .x = x, .y = y});
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

/// Plans all layers and the holes between them once.
///
/// \return The plan, or nothing if the holes don't fit.
std::optional<LayeredPlan> try_plan_layers(const RoomLayoutConfig& config, LevelRng& rng) {
    LayeredPlan plan;
    plan.hole_size = config.hole_size;
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

    // A player starting above a hole would fall right away, so rooms with a hole in the floor are skipped.
    RoomFlags floor_hole;
    for (const RoomPlan& layer : plan.layers) {
        floor_hole.emplace_back(layer.rooms.size(), false);
    }
    for (const Hole& hole : plan.holes) {
        const auto upper = static_cast<std::size_t>(hole.layer) + 1;
        floor_hole.at(upper).at(static_cast<std::size_t>(plan.layers.at(upper).room_at(hole.x, hole.y))) = true;
    }
    std::vector<std::pair<int, std::size_t>> starts;
    for (std::size_t layer = 0; layer < plan.layers.size(); ++layer) {
        for (std::size_t room = 0; room < plan.layers[layer].rooms.size(); ++room) {
            if (!floor_hole[layer][room]) {
                starts.emplace_back(static_cast<int>(layer), room);
            }
        }
    }
    std::tie(plan.start_layer, plan.start_room) =
        starts.at(static_cast<std::size_t>(random_int(rng, 0, static_cast<int>(starts.size()) - 1)));
    return plan;
}

}  // namespace

RoomPlan::RoomPlan(int width, int height)
    : width(width),
      height(height),
      tiles(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), Tile::kEmpty),
      room_of_tile(tiles.size(), -1) {}

bool LayeredPlan::is_hole(int layer, int x, int y) const {
    return std::any_of(holes.begin(), holes.end(), [&](const Hole& hole) {
        return hole.layer == layer && x >= hole.x && x < hole.x + hole_size && y >= hole.y && y < hole.y + hole_size;
    });
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
    if (config.layer_count < 1 || config.hole_size < 1 || config.min_holes < 0 || config.min_holes > config.max_holes ||
        config.max_plan_attempts < 1) {
        throw std::invalid_argument("invalid room layout settings for layers and holes");
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
    const GridRoom& start = plan.layers.at(static_cast<std::size_t>(plan.start_layer)).rooms.at(plan.start_room);
    const int start_x = start.x + (start.width / 2);
    const int start_y = start.y + (start.height / 2);

    std::string text;
    for (std::size_t index = 0; index < plan.layers.size(); ++index) {
        const RoomPlan& layer = plan.layers[index];
        const auto layer_number = static_cast<int>(index);
        text += "Layer " + std::to_string(layer_number) + ":\n";
        for (int y = box.y_max; y >= box.y_min; --y) {
            for (int x = box.x_min; x <= box.x_max; ++x) {
                if (layer_number == plan.start_layer && x == start_x && y == start_y) {
                    text += '@';
                } else if (plan.is_hole(layer_number, x, y) || plan.is_hole(layer_number - 1, x, y)) {
                    text += 'O';
                } else {
                    text += tile_symbol(layer.tile(x, y));
                }
            }
            text += '\n';
        }
    }
    return text;
}

Level RoomLayout::generate(float clearance, LevelRng& rng) const {
    // The player starts in the center of a room, which is at least half a tile away from any wall.
    if (config_.tile_size / 2.0f <= clearance) {
        throw std::runtime_error("level generation failed: tiles are too small for the player");
    }
    const LayeredPlan plan = plan_layers(config_, rng);
    const RoomPlan& bottom = plan.layers.at(0);

    // The grid is centered on the origin. Each layer's floor is the top of the slab below it. Adding up
    // the heights, instead of multiplying, makes the floors match the slab tops exactly.
    const float tile = config_.tile_size;
    const float x_min = -static_cast<float>(bottom.width) * tile / 2.0f;
    const float y_min = -static_cast<float>(bottom.height) * tile / 2.0f;
    std::vector<float> floor_z{0.0f};
    while (floor_z.size() < plan.layers.size()) {
        floor_z.push_back(floor_z.back() + config_.height + config_.slab_thickness);
    }
    const float height = floor_z.back() + config_.height;
    const auto tile_center = [&](int x, int y, float z) {
        return glm::vec3(x_min + ((static_cast<float>(x) + 0.5f) * tile),
                         y_min + ((static_cast<float>(y) + 0.5f) * tile), z);
    };

    World world({.x_min = x_min, .x_max = -x_min, .y_min = y_min, .y_max = -y_min, .height = height});
    for (std::size_t index = 0; index < plan.layers.size(); ++index) {
        const RoomPlan& layer = plan.layers[index];
        for (int y = 0; y < layer.height; ++y) {
            for (int x = 0; x < layer.width; ++x) {
                if (layer.tile(x, y) == Tile::kWall) {
                    world.add_obstacle({.base_center = tile_center(x, y, floor_z[index]),
                                        .half_size = tile / 2.0f,
                                        .height = config_.height,
                                        .yaw = 0.0f});
                }
            }
        }
    }
    // The floor between two layers, except at holes and where neither layer has anything.
    for (std::size_t index = 0; index + 1 < plan.layers.size(); ++index) {
        const RoomPlan& below = plan.layers[index];
        const RoomPlan& above = plan.layers[index + 1];
        const float slab_z = floor_z[index] + config_.height;
        for (int y = 0; y < below.height; ++y) {
            for (int x = 0; x < below.width; ++x) {
                const bool used = below.tile(x, y) != Tile::kEmpty || above.tile(x, y) != Tile::kEmpty;
                if (used && !plan.is_hole(static_cast<int>(index), x, y)) {
                    world.add_obstacle({.base_center = tile_center(x, y, slab_z),
                                        .half_size = tile / 2.0f,
                                        .height = config_.slab_thickness,
                                        .yaw = 0.0f});
                }
            }
        }
    }

    const GridRoom& start_room = plan.layers.at(static_cast<std::size_t>(plan.start_layer)).rooms.at(plan.start_room);
    const glm::vec3 start{
        x_min + ((static_cast<float>(start_room.x) + (static_cast<float>(start_room.width) / 2.0f)) * tile),
        y_min + ((static_cast<float>(start_room.y) + (static_cast<float>(start_room.height) / 2.0f)) * tile),
        floor_z.at(static_cast<std::size_t>(plan.start_layer))};
    return {.layout = LayoutKind::kRooms, .world = std::move(world), .player_start = start, .plan = draw_plan(plan)};
}

}  // namespace manifold
