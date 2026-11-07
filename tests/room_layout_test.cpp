#include "level/room_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "level/level.hpp"
#include "physics/collision.hpp"
#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

constexpr int kSeeds = 20;

RoomLayoutConfig make_config() { return {.grid_width = 40, .grid_height = 30, .placement_attempts = 2000}; }

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

/// Counts the open tiles reachable from the start room by walking between neighboring open tiles.
std::size_t count_reachable(const RoomPlan& plan) {
    std::vector<bool> visited(plan.tiles.size(), false);
    std::queue<std::pair<int, int>> queue;
    const GridRoom& start = plan.rooms[plan.start_room];
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

TEST(RoomPlanTest, AllRoomsAreReachableFromStart) {
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
    EXPECT_EQ(first.start_room, second.start_room);
    EXPECT_NE(first.tiles, make_plan(8).tiles);
}

TEST(RoomPlanTest, ThrowsWhenGridIsTooSmall) {
    RoomLayoutConfig config = make_config();
    config.grid_width = 2;
    EXPECT_THROW(make_plan(1, config), std::runtime_error);
}

TEST(RoomPlanTest, DrawingMarksStartAndUsesOneCharacterPerTile) {
    const RoomPlan plan = make_plan(3);
    const std::string text = draw_plan(plan);
    EXPECT_EQ(std::count(text.begin(), text.end(), '@'), 1);
    // All lines are equally long.
    std::size_t line_start = 0;
    std::optional<std::size_t> width;
    while (line_start < text.size()) {
        const std::size_t line_end = text.find('\n', line_start);
        ASSERT_NE(line_end, std::string::npos);
        const std::size_t length = line_end - line_start;
        if (width) {
            EXPECT_EQ(length, *width);
        }
        width = length;
        line_start = line_end + 1;
    }
    EXPECT_EQ(std::count(text.begin(), text.end(), '+'),
              std::count(plan.tiles.begin(), plan.tiles.end(), Tile::kOpening));
}

Level generate(std::uint64_t seed) {
    LevelRng rng(seed);
    return RoomLayout(make_config()).generate(0.3f, rng);
}

TEST(RoomLayoutTest, WallTilesBecomeBlocks) {
    const RoomLayoutConfig config = make_config();
    const Level level = generate(5);
    const RoomPlan plan = make_plan(5);  // The same seed gives the same plan as inside generate().
    EXPECT_EQ(level.layout, LayoutKind::kRooms);
    EXPECT_EQ(level.world.obstacles().size(),
              static_cast<std::size_t>(std::count(plan.tiles.begin(), plan.tiles.end(), Tile::kWall)));
    for (const Obstacle& obstacle : level.world.obstacles()) {
        EXPECT_FLOAT_EQ(obstacle.half_size, config.tile_size / 2.0f);
        EXPECT_FLOAT_EQ(obstacle.height, config.height);
        EXPECT_FLOAT_EQ(obstacle.yaw, 0.0f);
    }
    const WorldBounds& bounds = level.world.bounds();
    EXPECT_FLOAT_EQ(bounds.x_max - bounds.x_min, static_cast<float>(config.grid_width) * config.tile_size);
    EXPECT_FLOAT_EQ(bounds.y_max - bounds.y_min, static_cast<float>(config.grid_height) * config.tile_size);
    EXPECT_FLOAT_EQ(bounds.height, config.height);
}

TEST(RoomLayoutTest, PlayerStartsInCenterOfStartRoomWithoutCollisions) {
    const RoomLayoutConfig config = make_config();
    for (std::uint64_t seed = 0; seed < kSeeds; ++seed) {
        const Level level = generate(seed);
        const RoomPlan plan = make_plan(seed);
        const GridRoom& room = plan.rooms[plan.start_room];
        const WorldBounds& bounds = level.world.bounds();
        const float center_x =
            bounds.x_min + ((static_cast<float>(room.x) + (static_cast<float>(room.width) / 2.0f)) * config.tile_size);
        const float center_y =
            bounds.y_min + ((static_cast<float>(room.y) + (static_cast<float>(room.height) / 2.0f)) * config.tile_size);
        EXPECT_FLOAT_EQ(level.player_start.x, center_x);
        EXPECT_FLOAT_EQ(level.player_start.y, center_y);
        EXPECT_FLOAT_EQ(level.player_start.z, 0.0f);

        const VerticalCylinder body{.base = level.player_start, .radius = 0.3f, .height = 1.8f};
        for (const Obstacle& obstacle : level.world.obstacles()) {
            EXPECT_FALSE(find_contact(body, obstacle).has_value()) << "seed " << seed;
        }
    }
}

TEST(RoomLayoutTest, ThrowsWhenTilesAreTooSmallForClearance) {
    LevelRng rng(1);
    EXPECT_THROW(RoomLayout(make_config()).generate(1.0f, rng), std::runtime_error);
}

}  // namespace
}  // namespace manifold
