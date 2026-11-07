#include "level/debug_layout.hpp"

#include <cmath>
#include <format>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

#include <glm/vec3.hpp>

#include "level/level.hpp"
#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

/// Tries to place one obstacle at a random position with a random rotation.
///
/// \param[out] obstacle The placed obstacle, set only on success.
/// \return False if every attempt overlapped `keep_clear` or didn't fit into the bounds.
bool try_place_obstacle(const DebugLayoutConfig& config, const Circle& keep_clear, LevelRng& rng, Obstacle& obstacle) {
    const float half_size = config.obstacle_size / 2.0f;
    std::uniform_real_distribution<float> yaw_distribution(0.0f, std::numbers::pi_v<float> / 2.0f);

    for (int attempt = 0; attempt < config.max_attempts_per_obstacle; ++attempt) {
        // A square is symmetric under 90-degree rotation, so yaw in [0, pi/2) covers all orientations.
        const float yaw = yaw_distribution(rng);
        // Half-width of the rotated square's axis-aligned bounding box, to keep it inside the bounds.
        const float extent = half_size * (std::cos(yaw) + std::sin(yaw));
        const WorldBounds& bounds = config.bounds;
        if (bounds.x_max - bounds.x_min < 2.0f * extent || bounds.y_max - bounds.y_min < 2.0f * extent) {
            continue;
        }
        std::uniform_real_distribution<float> x_distribution(bounds.x_min + extent, bounds.x_max - extent);
        std::uniform_real_distribution<float> y_distribution(bounds.y_min + extent, bounds.y_max - extent);

        const Obstacle candidate{.base_center = {x_distribution(rng), y_distribution(rng), 0.0f},
                                 .half_size = half_size,
                                 .height = config.bounds.height,  // From the floor to the ceiling.
                                 .yaw = yaw};
        if (!intersects(candidate.footprint(), keep_clear)) {
            obstacle = candidate;
            return true;
        }
    }
    return false;
}

/// Lists the obstacles, one per line.
std::string describe_obstacles(const World& world) {
    std::string plan;
    for (const Obstacle& obstacle : world.obstacles()) {
        plan += std::format("Obstacle: center ({:6.2f}, {:6.2f}), size {} m, yaw {:5.1f} deg\n", obstacle.base_center.x,
                            obstacle.base_center.y, 2.0f * obstacle.half_size,
                            obstacle.yaw * 180.0f / std::numbers::pi_v<float>);
    }
    return plan;
}

}  // namespace

Level DebugLayout::generate(float clearance, LevelRng& rng) const {
    const WorldBounds& bounds = config_.bounds;
    const glm::vec3 start{(bounds.x_min + bounds.x_max) / 2.0f, (bounds.y_min + bounds.y_max) / 2.0f, 0.0f};
    const Circle keep_clear{.center = {start.x, start.y}, .radius = clearance};

    World world(bounds);
    for (int i = 0; i < config_.obstacle_count; ++i) {
        Obstacle obstacle;
        if (!try_place_obstacle(config_, keep_clear, rng, obstacle)) {
            throw std::runtime_error("level generation failed: no room for an obstacle");
        }
        world.add_obstacle(obstacle);
    }
    std::string plan = describe_obstacles(world);
    return {.layout = LayoutKind::kDebug, .world = std::move(world), .player_start = start, .plan = std::move(plan)};
}

}  // namespace manifold
