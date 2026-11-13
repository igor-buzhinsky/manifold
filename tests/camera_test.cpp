#include "render/camera.hpp"

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "world/player.hpp"

namespace manifold {
namespace {

// Projects a world point to normalized device coordinates, where the visible area is [-1, 1]^3.
glm::vec3 project(const Camera& camera, const glm::vec3& point) {
    const glm::vec4 clip = camera.projection() * camera.view() * glm::vec4(point, 1.0f);
    return glm::vec3(clip) / clip.w;
}

TEST(CameraTest, ForwardFollowsYawAndPitch) {
    Camera camera;
    camera.yaw = std::numbers::pi_v<float> / 2.0f;
    EXPECT_NEAR(glm::distance(camera.forward(), glm::vec3(0.0f, 1.0f, 0.0f)), 0.0f, 1e-6f);
    camera.pitch = std::numbers::pi_v<float> / 2.0f;
    EXPECT_NEAR(glm::distance(camera.forward(), glm::vec3(0.0f, 0.0f, 1.0f)), 0.0f, 1e-6f);
}

TEST(CameraTest, PointAheadIsInScreenCenter) {
    const Camera camera{.position = {1.0f, 2.0f, 1.5f}, .yaw = 0.0f};
    const glm::vec3 ndc = project(camera, {11.0f, 2.0f, 1.5f});
    EXPECT_NEAR(ndc.x, 0.0f, 1e-5f);
    EXPECT_NEAR(ndc.y, 0.0f, 1e-5f);
    EXPECT_GT(ndc.z, -1.0f);
    EXPECT_LT(ndc.z, 1.0f);
}

TEST(CameraTest, ScreenAxesMatchWorldDirections) {
    // Looking along +x with z up, +y is to the left.
    const Camera camera{.position = {0.0f, 0.0f, 1.5f}, .yaw = 0.0f};
    EXPECT_LT(project(camera, {10.0f, 1.0f, 1.5f}).x, 0.0f);
    EXPECT_GT(project(camera, {10.0f, 0.0f, 2.5f}).y, 0.0f);
}

TEST(CameraTest, LookingStraightDownShowsTheYawDirectionAtTheTop) {
    const Camera camera{.position = {0.0f, 0.0f, 20.0f},
                        .yaw = std::numbers::pi_v<float> / 2.0f,
                        .pitch = -std::numbers::pi_v<float> / 2.0f};
    const glm::vec3 below = project(camera, {0.0f, 0.0f, 0.0f});
    EXPECT_NEAR(below.x, 0.0f, 1e-5f);
    EXPECT_NEAR(below.y, 0.0f, 1e-5f);
    // +y, the yaw direction, is at the top, and +x to the right.
    EXPECT_GT(project(camera, {0.0f, 2.0f, 0.0f}).y, 0.0f);
    EXPECT_GT(project(camera, {2.0f, 0.0f, 0.0f}).x, 0.0f);
}

TEST(CameraTest, PointBehindIsNotVisible) {
    const Camera camera{.position = {0.0f, 0.0f, 1.5f}, .yaw = 0.0f};
    const glm::vec4 clip = camera.projection() * camera.view() * glm::vec4(-5.0f, 0.0f, 1.5f, 1.0f);
    EXPECT_LT(clip.w, 0.0f);
}

TEST(CameraTest, PlayerCameraIsAtEyes) {
    const Player player({.position = {3.0f, 4.0f, 0.0f}, .yaw = 1.0f, .radius = 0.3f, .height = 1.8f});
    const Camera camera = make_player_camera(player);
    EXPECT_EQ(camera.position, player.eye_position());
    EXPECT_FLOAT_EQ(camera.yaw, 1.0f);
    EXPECT_FLOAT_EQ(camera.pitch, 0.0f);
}

TEST(CameraTest, PlayerCameraFollowsPitch) {
    Player player({});
    player.rotate(0.0f, 0.3f);
    EXPECT_FLOAT_EQ(make_player_camera(player).pitch, 0.3f);
}

}  // namespace
}  // namespace manifold
