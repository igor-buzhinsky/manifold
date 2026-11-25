#pragma once

#include <array>
#include <cstdint>

#include <glm/vec3.hpp>

#include "util/asset_index.hpp"
#include "world/obstacle.hpp"

namespace manifold {

/// What the halves of the doors of a level are made of, which sets their texture, see make_door_texture().
enum class DoorLook : std::uint8_t {
    kWood,   ///< Upright wooden planks with an iron band across them, repeated up the door.
    kMetal,  ///< Brushed steel plates with a riveted rib across them, repeated up the door.
};

/// The name of a door look, for the log: `wood` or `metal`.
const char* door_look_name(DoorLook look);

/// Which of the two sides of a door, along the wall.
enum class DoorSide : std::uint8_t {
    kNegative,  ///< Towards -u: the side the door's own u axis points away from.
    kPositive,  ///< Towards +u.
};

/// Which sounds a door makes, chosen at random when the level is generated, see generate_level().
struct DoorSounds {
    AssetIndex open = 0;   ///< The sound of starting to open: which file of AssetKey::kDoorOpenSounds.
    AssetIndex close = 0;  ///< The sound of starting to close: which file of AssetKey::kDoorCloseSounds.
};

/// A sliding door in an opening between two rooms: two halves that slide apart into the walls on both sides.
///
/// In the door's own frame, u runs along the wall, across the opening, and v goes through the opening. A frame stands
/// at each end of the opening along u: a dark block from the floor to the top of the opening. The two halves fill the
/// gap between the frames. Opening, they slide apart along u, behind the frames and into the walls, until they are
/// hidden. They are kThickness thick along v. See World::add_door() and DoorState.
struct Door {
    static constexpr float kFrameWidth = 0.2f;  ///< Size of a frame along the wall, in metres.
    static constexpr float kFrameDepth = 0.4f;  ///< Size of a frame through the opening, in metres.
    static constexpr float kThickness = 0.1f;   ///< Size of the halves through the opening, in metres.
    static constexpr float kMoveTime = 1.0f;    ///< Seconds the door takes to open or to close.

    glm::vec3 base_center{0.0f};  ///< Center of the opening at its floor.
    float width = 0.0f;           ///< Width of the opening, from wall to wall, in metres. More than 2 kFrameWidth.
    float height = 0.0f;          ///< Height of the opening, in metres.
    float yaw = 0.0f;             ///< Direction of u, along the wall: radians counter-clockwise from +x.
    DoorSounds sounds;            ///< Which sounds it makes.

    /// Width of the gap between the frames, which the halves fill when closed, in metres.
    float gap() const { return width - (2.0f * kFrameWidth); }

    /// The frame at one side, a Surface::kDark block.
    Obstacle frame(DoorSide side) const;

    /// The block between the frames that a door blocks while it is not fully open: the closed halves.
    Obstacle blocker() const;

    /// The two halves, as blocks, at the negative side first.
    ///
    /// \param openness How far the door is open, from 0 (closed) to 1 (open, with the halves hidden in the walls).
    std::array<Obstacle, 2> halves(float openness) const;
};

/// How far a door is open and where it is moving.
///
/// A door moves between closed (openness 0) and open (openness 1) at a constant speed, in Door::kMoveTime.
struct DoorState {
    float openness = 0.0f;  ///< From 0 (closed) to 1 (open).
    float target = 0.0f;    ///< Where the door moves: 0 (closing, or closed) or 1 (opening, or open).

    /// Whether the door is opening or closing.
    bool moving() const { return openness != target; }

    /// Whether the door lets bodies through: it is fully open and not closing.
    bool passable() const { return openness >= 1.0f && target >= 1.0f; }

    /// Moves the door towards its target.
    ///
    /// \param dt Duration of the frame in seconds.
    void advance(float dt);
};

}  // namespace manifold
