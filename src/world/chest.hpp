#pragma once

#include <cstdint>
#include <string_view>

#include <glm/vec3.hpp>

#include "util/asset_index.hpp"
#include "world/geometry.hpp"
#include "world/obstacle.hpp"

namespace manifold {

/// What a chest is made of, which sets its colors.
enum class ChestLook : std::uint8_t {
    kWood,  ///< Plain brown wood.
    kGold,  ///< Shiny yellow gold.
};

/// Short name of a chest look, for logs and the level info: "wooden" or "golden".
std::string_view chest_look_name(ChestLook look);

/// A small chest that stands on the floor, its back to a wall and a little away from it.
///
/// For now it is a box with a lid of another color on top, and nearly only decoration: it doesn't collide with
/// anything, and activating it only plays its sound, see Game::activate(). A chest is not part of the wall it stands
/// at: it keeps its place if the wall goes.
///
/// In its own frame, x runs along its width and y from its back to its front.
struct Chest {
    static constexpr float kWidth = 0.7f;       ///< Side to side, in metres.
    static constexpr float kDepth = 0.45f;      ///< Back to front, in metres.
    static constexpr float kHeight = 0.45f;     ///< Floor to the top of the lid, in metres.
    static constexpr float kLidHeight = 0.15f;  ///< Height of the lid, in metres.
    /// How far the lid reaches out over the body on each side, so that it reads as a lid, in metres.
    static constexpr float kLidOverhang = 0.01f;
    /// Gap between the back of a chest that stands at a wall and the wall, in metres.
    static constexpr float kWallGap = 0.05f;

    glm::vec3 base_center{0.0f};        ///< Center of the bottom, on the floor.
    float yaw = 0.0f;                   ///< Direction the front faces: radians counter-clockwise from +x.
    ChestLook look = ChestLook::kWood;  ///< What the chest is made of.
    /// The sound it makes when activated: which file of AssetKey::kChestOpenSounds, see generate_level().
    AssetIndex open_sound = 0;

    /// A chest that stands at a wall, centered on a point, with kWallGap between its back and the wall.
    ///
    /// \param wall_point A point on the face of the wall, at the height of the floor.
    /// \param yaw        Direction away from the wall, which the chest's front faces: radians counter-clockwise
    ///                   from +x.
    /// \param look       What the chest is made of.
    static Chest at_wall(const glm::vec3& wall_point, float yaw, ChestLook look);

    /// The lower part, which the lid closes, from the floor up to the lid.
    Obstacle body() const;
    /// The lid, on top of the body. A little larger than the body, see kLidOverhang.
    Obstacle lid() const;
    /// The area the chest occupies in the horizontal plane: that of the lid, which is the larger part.
    RotatedRectangle footprint() const;
};

}  // namespace manifold
