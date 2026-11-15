#include "world/obstacle.hpp"

#include <glm/vec2.hpp>

#include "world/geometry.hpp"

namespace manifold {

RotatedRectangle Obstacle::footprint() const {
    return {.center = glm::vec2(base_center), .half_size = half_size, .yaw = yaw};
}

}  // namespace manifold
