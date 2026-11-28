#include "ui/damage_flash.hpp"

#include <gtest/gtest.h>

#include "ui/overlay_mesh.hpp"

namespace manifold {
namespace {

TEST(DamageFlashTest, FadesOutFromAPeakThatGrowsWithTheDamage) {
    EXPECT_FLOAT_EQ(damage_flash_opacity(0.0f, 1), kDamageFlashMinOpacity + (0.01f * 0.35f));
    EXPECT_FLOAT_EQ(damage_flash_opacity(0.0f, 100), kDamageFlashMaxOpacity);
    EXPECT_FLOAT_EQ(damage_flash_opacity(0.0f, 500), kDamageFlashMaxOpacity);
    EXPECT_FLOAT_EQ(damage_flash_opacity(kDamageFlashDuration / 2.0f, 100), kDamageFlashMaxOpacity / 2.0f);
    EXPECT_GT(damage_flash_opacity(0.0f, 50), damage_flash_opacity(0.0f, 10));
    EXPECT_FLOAT_EQ(damage_flash_opacity(kDamageFlashDuration, 100), 0.0f);
    EXPECT_FLOAT_EQ(damage_flash_opacity(-0.1f, 100), 0.0f);
    EXPECT_FLOAT_EQ(damage_flash_opacity(0.0f, 0), 0.0f);
}

TEST(DamageFlashTest, CoversThePictureOnlyWhileVisible) {
    OverlayMesh mesh;
    add_damage_flash(mesh, {.width = 800, .height = 450}, 0.0f);
    EXPECT_TRUE(mesh.vertices().empty());
    add_damage_flash(mesh, {.width = 800, .height = 450}, 0.5f);
    ASSERT_EQ(mesh.vertices().size(), 6U);
    for (const OverlayVertex& vertex : mesh.vertices()) {
        EXPECT_GE(vertex.position.x, 0.0f);
        EXPECT_LE(vertex.position.x, 800.0f);
        EXPECT_GE(vertex.position.y, 0.0f);
        EXPECT_LE(vertex.position.y, 450.0f);
        EXPECT_FLOAT_EQ(vertex.color.a, 0.5f);
        EXPECT_GT(vertex.color.r, vertex.color.g);
    }
}

}  // namespace
}  // namespace manifold
