#pragma once

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "world/geometry.hpp"

namespace manifold {

/// How the surfaces of an obstacle look.
enum class Surface : std::uint8_t {
    kTextured,  ///< The level texture.
    kDark,      ///< A plain, nearly black color, e.g. for door frames.
};

/// A block with a rectangular footprint, rotated around the vertical axis.
struct Obstacle {
    glm::vec3 base_center{0.0f};  ///< Center of the bottom face.
    glm::vec2 half_size{0.0f};    ///< Half of the side lengths of the footprint, along the block's own x and y axes.
    float height = 0.0f;
    float yaw = 0.0f;  ///< Radians, counter-clockwise around +z.
    Surface surface = Surface::kTextured;

    /// The area the obstacle occupies in the horizontal plane.
    RotatedRectangle footprint() const;
};

}  // namespace manifold
