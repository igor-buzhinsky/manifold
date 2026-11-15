#include "world/door.hpp"

#include <array>
#include <numbers>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "world/obstacle.hpp"

namespace manifold {
namespace {

constexpr Door kDoor{.base_center = {1.0f, 2.0f, 3.0f}, .width = 2.0f, .height = 3.0f, .yaw = 0.0f};

TEST(DoorTest, FramesAreDarkBlocksAtBothEndsOfTheOpening) {
    const Obstacle negative = kDoor.frame(DoorSide::kNegative);
    const Obstacle positive = kDoor.frame(DoorSide::kPositive);
    EXPECT_EQ(negative.surface, Surface::kDark);
    EXPECT_EQ(positive.surface, Surface::kDark);
    EXPECT_FLOAT_EQ(negative.base_center.x, 0.1f);
    EXPECT_FLOAT_EQ(positive.base_center.x, 1.9f);
    EXPECT_FLOAT_EQ(negative.base_center.y, 2.0f);
    EXPECT_FLOAT_EQ(negative.base_center.z, 3.0f);
    EXPECT_EQ(negative.half_size, glm::vec2(Door::kFrameWidth / 2.0f, Door::kFrameDepth / 2.0f));
    EXPECT_FLOAT_EQ(negative.height, 3.0f);
}

TEST(DoorTest, BlockerFillsTheGapBetweenTheFrames) {
    const Obstacle blocker = kDoor.blocker();
    EXPECT_FLOAT_EQ(kDoor.gap(), 1.6f);
    EXPECT_EQ(blocker.base_center, kDoor.base_center);
    EXPECT_EQ(blocker.half_size, glm::vec2(0.8f, Door::kThickness / 2.0f));
    EXPECT_FLOAT_EQ(blocker.height, 3.0f);
}

TEST(DoorTest, ClosedHalvesMeetInTheMiddle) {
    const std::array<Obstacle, 2> halves = kDoor.halves(0.0f);
    EXPECT_FLOAT_EQ(halves[0].base_center.x, 0.6f);
    EXPECT_FLOAT_EQ(halves[1].base_center.x, 1.4f);
    for (const Obstacle& half : halves) {
        EXPECT_EQ(half.half_size, glm::vec2(0.4f, Door::kThickness / 2.0f));
        EXPECT_FLOAT_EQ(half.height, 3.0f);
    }
}

TEST(DoorTest, OpenHalvesAreBehindTheFramesInTheWalls) {
    const std::array<Obstacle, 2> halves = kDoor.halves(1.0f);
    // The inner edges reach the outer sides of the frames, which are the ends of the opening.
    EXPECT_NEAR(halves[0].base_center.x + halves[0].half_size.x, 0.0f, 1e-6f);
    EXPECT_NEAR(halves[1].base_center.x - halves[1].half_size.x, 2.0f, 1e-6f);
    const std::array<Obstacle, 2> half_open = kDoor.halves(0.5f);
    EXPECT_FLOAT_EQ(half_open[0].base_center.x, 0.1f);
    EXPECT_FLOAT_EQ(half_open[1].base_center.x, 1.9f);
    // Openness outside of [0, 1] is clamped.
    EXPECT_FLOAT_EQ(kDoor.halves(2.0f)[0].base_center.x, halves[0].base_center.x);
}

TEST(DoorTest, YawTurnsTheWall) {
    Door door = kDoor;
    door.yaw = std::numbers::pi_v<float> / 2.0f;
    const Obstacle frame = door.frame(DoorSide::kPositive);
    EXPECT_NEAR(frame.base_center.x, 1.0f, 1e-6f);
    EXPECT_NEAR(frame.base_center.y, 2.9f, 1e-6f);
    EXPECT_FLOAT_EQ(frame.yaw, door.yaw);
}

TEST(DoorStateTest, MovesToItsTargetInMoveTime) {
    DoorState door;
    EXPECT_FALSE(door.moving());
    EXPECT_FALSE(door.passable());
    door.target = 1.0f;
    EXPECT_TRUE(door.moving());
    door.advance(Door::kMoveTime / 4.0f);
    EXPECT_FLOAT_EQ(door.openness, 0.25f);
    EXPECT_FALSE(door.passable());
    door.advance(Door::kMoveTime);
    EXPECT_FLOAT_EQ(door.openness, 1.0f);
    EXPECT_FALSE(door.moving());
    EXPECT_TRUE(door.passable());

    door.target = 0.0f;
    EXPECT_FALSE(door.passable());  // Closing doors block at once.
    door.advance(Door::kMoveTime / 2.0f);
    EXPECT_FLOAT_EQ(door.openness, 0.5f);
    door.advance(Door::kMoveTime);
    EXPECT_FLOAT_EQ(door.openness, 0.0f);
    EXPECT_FALSE(door.moving());
}

}  // namespace
}  // namespace manifold
