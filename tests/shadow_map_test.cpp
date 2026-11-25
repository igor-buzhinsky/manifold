#include "render/shadow_map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "world/door.hpp"

namespace manifold {
namespace {

/// Where OpenGL samples a cube map in a direction: the face index and the texture coordinates in [0, 1].
///
/// This follows the table "Selection of cube map images" of the OpenGL 4.1 specification, section 3.8.10.
struct CubeSample {
    int face = 0;
    glm::vec2 st{0.0f};
};

CubeSample sample_cube(const glm::vec3& r) {
    const glm::vec3 a = glm::abs(r);
    float sc = 0.0f;
    float tc = 0.0f;
    float ma = 0.0f;
    int face = 0;
    if (a.x >= a.y && a.x >= a.z) {
        face = r.x > 0.0f ? 0 : 1;
        sc = r.x > 0.0f ? -r.z : r.z;
        tc = -r.y;
        ma = a.x;
    } else if (a.y >= a.z) {
        face = r.y > 0.0f ? 2 : 3;
        sc = r.x;
        tc = r.y > 0.0f ? r.z : -r.z;
        ma = a.y;
    } else {
        face = r.z > 0.0f ? 4 : 5;
        sc = r.z > 0.0f ? r.x : -r.x;
        tc = -r.y;
        ma = a.z;
    }
    return {.face = face, .st = {((sc / ma) + 1.0f) / 2.0f, ((tc / ma) + 1.0f) / 2.0f}};
}

TEST(ShadowMapTest, FacesAreLaidOutAsOpenGlSamplesThem) {
    const glm::vec3 light{1.0f, -2.0f, 1.5f};
    const float range = 6.0f;
    // Directions inside each face, away from the edges and not symmetric, so that any flip shows.
    const std::array<glm::vec3, 8> directions{{
        {1.0f, 0.3f, -0.6f},
        {-1.0f, -0.5f, 0.2f},
        {0.4f, 1.0f, 0.7f},
        {-0.2f, -1.0f, 0.5f},
        {0.6f, -0.3f, 1.0f},
        {-0.7f, 0.1f, -1.0f},
        {0.9f, -0.8f, 0.1f},
        {-0.3f, 0.2f, -0.9f},
    }};
    for (const glm::vec3& direction : directions) {
        const CubeSample expected = sample_cube(direction);
        const glm::vec4 clip =
            cube_face_view_projection(light, range, expected.face) * glm::vec4(light + (2.0f * direction), 1.0f);
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        // Window coordinates go up with y, as texture coordinates t do when OpenGL draws into a texture.
        EXPECT_NEAR((ndc.x + 1.0f) / 2.0f, expected.st.x, 1e-5f) << "face " << expected.face;
        EXPECT_NEAR((ndc.y + 1.0f) / 2.0f, expected.st.y, 1e-5f) << "face " << expected.face;
        EXPECT_GT(ndc.z, -1.0f);
        EXPECT_LT(ndc.z, 1.0f);
    }
}

TEST(ShadowMapTest, FaceProjectionReachesFromNearPlaneToRange) {
    const glm::vec3 light{0.0f, 0.0f, 0.0f};
    const float range = 6.0f;
    const auto depth_at = [&](float distance) {
        const glm::vec4 clip = cube_face_view_projection(light, range, 0) * glm::vec4(distance, 0.0f, 0.0f, 1.0f);
        return clip.z / clip.w;
    };
    EXPECT_NEAR(depth_at(kShadowNearPlane), -1.0f, 1e-4f);
    EXPECT_NEAR(depth_at(range), 1.0f, 1e-4f);
}

TEST(ShadowCacheTest, NewLightsGetFreeSlotsToDraw) {
    ShadowCache cache(4);
    const std::vector<std::uint32_t> ids{7, 3};
    const ShadowCache::Assignment assignment = cache.assign(ids);
    ASSERT_EQ(assignment.slots.size(), 2U);
    ASSERT_TRUE(assignment.slots[0] && assignment.slots[1]);
    EXPECT_NE(*assignment.slots[0], *assignment.slots[1]);
    EXPECT_EQ(assignment.to_draw, (std::vector<std::size_t>{*assignment.slots[0], *assignment.slots[1]}));
}

TEST(ShadowCacheTest, KnownLightsKeepTheirSlotsWithoutDrawing) {
    ShadowCache cache(4);
    const std::vector<std::uint32_t> first{7, 3};
    const ShadowCache::Assignment before = cache.assign(first);
    const std::vector<std::uint32_t> second{3, 9, 7};
    const ShadowCache::Assignment after = cache.assign(second);
    EXPECT_EQ(after.slots[0], before.slots[1]);
    EXPECT_EQ(after.slots[2], before.slots[0]);
    ASSERT_TRUE(after.slots[1]);
    EXPECT_EQ(after.to_draw, std::vector<std::size_t>{*after.slots[1]});
}

TEST(ShadowCacheTest, ReusesSlotUsedLongestAgo) {
    ShadowCache cache(2);
    const std::vector<std::uint32_t> a{1};
    const std::vector<std::uint32_t> b{2};
    const std::vector<std::uint32_t> c{3};
    const std::optional<std::size_t> slot_a = cache.assign(a).slots[0];
    cache.assign(b);
    const std::optional<std::size_t> slot_c = cache.assign(c).slots[0];
    EXPECT_EQ(slot_c, slot_a);
    // Light 2 still has its map, light 1 lost it.
    EXPECT_TRUE(cache.assign(b).to_draw.empty());
    EXPECT_EQ(cache.assign(a).to_draw.size(), 1U);
}

TEST(ShadowCacheTest, LightsBeyondSlotCountGetNoSlot) {
    ShadowCache cache(2);
    const std::vector<std::uint32_t> ids{1, 2, 3};
    const ShadowCache::Assignment assignment = cache.assign(ids);
    EXPECT_TRUE(assignment.slots[0].has_value());
    EXPECT_TRUE(assignment.slots[1].has_value());
    EXPECT_FALSE(assignment.slots[2].has_value());
    EXPECT_EQ(assignment.to_draw.size(), 2U);
}

TEST(ShadowCacheTest, NeverTakesSlotOfLightInSameRequest) {
    ShadowCache cache(2);
    const std::vector<std::uint32_t> old_ids{1, 2};
    cache.assign(old_ids);
    // Light 1 keeps its slot, light 3 takes light 2's slot, light 4 gets none.
    const std::vector<std::uint32_t> ids{1, 3, 4};
    const ShadowCache::Assignment assignment = cache.assign(ids);
    ASSERT_TRUE(assignment.slots[0] && assignment.slots[1]);
    EXPECT_NE(*assignment.slots[0], *assignment.slots[1]);
    EXPECT_FALSE(assignment.slots[2].has_value());
}

TEST(ShadowMapTest, LightReachesDoorWhereverItsHalvesSlide) {
    // A door 2 m wide along x, so its open halves reach 1.8 m from its center along x.
    const Door door{.base_center = {10.0f, 0.0f, 3.0f}, .width = 2.0f, .height = 3.0f, .yaw = 0.0f, .sounds = {}};
    // Through the opening, the light is measured from the face of the halves.
    EXPECT_TRUE(light_reaches_door({10.0f, 5.0f, 4.0f}, 5.0f, door));
    EXPECT_FALSE(light_reaches_door({10.0f, 5.06f, 4.0f}, 5.0f, door));
    // Along the wall, from the far end of an open half.
    EXPECT_TRUE(light_reaches_door({16.7f, 0.0f, 4.0f}, 5.0f, door));
    EXPECT_FALSE(light_reaches_door({16.9f, 0.0f, 4.0f}, 5.0f, door));
    // Above and below.
    EXPECT_TRUE(light_reaches_door({10.0f, 0.0f, 10.9f}, 5.0f, door));
    EXPECT_FALSE(light_reaches_door({10.0f, 0.0f, 11.1f}, 5.0f, door));
    EXPECT_TRUE(light_reaches_door({10.0f, 0.0f, -1.9f}, 5.0f, door));
    EXPECT_FALSE(light_reaches_door({10.0f, 0.0f, -2.1f}, 5.0f, door));

    // Turned to run along y, the door reaches along y instead.
    Door turned = door;
    turned.yaw = std::numbers::pi_v<float> / 2.0f;
    EXPECT_TRUE(light_reaches_door({10.0f, 6.7f, 4.0f}, 5.0f, turned));
    EXPECT_FALSE(light_reaches_door({16.7f, 0.0f, 4.0f}, 5.0f, turned));
}

}  // namespace
}  // namespace manifold
