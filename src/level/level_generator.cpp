#include "level/level_generator.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <stdexcept>

#include <glm/vec3.hpp>

#include "world/geometry.hpp"
#include "world/world.hpp"

namespace manifold {

namespace {

// Places one obstacle, or returns false if every attempt overlapped `keep_clear`.
bool try_place_obstacle(const LevelConfig& config, const Circle& keep_clear, std::mt19937_64& rng, Obstacle& obstacle) {
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

}  // namespace

World generate_level(const LevelConfig& config, const Circle& keep_clear, std::uint64_t seed) {
    // Note: standard library distributions differ between implementations, so the same seed gives
    // the same level only with the same standard library.
    std::mt19937_64 rng(seed);
    World world(config.bounds);
    for (int i = 0; i < config.obstacle_count; ++i) {
        Obstacle obstacle;
        if (!try_place_obstacle(config, keep_clear, rng, obstacle)) {
            throw std::runtime_error("level generation failed: no room for an obstacle");
        }
        world.add_obstacle(obstacle);
    }
    return world;
}

}  // namespace manifold
