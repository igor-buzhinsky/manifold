#include "level/room_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <queue>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "game.hpp"
#include "level/level.hpp"
#include "physics/collision.hpp"
#include "world/geometry.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

constexpr int kSeeds = 20;

// A small grid keeps the tests fast. Holes rarely fit into it, so it needs many attempts.
RoomLayoutConfig make_config() {
    return {.grid_width = 40, .grid_height = 30, .placement_attempts = 2000, .max_plan_attempts = 1000};
}

RoomPlan make_plan(std::uint64_t seed, const RoomLayoutConfig& config = make_config()) {
    LevelRng rng(seed);
    return plan_rooms(config, rng);
}

bool is_open(Tile tile) { return tile == Tile::kFloor || tile == Tile::kOpening; }

TEST(RoomPlanTest, RoomsHaveConfiguredSizeAndFitIntoGrid) {
    const RoomLayoutConfig config = make_config();
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const RoomPlan plan = make_plan(seed);
        EXPECT_GT(plan.rooms.size(), 10U) << "seed " << seed;
        for (const GridRoom& room : plan.rooms) {
            EXPECT_GE(room.width, config.min_room_size);
            EXPECT_LE(room.width, config.max_room_size);
            EXPECT_GE(room.height, config.min_room_size);
            EXPECT_LE(room.height, config.max_room_size);
            // The walls must fit into the grid too.
            EXPECT_GE(room.x, 1);
            EXPECT_GE(room.y, 1);
            EXPECT_LE(room.x + room.width, plan.width - 1);
            EXPECT_LE(room.y + room.height, plan.height - 1);
        }
    }
}

TEST(RoomPlanTest, RoomInsidesAreFloorAndSurroundedByWalls) {
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const RoomPlan plan = make_plan(seed);
        for (std::size_t i = 0; i < plan.rooms.size(); ++i) {
            const GridRoom& room = plan.rooms[i];
            for (int y = room.y - 1; y <= room.y + room.height; ++y) {
                for (int x = room.x - 1; x <= room.x + room.width; ++x) {
                    const bool inside =
                        x >= room.x && x < room.x + room.width && y >= room.y && y < room.y + room.height;
                    if (inside) {
                        EXPECT_EQ(plan.tile(x, y), Tile::kFloor);
                        EXPECT_EQ(plan.room_at(x, y), static_cast<int>(i));
                    } else {
                        EXPECT_TRUE(plan.tile(x, y) == Tile::kWall || plan.tile(x, y) == Tile::kOpening);
                    }
                }
            }
        }
    }
}

TEST(RoomPlanTest, FloorTilesBelongToRooms) {
    const RoomPlan plan = make_plan(1);
    for (int y = 0; y < plan.height; ++y) {
        for (int x = 0; x < plan.width; ++x) {
            EXPECT_EQ(plan.tile(x, y) == Tile::kFloor, plan.room_at(x, y) >= 0);
        }
    }
}

/// Counts the open tiles reachable from the first room by walking between neighboring open tiles.
std::size_t count_reachable(const RoomPlan& plan) {
    std::vector<bool> visited(plan.tiles.size(), false);
    std::queue<std::pair<int, int>> queue;
    const GridRoom& start = plan.rooms.front();
    queue.emplace(start.x, start.y);
    visited[(static_cast<std::size_t>(start.y) * static_cast<std::size_t>(plan.width)) +
            static_cast<std::size_t>(start.x)] = true;
    std::size_t count = 0;
    while (!queue.empty()) {
        const auto [x, y] = queue.front();
        queue.pop();
        ++count;
        for (const auto& [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
            const int nx = x + dx;
            const int ny = y + dy;
            if (!is_open(plan.tile(nx, ny))) {
                continue;
            }
            const std::size_t index =
                (static_cast<std::size_t>(ny) * static_cast<std::size_t>(plan.width)) + static_cast<std::size_t>(nx);
            if (!visited[index]) {
                visited[index] = true;
                queue.emplace(nx, ny);
            }
        }
    }
    return count;
}

TEST(RoomPlanTest, AllRoomsAreReachable) {
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const RoomPlan plan = make_plan(seed);
        const auto open_tiles = static_cast<std::size_t>(std::count_if(plan.tiles.begin(), plan.tiles.end(), is_open));
        EXPECT_EQ(count_reachable(plan), open_tiles) << "seed " << seed;
    }
}

/// Openings found in a plan, grouped by the rooms they connect.
struct OpeningCount {
    /// For every pair of rooms with a wall between them: how many openings that wall has.
    std::map<std::pair<int, int>, int> per_room_pair;
    /// Openings that don't connect two different rooms.
    int stray = 0;
};

OpeningCount count_openings(const RoomPlan& plan) {
    OpeningCount count;
    for (int y = 0; y < plan.height; ++y) {
        for (int x = 0; x < plan.width; ++x) {
            const Tile tile = plan.tile(x, y);
            if (tile != Tile::kWall && tile != Tile::kOpening) {
                continue;
            }
            bool between_rooms = false;
            for (const auto& [dx, dy] : {std::pair{1, 0}, std::pair{0, 1}}) {
                const int first = plan.room_at(x - dx, y - dy);
                const int second = plan.room_at(x + dx, y + dy);
                if (first >= 0 && second >= 0 && first != second) {
                    between_rooms = true;
                    count.per_room_pair[std::minmax(first, second)] += tile == Tile::kOpening ? 1 : 0;
                }
            }
            count.stray += tile == Tile::kOpening && !between_rooms ? 1 : 0;
        }
    }
    return count;
}

TEST(RoomPlanTest, EveryTwoNeighboringRoomsHaveExactlyOneOpening) {
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const OpeningCount count = count_openings(make_plan(seed));
        EXPECT_EQ(count.stray, 0) << "seed " << seed;
        EXPECT_FALSE(count.per_room_pair.empty());
        for (const auto& [rooms, openings] : count.per_room_pair) {
            EXPECT_EQ(openings, 1) << "seed " << seed << ", rooms " << rooms.first << " and " << rooms.second;
        }
    }
}

TEST(RoomPlanTest, SomeSpaceStaysEmpty) {
    const RoomPlan plan = make_plan(1);
    EXPECT_GT(std::count(plan.tiles.begin(), plan.tiles.end(), Tile::kEmpty), 0);
}

TEST(RoomPlanTest, SameSeedGivesSamePlan) {
    const RoomPlan first = make_plan(7);
    const RoomPlan second = make_plan(7);
    EXPECT_EQ(first.tiles, second.tiles);
    EXPECT_NE(first.tiles, make_plan(8).tiles);
}

TEST(RoomPlanTest, ThrowsWhenGridIsTooSmall) {
    RoomLayoutConfig config = make_config();
    config.grid_width = 2;
    EXPECT_THROW(make_plan(1, config), std::runtime_error);
}

LayeredPlan make_layered_plan(std::uint64_t seed, const RoomLayoutConfig& config = make_config()) {
    LevelRng rng(seed);
    return plan_layers(config, rng);
}

/// Calls `visit(x, y)` for every tile of a hole.
template <typename Visit>
void for_each_hole_tile(const Hole& hole, Visit visit) {
    for (int y = hole.y; y < hole.y + hole.height; ++y) {
        for (int x = hole.x; x < hole.x + hole.width; ++x) {
            visit(x, y);
        }
    }
}

TEST(LayeredPlanTest, HasConfiguredLayersAndHoleCounts) {
    const RoomLayoutConfig config = make_config();
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const LayeredPlan plan = make_layered_plan(seed);
        ASSERT_EQ(plan.layers.size(), static_cast<std::size_t>(config.layer_count));
        EXPECT_GE(plan.attempts, 1);
        for (int lower = 0; lower + 1 < config.layer_count; ++lower) {
            const auto count = std::count_if(plan.holes.begin(), plan.holes.end(),
                                             [lower](const Hole& hole) { return hole.layer == lower; });
            EXPECT_GE(count, config.min_holes) << "seed " << seed;
            EXPECT_LE(count, config.max_holes) << "seed " << seed;
        }
    }
}

TEST(LayeredPlanTest, HolesLieInsideOneRoomOfBothLayersAwayFromOpenings) {
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const LayeredPlan plan = make_layered_plan(seed);
        for (const Hole& hole : plan.holes) {
            for (const std::size_t index :
                 {static_cast<std::size_t>(hole.layer), static_cast<std::size_t>(hole.layer) + 1}) {
                const RoomPlan& layer = plan.layers.at(index);
                const int room = layer.room_at(hole.x, hole.y);
                EXPECT_GE(room, 0);
                for_each_hole_tile(hole, [&](int x, int y) {
                    EXPECT_EQ(layer.room_at(x, y), room);
                    for (const auto& [dx, dy] :
                         {std::pair{0, 0}, std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
                        EXPECT_NE(layer.tile(x + dx, y + dy), Tile::kOpening) << "seed " << seed;
                    }
                });
            }
        }
    }
}

TEST(LayeredPlanTest, HolesAreOneByTwoTilesInBothDirections) {
    const RoomLayoutConfig config = make_config();
    std::set<std::pair<int, int>> shapes;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        for (const Hole& hole : make_layered_plan(seed).holes) {
            shapes.insert({hole.width, hole.height});
        }
    }
    EXPECT_EQ(shapes, (std::set<std::pair<int, int>>{{1, config.hole_length}, {config.hole_length, 1}}));
}

/// Checks whether the floor tiles of a room that are not in `hole` are connected.
bool rest_of_room_is_connected(const RoomPlan& layer, int room, const Hole& hole) {
    std::vector<std::pair<int, int>> free;
    for (int y = 0; y < layer.height; ++y) {
        for (int x = 0; x < layer.width; ++x) {
            if (layer.room_at(x, y) == room && !hole.contains(x, y)) {
                free.emplace_back(x, y);
            }
        }
    }
    if (free.empty()) {
        return false;
    }
    std::set<std::pair<int, int>> reached{free.front()};
    std::queue<std::pair<int, int>> queue;
    queue.push(free.front());
    while (!queue.empty()) {
        const auto [x, y] = queue.front();
        queue.pop();
        for (const auto& [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
            const std::pair next{x + dx, y + dy};
            if (layer.room_at(next.first, next.second) == room && !hole.contains(next.first, next.second) &&
                reached.insert(next).second) {
                queue.push(next);
            }
        }
    }
    return reached.size() == free.size();
}

TEST(LayeredPlanTest, HolesDontBlockTheWayThroughRooms) {
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const LayeredPlan plan = make_layered_plan(seed);
        for (const Hole& hole : plan.holes) {
            for (const int index : {hole.layer, hole.layer + 1}) {
                const RoomPlan& layer = plan.layers.at(static_cast<std::size_t>(index));
                EXPECT_TRUE(rest_of_room_is_connected(layer, layer.room_at(hole.x, hole.y), hole))
                    << "seed " << seed << ", layer " << index;
            }
        }
    }
}

/// The tiles where a ramp under `hole` starts (in the lower layer) and ends (in the upper layer).
std::pair<std::pair<int, int>, std::pair<int, int>> ramp_ends(const Hole& hole) {
    const std::pair<int, int> before = hole.along_x() ? std::pair{hole.x - 1, hole.y} : std::pair{hole.x, hole.y - 1};
    const std::pair<int, int> after =
        hole.along_x() ? std::pair{hole.x + hole.width, hole.y} : std::pair{hole.x, hole.y + hole.height};
    return hole.ramp == RampRise::kForward ? std::pair{before, after} : std::pair{after, before};
}

TEST(LayeredPlanTest, RampCountsAreWithinSettings) {
    const RoomLayoutConfig config = make_config();
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const LayeredPlan plan = make_layered_plan(seed);
        for (int lower = 0; lower + 1 < config.layer_count; ++lower) {
            const auto ramps = std::count_if(plan.holes.begin(), plan.holes.end(), [lower](const Hole& hole) {
                return hole.layer == lower && hole.ramp != RampRise::kNone;
            });
            EXPECT_GE(ramps, config.min_ramps) << "seed " << seed;
            EXPECT_LE(ramps, config.max_ramps) << "seed " << seed;
        }
    }
}

TEST(LayeredPlanTest, RampsLeadFromLowerRoomIntoUpperRoomInAllDirections) {
    std::set<std::pair<bool, RampRise>> directions;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const LayeredPlan plan = make_layered_plan(seed);
        for (const Hole& hole : plan.holes) {
            if (hole.ramp == RampRise::kNone) {
                continue;
            }
            directions.insert({hole.along_x(), hole.ramp});
            const RoomPlan& below = plan.layers.at(static_cast<std::size_t>(hole.layer));
            const RoomPlan& above = plan.layers.at(static_cast<std::size_t>(hole.layer) + 1);
            const auto [low, high] = ramp_ends(hole);
            EXPECT_EQ(below.room_at(low.first, low.second), below.room_at(hole.x, hole.y)) << "seed " << seed;
            EXPECT_EQ(above.room_at(high.first, high.second), above.room_at(hole.x, hole.y)) << "seed " << seed;
        }
    }
    EXPECT_EQ(directions.size(), 4U);
}

TEST(LayeredPlanTest, ThrowsOnImpossibleRampCounts) {
    RoomLayoutConfig config = make_config();
    config.min_ramps = 5;  // More than max_ramps.
    EXPECT_THROW(make_layered_plan(1, config), std::invalid_argument);
    config.max_ramps = 6;  // Now more than min_holes, so some levels could not have enough ramps.
    EXPECT_THROW(make_layered_plan(1, config), std::invalid_argument);
    config.min_holes = 5;
    EXPECT_NO_THROW(make_layered_plan(1, config));
}

TEST(LayeredPlanTest, EveryRoomHasAtMostOneHole) {
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const LayeredPlan plan = make_layered_plan(seed);
        std::map<std::pair<int, int>, int> holes_per_room;  // (layer, room) -> holes
        for (const Hole& hole : plan.holes) {
            for (const int layer : {hole.layer, hole.layer + 1}) {
                ++holes_per_room[{layer, plan.layers.at(static_cast<std::size_t>(layer)).room_at(hole.x, hole.y)}];
            }
        }
        for (const auto& [room, count] : holes_per_room) {
            EXPECT_EQ(count, 1) << "seed " << seed << ", layer " << room.first << ", room " << room.second;
        }
    }
}

TEST(LayeredPlanTest, StartIsOnFloorTileWithoutHoleFacingNoWall) {
    std::set<int> start_layers;
    std::set<int> directions;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const LayeredPlan plan = make_layered_plan(seed);
        start_layers.insert(plan.start_layer);
        directions.insert(plan.start_direction);
        const RoomPlan& layer = plan.layers.at(static_cast<std::size_t>(plan.start_layer));
        EXPECT_EQ(layer.tile(plan.start_x, plan.start_y), Tile::kFloor) << "seed " << seed;
        EXPECT_FALSE(plan.is_hole(plan.start_layer, plan.start_x, plan.start_y)) << "seed " << seed;
        EXPECT_FALSE(plan.is_hole(plan.start_layer - 1, plan.start_x, plan.start_y)) << "seed " << seed;

        ASSERT_GE(plan.start_direction, 0);
        ASSERT_LT(plan.start_direction, 4);
        constexpr std::array<std::pair<int, int>, 4> kSteps{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
        const auto [step_x, step_y] = kSteps.at(static_cast<std::size_t>(plan.start_direction));
        EXPECT_NE(layer.tile(plan.start_x + step_x, plan.start_y + step_y), Tile::kWall) << "seed " << seed;
    }
    // The start layer and direction are random too.
    EXPECT_GT(start_layers.size(), 1U);
    EXPECT_GT(directions.size(), 1U);
}

TEST(LayeredPlanTest, StartFacesAnyDirectionWhenWallsAreAllAround) {
    // A single room of one tile has walls on all sides and no openings.
    RoomLayoutConfig config = make_config();
    config.layer_count = 1;
    config.min_room_size = 1;
    config.max_room_size = 1;
    config.placement_attempts = 0;
    config.min_holes = 0;
    config.max_holes = 0;
    config.min_ramps = 0;
    config.max_ramps = 0;
    std::set<int> directions;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        directions.insert(make_layered_plan(seed, config).start_direction);
    }
    EXPECT_EQ(directions.size(), 4U);
}

TEST(LayeredPlanTest, SameSeedGivesSamePlan) {
    const LayeredPlan first = make_layered_plan(7);
    const LayeredPlan second = make_layered_plan(7);
    EXPECT_EQ(draw_plan(first), draw_plan(second));
    EXPECT_NE(draw_plan(first), draw_plan(make_layered_plan(8)));
}

TEST(LayeredPlanTest, SingleLayerHasNoHoles) {
    RoomLayoutConfig config = make_config();
    config.layer_count = 1;
    const LayeredPlan plan = make_layered_plan(1, config);
    EXPECT_EQ(plan.layers.size(), 1U);
    EXPECT_TRUE(plan.holes.empty());
    EXPECT_EQ(plan.start_layer, 0);
    EXPECT_EQ(plan.attempts, 1);
}

TEST(LayeredPlanTest, ThrowsWhenHolesNeverFit) {
    // Rooms of one tile have no space for a hole of 1 x 2 tiles.
    RoomLayoutConfig config = make_config();
    config.max_room_size = 1;
    config.max_plan_attempts = 3;
    EXPECT_THROW(make_layered_plan(1, config), std::runtime_error);
}

TEST(LayeredPlanTest, ThrowsOnInvalidHoleCounts) {
    RoomLayoutConfig config = make_config();
    config.min_holes = 4;
    config.max_holes = 3;
    EXPECT_THROW(make_layered_plan(1, config), std::invalid_argument);
}

TEST(LayeredPlanTest, DrawingShowsAllLayersWithStartAndHoles) {
    const LayeredPlan plan = make_layered_plan(3);
    const std::string text = draw_plan(plan);
    EXPECT_EQ(std::count(text.begin(), text.end(), '@'), 1);
    for (int layer = 0; layer < static_cast<int>(plan.layers.size()); ++layer) {
        EXPECT_NE(text.find("Layer " + std::to_string(layer) + ":\n"), std::string::npos);
    }
    // Every hole shows in the layer below and the layer above. The start is never on a hole.
    std::ptrdiff_t hole_tiles = 0;
    for (const Hole& hole : plan.holes) {
        hole_tiles += static_cast<std::ptrdiff_t>(hole.width) * hole.height;
    }
    const auto hole_symbols = std::count_if(
        text.begin(), text.end(), [](char c) { return c == 'O' || c == '>' || c == '<' || c == '^' || c == 'v'; });
    EXPECT_EQ(hole_symbols, 2 * hole_tiles);
    // All plan lines are equally long.
    std::size_t line_start = 0;
    std::optional<std::size_t> width;
    while (line_start < text.size()) {
        const std::size_t line_end = text.find('\n', line_start);
        ASSERT_NE(line_end, std::string::npos);
        if (text.compare(line_start, 6, "Layer ") != 0) {
            const std::size_t length = line_end - line_start;
            if (width) {
                EXPECT_EQ(length, *width);
            }
            width = length;
        }
        line_start = line_end + 1;
    }
}

Level generate(std::uint64_t seed, const RoomLayoutConfig& config = make_config()) {
    LevelRng rng(seed);
    return RoomLayout(config).generate(0.6f, rng);
}

/// Counts the tiles that get a slab block between layer `lower` and the layer above.
std::size_t count_slab_tiles(const LayeredPlan& plan, int lower) {
    const RoomPlan& below = plan.layers.at(static_cast<std::size_t>(lower));
    const RoomPlan& above = plan.layers.at(static_cast<std::size_t>(lower) + 1);
    std::size_t count = 0;
    for (int y = 0; y < below.height; ++y) {
        for (int x = 0; x < below.width; ++x) {
            const bool used = below.tile(x, y) != Tile::kEmpty || above.tile(x, y) != Tile::kEmpty;
            count += used && !plan.is_hole(lower, x, y) ? 1 : 0;
        }
    }
    return count;
}

TEST(RoomLayoutTest, WallTilesAndFloorsBetweenLayersBecomeBlocks) {
    const RoomLayoutConfig config = make_config();
    const Level level = generate(5);
    const LayeredPlan plan = make_layered_plan(5);  // The same seed gives the same plan as inside generate().
    EXPECT_EQ(level.layout, LayoutKind::kRooms);

    const float layer_step = config.height + config.slab_thickness;
    std::size_t walls = 0;
    std::size_t slabs = 0;
    for (const Obstacle& obstacle : level.world.obstacles()) {
        EXPECT_FLOAT_EQ(obstacle.half_size, config.tile_size / 2.0f);
        EXPECT_FLOAT_EQ(obstacle.yaw, 0.0f);
        const float layer = obstacle.base_center.z / layer_step;
        if (obstacle.height == config.height) {
            // Walls stand on the floor of a layer.
            EXPECT_NEAR(layer, std::round(layer), 1e-5f);
            ++walls;
        } else {
            EXPECT_FLOAT_EQ(obstacle.height, config.slab_thickness);
            EXPECT_NEAR(obstacle.base_center.z + config.slab_thickness, std::ceil(layer) * layer_step, 1e-4f);
            ++slabs;
        }
    }
    std::size_t expected_walls = 0;
    for (const RoomPlan& layer : plan.layers) {
        expected_walls += static_cast<std::size_t>(std::count(layer.tiles.begin(), layer.tiles.end(), Tile::kWall));
    }
    EXPECT_EQ(walls, expected_walls);
    EXPECT_EQ(slabs, count_slab_tiles(plan, 0) + count_slab_tiles(plan, 1));

    const WorldBounds& bounds = level.world.bounds();
    EXPECT_FLOAT_EQ(bounds.x_max - bounds.x_min, static_cast<float>(config.grid_width) * config.tile_size);
    EXPECT_FLOAT_EQ(bounds.y_max - bounds.y_min, static_cast<float>(config.grid_height) * config.tile_size);
    EXPECT_FLOAT_EQ(bounds.height, (3.0f * config.height) + (2.0f * config.slab_thickness));
}

TEST(RoomLayoutTest, PlayerStartsInCenterOfStartTileWithoutCollisions) {
    const RoomLayoutConfig config = make_config();
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(seed);
        const LayeredPlan plan = make_layered_plan(seed);
        const WorldBounds& bounds = level.world.bounds();
        EXPECT_FLOAT_EQ(level.player_start.x,
                        bounds.x_min + ((static_cast<float>(plan.start_x) + 0.5f) * config.tile_size));
        EXPECT_FLOAT_EQ(level.player_start.y,
                        bounds.y_min + ((static_cast<float>(plan.start_y) + 0.5f) * config.tile_size));
        EXPECT_FLOAT_EQ(level.player_start.z,
                        static_cast<float>(plan.start_layer) * (config.height + config.slab_thickness));
        EXPECT_FLOAT_EQ(level.player_yaw, static_cast<float>(plan.start_direction) * std::numbers::pi_v<float> / 2.0f);

        const VerticalCylinder body{.base = level.player_start, .radius = 0.6f, .height = 1.8f};
        for (const Obstacle& obstacle : level.world.obstacles()) {
            EXPECT_FALSE(find_contact(body, obstacle).has_value()) << "seed " << seed;
        }
        // The player stands on something: the level floor or a slab.
        const VerticalCylinder lowered{
            .base = level.player_start - glm::vec3(0.0f, 0.0f, 0.01f), .radius = 0.6f, .height = 1.8f};
        EXPECT_TRUE(find_contact(lowered, bounds).has_value() ||
                    std::any_of(level.world.obstacles().begin(), level.world.obstacles().end(),
                                [&](const Obstacle& obstacle) { return find_contact(lowered, obstacle).has_value(); }))
            << "seed " << seed;
    }
}

TEST(RoomLayoutTest, PlayerFallsThroughHoleToLayerBelow) {
    const RoomLayoutConfig config = make_config();
    const Level level = generate(2);
    const LayeredPlan plan = make_layered_plan(2);
    ASSERT_FALSE(plan.holes.empty());
    const Hole& hole = plan.holes.back();
    const WorldBounds& bounds = level.world.bounds();
    const float layer_step = config.height + config.slab_thickness;
    VerticalCylinder body{
        .base = {bounds.x_min +
                     ((static_cast<float>(hole.x) + (static_cast<float>(hole.width) / 2.0f)) * config.tile_size),
                 bounds.y_min +
                     ((static_cast<float>(hole.y) + (static_cast<float>(hole.height) / 2.0f)) * config.tile_size),
                 static_cast<float>(hole.layer + 1) * layer_step},
        .radius = 0.6f,
        .height = 1.8f};
    for (int frame = 0; frame < 100; ++frame) {
        body.base = move_and_slide(level.world, body, {0.0f, 0.0f, -0.1f}).position;
    }
    EXPECT_NEAR(body.base.z, static_cast<float>(hole.layer) * layer_step, 1e-4f);
}

TEST(RoomLayoutTest, RampsFillHolesFromFloorToFloor) {
    const RoomLayoutConfig config = make_config();
    const Level level = generate(4);
    const LayeredPlan plan = make_layered_plan(4);
    const auto ramp_holes = std::count_if(plan.holes.begin(), plan.holes.end(),
                                          [](const Hole& hole) { return hole.ramp != RampRise::kNone; });
    EXPECT_EQ(level.world.ramps().size(), static_cast<std::size_t>(ramp_holes));
    const float layer_step = config.height + config.slab_thickness;
    for (const Ramp& ramp : level.world.ramps()) {
        EXPECT_FLOAT_EQ(ramp.half_length, static_cast<float>(config.hole_length) * config.tile_size / 2.0f);
        EXPECT_FLOAT_EQ(ramp.half_width, config.tile_size / 2.0f);
        EXPECT_NEAR(ramp.rise, layer_step, 1e-5f);
        const float layer = ramp.base_center.z / layer_step;
        EXPECT_NEAR(layer, std::round(layer), 1e-5f);
    }
}

TEST(RoomLayoutTest, PlayerWalksUpGeneratedRamps) {
    const RoomLayoutConfig config = make_config();
    const LayeredPlan plan = make_layered_plan(6);
    const float layer_step = config.height + config.slab_thickness;
    int ramps = 0;
    for (const Hole& hole : plan.holes) {
        if (hole.ramp == RampRise::kNone) {
            continue;
        }
        ++ramps;
        // Start in the middle of the tile at the low end, facing up the ramp.
        Level level = generate(6);
        const WorldBounds& bounds = level.world.bounds();
        const auto [low, high] = ramp_ends(hole);
        level.player_start = {bounds.x_min + ((static_cast<float>(low.first) + 0.5f) * config.tile_size),
                              bounds.y_min + ((static_cast<float>(low.second) + 0.5f) * config.tile_size),
                              static_cast<float>(hole.layer) * layer_step};
        level.player_yaw =
            std::atan2(static_cast<float>(high.second - low.second), static_cast<float>(high.first - low.first));
        Game game(GameConfig{}, std::move(level));
        for (int frame = 0; frame < 180; ++frame) {
            game.update({.forward = 1.0f}, 1.0f / 60.0f);
        }
        EXPECT_NEAR(game.player().position().z, static_cast<float>(hole.layer + 1) * layer_step, 1e-3f)
            << "hole at " << hole.x << ", " << hole.y << " in layer " << hole.layer;
        EXPECT_TRUE(game.player().on_ground());
    }
    EXPECT_GT(ramps, 0);
}

/// Checks whether a point is inside an obstacle of a world.
bool inside_obstacle(const World& world, const glm::vec3& point) {
    const VerticalCylinder speck{.base = point, .radius = 0.01f, .height = 0.01f};
    return std::any_of(world.obstacles().begin(), world.obstacles().end(),
                       [&](const Obstacle& obstacle) { return find_contact(speck, obstacle).has_value(); });
}

TEST(RoomLayoutTest, TorchesHangOnWallsFacingRooms) {
    const RoomLayoutConfig config = make_config();
    const float layer_step = config.height + config.slab_thickness;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(seed);
        const World& world = level.world;
        EXPECT_FALSE(world.torches().empty()) << "seed " << seed;
        for (const Torch& torch : world.torches()) {
            const float layer = std::round(torch.mount.z / layer_step);
            EXPECT_NEAR(torch.mount.z - (layer * layer_step), config.torch_height, 1e-4f) << "seed " << seed;
            // A wall is right behind the mount, and the handle and flame are in the open.
            EXPECT_TRUE(inside_obstacle(world, torch.mount - (0.1f * torch.outward()))) << "seed " << seed;
            EXPECT_FALSE(inside_obstacle(world, torch.mount + (0.05f * torch.axis()))) << "seed " << seed;
            EXPECT_FALSE(inside_obstacle(world, torch.tip())) << "seed " << seed;
            EXPECT_FALSE(inside_obstacle(world, torch.flame_bottom() + glm::vec3(0.0f, 0.0f, kFlameHeight)))
                << "seed " << seed;
            // No ramp runs along the wall below the torch.
            const Circle around{.center = glm::vec2(torch.tip()), .radius = 0.1f};
            for (const Ramp& ramp : world.ramps()) {
                const bool same_layer = std::abs(ramp.base_center.z - (layer * layer_step)) < 1e-3f;
                EXPECT_FALSE(same_layer && intersects(ramp.footprint(), around)) << "seed " << seed;
            }
        }
    }
}

TEST(RoomLayoutTest, TorchChanceControlsTorchCount) {
    RoomLayoutConfig config = make_config();
    config.min_torch_chance = 0.0f;
    config.max_torch_chance = 0.0f;
    EXPECT_TRUE(generate(3, config).world.torches().empty());
    config.min_torch_chance = 1.0f;
    config.max_torch_chance = 1.0f;
    const std::size_t all = generate(3, config).world.torches().size();
    config.min_torch_chance = 0.1f;
    config.max_torch_chance = 0.1f;
    const std::size_t some = generate(3, config).world.torches().size();
    EXPECT_GT(some, all / 20);
    EXPECT_LT(some, all / 5);
}

TEST(RoomLayoutTest, TorchCountVariesBetweenLevelsWithinChances) {
    const RoomLayoutConfig config = make_config();  // Default chances: 5% to 10%.
    std::size_t lowest = 1000000;
    std::size_t highest = 0;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        RoomLayoutConfig every_side = config;
        every_side.min_torch_chance = 1.0f;
        every_side.max_torch_chance = 1.0f;
        const auto sides = static_cast<float>(generate(seed, every_side).world.torches().size());
        const auto torches = static_cast<float>(generate(seed, config).world.torches().size());
        // A bit outside of [5%, 10%], because the sides get torches at random.
        EXPECT_GT(torches, 0.03f * sides) << "seed " << seed;
        EXPECT_LT(torches, 0.13f * sides) << "seed " << seed;
        const auto share = static_cast<std::size_t>(1000.0f * torches / sides);
        lowest = std::min(lowest, share);
        highest = std::max(highest, share);
    }
    // The share of sides with a torch differs between levels.
    EXPECT_GT(highest - lowest, 20U);
}

TEST(RoomLayoutTest, AllTorchesOfALevelHaveTheSameFlameAndAllFlamesOccur) {
    std::set<TorchFlame> flames;
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(seed);
        const std::span<const Torch> torches = level.world.torches();
        ASSERT_FALSE(torches.empty());
        for (const Torch& torch : torches) {
            EXPECT_EQ(torch.flame, torches[0].flame) << "seed " << seed;
        }
        flames.insert(torches[0].flame);
    }
    EXPECT_EQ(flames.size(), 3U);
}

TEST(RoomLayoutTest, ThrowsOnInvalidTorchChances) {
    RoomLayoutConfig config = make_config();
    config.min_torch_chance = 0.2f;
    config.max_torch_chance = 0.1f;
    EXPECT_THROW(generate(1, config), std::invalid_argument);
    config.min_torch_chance = 0.5f;
    config.max_torch_chance = 1.5f;
    EXPECT_THROW(generate(1, config), std::invalid_argument);
    config.min_torch_chance = -0.1f;
    config.max_torch_chance = 0.1f;
    EXPECT_THROW(generate(1, config), std::invalid_argument);
    config.min_torch_chance = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(generate(1, config), std::invalid_argument);
}

TEST(RoomLayoutTest, ThrowsWhenTilesAreTooSmallForClearance) {
    LevelRng rng(1);
    EXPECT_THROW(RoomLayout(make_config()).generate(1.0f, rng), std::runtime_error);
}

}  // namespace
}  // namespace manifold
