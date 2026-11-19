#include "level/debug_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "level/level.hpp"
#include "util/random.hpp"
#include "world/geometry.hpp"
#include "world/obstacle.hpp"
#include "world/torch.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// Tries random positions and rotations for a footprint until it fits into the level and `fits` accepts it.
///
/// \param half_size Half of the footprint's side lengths.
/// \param max_yaw   Rotations are uniform in [0, max_yaw], or zero if max_yaw is zero.
/// \param fits      Called with a candidate footprint, returns whether it may be placed there.
/// \return The footprint, or nothing if no attempt fitted.
template <typename Fits>
std::optional<RotatedRectangle> place_randomly(const DebugLayoutConfig& config, const glm::vec2& half_size,
                                               float max_yaw, LevelRng& rng, Fits fits) {
    for (int attempt = 0; attempt < config.max_attempts_per_block; ++attempt) {
        RotatedRectangle footprint{.half_size = half_size, .yaw = random_float(rng, 0.0f, max_yaw)};
        // Keep the box around the rotated footprint inside the level.
        const glm::vec2 extent = bounding_half_size(footprint);
        if (config.x_max - config.x_min < 2.0f * extent.x || config.y_max - config.y_min < 2.0f * extent.y) {
            continue;
        }
        footprint.center = {random_float(rng, config.x_min + extent.x, config.x_max - extent.x),
                            random_float(rng, config.y_min + extent.y, config.y_max - extent.y)};
        if (fits(footprint)) {
            return footprint;
        }
    }
    return std::nullopt;
}

/// Places `count` square blocks of the given height with their bottom at `base_z`.
///
/// \throws std::runtime_error if a block doesn't fit.
void place_blocks(const DebugLayoutConfig& config, const Circle& keep_clear, float base_z, float height, World& world,
                  LevelRng& rng) {
    // A square is symmetric under 90-degree rotation, so yaw in [0, pi/2) covers all orientations.
    const float max_yaw = std::numbers::pi_v<float> / 2.0f;
    const glm::vec2 half_size(config.obstacle_size / 2.0f);
    for (int i = 0; i < config.obstacle_count; ++i) {
        const std::optional<RotatedRectangle> footprint =
            place_randomly(config, half_size, max_yaw, rng,
                           [&](const RotatedRectangle& candidate) { return !intersects(candidate, keep_clear); });
        if (!footprint) {
            throw std::runtime_error("level generation failed: no room for an obstacle");
        }
        world.add_obstacle({.base_center = {footprint->center, base_z},
                            .half_size = half_size,
                            .height = height,
                            .yaw = footprint->yaw});
    }
}

/// Places the ramps, away from the start, the obstacles, the ceiling blocks and each other.
///
/// \throws std::runtime_error if a ramp doesn't fit.
void place_ramps(const DebugLayoutConfig& config, const Circle& keep_clear, World& world, LevelRng& rng) {
    // A ramp goes up in one direction only, so all rotations differ.
    const float max_yaw = 2.0f * std::numbers::pi_v<float>;
    const glm::vec2 half_size{config.ramp_length / 2.0f, config.ramp_width / 2.0f};
    const auto fits = [&](const RotatedRectangle& candidate) {
        const auto clear_of = [&candidate](const auto& thing) { return !intersects(candidate, thing.footprint()); };
        return !intersects(candidate, keep_clear) && std::ranges::all_of(world.obstacles(), clear_of) &&
               std::ranges::all_of(world.ramps(), clear_of);
    };
    for (int i = 0; i < config.ramp_count; ++i) {
        const std::optional<RotatedRectangle> footprint = place_randomly(config, half_size, max_yaw, rng, fits);
        if (!footprint) {
            throw std::runtime_error("level generation failed: no room for a ramp");
        }
        world.add_ramp({.base_center = {footprint->center, 0.0f},
                        .half_length = half_size.x,
                        .half_width = half_size.y,
                        .rise = config.obstacle_height + config.ceiling_thickness,
                        .yaw = footprint->yaw});
    }
}

/// Places the level exit at random, away from the start, the obstacles at its height and the ramps.
///
/// \throws std::runtime_error if the exit doesn't fit.
void place_exit(const DebugLayoutConfig& config, const Circle& keep_clear, World& world, LevelRng& rng) {
    const auto fits = [&](const RotatedRectangle& candidate) {
        const LevelExit exit{.floor_point = {candidate.center, 0.0f}};
        const float bottom = exit.base_center().z;
        const auto blocks = [&](const Obstacle& obstacle) {
            return obstacle.base_center.z <= bottom + LevelExit::kSize &&
                   obstacle.base_center.z + obstacle.height >= bottom && intersects(candidate, obstacle.footprint());
        };
        const auto ramp_blocks = [&](const Ramp& ramp) { return intersects(candidate, ramp.footprint()); };
        return !intersects(candidate, keep_clear) && !std::ranges::any_of(world.obstacles(), blocks) &&
               !std::ranges::any_of(world.ramps(), ramp_blocks);
    };
    // The exit is not rotated.
    const std::optional<RotatedRectangle> footprint =
        place_randomly(config, glm::vec2(LevelExit::kSize / 2.0f), 0.0f, rng, fits);
    if (!footprint) {
        throw std::runtime_error("level generation failed: no room for the exit");
    }
    world.set_exit({.floor_point = {footprint->center, 0.0f}});
}

/// Checks whether an obstacle or a ramp is in the way of a torch: too close to its handle or flame.
bool torch_blocked(const World& world, const Torch& torch) {
    // A box around the handle and the flame, from the mount up to the top of the flame.
    constexpr float kClearance = 0.3f;
    const glm::vec3 middle = (torch.mount + torch.tip()) / 2.0f;
    const Circle around{.center = glm::vec2(middle), .radius = kClearance};
    const float bottom = torch.mount.z;
    const float top = torch.flame_bottom().z + kFlameHeight;
    const auto blocks = [&](const Obstacle& obstacle) {
        return obstacle.base_center.z < top && obstacle.base_center.z + obstacle.height > bottom &&
               intersects(obstacle.footprint(), around);
    };
    const auto ramp_blocks = [&](const Ramp& ramp) { return intersects(ramp.footprint(), around); };
    return std::ranges::any_of(world.obstacles(), blocks) || std::ranges::any_of(world.ramps(), ramp_blocks);
}

/// Hangs torches with the given flame on the four level walls, see DebugLayout.
void place_torches(const DebugLayoutConfig& config, TorchFlame flame, World& world) {
    const WorldBounds& bounds = world.bounds();
    const float pi = std::numbers::pi_v<float>;
    // Each wall: a start corner, the direction along the wall, its length and the direction into the level.
    struct Wall {
        glm::vec2 start;
        glm::vec2 along;
        float length;
        float yaw;
    };
    const float width = bounds.x_max - bounds.x_min;
    const float depth = bounds.y_max - bounds.y_min;
    const std::array<Wall, 4> walls{{
        {.start = {bounds.x_min, bounds.y_min}, .along = {1.0f, 0.0f}, .length = width, .yaw = pi / 2.0f},
        {.start = {bounds.x_max, bounds.y_min}, .along = {0.0f, 1.0f}, .length = depth, .yaw = pi},
        {.start = {bounds.x_max, bounds.y_max}, .along = {-1.0f, 0.0f}, .length = width, .yaw = -pi / 2.0f},
        {.start = {bounds.x_min, bounds.y_max}, .along = {0.0f, -1.0f}, .length = depth, .yaw = 0.0f},
    }};
    for (const Wall& wall : walls) {
        // The torches are centered on the wall, so the gaps at both ends are equal.
        const auto count = static_cast<int>(std::floor(wall.length / config.torch_spacing));
        const float first = (wall.length - (static_cast<float>(count - 1) * config.torch_spacing)) / 2.0f;
        for (int i = 0; i < count; ++i) {
            const glm::vec2 point =
                wall.start + ((first + (static_cast<float>(i) * config.torch_spacing)) * wall.along);
            const Torch torch{.mount = {point, config.torch_height}, .yaw = wall.yaw, .flame = flame};
            if (!torch_blocked(world, torch)) {
                world.add_torch(torch);
            }
        }
    }
}

/// Converts radians to degrees, for the level info.
float degrees(float radians) { return radians * 180.0f / std::numbers::pi_v<float>; }

/// Lists the obstacles, ceiling blocks, ramps and the exit, one per line.
std::string describe(const World& world) {
    std::string info;
    for (const Obstacle& obstacle : world.obstacles()) {
        info += std::format("{:13} center ({:6.2f}, {:6.2f}, {:4.2f}), size {} x {} m, yaw {:5.1f} deg\n",
                            obstacle.base_center.z > 0.0f ? "Ceiling block" : "Obstacle", obstacle.base_center.x,
                            obstacle.base_center.y, obstacle.base_center.z, 2.0f * obstacle.half_size.x,
                            2.0f * obstacle.half_size.y, degrees(obstacle.yaw));
    }
    for (const Ramp& ramp : world.ramps()) {
        info += std::format("{:13} center ({:6.2f}, {:6.2f}, {:4.2f}), up towards {:5.1f} deg\n", "Ramp",
                            ramp.base_center.x, ramp.base_center.y, ramp.base_center.z, degrees(ramp.yaw));
    }
    if (const std::optional<LevelExit>& exit = world.exit()) {
        info += std::format("{:13} center ({:6.2f}, {:6.2f}, {:4.2f}) on the floor\n", "Exit", exit->floor_point.x,
                            exit->floor_point.y, exit->floor_point.z);
    }
    return info;
}

}  // namespace

WorldBounds DebugLayoutConfig::bounds() const {
    return {.x_min = x_min,
            .x_max = x_max,
            .y_min = y_min,
            .y_max = y_max,
            .height = (5.0f * obstacle_height) + (4.0f * ceiling_thickness)};
}

Level DebugLayout::generate(float clearance, LevelRng& rng) const {
    const WorldBounds bounds = config_.bounds();
    const glm::vec3 start{(bounds.x_min + bounds.x_max) / 2.0f, (bounds.y_min + bounds.y_max) / 2.0f, 0.0f};
    const Circle keep_clear{.center = {start.x, start.y}, .radius = clearance};

    World world(bounds);
    place_blocks(config_, keep_clear, 0.0f, config_.obstacle_height, world, rng);
    // Ceiling blocks are above the obstacles, so they may be placed anywhere, whatever is below them.
    place_blocks(config_, keep_clear, config_.obstacle_height, config_.ceiling_thickness, world, rng);
    place_ramps(config_, keep_clear, world, rng);
    place_torches(config_, choose_torch_flame(rng), world);
    place_exit(config_, keep_clear, world, rng);
    std::string info = describe(world);
    return {.layout = LayoutKind::kDebug, .world = std::move(world), .player_start = start, .info = std::move(info)};
}

}  // namespace manifold
