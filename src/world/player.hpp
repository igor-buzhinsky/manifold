#pragma once

#include <glm/vec3.hpp>

#include "world/geometry.hpp"

namespace manifold {

// The player character. The position is the point between the feet, in metres.
// It is 3D already, although for now the player always stays on the floor (z = 0).
class Player {
public:
    Player(const glm::vec3& position, float radius) : position_(position), radius_(radius) {}

    const glm::vec3& position() const { return position_; }
    void set_position(const glm::vec3& position) { position_ = position; }

    // Radius of the player's collision body.
    float radius() const { return radius_; }

    // The area the player occupies on the floor.
    Circle footprint() const { return {.center = {position_.x, position_.y}, .radius = radius_}; }

private:
    glm::vec3 position_;
    float radius_;
};

}  // namespace manifold
