#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "world/door.hpp"

namespace manifold {

/// Textures larger than this in either dimension, in pixels, are not used.
constexpr int kMaxTextureSize = 128;

/// An image in memory, 8-bit RGBA, with sRGB colors.
struct Image {
    int width = 0;   ///< In pixels.
    int height = 0;  ///< In pixels.
    /// Four bytes (R, G, B, A) per pixel, row by row from the top row of the image.
    std::vector<std::uint8_t> pixels;
};

/// Loads a PNG file and converts it to RGBA.
///
/// \return The image, or an empty optional (after logging a warning) if the file can't be read or decoded.
std::optional<Image> load_image(const std::filesystem::path& path);

/// Loads the texture of a level: a PNG file of at most kMaxTextureSize pixels in each dimension.
///
/// \return The image, or an empty optional (after logging a warning) if the file can't be loaded or is too large.
std::optional<Image> load_level_texture(const std::filesystem::path& path);

/// The mean color of the pixels of `image`, in linear RGB, each component in [0, 1].
///
/// The sRGB colors of the pixels are converted to linear RGB before averaging, as the renderer samples them. Opacity
/// is ignored.
///
/// \return The mean color, or black for an empty image.
glm::vec3 mean_linear_color(const Image& image);

/// A plain grey 1 x 1 image, used when no texture can be loaded.
Image fallback_texture();

/// Width and height of the images of make_door_texture(), in pixels.
inline constexpr int kDoorTextureSize = 64;

/// Makes the texture of door halves, which are opaque:
/// - wood: four upright wooden planks with an iron band across them;
/// - metal: two brushed steel plates side by side, with a riveted rib across them.
///
/// The image is kDoorTextureSize pixels square and repeats seamlessly upwards and sideways. It is made in code, so it
/// doesn't depend on an asset file, and is the same every time.
Image make_door_texture(DoorLook look);

}  // namespace manifold
