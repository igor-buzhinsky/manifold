#include "level/room_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

#include "level/level.hpp"
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

}  // namespace

RoomPlan::RoomPlan(int width, int height)
    : width(width),
      height(height),
      tiles(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), Tile::kEmpty),
      room_of_tile(tiles.size(), -1) {}

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
    plan.start_room = static_cast<std::size_t>(random_int(rng, 0, static_cast<int>(plan.rooms.size()) - 1));
    return plan;
}

std::string draw_plan(const RoomPlan& plan) {
    // Crop to the tiles that are used.
    int x_min = plan.width;
    int x_max = -1;
    int y_min = plan.height;
    int y_max = -1;
    for (int y = 0; y < plan.height; ++y) {
        for (int x = 0; x < plan.width; ++x) {
            if (plan.tile(x, y) != Tile::kEmpty) {
                x_min = std::min(x_min, x);
                x_max = std::max(x_max, x);
                y_min = std::min(y_min, y);
                y_max = std::max(y_max, y);
            }
        }
    }

    const GridRoom& start = plan.rooms.at(plan.start_room);
    const int start_x = start.x + (start.width / 2);
    const int start_y = start.y + (start.height / 2);

    std::string text;
    for (int y = y_max; y >= y_min; --y) {
        for (int x = x_min; x <= x_max; ++x) {
            if (x == start_x && y == start_y) {
                text += '@';
                continue;
            }
            switch (plan.tile(x, y)) {
                case Tile::kEmpty:
                    text += ' ';
                    break;
                case Tile::kFloor:
                    text += '.';
                    break;
                case Tile::kWall:
                    text += '#';
                    break;
                case Tile::kOpening:
                    text += '+';
                    break;
            }
        }
        text += '\n';
    }
    return text;
}

Level RoomLayout::generate(float clearance, LevelRng& rng) const {
    // The player starts in the center of a room, which is at least half a tile away from any wall.
    if (config_.tile_size / 2.0f <= clearance) {
        throw std::runtime_error("level generation failed: tiles are too small for the player");
    }
    const RoomPlan plan = plan_rooms(config_, rng);

    // The grid is centered on the origin.
    const float tile = config_.tile_size;
    const float x_min = -static_cast<float>(plan.width) * tile / 2.0f;
    const float y_min = -static_cast<float>(plan.height) * tile / 2.0f;
    World world({.x_min = x_min, .x_max = -x_min, .y_min = y_min, .y_max = -y_min, .height = config_.height});
    for (int y = 0; y < plan.height; ++y) {
        for (int x = 0; x < plan.width; ++x) {
            if (plan.tile(x, y) == Tile::kWall) {
                world.add_obstacle({.base_center = {x_min + ((static_cast<float>(x) + 0.5f) * tile),
                                                    y_min + ((static_cast<float>(y) + 0.5f) * tile), 0.0f},
                                    .half_size = tile / 2.0f,
                                    .height = config_.height,
                                    .yaw = 0.0f});
            }
        }
    }

    const GridRoom& start_room = plan.rooms.at(plan.start_room);
    const glm::vec3 start{
        x_min + ((static_cast<float>(start_room.x) + (static_cast<float>(start_room.width) / 2.0f)) * tile),
        y_min + ((static_cast<float>(start_room.y) + (static_cast<float>(start_room.height) / 2.0f)) * tile), 0.0f};
    return {.layout = LayoutKind::kRooms, .world = std::move(world), .player_start = start, .plan = draw_plan(plan)};
}

}  // namespace manifold
