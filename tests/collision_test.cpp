#include "physics/collision.hpp"

#include <array>
#include <numbers>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "world/geometry.hpp"
#include "world/obstacle.hpp"
#include "world/world.hpp"

namespace manifold {
namespace {

constexpr WorldBounds kBounds{.x_min = -10.0f, .x_max = 10.0f, .y_min = -10.0f, .y_max = 10.0f, .height = 4.0f};
constexpr float kRadius = 0.3f;
constexpr float kHeight = 1.8f;

// An axis-aligned 2 x 2 m block from the floor to the ceiling, centered at (2, 0).
constexpr Obstacle kBlock{.base_center = {2.0f, 0.0f, 0.0f}, .half_size = glm::vec2(1.0f), .height = 4.0f, .yaw = 0.0f};

VerticalCylinder body_at(const glm::vec3& base) { return {.base = base, .radius = kRadius, .height = kHeight}; }

void expect_near(const glm::vec3& actual, const glm::vec3& expected, float tolerance = 1e-4f) {
    EXPECT_NEAR(glm::distance(actual, expected), 0.0f, tolerance) << actual.x << ", " << actual.y << ", " << actual.z;
}

TEST(ObstacleContactTest, NoContactWhenApart) {
    EXPECT_FALSE(find_contact(body_at({0.5f, 0.0f, 0.0f}), kBlock).has_value());
    EXPECT_FALSE(find_contact(body_at({0.69f, 0.0f, 0.0f}), kBlock).has_value());
}

TEST(ObstacleContactTest, SideContactPushesOutward) {
    const std::optional<Contact> contact = find_contact(body_at({0.8f, 0.0f, 0.0f}), kBlock);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {-1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(contact->depth, 0.1f, 1e-5f);
}

TEST(ObstacleContactTest, CornerContactPushesDiagonally) {
    // The corner (1, 1) is 0.2 m away along the diagonal.
    const float offset = 0.2f / std::numbers::sqrt2_v<float>;
    const std::optional<Contact> contact = find_contact(body_at({1.0f - offset, 1.0f + offset, 0.0f}), kBlock);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, glm::normalize(glm::vec3(-1.0f, 1.0f, 0.0f)));
    EXPECT_NEAR(contact->depth, 0.1f, 1e-5f);
}

TEST(ObstacleContactTest, BodyInsideLeavesThroughNearestSide) {
    const std::optional<Contact> contact = find_contact(body_at({2.0f, 0.8f, 0.0f}), kBlock);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {0.0f, 1.0f, 0.0f});
    EXPECT_NEAR(contact->depth, 0.2f + kRadius, 1e-5f);
}

TEST(ObstacleContactTest, RotatedObstacle) {
    // Rotated by 45 degrees, a corner of the block points to -x and reaches x = 2 - sqrt(2).
    Obstacle rotated = kBlock;
    rotated.yaw = std::numbers::pi_v<float> / 4.0f;
    const float corner_x = 2.0f - std::numbers::sqrt2_v<float>;
    EXPECT_FALSE(find_contact(body_at({corner_x - kRadius - 0.01f, 0.0f, 0.0f}), rotated).has_value());
    const std::optional<Contact> contact = find_contact(body_at({corner_x - kRadius + 0.1f, 0.0f, 0.0f}), rotated);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {-1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(contact->depth, 0.1f, 1e-5f);
}

TEST(ObstacleContactTest, BodyAboveLowObstacleHasNoContact) {
    Obstacle low = kBlock;
    low.height = 0.5f;
    EXPECT_FALSE(find_contact(body_at({2.0f, 0.0f, 0.6f}), low).has_value());
}

TEST(ObstacleContactTest, BodySlightlyInsideLowObstacleTopIsPushedUp) {
    Obstacle low = kBlock;
    low.height = 0.5f;
    const std::optional<Contact> contact = find_contact(body_at({2.0f, 0.0f, 0.4f}), low);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {0.0f, 0.0f, 1.0f});
    EXPECT_NEAR(contact->depth, 0.1f, 1e-5f);
}

TEST(BoundsContactTest, InsideHasNoContact) {
    EXPECT_FALSE(find_contact(body_at({0.0f, 0.0f, 0.0f}), kBounds).has_value());
    EXPECT_FALSE(find_contact(body_at({9.7f, -9.7f, 0.0f}), kBounds).has_value());
}

TEST(BoundsContactTest, WallsPushInward) {
    const std::optional<Contact> contact = find_contact(body_at({9.9f, 0.0f, 0.0f}), kBounds);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {-1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(contact->depth, 0.2f, 1e-5f);
}

TEST(BoundsContactTest, FloorAndCeilingPushVertically) {
    const std::optional<Contact> below = find_contact(body_at({0.0f, 0.0f, -0.5f}), kBounds);
    ASSERT_TRUE(below.has_value());
    expect_near(below->normal, {0.0f, 0.0f, 1.0f});
    const std::optional<Contact> above = find_contact(body_at({0.0f, 0.0f, 2.5f}), kBounds);
    ASSERT_TRUE(above.has_value());
    expect_near(above->normal, {0.0f, 0.0f, -1.0f});
}

// A ramp 4 m long and 2 m wide, rising by 3 m towards +x. Its low end is at x = -2, the high end at x = 2.
constexpr Ramp kRamp{.base_center = {0.0f, 0.0f, 0.0f}, .half_length = 2.0f, .half_width = 1.0f, .rise = 3.0f};

TEST(RampContactTest, NoContactWhenApart) {
    EXPECT_FALSE(find_contact(body_at({-3.0f, 0.0f, 0.0f}), kRamp).has_value());
    EXPECT_FALSE(find_contact(body_at({0.0f, 2.0f, 0.0f}), kRamp).has_value());
}

TEST(RampContactTest, NoContactAboveSlope) {
    // At the body's axis the slope is 1.5 m high, and 1.5 + 0.3 * 0.75 m at its uphill edge.
    EXPECT_FALSE(find_contact(body_at({0.0f, 0.0f, 1.8f}), kRamp).has_value());
}

TEST(RampContactTest, BodyOnSlopeIsPushedUpToHighestPointBelowIt) {
    const std::optional<Contact> contact = find_contact(body_at({0.0f, 0.0f, 1.5f}), kRamp);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {0.0f, 0.0f, 1.0f});
    EXPECT_NEAR(contact->depth, kRadius * 0.75f, 1e-5f);
}

TEST(RampContactTest, BodyAtLowEndIsLiftedOntoSlope) {
    // The body reaches 0.1 m over the low end, where the slope is 0.075 m high.
    const std::optional<Contact> contact = find_contact(body_at({-2.0f - kRadius + 0.1f, 0.0f, 0.0f}), kRamp);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {0.0f, 0.0f, 1.0f});
    EXPECT_NEAR(contact->depth, 0.075f, 1e-5f);
}

TEST(RampContactTest, BodyAtHighEndIsPushedAway) {
    const std::optional<Contact> contact = find_contact(body_at({2.0f + kRadius - 0.1f, 0.0f, 0.0f}), kRamp);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(contact->depth, 0.1f, 1e-5f);
}

TEST(RampContactTest, BodyAtSideIsPushedAway) {
    const std::optional<Contact> contact = find_contact(body_at({1.0f, -1.0f - kRadius + 0.1f, 0.0f}), kRamp);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {0.0f, -1.0f, 0.0f});
    EXPECT_NEAR(contact->depth, 0.1f, 1e-5f);
}

TEST(RampContactTest, YawTurnsTheSlope) {
    // Rising towards +y, the high end is at y = 2.
    Ramp ramp = kRamp;
    ramp.yaw = std::numbers::pi_v<float> / 2.0f;
    const std::optional<Contact> contact = find_contact(body_at({0.0f, 2.0f + kRadius - 0.1f, 0.0f}), ramp);
    ASSERT_TRUE(contact.has_value());
    expect_near(contact->normal, {0.0f, 1.0f, 0.0f});
    EXPECT_NEAR(contact->depth, 0.1f, 1e-5f);
}

TEST(MoveAndSlideTest, WalkingIntoRampGoesUpIt) {
    World world(kBounds);
    world.add_ramp(kRamp);
    VerticalCylinder body = body_at({-3.0f, 0.0f, 0.0f});
    for (int i = 0; i < 30; ++i) {
        body.base = move_and_slide(world, body, {0.1f, 0.0f, 0.0f}).position;
    }
    // At x = 0, the uphill edge of the body is at x = 0.3, where the slope is 1.5 + 0.225 m high.
    expect_near(body.base, {0.0f, 0.0f, 1.725f});
}

World make_world() {
    World world(kBounds);
    world.add_obstacle(kBlock);
    return world;
}

TEST(MoveAndSlideTest, FreeMovementIsUnchanged) {
    expect_near(move_and_slide(make_world(), body_at({-5.0f, 0.0f, 0.0f}), {1.0f, 2.0f, 0.0f}).position,
                {-4.0f, 2.0f, 0.0f});
}

TEST(MoveAndSlideTest, StopsAtWallWhenMovingStraightIntoIt) {
    expect_near(move_and_slide(make_world(), body_at({0.0f, 0.0f, 0.0f}), {2.0f, 0.0f, 0.0f}).position,
                {1.0f - kRadius, 0.0f, 0.0f});
}

TEST(MoveAndSlideTest, SlidesAlongWall) {
    // Moving diagonally into the block's -x side keeps the movement along the side.
    expect_near(move_and_slide(make_world(), body_at({0.6f, -0.5f, 0.0f}), {0.5f, 0.5f, 0.0f}).position,
                {1.0f - kRadius, 0.0f, 0.0f});
}

TEST(MoveAndSlideTest, LargeMovementDoesNotTunnelThroughObstacle) {
    const glm::vec3 end = move_and_slide(make_world(), body_at({0.0f, 0.0f, 0.0f}), {6.0f, 0.0f, 0.0f}).position;
    EXPECT_LE(end.x, 1.0f - kRadius + 1e-4f);
}

TEST(MoveAndSlideTest, StaysInsideLevel) {
    const glm::vec3 end = move_and_slide(make_world(), body_at({-9.0f, -9.0f, 0.0f}), {-5.0f, -1.0f, 0.0f}).position;
    expect_near(end, {-10.0f + kRadius, -10.0f + kRadius, 0.0f});
}

TEST(MoveAndSlideTest, SlidesAlongLevelWall) {
    const glm::vec3 end = move_and_slide(make_world(), body_at({-9.0f, -5.0f, 0.0f}), {-2.0f, 1.0f, 0.0f}).position;
    expect_near(end, {-10.0f + kRadius, -4.0f, 0.0f});
}

TEST(MoveAndSlideTest, FreeMovementHasNoCorrection) {
    EXPECT_EQ(move_and_slide(make_world(), body_at({-5.0f, 0.0f, 1.0f}), {0.0f, 0.0f, -0.5f}).correction,
              glm::vec3(0.0f));
}

TEST(MoveAndSlideTest, LandsOnFloor) {
    const SlideResult result = move_and_slide(make_world(), body_at({-5.0f, 0.0f, 0.2f}), {0.0f, 0.0f, -0.5f});
    expect_near(result.position, {-5.0f, 0.0f, 0.0f});
    expect_near(result.correction, {0.0f, 0.0f, 0.3f});
}

TEST(MoveAndSlideTest, BumpsIntoCeiling) {
    const SlideResult result = move_and_slide(make_world(), body_at({-5.0f, 0.0f, 2.0f}), {0.0f, 0.0f, 0.5f});
    expect_near(result.position, {-5.0f, 0.0f, 4.0f - kHeight});
    EXPECT_LT(result.correction.z, 0.0f);
}

TEST(CollisionTest, BlockersStopTheBodyLikeObstacles) {
    const World world(kBounds);
    const VerticalCylinder body = body_at({0.0f, 0.0f, 0.0f});
    const std::array<Obstacle, 1> blockers{kBlock};
    EXPECT_FLOAT_EQ(move_and_slide(world, body, {3.0f, 0.0f, 0.0f}).position.x, 3.0f);
    EXPECT_NEAR(move_and_slide(world, body, {3.0f, 0.0f, 0.0f}, blockers).position.x, 1.0f - kRadius, 1e-5f);
}

}  // namespace
}  // namespace manifold
