#include "render/texture_image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <stb_image_write.h>

#include "world/door.hpp"

namespace manifold {
namespace {

/// Creates an empty directory for one test and removes it afterwards.
class TextureImageTest : public testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() /
               ("manifold_" + std::string(testing::UnitTest::GetInstance()->current_test_info()->name()));
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    /// Writes a PNG of one color at `relative` inside the test directory, creating directories as needed.
    std::filesystem::path write_png(const std::string& relative, int width, int height) const {
        const std::filesystem::path path = dir_ / relative;
        std::filesystem::create_directories(path.parent_path());
        const std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4,
                                               200);
        EXPECT_NE(stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4), 0);
        return path;
    }

    /// Writes a file that is not a valid PNG.
    std::filesystem::path write_broken(const std::string& relative) const {
        const std::filesystem::path path = dir_ / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "not a png";
        return path;
    }

    const std::filesystem::path& dir() const { return dir_; }

private:
    std::filesystem::path dir_;
};

TEST_F(TextureImageTest, LoadsSizeAndRgbaPixels) {
    const std::optional<Image> image = load_image(write_png("a.png", 3, 2));
    ASSERT_TRUE(image.has_value());
    EXPECT_EQ(image->width, 3);
    EXPECT_EQ(image->height, 2);
    ASSERT_EQ(image->pixels.size(), 3U * 2U * 4U);
    EXPECT_EQ(image->pixels[0], 200);
}

TEST_F(TextureImageTest, BrokenOrMissingFileGivesNothing) {
    testing::internal::CaptureStderr();
    EXPECT_FALSE(load_image(write_broken("broken.png")).has_value());
    EXPECT_FALSE(load_image(dir() / "missing.png").has_value());
    EXPECT_NE(testing::internal::GetCapturedStderr().find("[warning]"), std::string::npos);
}

TEST_F(TextureImageTest, FindsPngFilesInSubdirectoriesSorted) {
    const auto b = write_png("pack2/b.png", 1, 1);
    const auto a = write_png("pack1/nested/a.PNG", 1, 1);
    const auto c = write_png("pack2/c.png", 1, 1);
    write_broken("pack2/readme.md");
    EXPECT_EQ(find_textures(dir()), (std::vector<std::filesystem::path>{a, b, c}));
}

TEST_F(TextureImageTest, MissingDirectoryGivesNoTextures) { EXPECT_TRUE(find_textures(dir() / "missing").empty()); }

TEST_F(TextureImageTest, SkipsTexturesThatAreTooLargeOrBroken) {
    const std::vector<std::filesystem::path> candidates{
        write_png("wide.png", kMaxTextureSize + 1, 4),
        write_png("tall.png", 4, kMaxTextureSize + 1),
        write_broken("broken.png"),
        write_png("ok.png", kMaxTextureSize, kMaxTextureSize),
    };
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
        testing::internal::CaptureStderr();
        const std::optional<PickedTexture> picked = pick_texture(candidates, seed);
        const std::string log = testing::internal::GetCapturedStderr();
        ASSERT_TRUE(picked.has_value());
        EXPECT_EQ(picked->path, candidates[3]);
        EXPECT_EQ(picked->image.width, kMaxTextureSize);
        EXPECT_NE(log.find("[info] Texture: " + candidates[3].string()), std::string::npos) << log;
    }
}

TEST_F(TextureImageTest, WarnsAboutTooLargeTexture) {
    const std::vector<std::filesystem::path> candidates{write_png("big.png", kMaxTextureSize * 2, 8)};
    testing::internal::CaptureStderr();
    EXPECT_FALSE(pick_texture(candidates, 1).has_value());
    EXPECT_NE(testing::internal::GetCapturedStderr().find("[warning] Texture " + candidates[0].string() + " is 256x8"),
              std::string::npos);
}

TEST_F(TextureImageTest, SameSeedPicksSameTextureAndSeedsVary) {
    std::vector<std::filesystem::path> candidates;
    candidates.reserve(6);
    for (int i = 0; i < 6; ++i) {
        candidates.push_back(write_png("t" + std::to_string(i) + ".png", 2, 2));
    }
    testing::internal::CaptureStderr();
    EXPECT_EQ(pick_texture(candidates, 5).value().path, pick_texture(candidates, 5).value().path);
    bool varies = false;
    for (std::uint64_t seed = 1; seed < 20; ++seed) {
        varies = varies || pick_texture(candidates, seed).value().path != pick_texture(candidates, 0).value().path;
    }
    testing::internal::GetCapturedStderr();
    EXPECT_TRUE(varies);
}

TEST_F(TextureImageTest, NoCandidatesGivesNothing) { EXPECT_FALSE(pick_texture({}, 0).has_value()); }

TEST(FallbackTextureTest, IsOneOpaquePixel) {
    const Image image = fallback_texture();
    EXPECT_EQ(image.width, 1);
    EXPECT_EQ(image.height, 1);
    ASSERT_EQ(image.pixels.size(), 4U);
    EXPECT_EQ(image.pixels[3], 255);
}

TEST(MeanLinearColorTest, AveragesInLinearRgb) {
    // Black and white average to 0.5 in linear RGB, not to sRGB 128 (0.216 linear). Opacity is ignored.
    const Image image{.width = 2, .height = 1, .pixels = {0, 0, 255, 0, 255, 255, 255, 255}};
    const glm::vec3 mean = mean_linear_color(image);
    EXPECT_FLOAT_EQ(mean.r, 0.5f);
    EXPECT_FLOAT_EQ(mean.g, 0.5f);
    EXPECT_FLOAT_EQ(mean.b, 1.0f);
}

TEST(MeanLinearColorTest, ConvertsSrgbToLinear) {
    const glm::vec3 mean = mean_linear_color({.width = 1, .height = 1, .pixels = {128, 10, 188, 255}});
    EXPECT_NEAR(mean.r, 0.2158f, 1e-4f);
    EXPECT_NEAR(mean.g, 10.0f / 255.0f / 12.92f, 1e-6f);
    EXPECT_NEAR(mean.b, 0.5029f, 1e-4f);
}

TEST(MeanLinearColorTest, EmptyImageIsBlack) { EXPECT_EQ(mean_linear_color(Image{}), glm::vec3(0.0f)); }

/// The sRGB color of pixel (x, y) of an RGBA image.
std::array<int, 3> pixel_at(const Image& image, int x, int y) {
    const auto index = static_cast<std::size_t>((y * image.width) + x) * 4;
    return {image.pixels.at(index), image.pixels.at(index + 1), image.pixels.at(index + 2)};
}

TEST(DoorTextureTest, IsOpaqueSquareAndTheSameEveryTime) {
    for (const DoorLook look : {DoorLook::kWood, DoorLook::kMetal}) {
        const Image door = make_door_texture(look);
        EXPECT_EQ(door.width, kDoorTextureSize);
        EXPECT_EQ(door.height, kDoorTextureSize);
        ASSERT_EQ(door.pixels.size(), static_cast<std::size_t>(kDoorTextureSize) * kDoorTextureSize * 4);
        for (std::size_t i = 3; i < door.pixels.size(); i += 4) {
            EXPECT_EQ(door.pixels[i], 255);
        }
        EXPECT_EQ(make_door_texture(look).pixels, door.pixels);
    }
    EXPECT_NE(make_door_texture(DoorLook::kWood).pixels, make_door_texture(DoorLook::kMetal).pixels);
}

TEST(DoorTextureTest, WoodIsBrownAndTheBandIsGrey) {
    const Image wood = make_door_texture(DoorLook::kWood);
    const std::array<int, 3> plank = pixel_at(wood, 5, 10);
    EXPECT_GT(plank[0], plank[2] + 30);
    const std::array<int, 3> band = pixel_at(wood, 5, 45);
    EXPECT_LT(band[0] - band[2], 20);
}

TEST(DoorTextureTest, MetalIsGreyWithALitRibAndDarkSeams) {
    const Image metal = make_door_texture(DoorLook::kMetal);
    const std::array<int, 3> plate = pixel_at(metal, 10, 10);
    EXPECT_LE(plate[0], plate[2]);  // Grey, slightly blue.
    EXPECT_LT(plate[2] - plate[0], 20);
    const std::array<int, 3> seam = pixel_at(metal, 32, 10);
    EXPECT_LT(seam[0], plate[0] / 2);
    const std::array<int, 3> rib_top = pixel_at(metal, 10, 40);
    EXPECT_GT(rib_top[0], plate[0]);
}

}  // namespace
}  // namespace manifold
